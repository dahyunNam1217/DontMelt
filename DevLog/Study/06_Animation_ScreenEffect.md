# 애니메이션·화면 효과

## 기본 애니메이션 위에 Bone 회전 더하기

걷기·달리기 애니메이션은 그대로 두고, 몸통·머리·팔 Bone에 추가 회전을 더해 흔들림을 만든다. `UDMJellyAnimInstance`가 회전값을 계산하고, AnimGraph가 그 값을 Bone에 적용한다.

몸통 회전을 한 Bone에 몰면 그 지점에서 꺾여 보인다. 그래서 `Spine1`·`Spine2`·`Ribcage`에 비율로 나눠 줬다. 착지 압축은 `Ribcage` Translation에만 넣어, Pelvis·Capsule·실제 이동 위치는 건드리지 않는다.

Bone Space와 Component Space를 혼동하면 회전 축이 엉뚱하게 잡힌다. Skeleton의 Bone 이름과 계층도 계산 쪽 이름과 맞아야 한다.

소스: [DMJellyAnimInstance.cpp](../../Source/DontMelt/Private/Animations/DMJellyAnimInstance.cpp) · [DMJellyAnimInstance.h](../../Source/DontMelt/Public/Animations/DMJellyAnimInstance.h)

## 감쇠 스프링과 수치 적분

스프링은 목표값으로 돌아가려는 복원력과, 진동을 줄이는 감쇠를 함께 쓴다. 강성과 감쇠 계수를 직접 넣지 않고, 흔들림의 빠르기(자연주파수 f)와 멈추는 정도(감쇠비 ζ)를 정한 뒤 계수를 계산한다.

```text
ω         = 2πf
Stiffness = ω²
Damping   = 2ζω
가속도    = Stiffness × (목표 − 현재) − Damping × 각속도
각속도   += 가속도 × dt
현재     += 각속도 × dt    ← 갱신된 각속도를 사용 (Semi-implicit Euler)
```

기준값은 1.8Hz, 감쇠비 0.55다. 감쇠비가 1보다 작아 한두 번 출렁이다 멈춘다.

명시적 Euler는 이전 속도로 위치를 갱신해 에너지가 늘어나기 쉽고, 강성이 높으면 발산한다. Semi-implicit Euler는 계산량이 같으면서 이 문제가 훨씬 덜하다. 그래도 긴 프레임에서는 dt가 커져 불안정하므로, 한 프레임을 최대 12단계로 나눠 적분하고 각속도는 ±360°/s로 제한했다.

네트워크 위치 보정이나 순간이동으로 위치가 크게 튀면 스프링이 그 변화를 가속도로 받아 크게 휘청인다. 위치 변화가 크거나 업데이트 간격이 0.25초를 넘으면 스프링 상태를 초기화한다. 시각 상태만 초기화하고 실제 위치는 건드리지 않는다.

소스: [DMJellyAnimInstance.cpp](../../Source/DontMelt/Private/Animations/DMJellyAnimInstance.cpp) — `AdvanceJellySpring`

## Ragdoll과 판정 분리

Ragdoll은 사망 때만 켠다. 플레이어와 NPC 최대 64명의 Bone을 상시 시뮬레이션하면 비용이 크고, 결과가 클라이언트마다 달라 위치 판정과 어긋난다.

`UDMJellyPhysicsComponent`는 서버에서 사망이 확정된 뒤 `MulticastStartDeathRagdollPresentation`으로 시작 신호만 보낸다. Bone 자세는 동기화하지 않는다. 쓰러진 모양이 클라이언트마다 조금 달라도 게임 판정에 영향이 없기 때문이다. 시체 Mesh는 `WorldStatic`·`WorldDynamic`에만 막히게 해서, 다른 Pawn이나 전투 판정 트레이스를 가리지 않게 했다.

생존 여부의 원본은 여전히 PlayerState의 `LifeState`다. Mesh가 쓰러졌다는 사실로 사망을 판단하지 않는다.

소스: [DMJellyPhysicsComponent.cpp](../../Source/DontMelt/Private/Characters/Components/DMJellyPhysicsComponent.cpp)

## PostProcess MID로 로컬 화면 효과

시야 교란 스킬은 대상 플레이어 화면에만 나타나야 한다. 서버는 효과 시작 서버 시각을 대상에게 보내고, 대상 클라이언트가 `UMaterialInstanceDynamic`을 만들어 카메라 PostProcess에 Blendable로 붙인다.

RPC가 늦게 도착해도 효과가 서버 시간과 맞게 끝나야 한다. 그래서 도착 시점에 이미 지난 시간(`DM_ElapsedOffset`)을 머티리얼 파라미터로 넘겨 진입·유지·종료 구간을 그만큼 앞당겨 재생하고, 남은 시간 뒤에 효과를 제거한다. 이미 효과가 끝난 뒤에 RPC가 도착했다면 시작하지 않는다.

```cpp
LocalVisionDistortMID->SetScalarParameterValue(TEXT("DM_LocalStartTime"), World->GetTimeSeconds());
LocalVisionDistortMID->SetScalarParameterValue(TEXT("DM_ElapsedOffset"), ElapsedServerTime);
CameraComponent->PostProcessSettings.AddBlendable(LocalVisionDistortMID.Get(), 1.0f);
```

MID는 화면만 바꾼다. 효과의 실제 지속과 행동 제한은 서버 상태가 정한다.

소스: [DMMatchPlayerController.cpp](../../Source/DontMelt/Private/Framework/Match/DMMatchPlayerController.cpp)

[DevLog 목차](../README.md)
