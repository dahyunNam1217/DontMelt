# 11.5 — 젤리 관성 애니메이션·사망 Ragdoll

## 1. 범위

플레이어와 NPC가 같은 젤리 움직임을 쓰도록 Idle·Walk·Sprint·회전·점프·착지에 관성 표현을 더했다. 실제 이동·충돌과 서버 위치 판정은 `CharacterMovement`가 그대로 맡는다. 평소 이동 중에 전신 물리로 캐릭터를 움직이는 구조가 아니다.

```text
CharacterMovement / CharacterState   실제 이동·판정
         ↓
UDMJellyAnimInstance                 감쇠 스프링 7개, 본 회전·착지 계산
         ↓
ABP_DMJelly                          기존 Locomotion + Transform Bone 8개
         ↓
SkeletalMesh                         젤리형 화면 표현

UDMJellyPhysicsComponent             실제 사망 시 Ragdoll
```

작업은 P-0(에셋 구조 확인)부터 P-10(통합 점검)까지 단계로 나눠 진행했다.

## 2. 에셋

공용 Mesh `riggedcapsuleman`, `ABP_DMJelly`, Skeleton·PhysicsAsset을 플레이어와 NPC가 함께 쓴다. Mesh 기본 Transform은 Location `(0, 0, -88)`, Rotation `(0, 0, -90)`, Scale `(0.45, 0.45, 0.45)`다. 기존 Idle/Walk/Sprint/Jump, DefaultSlot, `EDMPoseType` Blend는 그대로 둔다.

## 3. 몸통 관성

| 표현 | 기준값 |
|---|---:|
| Idle 좌우 / 전후 | ±2° / ±1° |
| Idle 주기 | 1.8초 |
| Walk / Sprint 전방 기울기 | 최대 5° / 12° |
| Walk / Sprint 좌우 흔들림 | ±4° / ±5° |
| 보행 주파수 | 1.5~2.7Hz |
| 출발 역방향 관성 | 최대 8° |
| 급정지 관성 | 최대 15° |
| 방향 전환 | 최대 11° |
| 이동 중 회전 지연 | 최대 10° |
| 전후 / 좌우 / Yaw 최종 Clamp | ±20° / ±17° / ±10° |

스프링은 자연주파수 1.8Hz, 감쇠비 0.55를 기준으로 각도·각속도를 저장해 Semi-implicit Euler로 적분한다. 몸통 회전은 세 본에 나눠 준다.

| Bone | Pitch | Yaw | Roll |
|---|---:|---:|---:|
| Spine1 | 55% | 20% | 35% |
| Spine2 | 30% | 35% | 45% |
| Ribcage | 15% | 45% | 20% |

## 4. 점프와 착지

점프 상승 때 상체를 최대 -5°, 하강 때 +3° 기울인다. 공중 체류 0.12초 이상, 하강 속도 240cm/s 이상이면 착지 충격을 계산한다. 착지 압축은 `Ribcage` Translation에만 적용하고 Pelvis·Capsule·실제 이동 위치는 바꾸지 않는다. 진폭은 Bone Space 4~9 단위, 감쇠 3.8/s, 지속 0.55초 이하다.

작업 중 착지 Translation이 `Spine1`·`Spine2`에도 연결돼 몸통이 과하게 눌리는 구성이 있었다. 최종 AnimGraph에서는 Ribcage에만 연결한다.

## 5. 머리·양팔 보조 흔들림

몸통 3개에 머리·양팔 4개 스프링을 더해 캐릭터당 7개 상태를 쓴다. Head Pitch/Roll ±7°, Yaw ±3°, Upperarm Pitch/Roll ±14°/±12°, Forearm ±6°/±5°가 기본 Clamp다. Forearm은 Upperarm 결과의 35%를 받는다. AnimDynamics Body나 전신 PhysicsAsset 시뮬레이션은 늘리지 않았다.

## 6. 행동 상태별 가중치

| 상황 | 젤리 가중치 |
|---|---:|
| 이동 | 100% |
| Attack | 최대 25% |
| Defense | 최대 15% |
| Stunned | 최대 20% |
| Interact·Emote·FakeDeath·SkillCast·특수 자세 | 0% |
| 몽타주 재생 중 | 즉시 0% |
| 실제 사망 | 0%, 스프링 초기화 |

몽타주 보호 규칙이 공격·방어의 부분 강도보다 우선한다.

## 7. 원격 스턴과 위치 보정

실제 `ActiveStatusTags`는 서버 원본이고 소유자에게만 복제한다. 원격 관찰자용으로는 `bStunPresentationActive`를 최소 표현 상태로 복제하고, AnimInstance가 이 값을 읽는다. 이 bool은 실제 스턴 판정에 쓰지 않는다.

위치가 불연속으로 튀거나 AnimInstance 업데이트 간격이 0.25초를 넘으면 스프링을 초기화한다. 허용 거리는 최소 200cm에 현재 이동 속도를 반영해 계산한다. 본 회전, 스프링 상태, 보행 위상은 복제하지 않는다.

## 8. AnimGraph

```text
SM_Locomotion
 → DefaultSlot
 → Blend Poses(EDMPoseType)
 → Save Cached Pose CP_DMBasePose
 → Local to Component Space
 → Transform (Modify) Bone × 8
 → Component to Local Space
 → Output Pose
```

`Transform (Modify) Bone`은 Component Space 포즈를 받으므로 앞뒤에 공간 변환 노드를 둔다.

| Bone | Rotation 입력 | Translation |
|---|---|---|
| Spine1 | `JellySpine1Rotation` | Ignore |
| Spine2 | `JellySpine2Rotation` | Ignore |
| Ribcage | `JellyRibcageRotation` | `JellyLandingRibcageOffset` |
| Head | `JellyHeadRotation` | Ignore |
| L/R Upperarm | `JellyLeftUpperarmRotation` / `JellyRightUpperarmRotation` | Ignore |
| L/R Forearm | `JellyLeftForearmRotation` / `JellyRightForearmRotation` | Ignore |

각 노드는 기존 회전에 Add, Bone Space, Scale Ignore, Alpha는 `JellyPresentationAlpha`다. Ribcage만 Translation Add를 쓴다.

## 9. 성능

NPC 60명까지 같은 AnimInstance를 쓰므로, 남는 비용은 NPC 수만큼의 애니메이션 평가다. 화면 밖 NPC의 포즈 갱신을 끄는 설정은 발소리 노티파이를 멈추게 해 쓰지 않고, 대신 NPC 전용 Update Rate Optimization을 후보로 두었다([NPC 성능 분석](../../Performance/01_NPC_Performance_Analysis.md)).

관련 문서: [젤리 애니메이션 구현](../../Technical/12_Jelly_Animation_Ragdoll.md)

[Stages 목차](README.md)
