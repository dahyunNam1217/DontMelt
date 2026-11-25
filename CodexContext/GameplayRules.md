# GameplayRules — 확정된 게임 규칙

## P3-9 최신 기준 — 2026-11-17

기준 HEAD: `001a08241ecd01dfa1ef565f11c7791eaf73eaeb`. [최신 구현 대조와 검증 한계](ImplementationStatus.md#p3-9--최신-아키텍처-결정-및-구현-대조-2026-11-17)를 함께 읽는다. 승인 규칙은 아래 [경기 기본값과 세부 규칙](#1-경기-기본값)에서 확인한다. 현재 구현과 검증 근거는 [기준 체크포인트](ImplementationStatus.md#p1-p9-체크포인트-구현과-검증)에 있다.

- Player/NPC 정상 게임 화면의 위장 일관성을 유지한다. 플레이어 발소리만 허용하고 NPC 이동 발소리는 금지한다. NPC 웃음·비명은 별개다.
- 공통 C++ 기본값은 WalkSpeed **200cm/s**, SprintSpeed **500cm/s**. NPC 목표는 시작 인원 ×15 → **30·45·60명**이다. BP/DataAsset 오버라이드·실제 속도·수량·음향은 미검증이다.
- 참가자 도착·준비·필수 초기화 제한은 서버 StartPlay 기준 기본 **90초**이며 튜토리얼 기본 300초와 분리한다. 미도착을 명시적 이탈로 단정하지 않는다.
- 결과 Row는 등록 수 `TXT_RelicCount`와 누적 획득 수 `TXT_AcquiredRelicCount`를 함께 표시하는 C++ 계약이다. 일반 순위는 최종 등록 수, 전멸 승리만 승자 단독 1위 후 나머지 등록 수 공동 순위다. 획득 수·킬 수는 순위 지표가 아니다.
- RandomEvent HUD는 설명만 표시한다. 기존 공격·방어·스킬·유물·리스폰 규칙은 보존하며 이 문서 작업으로 새 규칙을 확정하지 않는다.

## P2 승인 동기화 — 2026-11-13 (당시 기록; 최신 기준은 상단 P3-9)

이번 사용자 P2 요청의 명시 범위만 **설계 승인**으로 채택한다. 과거 승인으로 소급하지 않는다. 기준 HEAD는 `914173a`이며 구현 사실·설계 승인·에셋 적용·실행 검증은 별개다. 상세 근거와 미결정 사항은 [ImplementationStatus 10절](ImplementationStatus.md#10-p2-승인-설계-및-현재-구현--2026-11-13)을 따른다.

**아래 기존 본문과의 관계:** 이 절 뒤의 기존 본문은 삭제하지 않고 이전 설계·조사 기록으로 보존한다. E1~E10과 충돌하는 값·완료/승인/미확인 표현은 이 절과 중앙10절이 대체한다. 특히90초·Walk300·NPC×10/40·NPC 발소리·3요소 이벤트 HUD·타임아웃 즉시 재요청은 최신 목표가 아니다. E1~E10 밖의 항목을 이번 승인으로 일괄 확정하지 않는다.

### E1 — 결과 통계와 순위 분리

**설계 승인:** 결과표 유물 열은 경기 중 누적 획득 횟수, 최종 순위는 종료 시 등록 유물 수를 사용하며 동률 공동 순위를 허용한다. 두 통계를 별도 관리하고 표시 변경으로 서버 승리 판정을 바꾸지 않는다.

**구현 확인 / 부분 구현:** `ADMRelicManager::TryCommitRelicAcquisition` → `ADMMatchPlayerState::IncrementAcquiredRelicCount` → `ADMMatchGameMode::BuildFinalPlayerResults`로 누적 통계를 보관한다. `FDMMatchResultEntry`의 `AcquiredRelicCount`와 `RelicCount`가 분리되어 있다. 그러나 `UDMPlayerResultRowWidget::SetResultEntry`는 현재 `RelicCount`를 표시하므로 목표 표시와 불일치한다. 성공 Pickup/Steal Commit마다 증가하는 것이 현재 동작이다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](ImplementationStatus.md)의 E1 절을 함께 읽는다.

### E2 — 전멸 승자 단독 1위

**설계 승인:** 일반 종료는 최종 등록 수 순위다. 전멸 승리에서는 등록 수와 무관하게 승자를 단독1위로 고정하고, 나머지는 2위부터 등록 수 기준 공동 순위를 부여한다. 승리 배너와 결과표의 승자 의미가 모순되지 않아야 한다.

**구현 확인 / 부분 구현:** `DMMatchResultRules::SortAndRankResults`는 등록 수 내림차순이며 같은 등록 수에서만 승자를 우선한다. `ADMMatchGameMode::BuildFinalPlayerResults`가 이 함수를 호출한다. 따라서 등록 수가 적은 전멸 승자가 결과표1위가 아닐 수 있다. 현재 함수는 종료 사유별 승자 고정 계약을 받지 않는다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](ImplementationStatus.md)의 E2 절을 함께 읽는다.

### E3 — 참가자 도착 대기 120초

**설계 승인:** 기본 도착 제한은120초다. 경기 맵 서버 GameMode가 승인 참가자의 준비와 시스템 초기화를 검증한다. PC 존재만으로 완료하지 않는다. 도착 제한과 튜토리얼 제한을 분리하고 무한 대기를 방지한다. 미도착·명시적 이탈·Spawn/Possess 실패는 가능한 범위에서 진단·사용자 안내를 구분한다.

**구현 확인:** `ADMMatchGameMode::StartPlay`가 아직 명단이 확정되지 않은 Waiting에서 타이머를 설정한다. 클라이언트 로딩 시작 시점 기준이 아니다. `HandleStartingNewPlayer_Implementation`은 부모 Spawn/Possess 후 Pawn/Controller를 확인한다. `IsStartingRosterComplete`는 기대 인원, 유효 PlayerState, 유효·고유 BaseSlot, Alive, Pawn·Controller·PlayerState 연결을 확인한다. `TryFinalizeStartingRoster`는 스폰 지점 및 NPC/유물/퀘스트/이벤트 초기화 후 도착 타이머를 해제하고 튜토리얼 게이트로 이동한다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](ImplementationStatus.md)의 E3 절을 함께 읽는다.

### E4 — 선택 휠과 지속형 Emote, 별도 FakeDeath

**설계 승인:** 휠은 Emote 선택 UI, 홀드는 선택한 지속형 Emote 실행·유지 방식이다. 입력 종료·취소·상태 전환에서 종료하고, Emote 퀘스트 시작·지속·종료 계약을 유지한다. FakeDeath는 별도 동작 기능으로 관리한다. 기존 문서의 휠 항목 목록만으로 FakeDeath를 일반 Emote와 같은 구현으로 확정하지 않는다.

**구현 확인 / 부분 구현:** `ADMPlayerCharacter::BindPoseAndEmoteInput`, `InputEmoteStarted`, `InputEmoteCompleted`는 기본 `DefaultEmoteId=Scream` 홀드 요청을 연결한다. `UDMCharacterStateComponent::RequestBeginEmote_Implementation` / `RequestEndEmote_Implementation`과 서버 행동·퀘스트 이벤트 경로가 있다. C++ 입력 설정에서 선택 휠 연결은 확인하지 못했다. FakeDeath enum·제한·Anim 분기는 있으나 명시적인 C++ 진입 호출 및 완전한 해제 경로는 확인하지 못했다. BP 전체 미구현이라고 단정하지 않는다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](ImplementationStatus.md)의 E4 절을 함께 읽는다.

### E5 — 공통 이동 속도와 NPC 목표 수

**설계 승인:** Player와 NPC 모두 공통 CharacterBase 기준 Walk200cm/s·Sprint500cm/s를 사용한다. 시작 참가자당 NPC15명, 최대60명이다. 2/3/4인 목표는30/45/60명이다.

**구현 확인:** `ADMJellyCharacterBase`의 WalkSpeed/SprintSpeed 기본값은200/500이며 `PostInitializeComponents`와 이동 컴포넌트 `GetMaxSpeed` 경로에서 사용한다. `UDMMatchRuleDataAsset::DefaultNPCMultiplier` 기본15, `ADMMatchGameMode::TryFinalizeStartingRoster`의 인원×배율 스냅샷, `UDMNPCPopulationDirector::InitializePopulation`의 최대60 검증을 확인했다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](ImplementationStatus.md)의 E5 절을 함께 읽는다.

### E6 — NPC 이동 발소리 금지

**설계 승인:** NPC는 이동 발소리를 내지 않고 PlayerCharacter만 이동 발소리를 낸다. 사용자가 직접 확인한 의도이며 정책 자체를 재설계하지 않는다. NPC 비명·웃음·이벤트 사운드는 별개다. 이동 발소리는 플레이어 식별 단서가 될 수 있고 발소리 증폭·투명화 표현과 연결된다.

**구현 확인:** `UDMCharacterAudioComponent::CanPlayFootstepLocally`는 소유자가 `ADMPlayerCharacter`인 경우만 허용한다. `HandleFootstepNotify`는 이 검사를 거쳐 로컬 재생하며 발소리 전용 RPC를 추가하지 않는다. 조사한 C++ 발소리 경로에서 NPC 허용 우회는 확인하지 못했다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](ImplementationStatus.md)의 E6 절을 함께 읽는다.

### E7 — RandomEvent HUD 설명 단일화

**설계 승인:** `TXT_EventDescription`만 표시한다. 이름과 진행바는 표시하지 않으며 필수 UI 바인딩에서도 해제하는 것이 목표다. 설명 갱신, 경고·진행·종료 전환, GameState 연결, 종료 정리는 유지한다. DataAsset DisplayName과 서버 이벤트 판정 데이터는 HUD 미사용을 이유로 제거하지 않는다.

**구현 확인 / 부분 구현:** `UDMRandomEventHUDWidget::RefreshPresentation`은 현재 TXT_EventName/TXT_EventDescription/PB_EventProgress 세 요소를 모두 요구하고 이름·진행률도 갱신한다. `UDMGameRootWidget`은 GameState 및 표시 활성 상태를 연결한다. 조사한 C++에서 외부 시스템이 이름·진행바 자식 포인터를 직접 사용하는 경로는 확인하지 못했다. 기존 세 요소 명세는 이전 버전이다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](ImplementationStatus.md)의 E7 절을 함께 읽는다.

### E8 — Steam 안전 복구 상태 머신 유지

**설계 승인:** 기존 상태 머신을 유지하며 전면 재작성하지 않는다. Create/Find/Join/Destroy/Update별 비동기 결과를 구분하고 기본 요청30초를 유지한다. 타임아웃은 플랫폼 취소 완료가 아니다. 작업 중 중복 요청을 거절하고 원 요청 완료·필요한 잔여 세션 정리·안전한 재시도 조건 확인 후 다음 요청을 허용한다. 불확실하면 차단·재시작 안내한다. 영구 차단 후 무의미한 로비 재시도를 반복하지 않는다.

**구현 확인:** `UDMOnlineSessionSubsystem::HandleOperationTimeout`은 지연 완료를 기다리고 Find 취소를 시도한다. Create/Join 지연·실패는 `BeginRecoveryDestroy`로 잔여 세션을 처리한다. Destroy 완료는 성공 플래그와 NamedSession 잔존을 확인한다. Find는 취소/완료 결과에 따라 종료하며 Update 지연 완료는 실패 처리한다. `CompleteOperation`은 Delegate·Timer·상태를 정리한 뒤 원 작업 결과를 알린다. `FailClosedRecovery`의 bRecoveryBlocked와 `RejectBlockedOperation`은 불확실한 상태에서 새 요청을 막는다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](ImplementationStatus.md)의 E8 절을 함께 읽는다.

### E9-A — 전원 튜토리얼과 300초 제한

**설계 승인:** 경기 시작 전 전원 완료, 기존300초 제한, 완료·실패·시간 초과·이탈 구분, 서버 최종 판정을 채택한다.

**구현 확인:** `ADMMatchGameMode::StartPreMatchTutorialGate`는 확정 명단을 기준으로 완료 집합·300초 타이머·Client 튜토리얼 표시를 관리하고 완료 후 Countdown으로 진행한다. 표시 실패·시간 초과는 현재 내부 실패 중단 경로다. 도착120초 타이머와 별도다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](ImplementationStatus.md)의 E9-A 절을 함께 읽는다.

### E9-B — 서버 리스폰 재시도 상한

**설계 승인:** 최초 실패 후 최대3회 재시도, 서버 관리, 무한 재시도 방지, 성공 시 Pawn·Controller·PlayerState 연결 복구, 실패 원인·소진 기록을 유지한다.

**구현 확인:** `UDMRespawnCoordinator::HandleRespawnTimerExpired` / `ScheduleRespawnRetry`가 재시도와 연결·보호 적용을 관리한다. 기본 재시도 간격은 GameMode의1초이며 최소0.05초 보정이 있다. 최대3회 소진 시 현재 코드는 전체 경기 내부 실패 중단을 요청한다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](ImplementationStatus.md)의 E9-B 절을 함께 읽는다.

### E9-C — 플레이어 실제 사망에 대한 NPC 비웃음

**설계 승인:** 실제 플레이어 사망 후 주변 NPC 반응과 비웃음 표현을 유지한다. Panic/Stun·행동 상태 충돌을 막고 다른 참가자에게 제공하는 사망 위치·상황 단서를 고려한다. 단순한 장식 효과로 취급하지 않는다.

**구현 확인:** GameMode 사망 처리 → `UDMNPCPopulationDirector::HandleNearbyPlayerDeath` → `ADMJellyAIController::TryStartDeathMockingReaction`의 행동 전환·이동 중지·Laugh 요청 경로가 있다. 규칙 DataAsset C++ 기본값은 반경800cm, 최대3명, 지속2.5초다. 이는 현재 구현 기준값이며 실제 에셋 적용 수치와 재생 성공을 의미하지 않는다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](ImplementationStatus.md)의 E9-C 절을 함께 읽는다.

### E10 — 정보 공개 범주와 Listen Host 신뢰 경계

**설계 승인:** Server Authority·최소 전송·OwnerOnly를 유지한다. 전체 Client 공개 / Owner Client 전용 / 스킬 시전자 전용 / 관전자 예외 / 서버 전용으로 구분한다. 판정 원본과 표현 데이터를 분리한다. UI 비표시를 비전송 보장으로 취급하지 않는다. Listen Host는 권위 서버를 실행하는 신뢰 서버다. Host에게 서버 원본까지 은폐한다고 보장하지 않는다.

| 범주 | 목표 계약 | 현재 코드 관측 / 승인 경계 |
|---|---|---|
| 전체 Client 공개 | 공개 경기 상태·필요한 관찰 표현만 | GameState Phase·공개 이벤트/퀘스트·최종 결과, PlayerState rematch ready, Character Action/스턴·투명 표현. Relevant 범위와 필드별 재검수 필요 |
| Owner 전용 | 개인 상태·개인 응답 최소화 | MatchPlayerState LifeState/BaseSlot/운반 여부, CharacterState 상세 태그·투명화 시간은 OwnerOnly |
| 시전자 전용 | 기능에 필요한 최소 정보의 예외 명세 | ClientSetFootstepAmplifyPresentation은 대상 PlayerState를 전달. 현재 전달 사실이며 해당 참조 전체의 최종 예외 승인은 시스템 명세 확인 필요 |
| 관전자 예외 | 관전 정책상 필요한 범위만 | ClientBeginSpectatorSequence의 Context·KillerPawn 등은 현재 구현. 숨길 정보와 예외 범위를 별도 확인 |
| 서버 전용 | 원본 판정·불필요한 신원/운반 관계 | RelicManager 비복제, 스킬 서버 대상 선정. 클라이언트가 대상 Actor 대신 선택 정책을 제출 |

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](ImplementationStatus.md)의 E10 절을 함께 읽는다.

**검증 상태:** BP/DataAsset/Animation 실제 적용은 에셋 미검증, 빌드·PIE·Steam은 실행 미검증이다. 아래 완료 기준은 향후 검증 시나리오이며 통과 기록이 아니다. C++·WBP·Config 변경은 이번 문서 승인에 포함되지 않는다. [공통 게임 규칙](GameplayRules.md) · [네트워크 정책](NetworkPolicy.md)를 상호 참조한다.

---

> **규칙 기준:** 첨부 `Docs/Design/GameDesign.md` 및 단계별 최종 설계를 정규화했다. 이 수치는 **목표 기획**이며, 실제 적용된 DataAsset/Blueprint/C++의 값을 보증하지 않는다. 수치 수정은 규칙 원본 한 곳과 참조 지점을 함께 확인한다.

## 1. 경기 기본값

P2 E3의 도착 설계 승인 기록은 **120초**다. 아래 **90초**는 현재 `ADMMatchGameMode`의 도착 제한 C++ 기본값이며, 최종 설계 목표는 미확정이다.

| 항목 | 규칙 |
|---|---|
| 매치 인원 | **2~4명** (Listen Server, 개인전) |
| 시작 잠금 | Host + 참가자 수 + 모든 인원 Ready + 맵/Steam 광고 상태 서버 검사 |
| 참가 확정 | 시작 시 `StartingPlayerCount`·BaseSlot·유물·NPC 수 스냅샷 고정 |
| 경기 중 입장 | 불가. 호스트 이전·재접속 미지원 |
| 경기 맵 도착 제한 | C++ 기본 **90초**, 실패 시 `Aborted`. P2 E3 설계 기록 **120초**, 최종 목표 미확정 |
| 정규 시간 | **900초** |
| 연장전 | 공동 선두 발생 시 소개 **5초** 이후 최대 **180초** |
| 결과 화면 | 서버 기준 **30초** 뒤 자동 로비 복귀 |
| 결과 캐시 ACK | 각 Client 캐시 후 `ResultId` ACK; 서버는 전원 확인 또는 최대 **5초** 대기 후 이동 진행 |

### Match Phase

```text
Waiting → Countdown → Playing ──즉시/단독 승리──→ Finished
                          │
                          └─정규 종료 공동 선두→ OvertimeIntro → Overtime → Finished
모든 진행 Phase ──필수 실패/이탈/중단──→ Aborted
Finished, Aborted → 매치 내에서 되돌아가지 않음
```

서버만 Phase 전환 및 종료를 확정한다. Client 시간은 `PhaseEndServerTime - GetServerWorldTimeSeconds()`로 계산한다.

## 2. 유물 수량·승패

| 시작 인원 | 전체 유물 (`인원+1`) | 즉시 승리 등록 개수 | 목표 NPC (×15) |
|---:|---:|---:|---:|
| 2 | **3** | **3** | **30** |
| 3 | **4** | **3** | **45** |
| 4 | **5** | **4** | **60** |

각 활성 BaseSlot에 유물 하나, 중앙 공용 위치에 하나가 있다. **등록 유물 수**는 `Registered` + `DepositProtected`만 포함하며, Carried/Dropped는 제외한다. 등록 수 원본은 `ADMRelicManager`가 상태를 집계한 결과다.

| 종료 상황 | 승자 결정 |
|---|---|
| 즉시 유물 승리 | 자기 BaseSlot에 목표 수를 서버가 등록 Commit |
| 전멸 승리 | **3~4인**, 옵션 활성, 마지막 연결 생존자 한 명일 때만. **2인전 제외** |
| 정규 시간 종료 | 등록 유물 수 단독 선두면 승리, 공동 선두면 연장전 |
| 연장 종료 | 등록 수 단독 선두 승리, 공동 최고면 승자 없음 |
| 비정상 중단 | `Aborted`, 정상 승리와 다른 사유로 결과 구성 |

한 번 확정된 결과를 덮어쓰지 않는다. 종료 순위는 최종 등록 수 기준(동점 공동 순위). **결과 화면의 '유물' 열은 누적 획득 수**이며 순위 지표와 다르다. 전멸 승리 때만 승자를 단독 1위로 고정한다.

## 3. 유물 조작과 상태

```text
PublicSpawn / Dropped ──Pickup(2s)──→ Carried
Registered ──Steal(2s)──→ Carried
Carried ──Deposit(2s)──→ DepositProtected ──5s──→ Registered
Carried ──Q 또는 사망──→ Dropped
```

| 동작 | 입력 | 시간 / 제약 |
|---|---|---|
| 필드 유물 습득 | **F 길게** | **2초**. 1인 운반 한도 **1개** |
| 상자 열기/닫기 | **F** | 열기/닫기는 2초 홀드가 아님 |
| 자기 상자 등록 | 패널에서 **Space 길게** | **2초**, 운반 중일 때만 |
| 다른 상자 탈취 | 패널에서 **Space 길게** | **2초**, `Registered` 대상만 |
| 수동 드롭 | **Q** | 서버가 드롭 위치·벽 차폐 검사 |
| 등록 보호 | — | **5초** 동안 탈취 불가; 수량에는 포함 |
| 기본 후보 거리 | — | 설계 기본 **250cm**, 로컬 안내와 서버 재검사 분리 |

- `RequestId` 중복·과거 요청 거절, 유물 단위 선착순 예약, **진행 중 0.1초 서버 거리·LOS 검증**, 완료 시 재검증.
- 입력 해제/사망/스턴/자세/거리 이탈/유물 상태 변경/Phase 전환 시 취소. 실패·사용자 취소·시스템 취소를 구분해 페널티 오적용 방지.
- NPC 등 Pawn이 중간을 지나가는 것은 LOS 차단으로 취급하지 않는다.
- 상자 등록 수는 **상자를 연 사람에게만 서버 스냅샷**으로 전달한다.

## 4. 캐릭터·입력·자세

| 구분 | 기본 입력 |
|---|---|
| 이동·시점·점프 | WASD / Mouse / Space |
| 달리기 | Shift hold |
| 1·3인칭 | RMB (일반 입력) |
| 공격·방어 | LMB / E |
| 상호작용·유물 드롭 | F / Q |
| 자세 | R (기본→납작→웅크리기→대자→기본) |
| 감정표현 | T 휠 (Dance/Laugh/Cheer/FakeDeath 등) |
| 스킬 | 1 투명화, 2 발소리, 3 시야 교란 |
| PTT·마이크 | V / M |
| 메뉴 | Esc (Listen 게임 전체 Pause 금지) |

자세 중에는 일반 공격·방어·Sprint·스킬·상호작용·감정표현 불가. 자세 모드에서 LMB는 다음 자세, RMB는 카메라 전환. 상자 모드 Space는 Hold 등록/탈취로 바뀐다. Context 변경 시 이미 눌린 키가 중복 적용되지 않아야 한다.

**이동 속도 주의:** 기존 단계 문서에는 WalkSpeed=300 기록이 남아 있고, 별도 최근 수정 요청은 **WalkSpeed 200cm/s, SprintSpeed 500cm/s**(플레이어·NPC 동일)이다. 이 변경이 현재 소스와 BP에 반영됐는지는 확인 불가. Codex는 적용 전 현재 설정 및 최신 사용자 의도를 대조한다.

## 5. 전투·사망

| 항목 | 기준 |
|---|---:|
| 공격 쿨다운 | **8초** |
| 방어 지속 / 쿨다운 | **1초 / 10초** |
| 방어 횟수 | 유효 공격 **1회**, 전방 한정 |
| NPC 오인 공격 | NPC는 생존, 공격자 **3초 스턴** |
| 리스폰 | 사망 확정 후 **30초** |
| 리스폰 보호 | **3초** (공격/방어/상호작용/스킬 불가) |

- 공격은 HP 감소형이 아니라 유효 1타 사망형. 공격 몽타주의 서버 `AttackHit` Notify 시점에서 범위/전방/거리/월드 차폐를 판정한다.
- `AttackRange`, `AttackAngle`, `DefenseArcAngle` 등의 실제 수치는 DataAsset 확인이 필요하며 문서만으로 확정할 수 없다. 각도의 전체값/반각 내적 해석에 주의한다.
- 한 공격의 여러 유효 대상을 처리할 수 있지만 동일 Sequence의 Notify는 중복 처리하지 않는다.
- 리스폰 보호·유효 방어는 사망을 막는다. 보호 중 방어 횟수를 소비하지 않는다.
- 사망 시 운반 유물 Drop, 상호작용/스킬/액션 정리, Ragdoll 표현, 서버 리스폰/로컬 관전.

## 6. 스킬

현재 구현에 크기 증폭·이동 둔화가 추가됐다. 기존 설계 승인과 별도로 기록하며 적용 경로·확인 대기 중인 DA 값은 [스킬 명세](FeatureSpecs/07_Skills.md)에 정리한다.

| 스킬 | 입력 | 쿨다운 | 효과 |
|---|---|---:|---|
| 투명화 | 1 | **25초** | **5초**, 자신. 유물 운반 중 불가, 공격 시 서버가 종료 |
| 발소리 증폭 | 2 → LMB/RMB | **60초** | **8초**, 선택된 상대의 발소리가 시전자에게 크게 들림 |
| 시야 교란 | 3 → LMB/RMB | **600초** | 핵심 효과 **5초** (+진입 0.75초/복귀 0.5초) |

대상 모드: LMB=적격 후보 무작위, RMB=등록 유물 수가 가장 많은 적격 후보 중 무작위. 서버가 후보를 정하며 NPC·본인·사망자·보호 대상 제외. 거리/LOS 제한 없음. 취소·실패·후보 없음은 쿨다운 미소모. 쿨다운은 사망·관전·리스폰에도 계속 흐름.

## 7. NPC

- 목표 수는 시작 인원 ×15, 최대 60. `DM_NPCSpawn` TargetPoint가 충분해야 하고 스폰 지점을 섞는다.
- Idle/Walk/Sprint/Jump/RelicApproach 등 가중치 기반 타이머 AI; NPC는 유물을 **실제로 획득/등록/탈취하지 않는다**.
- 오인 공격 시 맞은 NPC + 근처 최대 6명(기본 반경 1200cm)이 비명·도주, 기본 도주 거리 900cm.
- 플레이어와 **외형 및 걷기/달리기 속도 계약**을 공유하되, 게임 결과에 개입하는 권한은 갖지 않는다.

## 8. 퀘스트·랜덤 이벤트

퀘스트 4종: Jump 횟수, Sprint 누적 시간, Emote 연속 유지, 지정 Pose 유지. 서버의 **실제 행동**으로 진행도를 계산한다. 설계상 시작 시도 시각은 **Playing +120/+360/+600초**(2/6/10분), 2~3회 실행 시도; 이벤트와 동시 실행 금지. 초기 테스트 예시는 점프 3회/각 지속형 3초/제한 20초. **퀘스트 성공 보상 없음.** 마감 실패 시 주변 **3D 사이렌**, 이후 완수 시 해제.

랜덤 이벤트는 `GameDesign.md` 기준 **Playing +240/+480/+720초**(4/8/12분)에 2~3회 시도, 조건 미충족 시 건너뛰며 연속 동일 이벤트 금지. **정전:** 3초 예고, 2초 전 깜빡임, 10초 지속. **젤리 패닉:** NPC가 1~3초 간격으로 패닉 반응, 10초. **입력 반전:** 살아 있는 플레이어의 WASD 이동만 10초 반전. 이벤트 시작 직전에 조건 재검증(Playing, 퀘스트 없음, 필요한 생존 조건).

## 9. 사망 관전·음성

| 사망 후 경과 | 로컬 화면 |
|---|---|
| 0~5초 | 시체 중심 DeathCam |
| 5~10초 | 공격자의 **현재 시점** (녹화 Replay 아님) |
| 10~25초 | 벽/바닥 통과 방지된 자유 카메라 |
| 25~30초 | Fade + 5→1 카운트다운 |
| 30초 이후 | 서버 성공 시 새 Pawn에 조작 복귀, 실패면 재시도 |

Voice: Lobby 전체 청취; 살아 있는 플레이어는 **300cm 안 최대, 2500cm 밖 무음**인 근접 음성; 사망자는 거리 관계없는 전체 청취. 원격 Pawn 위치를 모르면 생존 청취자는 해당 Talker를 음소거해 전역 누출을 방지. 발화자 이름/방향/음성 UI를 추가하지 않는다.

## 10. 비공개 정보·예외 공개

- 평상시 상대 이름·진영·운반자·등록 수·스킬 대상 등을 전역 전달하거나 HUD에 표시하지 않는다.
- **정규 경기 종료 60초 전, 3초 동안** 전체 유물 위치를 일시 공개 (운반자 이름은 비공개).
- **연장 소개 5초 동안** BaseSlot 문양별 등록 유물 수를 일시 공개한 뒤 제거.
- 결과 화면에서는 참가자 이름·순위·누적 획득 유물·킬을 표시한다.

## 11. 근거 / 충돌 주의

[최종 기획서](../Docs/Design/GameDesign.md) · [입력](../Docs/Design/EnhancedInput.md) · [단계별 설계](../Docs/Design/Stages/README.md) · [결과 판정](../Docs/Technical/03_Match_Flow_Result.md) · [유물](../Docs/Technical/06_Relic_Interaction.md).

과거 프로젝트 개요의 **2인 유물 4개**, 과거 NPC **N×10**, 과거 속도 **300**은 현재 문서 및 후속 사용자 변경 요청과 차이가 있다. 소스가 없으므로 어느 버전이 빌드에 들어갔는지는 판별하지 못한다. 수치를 임의로 섞지 말 것.
