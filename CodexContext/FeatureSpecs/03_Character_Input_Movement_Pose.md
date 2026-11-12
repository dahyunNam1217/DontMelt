# 03. 캐릭터·입력·이동·자세·감정표현

## P2 승인 동기화 — 2026-11-13

이번 사용자 P2 요청의 명시 범위만 **설계 승인**으로 채택한다. 과거 승인으로 소급하지 않는다. 기준 HEAD는 `914173a`이며 구현 사실·설계 승인·에셋 적용·실행 검증은 별개다. 상세 근거와 미결정 사항은 [ImplementationStatus 10절](../ImplementationStatus.md#10-p2-승인-설계-및-현재-구현--2026-11-13)을 따른다.

**아래 기존 본문과의 관계:** 이 절 뒤의 기존 본문은 삭제하지 않고 이전 설계·조사 기록으로 보존한다. E1~E10과 충돌하는 값·완료/승인/미확인 표현은 이 절과 중앙10절이 대체한다. 특히90초·Walk300·NPC×10/40·NPC 발소리·3요소 이벤트 HUD·타임아웃 즉시 재요청은 최신 목표가 아니다. E1~E10 밖의 항목을 이번 승인으로 일괄 확정하지 않는다.

### E4 — 선택 휠과 지속형 Emote, 별도 FakeDeath

**설계 승인:** 휠은 Emote 선택 UI, 홀드는 선택한 지속형 Emote 실행·유지 방식이다. 입력 종료·취소·상태 전환에서 종료하고, Emote 퀘스트 시작·지속·종료 계약을 유지한다. FakeDeath는 별도 동작 기능으로 관리한다. 기존 문서의 휠 항목 목록만으로 FakeDeath를 일반 Emote와 같은 구현으로 확정하지 않는다.

**구현 확인 / 부분 구현:** `ADMPlayerCharacter::BindPoseAndEmoteInput`, `InputEmoteStarted`, `InputEmoteCompleted`는 기본 `DefaultEmoteId=Scream` 홀드 요청을 연결한다. `UDMCharacterStateComponent::RequestBeginEmote_Implementation` / `RequestEndEmote_Implementation`과 서버 행동·퀘스트 이벤트 경로가 있다. C++ 입력 설정에서 선택 휠 연결은 확인하지 못했다. FakeDeath enum·제한·Anim 분기는 있으나 명시적인 C++ 진입 호출 및 완전한 해제 경로는 확인하지 못했다. BP 전체 미구현이라고 단정하지 않는다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E4 절을 함께 읽는다.

### E5 — 공통 이동 속도와 NPC 목표 수

**설계 승인:** Player와 NPC 모두 공통 CharacterBase 기준 Walk200cm/s·Sprint500cm/s를 사용한다. 시작 참가자당 NPC15명, 최대60명이다. 2/3/4인 목표는30/45/60명이다.

**구현 확인:** `ADMJellyCharacterBase`의 WalkSpeed/SprintSpeed 기본값은200/500이며 `PostInitializeComponents`와 이동 컴포넌트 `GetMaxSpeed` 경로에서 사용한다. `UDMMatchRuleDataAsset::DefaultNPCMultiplier` 기본15, `ADMMatchGameMode::TryFinalizeStartingRoster`의 인원×배율 스냅샷, `UDMNPCPopulationDirector::InitializePopulation`의 최대60 검증을 확인했다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E5 절을 함께 읽는다.

**검증 상태:** BP/DataAsset/Animation 실제 적용은 에셋 미검증, 빌드·PIE·Steam은 실행 미검증이다. 아래 완료 기준은 향후 검증 시나리오이며 통과 기록이 아니다. C++·WBP·Config 변경은 이번 문서 승인에 포함되지 않는다. [공통 게임 규칙](../GameplayRules.md) · [네트워크 정책](../NetworkPolicy.md)를 상호 참조한다.

---

## 목표

플레이어와 NPC가 동일한 젤리 외형·이동 애니메이션을 사용하고, 입력 모드가 상호작용/관전/메뉴 등으로 바뀌어도 정상 복구된다. 로컬 예측은 조작 반응을 위한 것이며 이동 판정은 CharacterMovement 서버 보정이 담당한다.

## 입력 컨텍스트

| Context | 키/목적 |
|---|---|
| `IMC_Gameplay` | WASD, Look, Space, Shift, F, Q, LMB, E, R, T, 1/2/3, Esc |
| `IMC_Pose` | WASD/Look/Jump, LMB 자세 순환, RMB 시점, R 종료 |
| `IMC_Interaction` | F 상자 닫기, Space 길게 등록/탈취 |
| `IMC_SkillTargeting` | LMB 무작위, RMB 선두, Cancel |
| `IMC_RadialMenu` | Emote 항목 선택·취소 |
| `IMC_Spectator` | 사망 자유 관전 이동·회전 |
| `IMC_Voice` | V PTT, M 마이크 음소거 (로비·경기 공통) |

`bIgnoreAllPressedKeysUntilRelease`를 사용해 Context 전환 당시 눌린 F·Space 등이 새 Context의 이벤트로 재처리되지 않게 한다. `PawnClientRestart`/Pawn 변경/메뉴 닫기 시 Mapping·마우스 캡처·GameOnly 입력 모드를 복구한다.

## 책임과 이동 예측

- `ADMJellyCharacterBase` — 공용 Mesh/이동/오디오/애니메이션.
- `ADMPlayerCharacter` — 플레이어 입력 바인딩, 1·3인칭 카메라, Component 호출.
- `UDMCharacterMovementComponent` — `FSavedMove_DMCharacter`, `FLAG_Custom_0` Sprint 의도 압축, `CanCombineWith`, `UpdateFromCompressedFlags`, `GetMaxSpeed`.
- `UDMPoseComponent` — Pose 활성 및 순환 상태, 자세 제한; `UDMCharacterStateComponent` — ActionState/상태 태그.
- NPC는 입력 Action을 실행하지 않으나 **기본 이동 속도 계약은 플레이어와 맞춤**.

## 확정 동작

- 기본 1인칭, RMB로 3인칭 전환. 일반 이동에서 1인칭은 카메라 방향, 3인칭은 이동 방향으로 캐릭터 회전.
- Pose는 기본→납작→웅크리기→대자→기본; 자세를 유지한 채 이동/점프 가능하며 공격·방어·Sprint·스킬·유물 상호작용·Emote 불가.
- Emote 휠은 T, Dance/Laugh/Cheer/FakeDeath 등을 고른다. `FakeDeath`는 실제 `LifeState=Dead`가 아니며 관전·리스폰을 일으키지 않는다.
- Sprint 전환은 SavedMove의 예측/보정 경로에 넣고, 별도 매 프레임 Reliable RPC 또는 별도 이동 상태 Enum 복제를 신설하지 않는다.

## 현재 수치 충돌 / 검증

설계 문서의 WalkSpeed=300 기록과 이후 사용자 요청 WalkSpeed=200, SprintSpeed=500cm/s가 서로 다르다. **현재 반영 여부 미검증**. `CharacterMovement::GetMaxSpeed`와 C++ 생성자, 캐릭터 공용 BP 및 NPC 자식 BP의 `MaxWalkSpeed` 오버라이드를 모두 확인해 임의 이중 수정 금지.

## 완료 기준

- Host/Client 1·3인칭, Sprint 시작/해제의 이동 보정과 애니메이션 반영.
- 지연/PacketLoss에서도 Sprint SavedMove 재생 정확성 확인.
- F로 상자 연 직후 바로 닫히지 않는지, Space를 길게 눌렀다 떼었을 때 취소되는지.
- Pose↔Gameplay↔Skill↔Spectator/Respawn↔Menu에서 입력 중복·마우스 캡처 이상 없는지.
- 플레이어/NPC 공통 속도 및 메시 크기·애니메이션 확인.

[입력 설계](../../Docs/Design/EnhancedInput.md) · [이동 구현](../../Docs/Technical/05_Input_Movement.md) · [이동·자세 단계](../../Docs/Design/Stages/03_MovementPose.md)
