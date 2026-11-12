# 13. 젤리 관성 애니메이션·사망 Ragdoll

## 기본 구조

공통 Skeletal Mesh와 `ABP_DMJelly`를 플레이어·NPC가 함께 사용. 실제 이동·충돌은 `CharacterMovement`. **일반 이동 중 전신 물리를 켜지 않고** `UDMJellyAnimInstance`의 감쇠 스프링에서 본 회전을 계산해 `AnimGraph`에 전달. 실제 사망 때만 `UDMJellyPhysicsComponent`가 Ragdoll 전환.

## 주요 값(설계 기준)

- Spring **7개**(몸통 3 + 머리·양팔 보조 4). NaturalFrequency **1.8Hz**, DampingRatio **0.55**. Semi-implicit Euler, 최대 **12** substep, 각속도 ±360°/s 제한.
- 몸통 회전 최종 Clamp: Pitch ±20°, Roll ±17°, Yaw ±10°. Walk/Sprint 전방 기울기 최대 5°/12°, 급정지 최대 15°.
- 위치 불연속이나 AnimUpdate 간격 **0.25초 초과** 시 시각적 스프링 초기화. 캡슐/실제 이동 위치는 고치지 않음.
- 상태 가중치: 이동 100%, 공격 최대 25%, 방어 최대 15%, 스턴 최대 20%, 특수 Action·Montage 중 0%, 사망 0%.
- `bStunPresentationActive`는 관찰자 표현용. 실제 StatusTag/행동 판정을 대체하지 않음.

## Bone 변형 계약

```text
SM_Locomotion → DefaultSlot → Pose Blend → Cache Pose
→ LocalToComponentSpace → Transform(Modify)Bone × 8
→ ComponentToLocalSpace → Output
```

| 본 | 회전 입력 | 위치 입력 |
|---|---|---|
| Spine1 / Spine2 | JellySpine1Rotation / JellySpine2Rotation | 무시 |
| Ribcage | JellyRibcageRotation | **JellyLandingRibcageOffset만** |
| Head | JellyHeadRotation | 무시 |
| L/R Upperarm | JellyLeft/RightUpperarmRotation | 무시 |
| L/R Forearm | JellyLeft/RightForearmRotation | 무시 |

`Ribcage` 착지 Translation을 Spine1/Spine2에도 중복 적용하지 않는다. BP Bone 이름은 실제 Skeleton에서 확인해야 함. 기존 몽타주·포즈·Idle/Walk/Jump를 제거하지 않는다.

## Ragdoll

사망 확정 이후 `StartDeathRagdollPresentation`→필요한 클라이언트 `MulticastStartDeathRagdollPresentation`. Mesh 물리를 시뮬레이션하고 Pawn/Visibility는 차단하지 않고 월드 충돌만 유지하여 전투 LOS를 망치지 않는다. 표현용 물리 위치는 게임 서버의 생명·리스폰 판정을 변경하지 않는다. Ragdoll의 정확한 위치 동기화 여부와 서버 적용 경로는 코드 확인.

## 성능/에셋 검증

문서에 공용 `riggedcapsuleman` Mesh, 위치 `(0,0,-88)`, 회전 `(0,0,-90)`, Scale `(0.45,0.45,0.45)`가 기록되어 있지만, 현재 Blueprint/asset 기준값 미검증. `AlwaysTickPose` 및 URO/Notify 최적화가 발소리 정보를 제거하지 않는지 확인.

## 완료 기준

- 플레이어/NPC 회전·점프·착지/공격·방어·스턴·기절에서 관성 표현 과장/NaN 없음.
- 지연 보정/Teleport/리스폰/사망 시 스프링 초기화, 원격 관찰자 스턴 표현.
- 죽은 Pawn이 공격 LOS·NPC 이동 막지 않음, Ragdoll 반복 호출 안전, PhysicsAsset 없음 처리.
- NPC 60명 부하에서 AnimGraph/발소리 Notify 회귀 및 실제 프레임 측정.

[물리 설계](../../Docs/Design/Stages/11_5_JellyAnimationAndRagdoll.md) · [기술](../../Docs/Technical/12_Jelly_Animation_Ragdoll.md) · [성능](../../Docs/Performance/01_NPC_Performance_Analysis.md)
