# 13. 젤리 관성 애니메이션·사망 Ragdoll

## P2 승인 동기화 — 2026-11-13

이번 사용자 P2 요청의 명시 범위만 **설계 승인**으로 채택한다. 과거 승인으로 소급하지 않는다. 기준 HEAD는 `914173a`이며 구현 사실·설계 승인·에셋 적용·실행 검증은 별개다. 상세 근거와 미결정 사항은 [ImplementationStatus 10절](../ImplementationStatus.md#10-p2-승인-설계-및-현재-구현--2026-11-13)을 따른다.

**아래 기존 본문과의 관계:** 이 절 뒤의 기존 본문은 삭제하지 않고 이전 설계·조사 기록으로 보존한다. E1~E10과 충돌하는 값·완료/승인/미확인 표현은 이 절과 중앙10절이 대체한다. 특히90초·Walk300·NPC×10/40·NPC 발소리·3요소 이벤트 HUD·타임아웃 즉시 재요청은 최신 목표가 아니다. E1~E10 밖의 항목을 이번 승인으로 일괄 확정하지 않는다.

### E4 — 선택 휠과 지속형 Emote, 별도 FakeDeath

**설계 승인:** 휠은 Emote 선택 UI, 홀드는 선택한 지속형 Emote 실행·유지 방식이다. 입력 종료·취소·상태 전환에서 종료하고, Emote 퀘스트 시작·지속·종료 계약을 유지한다. FakeDeath는 별도 동작 기능으로 관리한다. 기존 문서의 휠 항목 목록만으로 FakeDeath를 일반 Emote와 같은 구현으로 확정하지 않는다.

**구현 확인 / 부분 구현:** `ADMPlayerCharacter::BindPoseAndEmoteInput`, `InputEmoteStarted`, `InputEmoteCompleted`는 기본 `DefaultEmoteId=Scream` 홀드 요청을 연결한다. `UDMCharacterStateComponent::RequestBeginEmote_Implementation` / `RequestEndEmote_Implementation`과 서버 행동·퀘스트 이벤트 경로가 있다. C++ 입력 설정에서 선택 휠 연결은 확인하지 못했다. FakeDeath enum·제한·Anim 분기는 있으나 명시적인 C++ 진입 호출 및 완전한 해제 경로는 확인하지 못했다. BP 전체 미구현이라고 단정하지 않는다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E4 절을 함께 읽는다.

### E6 — NPC 이동 발소리 금지

**설계 승인:** NPC는 이동 발소리를 내지 않고 PlayerCharacter만 이동 발소리를 낸다. 사용자가 직접 확인한 의도이며 정책 자체를 재설계하지 않는다. NPC 비명·웃음·이벤트 사운드는 별개다. 이동 발소리는 플레이어 식별 단서가 될 수 있고 발소리 증폭·투명화 표현과 연결된다.

**구현 확인:** `UDMCharacterAudioComponent::CanPlayFootstepLocally`는 소유자가 `ADMPlayerCharacter`인 경우만 허용한다. `HandleFootstepNotify`는 이 검사를 거쳐 로컬 재생하며 발소리 전용 RPC를 추가하지 않는다. 조사한 C++ 발소리 경로에서 NPC 허용 우회는 확인하지 못했다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E6 절을 함께 읽는다.

**검증 상태:** BP/DataAsset/Animation 실제 적용은 에셋 미검증, 빌드·PIE·Steam은 실행 미검증이다. 아래 완료 기준은 향후 검증 시나리오이며 통과 기록이 아니다. C++·WBP·Config 변경은 이번 문서 승인에 포함되지 않는다. [공통 게임 규칙](../GameplayRules.md) · [네트워크 정책](../NetworkPolicy.md)를 상호 참조한다.

---

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
