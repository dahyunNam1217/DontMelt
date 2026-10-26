# 03. 경기 흐름 — Phase, 서버 시간, 승패와 결과

## Phase 전환

`EDMMatchPhase`는 정상 경로 `Waiting → Countdown → Playing → OvertimeIntro → Overtime → Finished`와 예외 종료 `Aborted`를 정의한다. 모든 전환은 `ADMMatchGameMode::TryTransitionPhase`를 거치고, 허용 여부는 한 함수에서 판단한다.

```cpp
bool ADMMatchGameMode::IsPhaseTransitionAllowed(const EDMMatchPhase FromPhase, const EDMMatchPhase ToPhase)
{
    switch (FromPhase)
    {
    case EDMMatchPhase::Waiting:
        return ToPhase == EDMMatchPhase::Countdown || ToPhase == EDMMatchPhase::Aborted;
    case EDMMatchPhase::Countdown:
        return ToPhase == EDMMatchPhase::Playing || ToPhase == EDMMatchPhase::Aborted;
    case EDMMatchPhase::Playing:
        return ToPhase == EDMMatchPhase::OvertimeIntro || ToPhase == EDMMatchPhase::Finished
            || ToPhase == EDMMatchPhase::Aborted;
    case EDMMatchPhase::OvertimeIntro:
        return ToPhase == EDMMatchPhase::Overtime || ToPhase == EDMMatchPhase::Aborted;
    case EDMMatchPhase::Overtime:
        return ToPhase == EDMMatchPhase::Finished || ToPhase == EDMMatchPhase::Aborted;
    case EDMMatchPhase::Finished:
    case EDMMatchPhase::Aborted:
    default:
        return false;   // 종료 후에는 어떤 Phase로도 돌아가지 않는다.
    }
}
```

| 서버 처리 | 역할 |
|---|---|
| `TryFinalizeStartingRoster` | 참가자·리스폰 지점 검증 후 시작 인원, 유물 수, NPC 수를 스냅샷으로 고정 |
| `HandleStartingRosterTimeout` | 승인된 참가자가 90초 안에 오지 않으면 중단 → 결과 화면 → 로비 복귀 |
| `StartCountdown` / `StartPlaying` | 인원과 Phase를 다시 확인하고 진행, 퀘스트·이벤트 시작 |
| `HandleRegularTimeExpired` | 정규 시간 종료 평가 (단독 1위 승리 / 동률이면 연장전) |
| `TryStartOvertimeIntro` / `StartOvertime` | 연장전 소개와 본 진행 |
| `TryFinishMatch` / `TryAbortMatch` | 결과를 한 번만 확정하고 종료 처리 |

경기 중 참가자가 나가도 시작 스냅샷은 다시 계산하지 않는다. 클라이언트 HUD는 남은 시간을 서버에 묻지 않고, 복제된 `PhaseEndServerTime`과 `GetServerWorldTimeSeconds()`의 차이로 계산한다.

## 승패 판정

| 종료 사유 | 조건 |
|---|---|
| `RelicVictory` | 자기 진영 등록 유물이 목표 수(`RequiredRelicCountSnapshot`)에 도달 |
| `EliminationVictory` | 3인 이상, 옵션이 켜져 있고, 연결된 참가자 중 생존자가 정확히 1명 |
| `RegularTimeVictory` | 정규 시간 종료 시 등록 수 단독 1위 |
| 연장전 종료 | 등록 수 단독 1위, 최다 동률이면 승자 없음 |
| `Aborted` | 참가자 이탈, 필수 조건 실패, 도착 대기 시간 초과 |

판정 규칙은 `DMMatchResultRules` 순수 함수에 있다. GameMode가 PlayerState와 RelicManager에서 값을 모아 `FDMMatchParticipantSnapshot`을 만들고, 규칙 함수는 그 값만으로 계산한다. 월드가 필요 없어 Automation Test(`DontMelt.Match.ResultRules.*`) 4개로 검증한다.

한 프레임에 여러 조건이 성립할 수 있으므로, 종료는 서버의 단일 경로에서 처리하고 먼저 확정된 결과만 남긴다.

## 결과 데이터

`BuildFinalPlayerResults(EndReason, WinningBaseSlot)`가 참가자별 BaseSlot, **종료 시 등록 유물 수**(순위 기준), **누적 획득 수**(표시용), 킬 수를 기록한다.

- 등록 수 내림차순 정렬, 같은 등록 수는 공동 순위 (4, 2, 2, 1 → 1, 2, 2, 4)
- 전멸 승리일 때만 승자를 단독 1위로 고정
- 킬 수와 누적 획득 수는 승리 조건에 쓰지 않는다

결과마다 `FGuid ResultId`를 발급해 `GameState::MatchResult`로 복제한다. 클라이언트는 GameInstance에 캐시를 마친 뒤 ACK를 보내고, 서버는 전원 확인(최대 5초) 후 로비로 이동한다. 결과 화면이 방치되면 30초 뒤 자동으로 로비에 돌아간다.

| 관련 문서 | 내용 |
|---|---|
| [결과 수신 전 이동과 ACK](../Debugging/Network/07_MatchResult_Travel_Race.md) | ResultId, 캐시 후 ACK, 호출 누락 |
| [Waiting 영구 정지](../Debugging/Network/10_Match_Waiting_Timeout.md) | 도착 대기 기한 |
| [정규 시간 승리 누락](../Debugging/Gameplay/10_RegularTime_Winner.md) | 설계서와 코드 대조 |
| [전멸 승리 순위](../Debugging/Gameplay/09_EliminationVictory_Ranking.md) | 승자 고정 규칙과 테스트 |
| [결과표 데이터 분리](../Refactoring/03_MatchResult_Data.md) | 등록 수와 누적 획득 수 |

## 관련 코드

- [DMMatchTypes.h](../../Source/DontMelt/Public/Core/DMMatchTypes.h) — `EDMMatchPhase`, `FDMMatchPhaseState`, `FDMMatchResult`
- [DMMatchGameMode.cpp](../../Source/DontMelt/Private/Framework/Match/DMMatchGameMode.cpp) — `TryTransitionPhase`, `HandleRegularTimeExpired`, `BuildFinalPlayerResults`, `TryFinishMatch`, `CommitServerTravel`
- [DMMatchResultRules.h](../../Source/DontMelt/Public/Framework/Match/DMMatchResultRules.h) — `FindRelicVictoryWinner`, `FindEliminationWinner`, `SortAndRankResults`
- [DMMatchResultRulesTests.cpp](../../Source/DontMelt/Private/Tests/DMMatchResultRulesTests.cpp)

---

[Technical 목차](README.md)
