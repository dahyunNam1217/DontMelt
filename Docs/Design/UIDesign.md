# Don't Melt — UI 기획서

## P2 승인 동기화 — 2026-11-13

이번 사용자 P2 요청의 명시 범위만 **설계 승인**으로 채택한다. 과거 승인으로 소급하지 않는다. 기준 HEAD는 `914173a`이며 구현 사실·설계 승인·에셋 적용·실행 검증은 별개다. 상세 근거와 미결정 사항은 [ImplementationStatus 10절](../../CodexContext/ImplementationStatus.md#10-p2-승인-설계-및-현재-구현--2026-11-13)을 따른다.

**아래 기존 본문과의 관계:** 이 절 뒤의 기존 본문은 삭제하지 않고 이전 설계·조사 기록으로 보존한다. E1~E10과 충돌하는 값·완료/승인/미확인 표현은 이 절과 중앙10절이 대체한다. 특히90초·Walk300·NPC×10/40·NPC 발소리·3요소 이벤트 HUD·타임아웃 즉시 재요청은 최신 목표가 아니다. E1~E10 밖의 항목을 이번 승인으로 일괄 확정하지 않는다.

### E1 — 결과 통계와 순위 분리

**설계 승인:** 결과표 유물 열은 경기 중 누적 획득 횟수, 최종 순위는 종료 시 등록 유물 수를 사용하며 동률 공동 순위를 허용한다. 두 통계를 별도 관리하고 표시 변경으로 서버 승리 판정을 바꾸지 않는다.

**구현 확인 / 부분 구현:** `ADMRelicManager::TryCommitRelicAcquisition` → `ADMMatchPlayerState::IncrementAcquiredRelicCount` → `ADMMatchGameMode::BuildFinalPlayerResults`로 누적 통계를 보관한다. `FDMMatchResultEntry`의 `AcquiredRelicCount`와 `RelicCount`가 분리되어 있다. 그러나 `UDMPlayerResultRowWidget::SetResultEntry`는 현재 `RelicCount`를 표시하므로 목표 표시와 불일치한다. 성공 Pickup/Steal Commit마다 증가하는 것이 현재 동작이다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../../CodexContext/ImplementationStatus.md)의 E1 절을 함께 읽는다.

### E2 — 전멸 승자 단독 1위

**설계 승인:** 일반 종료는 최종 등록 수 순위다. 전멸 승리에서는 등록 수와 무관하게 승자를 단독1위로 고정하고, 나머지는 2위부터 등록 수 기준 공동 순위를 부여한다. 승리 배너와 결과표의 승자 의미가 모순되지 않아야 한다.

**구현 확인 / 부분 구현:** `DMMatchResultRules::SortAndRankResults`는 등록 수 내림차순이며 같은 등록 수에서만 승자를 우선한다. `ADMMatchGameMode::BuildFinalPlayerResults`가 이 함수를 호출한다. 따라서 등록 수가 적은 전멸 승자가 결과표1위가 아닐 수 있다. 현재 함수는 종료 사유별 승자 고정 계약을 받지 않는다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../../CodexContext/ImplementationStatus.md)의 E2 절을 함께 읽는다.

### E7 — RandomEvent HUD 설명 단일화

**설계 승인:** `TXT_EventDescription`만 표시한다. 이름과 진행바는 표시하지 않으며 필수 UI 바인딩에서도 해제하는 것이 목표다. 설명 갱신, 경고·진행·종료 전환, GameState 연결, 종료 정리는 유지한다. DataAsset DisplayName과 서버 이벤트 판정 데이터는 HUD 미사용을 이유로 제거하지 않는다.

**구현 확인 / 부분 구현:** `UDMRandomEventHUDWidget::RefreshPresentation`은 현재 TXT_EventName/TXT_EventDescription/PB_EventProgress 세 요소를 모두 요구하고 이름·진행률도 갱신한다. `UDMGameRootWidget`은 GameState 및 표시 활성 상태를 연결한다. 조사한 C++에서 외부 시스템이 이름·진행바 자식 포인터를 직접 사용하는 경로는 확인하지 못했다. 기존 세 요소 명세는 이전 버전이다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../../CodexContext/ImplementationStatus.md)의 E7 절을 함께 읽는다.

### E8 — Steam 안전 복구 상태 머신 유지

**설계 승인:** 기존 상태 머신을 유지하며 전면 재작성하지 않는다. Create/Find/Join/Destroy/Update별 비동기 결과를 구분하고 기본 요청30초를 유지한다. 타임아웃은 플랫폼 취소 완료가 아니다. 작업 중 중복 요청을 거절하고 원 요청 완료·필요한 잔여 세션 정리·안전한 재시도 조건 확인 후 다음 요청을 허용한다. 불확실하면 차단·재시작 안내한다. 영구 차단 후 무의미한 로비 재시도를 반복하지 않는다.

**구현 확인:** `UDMOnlineSessionSubsystem::HandleOperationTimeout`은 지연 완료를 기다리고 Find 취소를 시도한다. Create/Join 지연·실패는 `BeginRecoveryDestroy`로 잔여 세션을 처리한다. Destroy 완료는 성공 플래그와 NamedSession 잔존을 확인한다. Find는 취소/완료 결과에 따라 종료하며 Update 지연 완료는 실패 처리한다. `CompleteOperation`은 Delegate·Timer·상태를 정리한 뒤 원 작업 결과를 알린다. `FailClosedRecovery`의 bRecoveryBlocked와 `RejectBlockedOperation`은 불확실한 상태에서 새 요청을 막는다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../../CodexContext/ImplementationStatus.md)의 E8 절을 함께 읽는다.

**검증 상태:** BP/DataAsset/Animation 실제 적용은 에셋 미검증, 빌드·PIE·Steam은 실행 미검증이다. 아래 완료 기준은 향후 검증 시나리오이며 통과 기록이 아니다. C++·WBP·Config 변경은 이번 문서 승인에 포함되지 않는다. [공통 게임 규칙](../../CodexContext/GameplayRules.md) · [네트워크 정책](../../CodexContext/NetworkPolicy.md)를 상호 참조한다.

---

| 항목 | 내용 |
|---|---|
| 구현 | UMG Widget Blueprint + C++ 위젯 클래스 (`BindWidget`) |
| 원칙 | 게임 판정과 UI 표시를 분리하고, 공개해도 되는 정보만 표시한다 |

## 1. 화면 구성

| 화면 | 주요 정보와 조작 |
|---|---|
| MainMenu | 방 생성·참가, 방 목록, 설정, 종료 |
| Lobby | 플레이어 목록, Host, Ready, 시작 요청 |
| Gameplay HUD | 경기 상태·남은 시간, 전투·스킬 쿨다운, 운반 유물, 상태 효과 |
| Interaction | 상자 열기·닫기, 유물 습득, 등록·탈취 진행 표시 |
| Skill Targeting | 랜덤·선두 대상 선택, 취소 |
| Quest / Event | 목표·진행도·남은 시간, 이벤트 이름·설명·진행 바 |
| Spectator | 사망 후 관전 안내, 카운트다운 |
| MatchResult | 승패 제목, 4열 결과표, 재경기·로비 복귀 |
| Ingame Menu / Settings | 공통 5개 설정 탭, 변경 확인창 |
| Network Failure | 연결 실패·호스트 이탈 안내, 메인 메뉴 복귀 |

기본 HUD는 플레이 중 계속 표시하고, 상자와 스킬 대상 선택만 모달로 띄운다. 플레이 중에는 상대 닉네임, 진영, 유물 소지 정보를 붙이지 않는다.

## 2. 경기 UI 계층

```text
WBP_DMGameRoot
├─ GameplayLayer
│  ├─ WBP_DMGameplayHUD
│  │  ├─ WBP_DMCombatHUD
│  │  ├─ WBP_DMSkillBar
│  │  ├─ WBP_DMRelicHUD
│  │  └─ WBP_DMStatusHUD
│  ├─ WBP_DMInteractionPrompt
│  └─ WBP_DMVoiceStatus
├─ ActivityLayer        퀘스트·이벤트·자세
├─ ModalLayer           상자·스킬 대상·감정표현 / 공통 Dim
├─ AnnouncementLayer    알림·유물 공개·연장전 소개
├─ SpectatorLayer       관전 전용 HUD
└─ MenuLayer            인게임 메뉴·경기 결과
```

`ADMMatchPlayerController`가 로컬 `WBP_DMGameRoot`를 만들고 정리하며 입력 모드를 관리한다. 카메라 암전은 `APlayerCameraManager`의 Fade를 쓴다.

## 3. 위젯별 데이터 계약

| 위젯 | 표시 데이터 | 데이터 원본 |
|---|---|---|
| `WBP_DMGameplayHUD` | `TXT_MatchPhase`, `TXT_RemainingTime` | `ADMMatchGameState` |
| `WBP_DMCombatHUD` | 공격·방어 쿨다운 | `UDMCombatComponent` |
| `WBP_DMSkillBar`, `WBP_DMSkillSlot` | 스킬 키·아이콘·쿨다운 | `UDMSkillComponent` |
| `WBP_DMRelicHUD` | 운반 여부, Q 드롭 안내 | 소유자 전용 운반 상태 |
| `WBP_DMInteractionPrompt` | 가능한 F 상호작용 안내 | 로컬 후보 탐색 (서버와 같은 시야 판정) |
| `WBP_DMChestPanel` | 상자 이름·등록 수·진행 | 상자를 열 때 받은 서버 스냅샷 |
| `WBP_DMSkillTargetPanel` | 랜덤·선두 선택 | Controller + 서버 스킬 사용 가능 여부 |
| `WBP_DMQuestHUD` | 개인 진행·공개 퀘스트 | `UDMQuestProgressComponent`, GameState |
| `WBP_DMRandomEventHUD` | 이벤트 이름·설명·진행 바 | GameState 공개 이벤트 상태 |
| `WBP_DMSpectatorHUD` | 관전 안내·부활 상태 | 소유 Controller |
| `WBP_DMMatchResult` | 결과표·버튼·자동 복귀 시간 | 최종 `FDMMatchResult` |

- **SkillSlot:** `TXT_SkillKey`, `TXT_Cooldown`, `PB_Cooldown`이 기본이고 `IMG_SkillIcon`은 선택 바인딩이다. 슬롯에 이름·설명을 길게 반복하지 않는다.
- **StatusHUD:** `TXT_Stunned`, `TXT_SpawnProtected`, `TXT_Invisible`, `TXT_VisionDistorted`, `TXT_FootstepObserved`. 입력 반전은 이벤트 안내에 묶고 상태 HUD에 따로 두지 않는다.
- **상자 패널:** `F` 한 번으로 열고, `Space`를 누르는 동안 등록·탈취를 진행하고, 떼면 취소한다. 진행 상태는 상자 패널 한 곳에만 표시해, 유물 HUD와 같은 값을 각자 갱신하지 않게 한다.

## 4. 결과 화면

| 요소 | 내용 |
|---|---|
| `TXT_ResultTitle` | 승리·패배 등 결과 제목 |
| `TXT_ResultDetail` | 승자 없음, 경기 중단 등 특수 사유 |
| `PlayerResultRows` | 참가자별 동적 Row (이름 / 등수 / 획득 유물 / 킬) |
| `TXT_AutoReturnCountdown` | 서버 기준 30초 로비 자동 복귀 |
| `BTN_ReturnToLobby`, `BTN_RestartMatch` | Host 전용 |
| `BTN_RematchReady` | Client 재경기 준비 |
| `BTN_QuitGame` | 앱 종료가 아니라 메인 메뉴 이동 |

- 등수는 종료 시 **등록 유물 수** 기준이고, 같으면 공동 순위다. 전멸 승리일 때만 승자를 1위로 고정한다.
- 유물 열은 경기 중 **누적 획득 수**다. 탈취가 있어 등록 수와 다를 수 있다.
- 일반 Client의 버튼이 서버 전체를 이동시키지 않는다.
- 결과 화면은 다른 모든 경기 UI보다 우선한다.

## 5. 설정 UI

탭은 Graphics, Audio, Controls, Language, Voice 순서다. 타이틀·로비·인게임 메뉴에서 같은 위젯을 쓴다.

- 현재 입력값과 저장값을 비교해 변경 여부를 계산한다. 바꿨다가 원래대로 되돌리면 변경 없음으로 본다.
- 변경 중에 탭을 옮기거나 닫으려 하면 **적용 / 변경 취소 / 계속 수정** 중 하나를 고르게 한다. 적용은 모든 탭의 값을 한 번에 저장한다.
- 해상도·창 모드를 바꾸면 15초 확인창을 띄우고, 응답이 없으면 이전 설정으로 되돌린다.
- 마우스 감도·음량은 로컬에 저장하고 서버로 복제하지 않는다.

## 6. 입력과 포커스

- 상자·대상 선택·감정표현 UI는 게임플레이 Context와 충돌하지 않게 전환한다.
- 사망 후에는 일반 HUD 입력을 막고 관전 입력만 받는다. 리스폰하면 새 Pawn으로 다시 바인딩한다.
- Esc 메뉴를 열어도 Listen Server 게임 전체를 멈추지 않는다.
- 일반 조작으로 돌아올 때마다 `GameOnly` 입력 모드와 마우스 캡처를 다시 맞춘다.
- 여러 모달이 겹치면 우선순위가 높은 화면(결과 > 메뉴 > 모달)의 입력을 유지한다.
- `Visibility`와 Hit Test를 구분해 보이지 않는 위젯이 마우스 입력을 가로채지 않게 한다.

## 7. 갱신 방식

Widget은 Controller가 연결해 준 Gameplay Component를 읽고, RepNotify·Delegate가 오면 해당 UI만 갱신한다. 쿨다운·카운트다운처럼 시간이 흐르는 숫자는 서버 종료 시각과의 차이로 계산하고, 필요한 동안만 짧은 타이머로 다시 그린다. UI 때문에 서버를 매 프레임 호출하거나 Tick에서 Actor를 검색하지 않는다.

| 상황 | 갱신 | 정리 시점 |
|---|---|---|
| Phase 변경 | GameState OnRep / Delegate | 결과·Travel |
| 스킬 쿨다운 | SkillComponent 변경 알림 | 새 Pawn 재바인딩 |
| 상자 확인 | 서버 응답 스냅샷 | F로 닫기·사망·거리 이탈 |
| 퀘스트 진행 | 소유자 진행도 변경 | 퀘스트 종료·관전 |
| 이벤트 시작 | GameState 공개 이벤트 | 시간 종료·연장전 |
| 관전 | Controller 로컬 단계 | 리스폰 Handoff |

## 8. 디자인 규칙

진영 문양은 상대의 실제 정체와 연결하지 않는다. 상자에서 읽은 유물 수는 그 상자 패널에만 표시한다. 결과 화면에서는 이름을 공개하지만, 플레이 중에는 플레이어 식별 UI를 붙이지 않는다. 한글·영문 폰트를 섞어 쓰므로 Designer의 기본 텍스트뿐 아니라 C++ `SetText`로 바뀌는 문자열의 글리프까지 확인한다.

관련 문서: [게임 기획서](GameDesign.md) · [10단계 UI·결과](Stages/10_UIAndMatchResult.md) · [UI 구현](../Technical/11_UMG_UI.md)
