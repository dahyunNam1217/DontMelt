# 02. 경기 흐름·승패·결과 전달

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
