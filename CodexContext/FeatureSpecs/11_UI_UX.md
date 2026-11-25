# 11. UMG·상태 HUD·결과 화면·입력 포커스

## P3-9 최신 기준 — 2026-11-17

기준 HEAD: `001a08241ecd01dfa1ef565f11c7791eaf73eaeb`. [최신 구현 대조와 검증 한계](../ImplementationStatus.md#p3-9--최신-아키텍처-결정-및-구현-대조-2026-11-17)를 함께 읽는다. 아래 최신 절과 충돌하는 P2/기존 본문은 당시 설계·조사 이력으로 보존하며 현재 완료 근거로 사용하지 않는다.

- `Private/UI/DMPlayerResultRowWidget.cpp::SetResultEntry`는 `RelicCount`와 `AcquiredRelicCount`를 각각 표시한다. `Public/UI/DMPlayerResultRowWidget.h`의 `TXT_AcquiredRelicCount`는 **BindWidget 필수**이며 기존 필수 항목과 하나라도 없으면 Row 갱신이 반환된다. WBP 변수·형식·부모 연결은 미검증이다.
- `Private/Framework/Match/DMMatchGameMode.cpp::BuildFinalPlayerResults(EndReason, WinningBaseSlot)`는 전멸 종료에만 `SortAndRankResults(..., true)`를 전달한다. 승자는 단독1위, 나머지는 등록 수 내림차순 공동 순위(예:1·2·2·4), 일반 종료는 등록 수 순위다.
- `Private/Tests/DMMatchResultRulesTests.cpp`의 `DontMelt.Match.ResultRules.EliminationRanking` 추가를 확인했다. 승자 등록 수 0/2/5, 전원0, 통계 보존, 반복 정렬, 일반 종료 비고정을 검사한다. 테스트 실행은 미실행이다.
- `Public/UI/DMRandomEventHUDWidget.h`는 `TXT_EventDescription`만 BindWidget 필수다. `TXT_EventName`·`PB_EventProgress`는 BindWidgetOptional 호환 필드이며 존재하면 Collapsed 처리한다.
- `Private/UI/DMRandomEventHUDWidget.cpp::RefreshPresentation`은 설명·예고 접두사만 표시하며 이름/진행률 갱신을 제거했다. GameState 공개 이벤트와 DataAsset 설명을 사용하고 정의 누락 시 대체 문구를 표시한다.
- 기존0.1초 로컬 Timer는 예고→진행 전환과 종료 숨김을 위해 유지한다. 비활성·만료·Destruct 시 정리 경로가 있다. 서버 이벤트 판정과 복제 계약을 변경한 작업이 아니다.
- 설명만 있는 WBP 컴파일, 경고→진행→종료, 정의 누락, Host/Client·Travel 재생성은 미검증이다.

## P2 승인 동기화 — 2026-11-13 (당시 기록; 최신 기준은 상단 P3-9)

이번 사용자 P2 요청의 명시 범위만 **설계 승인**으로 채택한다. 과거 승인으로 소급하지 않는다. 기준 HEAD는 `914173a`이며 구현 사실·설계 승인·에셋 적용·실행 검증은 별개다. 상세 근거와 미결정 사항은 [ImplementationStatus 10절](../ImplementationStatus.md#10-p2-승인-설계-및-현재-구현--2026-11-13)을 따른다.

**아래 기존 본문과의 관계:** 이 절 뒤의 기존 본문은 삭제하지 않고 이전 설계·조사 기록으로 보존한다. E1~E10과 충돌하는 값·완료/승인/미확인 표현은 이 절과 중앙10절이 대체한다. 특히90초·Walk300·NPC×10/40·NPC 발소리·3요소 이벤트 HUD·타임아웃 즉시 재요청은 최신 목표가 아니다. E1~E10 밖의 항목을 이번 승인으로 일괄 확정하지 않는다.

### E1 — 결과 통계와 순위 분리

**설계 승인:** 결과표 유물 열은 경기 중 누적 획득 횟수, 최종 순위는 종료 시 등록 유물 수를 사용하며 동률 공동 순위를 허용한다. 두 통계를 별도 관리하고 표시 변경으로 서버 승리 판정을 바꾸지 않는다.

**P2 당시 기록:** 획득 수 열은 미반영 상태였다. `ADMRelicManager::TryCommitRelicAcquisition`이 성공하면 `ADMMatchPlayerState::IncrementAcquiredRelicCount`로 누적 획득 수를 기록하고 `ADMMatchGameMode::BuildFinalPlayerResults`가 등록 수와 획득 수를 구분해 결과를 구성한다. **현재 C++:** `RelicCount`는 경기 종료 시 등록 수, `AcquiredRelicCount`는 경기 중 누적 획득 수다. `SetResultEntry`는 두 값을 각각 `TXT_RelicCount`와 `TXT_AcquiredRelicCount`에 설정한다. 실제 WBP 바인딩은 미검증이다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E1 절을 함께 읽는다.

### E2 — 전멸 승자 단독 1위

**설계 승인:** 일반 종료는 최종 등록 수 순위다. 전멸 승리에서는 등록 수와 무관하게 승자를 단독1위로 고정하고, 나머지는 2위부터 등록 수 기준 공동 순위를 부여한다. 승리 배너와 결과표의 승자 의미가 모순되지 않아야 한다.

**P2 당시 기록:** 정렬은 등록 수를 우선해 전멸 승자의 단독 1위를 보장하지 않았다. **현재 C++:** 서버가 전멸 승리로 경기를 종료하면 `BuildFinalPlayerResults`가 `SortAndRankResults`에 단독 1위 옵션을 전달한다. 나머지 참가자는 등록 수 내림차순으로 정렬하고, 동률에는 공동 순위를 부여한다. 실제 WBP 표시는 미검증이다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E2 절을 함께 읽는다.

### E7 — RandomEvent HUD 설명 단일화

**설계 승인:** `TXT_EventDescription`만 표시한다. 이름과 진행바는 표시하지 않으며 필수 UI 바인딩에서도 해제하는 것이 목표다. 설명 갱신, 경고·진행·종료 전환, GameState 연결, 종료 정리는 유지한다. DataAsset DisplayName과 서버 이벤트 판정 데이터는 HUD 미사용을 이유로 제거하지 않는다.

**구현 확인 / 부분 구현:** `UDMRandomEventHUDWidget::RefreshPresentation`은 현재 TXT_EventName/TXT_EventDescription/PB_EventProgress 세 요소를 모두 요구하고 이름·진행률도 갱신한다. `UDMGameRootWidget`은 GameState 및 표시 활성 상태를 연결한다. 조사한 C++에서 외부 시스템이 이름·진행바 자식 포인터를 직접 사용하는 경로는 확인하지 못했다. 기존 세 요소 명세는 이전 버전이다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E7 절을 함께 읽는다.

### E8 — Steam 안전 복구 상태 머신 유지

**설계 승인:** 기존 상태 머신을 유지하며 전면 재작성하지 않는다. Create/Find/Join/Destroy/Update별 비동기 결과를 구분하고 기본 요청30초를 유지한다. 타임아웃은 플랫폼 취소 완료가 아니다. 작업 중 중복 요청을 거절하고 원 요청 완료·필요한 잔여 세션 정리·안전한 재시도 조건 확인 후 다음 요청을 허용한다. 불확실하면 차단·재시작 안내한다. 영구 차단 후 무의미한 로비 재시도를 반복하지 않는다.

**구현 확인:** `UDMOnlineSessionSubsystem::HandleOperationTimeout`은 지연 완료를 기다리고 Find 취소를 시도한다. Create/Join 지연·실패는 `BeginRecoveryDestroy`로 잔여 세션을 처리한다. Destroy 완료는 성공 플래그와 NamedSession 잔존을 확인한다. Find는 취소/완료 결과에 따라 종료하며 Update 지연 완료는 실패 처리한다. `CompleteOperation`은 Delegate·Timer·상태를 정리한 뒤 원 작업 결과를 알린다. `FailClosedRecovery`의 bRecoveryBlocked와 `RejectBlockedOperation`은 불확실한 상태에서 새 요청을 막는다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E8 절을 함께 읽는다.

**검증 상태:** BP/DataAsset/Animation 실제 적용은 에셋 미검증, 빌드·PIE·Steam은 실행 미검증이다. 아래 완료 기준은 향후 검증 시나리오이며 통과 기록이 아니다. C++·WBP·Config 변경은 이번 문서 승인에 포함되지 않는다. [공통 게임 규칙](../GameplayRules.md) · [네트워크 정책](../NetworkPolicy.md)를 상호 참조한다.

---

## UI 책임

`ADMMatchPlayerController`가 소유 클라이언트에 `WBP_DMGameRoot` 한 번 생성/정리한다. `UDMGameRootWidget` 및 자식 `UDM*Widget` C++ 클래스는 GameState/PlayerState/Component/Local Subsystem 이벤트의 **표현만** 담당. UI에서 유물 등록 수·승패·기절 판정을 다시 하지 않는다.

## 레이어

미니맵은 `Playing`에서 자기 Pawn과 활성 자기 BaseZone을 표시한다. 상세 도움말은 메뉴·상자·스킬 선택이 아닌 허용 상황에서 입력을 누르는 동안 표시한다. 실제 WBP 레이어 연결은 확인 대상이다.

```text
WBP_DMGameRoot
├─ GameplayLayer: GameplayHUD(Combat/Skill/Relic/Status), InteractionPrompt, VoiceStatus
├─ ActivityLayer: Quest/RandomEvent/Pose
├─ ModalLayer: Chest/SkillTargeting/Emote + Dim
├─ AnnouncementLayer: 공개 타이밍/연장전 등 알림
├─ SpectatorLayer: 사망 관전 HUD
└─ MenuLayer: 일시 메뉴 / MatchResult
```

우선순위: 결과 > 메뉴 > 모달 > 기본 HUD. Widget Visibility와 Hit Test를 구분하여 **보이지 않는 UI가 입력을 가로채지 않게** 한다. Esc는 로컬 메뉴만 열며 Listen Server 월드 전체 일시 정지 금지.

## 데이터 계약

| 위젯 | 내용 | 원본 |
|---|---|---|
| `WBP_DMGameplayHUD` | Phase·남은 서버 시간 | MatchGameState |
| `WBP_DMCombatHUD` | 공격/방어 쿨다운 수치·진행 | CombatComponent |
| `WBP_DMSkillBar`/Slot | 조작키, 아이콘(선택), 남은초, ProgressBar | SkillComponent |
| `WBP_DMRelicHUD` | 운반 상태·Q 드롭 | 소유자 운반 표시 |
| `WBP_DMInteractionPrompt` | F 상호작용 후보 안내 | 로컬 후보 검색, 서버와 별도 검증 |
| `WBP_DMChestPanel` | 열어본 상자의 등록 수·Space Hold 진행 | 해당 소유자 서버 스냅샷/Session |
| `WBP_DMQuestHUD` | 개인 목표·진행도/마감 | QuestProgress (OwnerOnly), 공개 Quest |
| `WBP_DMRandomEventHUD` | 현재 이벤트 안내 | 공개 이벤트 상태 |
| `WBP_DMStatusHUD` | Stunned, SpawnProtected, Invisible, VisionDistorted, FootstepObserved | 적합한 소유 컴포넌트 |
| `WBP_DMSpectatorHUD` | 관전 단계/카운트다운 | 로컬 SpectatorPresentation |
| `WBP_DMMatchResult` | 이름·등수·누적 획득 유물 수·Kill, 30초 자동 복귀 | 최종 서버 Result |

**문서 충돌:** 원본 `UIDesign.md`/기술 문서에는 RandomEventHUD를 이름·설명·진행바로 적었지만 이후 UI 대화에는 **설명 텍스트 하나만** 유지하려는 지시가 있었다. 현재 Widget 에셋/C++를 확인하지 못했으므로 `ImplementationStatus.md`에 검증 필요로 표시. 다른 UI들도 실제 BindWidget 구성 확인 전 임의 필드 추가하지 않는다.

## 갱신·수명

미니맵은 활성 UI에서 로컬 위치 갱신을 수행하고 비활성화에서 타이머를 해제한다. 퀘스트는 RunId로 완료·퇴장 수명을 관리하고 상태 HUD는 소유자 타이밍과 자기 Controller의 경고를 사용한다.

- GameState RepNotify, Component Delegate 갱신. 쿨다운 표시·상호작용 후보만 **필요할 때 타이머**(문서상 InteractionPrompt 기본 0.2초)로 갱신. Actor 전수 탐색/서버 폴링 금지.
- Pawn 교체 시 이전 Pawn/PlayerState Delegate 해제 후 새 원본 바인딩.
- ChestPanel은 F 한 번으로 열고 닫고 Space Hold/Release로 진행/취소. ChestPanel과 RelicHUD에 같은 상자 진행 상태를 중복 소유시키지 않음.
- 사망 시 관전 HUD로 전환, 결과가 오면 그 위에 결과 UI만 우선 표시.
- `BindWidget` 이름과 Widget Blueprint 실제 변수 이름이 정확히 맞아야 함. 한글/영문 혼용 폰트에서 **C++ SetText 문자열도** 글리프 확인.

## 결과 버튼·로비 복귀

Host만 전체 로비 복귀/재시작을 승인. Client는 Rematch Ready 등 소유자 동작. Result는 `ResultId` 캐시 후 ACK이며, 자동 복귀 30초. 경기 결과표 `유물` 열은 등록 수가 아닌 **누적 획득 유물 수**.

## 완료 기준

- Host와 원격 Client 같은 HUD Phase·시각, 상호작용 안내/상자/쿨다운/결과 정상.
- 모달에서 입력 Context/마우스 캡처 복귀, 위젯 배경 Hit Test로 입력 안 막힘.
- 사망/리스폰 반복·ServerTravel 후 Delegate/Timer/Widget 중복 없음.
- 결과 화면 경쟁상태, Client가 Host 권한 버튼 실행 불가, 숨겨진 상대 정보 비노출.
- `.uasset`을 열어 `BindWidget`·Designer 글꼴·부모 위젯 참조 모두 확인.

[UI 설계](../../Docs/Design/UIDesign.md) · [구현](../../Docs/Technical/11_UMG_UI.md) · [설정 입력 버그](../../Docs/Debugging/UI/02_InputMode_Restore.md)
