# 12. 젤리 애니메이션 — 절차적 관성 표현과 사망 Ragdoll

## 물리 시뮬레이션을 상시로 켜지 않은 이유

플레이어 4명과 NPC 최대 60명이 같은 젤리 캐릭터를 쓴다. 모든 캐릭터의 본을 상시 물리 시뮬레이션하면 비용이 크고, 시뮬레이션 결과가 클라이언트마다 달라 위치 판정과도 어긋난다. 그래서 역할을 나눴다.

| 상황 | 담당 | 방식 |
|---|---|---|
| 평소 이동 | `UDMJellyAnimInstance` | 감쇠 스프링 7개로 관성·흔들림 각도를 계산해 AnimGraph에 전달 |
| 실제 이동·충돌 | `CharacterMovement` | 그대로 유지 |
| 사망 | `UDMJellyPhysicsComponent` | 그때만 Mesh 물리 시뮬레이션 전환 |

## AnimInstance 계산

`NativeUpdateAnimation`이 수평 속도, 가속·감속, 착지·공중 상태를 읽어 몸통·머리·양팔의 목표 각도를 정하고, `AdvanceJellySpring`이 현재 각도를 목표로 끌어당긴다.

```text
AngularFrequency = 2πf
Stiffness        = ω²
Damping          = 2ζω
Acceleration     = Stiffness × (Target − Current) − Damping × Velocity
```

자연주파수 1.8Hz, 감쇠비 0.55를 기준으로, 각도와 각속도를 Semi-implicit Euler로 적분한다.

| 처리 | 이유 |
|---|---|
| 프레임을 최대 12개의 작은 단계로 나눠 적분 | 강성이 높을 때 긴 프레임에서 생기는 수치 발산 방지 |
| 각속도 ±360°/s 제한 | 비정상적으로 큰 값 차단 |
| 위치가 크게 튀거나 업데이트 간격이 0.25초를 넘으면 스프링 초기화 | 네트워크 위치 보정이나 순간이동 뒤 뼈가 튀는 것 방지. 물리 위치가 아니라 **시각 상태만** 초기화 |
| 몸통 회전을 `Spine1`·`Spine2`·`Ribcage`에 비율로 분산 | 한 본에 몰리면 꺾여 보인다 |
| 착지 압축은 `Ribcage` Translation에만 | Pelvis·Capsule·실제 이동 위치는 건드리지 않는다 |
| 공격 25%, 방어 15%, 스턴 20%, 몽타주 재생 중 0% | 의도한 동작 애니메이션을 흔들림이 덮지 않게 한다 |

이 값들은 **AnimGraph에 넘기는 시각적 회전 파라미터**다. 복제하지 않고, 각 클라이언트가 관측한 속도로 계산한다. 원격 캐릭터의 스턴 흔들림은 실제 상태 태그 대신 복제된 표현용 bool(`bStunPresentationActive`)로 판단한다.

## 사망 Ragdoll

`StartDeathRagdollPresentation`은 서버에서만 시작하고, `MulticastStartDeathRagdollPresentation`으로 각 클라이언트에 알린다.

```cpp
// ApplyDeathRagdollPresentation
// 사망 캐릭터의 Capsule 충돌을 끄고, Mesh는 월드에만 부딪히게 한다.
MeshComponent->SetCollisionResponseToAllChannels(ECR_Ignore);
MeshComponent->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);
MeshComponent->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);
MeshComponent->SetSimulatePhysics(true);
```

- 시체가 다른 Pawn이나 전투 판정용 Visibility 트레이스를 막지 않는다.
- 이미 Ragdoll 중이면 다시 전환하지 않고, PhysicsAsset이 없으면 경고 후 실행하지 않는다.
- 시작 신호만 보내고 뼈별 자세는 동기화하지 않는다. 클라이언트마다 쓰러진 모양이 조금 달라도 게임 판정에는 영향이 없다.
- 생존 상태의 원본은 여전히 서버 PlayerState의 `LifeState`다.

## 관련 코드

- [DMJellyAnimInstance.cpp](../../Source/DontMelt/Private/Animations/DMJellyAnimInstance.cpp) — `NativeUpdateAnimation`, `AdvanceJellySpring`, `UpdateJellyBodyMotion`, `UpdateJellySecondaryMotion`
- [DMJellyPhysicsComponent.cpp](../../Source/DontMelt/Private/Characters/Components/DMJellyPhysicsComponent.cpp) — `StartDeathRagdollPresentation`, `ApplyDeathRagdollPresentation`
- [DMJellyCharacterBase.cpp](../../Source/DontMelt/Private/Characters/DMJellyCharacterBase.cpp)

## 핵심 코드

```cpp
const float AngularFrequency = 2.0f * PI * SpringFrequency;
const float Stiffness = AngularFrequency * AngularFrequency;
const float Damping = 2.0f * DampingRatio * AngularFrequency;

// 높은 강성에서 프레임 간격이 커질 때 발생하는
// 수치 불안정을 줄이기 위해 작은 시간 단계로 나눈다.
const float MaxStepSeconds = FMath::Max(JellyTuning.SpringMaxStepSeconds, KINDA_SMALL_NUMBER);
const int32 StepCount = FMath::Clamp(FMath::CeilToInt(DeltaSeconds / MaxStepSeconds), 1, 12);
const float StepSeconds = DeltaSeconds / static_cast<float>(StepCount);

for (int32 StepIndex = 0; StepIndex < StepCount; ++StepIndex)
{
    // 목표 각도와의 차이를 이용해 복원 가속도를 구한다.
    const float SpringAcceleration = Stiffness * (TargetAngle - InOutAngle) - Damping * InOutVelocity;

    // Semi-implicit Euler 방식으로 적분한다.
    InOutVelocity += SpringAcceleration * StepSeconds;

    // 비정상적으로 큰 각속도를 제한한다.
    InOutVelocity = FMath::Clamp(InOutVelocity, -360.0f, 360.0f);

    InOutAngle += InOutVelocity * StepSeconds;
}
```

수치·본 구성·AnimGraph 연결은 [11.5단계 설계](../Design/Stages/11_5_JellyAnimationAndRagdoll.md)에 있다.

---

[Technical 목차](README.md)
