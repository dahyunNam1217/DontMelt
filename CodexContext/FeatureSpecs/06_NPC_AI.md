# 06. NPC 군중·AI·오인 공격 반응

## P3-9 최신 기준 — 2026-11-17

기준 HEAD: `001a08241ecd01dfa1ef565f11c7791eaf73eaeb`. [최신 구현 대조와 검증 한계](../ImplementationStatus.md#p3-9--최신-아키텍처-결정-및-구현-대조-2026-11-17)를 함께 읽는다. 아래 최신 절과 충돌하는 P2/기존 본문은 당시 설계·조사 이력으로 보존하며 현재 완료 근거로 사용하지 않는다.

- `Public/Characters/DMJellyCharacterBase.h` 기본값 WalkSpeed=200/SprintSpeed=500, `Private/Characters/DMJellyCharacterBase.cpp::PostInitializeComponents`의 MaxWalkSpeed 적용, `Private/Characters/Components/DMCharacterMovementComponent.cpp::GetMaxSpeed`의 Sprint 경로를 확인했다. Player/NPC는 이 공통 기반을 상속한다.
- `Public/Data/DMMatchRuleDataAsset.h::DefaultNPCMultiplier=15`, GameMode 시작 인원×배율 스냅샷, `Private/AI/DMNPCPopulationDirector.cpp::InitializePopulation`의 1~60 검증을 확인했다. 2/3/4인 목표는30/45/60명이다.
- `Private/Characters/Components/DMCharacterAudioComponent.cpp::CanPlayFootstepLocally`는 ADMPlayerCharacter만 허용한다. NPC 발소리 제거 정책은 기존 구현이며 이번 커밋이 새로 제거한 것으로 기록하지 않는다. BP 직접 재생·Sound Notify 우회는 미검증이다.
- P3 커밋에서 P3-5에 해당하는 직접 변경은 규칙 DataAsset 헤더 주석의 4×10=40 → 4×15=60 정정이다. 이동·배율·발소리 동작은 이전부터 존재하던 코드를 재확인한 것이다. 에셋 기본값·실제 속도·수량·음향·60NPC 성능은 미검증이다.

## P2 승인 동기화 — 2026-11-13 (당시 기록; 최신 기준은 상단 P3-9)

이번 사용자 P2 요청의 명시 범위만 **설계 승인**으로 채택한다. 과거 승인으로 소급하지 않는다. 기준 HEAD는 `914173a`이며 구현 사실·설계 승인·에셋 적용·실행 검증은 별개다. 상세 근거와 미결정 사항은 [ImplementationStatus 10절](../ImplementationStatus.md#10-p2-승인-설계-및-현재-구현--2026-11-13)을 따른다.

**아래 기존 본문과의 관계:** 이 절 뒤의 기존 본문은 삭제하지 않고 이전 설계·조사 기록으로 보존한다. E1~E10과 충돌하는 값·완료/승인/미확인 표현은 이 절과 중앙10절이 대체한다. 특히90초·Walk300·NPC×10/40·NPC 발소리·3요소 이벤트 HUD·타임아웃 즉시 재요청은 최신 목표가 아니다. E1~E10 밖의 항목을 이번 승인으로 일괄 확정하지 않는다.

### E5 — 공통 이동 속도와 NPC 목표 수

**설계 승인:** Player와 NPC 모두 공통 CharacterBase 기준 Walk200cm/s·Sprint500cm/s를 사용한다. 시작 참가자당 NPC15명, 최대60명이다. 2/3/4인 목표는30/45/60명이다.

**구현 확인:** `ADMJellyCharacterBase`의 WalkSpeed/SprintSpeed 기본값은200/500이며 `PostInitializeComponents`와 이동 컴포넌트 `GetMaxSpeed` 경로에서 사용한다. `UDMMatchRuleDataAsset::DefaultNPCMultiplier` 기본15, `ADMMatchGameMode::TryFinalizeStartingRoster`의 인원×배율 스냅샷, `UDMNPCPopulationDirector::InitializePopulation`의 최대60 검증을 확인했다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E5 절을 함께 읽는다.

### E6 — NPC 이동 발소리 금지

**설계 승인:** NPC는 이동 발소리를 내지 않고 PlayerCharacter만 이동 발소리를 낸다. 사용자가 직접 확인한 의도이며 정책 자체를 재설계하지 않는다. NPC 비명·웃음·이벤트 사운드는 별개다. 이동 발소리는 플레이어 식별 단서가 될 수 있고 발소리 증폭·투명화 표현과 연결된다.

**구현 확인:** `UDMCharacterAudioComponent::CanPlayFootstepLocally`는 소유자가 `ADMPlayerCharacter`인 경우만 허용한다. `HandleFootstepNotify`는 이 검사를 거쳐 로컬 재생하며 발소리 전용 RPC를 추가하지 않는다. 조사한 C++ 발소리 경로에서 NPC 허용 우회는 확인하지 못했다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E6 절을 함께 읽는다.

### E9-C — 플레이어 실제 사망에 대한 NPC 비웃음

**설계 승인:** 실제 플레이어 사망 후 주변 NPC 반응과 비웃음 표현을 유지한다. Panic/Stun·행동 상태 충돌을 막고 다른 참가자에게 제공하는 사망 위치·상황 단서를 고려한다. 단순한 장식 효과로 취급하지 않는다.

**구현 확인:** GameMode 사망 처리 → `UDMNPCPopulationDirector::HandleNearbyPlayerDeath` → `ADMJellyAIController::TryStartDeathMockingReaction`의 행동 전환·이동 중지·Laugh 요청 경로가 있다. 규칙 DataAsset C++ 기본값은 반경800cm, 최대3명, 지속2.5초다. 이는 현재 구현 기준값이며 실제 에셋 적용 수치와 재생 성공을 의미하지 않는다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E9-C 절을 함께 읽는다.

**검증 상태:** BP/DataAsset/Animation 실제 적용은 에셋 미검증, 빌드·PIE·Steam은 실행 미검증이다. 아래 완료 기준은 향후 검증 시나리오이며 통과 기록이 아니다. C++·WBP·Config 변경은 이번 문서 승인에 포함되지 않는다. [공통 게임 규칙](../GameplayRules.md) · [네트워크 정책](../NetworkPolicy.md)를 상호 참조한다.

---

## 목표

NPC와 플레이어는 같은 젤리 캐릭터 기반 메시·이동·애니메이션을 쓴다. 행동 단서를 완전히 동일하게 만드는 것이 아니라, 플레이어가 NPC로 위장할 수 있는 군중을 제공한다. NPC는 **유물이나 전투 결과를 실제로 바꾸지 않는다**.

## 책임

`UDMNPCPopulationDirector`: 인원 스냅샷 기반 스폰·등록/회수·지역별 분산·오인 공격 집단 반응·전역 패닉 배포. `ADMJellyAIController`: 개인 행동·NavMove·타이머·MoveComplete. `ADMNPCCharacter`: 공용 캐릭터 메시·이동·행동 표현. 서버 AI 전용 판단, 클라이언트는 이동/표현만 복제.

## 확정 수치

- `DefaultNPCMultiplier=15`, 2인=30/3인=45/4인=60명 (구 10배/40명보다 최신 목표).
- Director 허용 범위 1~60(기술 문서 기록). 맵은 4인용 최소 **60개** 이상의 `DM_NPCSpawn` TargetPoint 필요.
- 행동 가중치: Walk 40, RelicApproach 20, Sprint 15, Idle 10, Jump 5. 확률은 이 가중치로 정규화하되 DataTable `DT_NPCBehaviorProfiles`/RuleDataAsset 실존 값 확인.
- 첫 행동 지연 0.15~1.0초, 이동 후 0.15~0.4초, Move 실패 0.5초, 유물 접근 후 정지 2초(문서 기본).
- 주변 반응: 기본 반경 **1200cm**, 추가 NPC **최대 6명**, 도주 기본 **900cm**.

## 상태와 흐름

```text
서버 타이머 → 가중치 행동 선택 → Idle/Jump/Nav Move/RelicApproach
             └─MoveCompleted/타이머→다음 행동 스케줄
오인 공격 → 맞은 NPC React + 가장 가까운 주변 NPC(최대 6) Flee
랜덤 패닉 → Director가 분산 지연으로 React/Panic 이동 지시
```

NPC는 유물·상자에 접근한 뒤 머물 수 있지만 Pickup, Deposit, Steal은 금지. 가짜 공격·가짜 죽음은 NPC 일반 행동에 포함하지 않는다. 스폰 지점은 매치마다 셔플하여 2인 등 인원이 적을 때 동일 지역만 채워지는 편향을 피한다. 필수 스폰 실패 시 부분 생성 개체를 회수하고 초기화를 실패로 처리한다.

## 성능 제약

- Behavior Tree/StateTree/개체 Tick 대신 Timer·MoveCompleted 사용.
- NPC **40명** 실측: AI 평균 약 0.12~0.25ms, Move/CharacterMovement 비용은 실행 프로세스에 크게 영향. 60명 측정 결과는 아직 증거 없음.
- NPC NetUpdateFrequency 30/10, URO, NavWalking, Audio Concurrency는 **검토 후보**. 이미 적용했다고 단정 금지.
- Render 기반 Pose 갱신 중단은 발소리 AnimNotify를 없애 위장 게임 규칙을 훼손할 수 있음. 최적화 시 오디오 테스트 필수.

## 완료 기준

- 2/3/4인 NPC 정확 30/45/60 및 스폰 실패 정리.
- 행동 선택·경로 실패·오인 공격 주변 6명·패닉 도중 사망/경기 종료.
- 실시간 타이머 누수, 반복 MoveTo 폭주, RPC 폭주, 발소리 동작 누락 없음.
- PIE 한 프로세스 수치가 아닌 별도 프로세스/패키지 환경에서 NPC 60명 측정.

[AI 설계](../../Docs/Design/Stages/06_NPCAI.md) · [AI 기술](../../Docs/Technical/08_NPC_AI.md) · [성능 측정](../../Docs/Performance/01_NPC_Performance_Analysis.md)
