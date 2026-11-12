# 02. 경기 흐름·승패·결과 전달

## P2 승인 동기화 — 2026-11-13

이번 사용자 P2 요청의 명시 범위만 **설계 승인**으로 채택한다. 과거 승인으로 소급하지 않는다. 기준 HEAD는 `914173a`이며 구현 사실·설계 승인·에셋 적용·실행 검증은 별개다. 상세 근거와 미결정 사항은 [ImplementationStatus 10절](../ImplementationStatus.md#10-p2-승인-설계-및-현재-구현--2026-11-13)을 따른다.

**아래 기존 본문과의 관계:** 이 절 뒤의 기존 본문은 삭제하지 않고 이전 설계·조사 기록으로 보존한다. E1~E10과 충돌하는 값·완료/승인/미확인 표현은 이 절과 중앙10절이 대체한다. 특히90초·Walk300·NPC×10/40·NPC 발소리·3요소 이벤트 HUD·타임아웃 즉시 재요청은 최신 목표가 아니다. E1~E10 밖의 항목을 이번 승인으로 일괄 확정하지 않는다.

### E1 — 결과 통계와 순위 분리

**설계 승인:** 결과표 유물 열은 경기 중 누적 획득 횟수, 최종 순위는 종료 시 등록 유물 수를 사용하며 동률 공동 순위를 허용한다. 두 통계를 별도 관리하고 표시 변경으로 서버 승리 판정을 바꾸지 않는다.

**구현 확인 / 부분 구현:** `ADMRelicManager::TryCommitRelicAcquisition` → `ADMMatchPlayerState::IncrementAcquiredRelicCount` → `ADMMatchGameMode::BuildFinalPlayerResults`로 누적 통계를 보관한다. `FDMMatchResultEntry`의 `AcquiredRelicCount`와 `RelicCount`가 분리되어 있다. 그러나 `UDMPlayerResultRowWidget::SetResultEntry`는 현재 `RelicCount`를 표시하므로 목표 표시와 불일치한다. 성공 Pickup/Steal Commit마다 증가하는 것이 현재 동작이다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E1 절을 함께 읽는다.

### E2 — 전멸 승자 단독 1위

**설계 승인:** 일반 종료는 최종 등록 수 순위다. 전멸 승리에서는 등록 수와 무관하게 승자를 단독1위로 고정하고, 나머지는 2위부터 등록 수 기준 공동 순위를 부여한다. 승리 배너와 결과표의 승자 의미가 모순되지 않아야 한다.

**구현 확인 / 부분 구현:** `DMMatchResultRules::SortAndRankResults`는 등록 수 내림차순이며 같은 등록 수에서만 승자를 우선한다. `ADMMatchGameMode::BuildFinalPlayerResults`가 이 함수를 호출한다. 따라서 등록 수가 적은 전멸 승자가 결과표1위가 아닐 수 있다. 현재 함수는 종료 사유별 승자 고정 계약을 받지 않는다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E2 절을 함께 읽는다.

### E3 — 참가자 도착 대기 120초

**설계 승인:** 기본 도착 제한은120초다. 경기 맵 서버 GameMode가 승인 참가자의 준비와 시스템 초기화를 검증한다. PC 존재만으로 완료하지 않는다. 도착 제한과 튜토리얼 제한을 분리하고 무한 대기를 방지한다. 미도착·명시적 이탈·Spawn/Possess 실패는 가능한 범위에서 진단·사용자 안내를 구분한다.

**구현 확인:** `ADMMatchGameMode::StartPlay`가 아직 명단이 확정되지 않은 Waiting에서 타이머를 설정한다. 클라이언트 로딩 시작 시점 기준이 아니다. `HandleStartingNewPlayer_Implementation`은 부모 Spawn/Possess 후 Pawn/Controller를 확인한다. `IsStartingRosterComplete`는 기대 인원, 유효 PlayerState, 유효·고유 BaseSlot, Alive, Pawn·Controller·PlayerState 연결을 확인한다. `TryFinalizeStartingRoster`는 스폰 지점 및 NPC/유물/퀘스트/이벤트 초기화 후 도착 타이머를 해제하고 튜토리얼 게이트로 이동한다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E3 절을 함께 읽는다.

### E9-A — 전원 튜토리얼과 300초 제한

**설계 승인:** 경기 시작 전 전원 완료, 기존300초 제한, 완료·실패·시간 초과·이탈 구분, 서버 최종 판정을 채택한다.

**구현 확인:** `ADMMatchGameMode::StartPreMatchTutorialGate`는 확정 명단을 기준으로 완료 집합·300초 타이머·Client 튜토리얼 표시를 관리하고 완료 후 Countdown으로 진행한다. 표시 실패·시간 초과는 현재 내부 실패 중단 경로다. 도착120초 타이머와 별도다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E9-A 절을 함께 읽는다.

**검증 상태:** BP/DataAsset/Animation 실제 적용은 에셋 미검증, 빌드·PIE·Steam은 실행 미검증이다. 아래 완료 기준은 향후 검증 시나리오이며 통과 기록이 아니다. C++·WBP·Config 변경은 이번 문서 승인에 포함되지 않는다. [공통 게임 규칙](../GameplayRules.md) · [네트워크 정책](../NetworkPolicy.md)를 상호 참조한다.

---

## 책임

`ADMMatchGameMode`만 Phase 전환·종료·승리 판정을 확정한다. `ADMMatchGameState`는 공개 Phase·종료 시각·결과만 복제한다. `DMMatchResultRules`는 참가자/등록 수 스냅샷 기반 순수 함수; `UDMGameInstance`는 Travel 동안 결과 캐시만 보유한다.

## Phase 계약

```text
Waiting → Countdown → Playing → (OvertimeIntro → Overtime) → Finished
Waiting / Countdown / Playing / OvertimeIntro / Overtime → Aborted
Finished / Aborted : 종료 상태, 매치 내 후속 전이 없음
```

- `TryFinalizeStartingRoster`: 참가자 수·BaseSlot·유물 수·필요 등록 수·NPC 수 고정.
- 승인 참가자 90초 도착 실패 → `Aborted`(중단 사유 별도 표시).
- Countdown 이후 Playing 정규 **900초**. 종료 때 등록 유물 단독 1위면 Finished, 공동 선두면 소개 **5초** 후 Overtime **180초**.
- 등록 목표 2인 3, 3인 3, 4인 4. 등록 성공 Commit 후 서버가 재평가.
- 전멸 승리는 3~4인 + 옵션 ON + 마지막 연결 생존자 1명만 허용.
- 둘 이상의 이벤트 동시 발생 시 서버의 먼저 성공한 Commit과 단일 결과 확정 경로에 따른다.

## 결과 구조

- 참가자 스냅샷의 **등록 수**로 등수 정렬; 동점 공동 순위(4,2,2,1 → 1,2,2,4).
- 결과표에는 **누적 획득 수**를 유물 열에 표시; **킬 수**는 순위 결정에 사용하지 않는다.
- 전멸 승리에 한해 최종 승자를 단독 1위로 고정.
- `FGuid ResultId` 발급→`MatchResult` 복제→클라이언트 `GameInstance` 캐시→**같은 ResultId ACK**→전원 ACK 또는 **5초** 대기→서버 Travel 승인. 화면 자동 복귀 **30초**.
- `TrySetMatchResult`는 이미 확정된 결과를 다시 쓰지 않는다. Host가 OnRep을 못 받는 상황의 UI 갱신을 따로 처리한다.

## 수명·실패 경계

- `Finished`/`Aborted` 이후 사망 관전·상호작용/스킬/전투 Commit 신규 시작 금지.
- GameState와 PlayerController 등 **서로 다른 Actor 메시 순서**에 결과 캐시를 의존시키지 않는다.
- 비정상 종료의 사유를 정상 패배와 혼동하지 않고, 참가자 이탈 시 스냅샷을 재산출하지 않는다.

## 확인할 구현 후보

`Source/DontMelt/Private/Framework/Match/DMMatchGameMode.cpp`: `TryTransitionPhase`, `HandleRegularTimeExpired`, `TryStartOvertimeIntro`, `TryFinishMatch`, `TryAbortMatch`, `BuildFinalPlayerResults`, `CommitServerTravel`.

`Source/DontMelt/Public/Framework/Match/DMMatchResultRules.h`와 `Private/Tests/DMMatchResultRulesTests.cpp`의 테스트(`DontMelt.Match.ResultRules.*`) 유무 및 최신 실행 결과 확인.

## 완료 기준

- 2/3/4인 목표값 및 NPC 배율 스냅샷, 참가자 이탈 후 불변성.
- 정규 시간 종료 단독 선두/동률/연장 종료 동률/전멸 옵션/2인 제외.
- 결과 랭킹·누적 획득 수 구분, 중복 종료 요청 멱등성.
- 지연·PacketLoss에서 ResultId ACK/5초 상한/30초 자동 로비 복귀와 Host 화면.

[기획](../../Docs/Design/GameDesign.md) · [기술](../../Docs/Technical/03_Match_Flow_Result.md) · [결과 경합](../../Docs/Debugging/Network/07_MatchResult_Travel_Race.md)
