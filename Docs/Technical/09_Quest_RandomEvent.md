# 09. 퀘스트와 랜덤 이벤트 — 서버 일정과 개인 진행도의 분리

## 구성

경기 중 퀘스트와 랜덤 이벤트를 열면서, **한 플레이어의 행동**과 **모두에게 공개할 진행 상태**를 구분해야 했다. 퀘스트는 행동으로 정체가 드러날 수 있는 장치라, 누가 진행 중인지가 다른 사람에게 새면 안 된다.

```text
ADMMatchGameMode
├─ UDMQuestDirector        전역 퀘스트 일정·선택·마감·순위
│    └─ ADMMatchPlayerState / UDMQuestProgressComponent   개인 진행도 (OwnerOnly)
├─ UDMRandomEventDirector  이벤트 일정·예고·효과·종료
│    └─ Blackout / Panic / InputReverse
└─ ADMMatchGameState       공개 퀘스트·이벤트 상태
```

퀘스트와 이벤트는 동시에 실행하지 않는다. 연장전 소개·종료·중단으로 넘어가면 진행 중인 타이머와 효과를 정리한다. 진행도는 Tick으로 계산하지 않는다.

## 퀘스트

| 퀘스트 | 진행 방식 | 진행 이벤트 |
|---|---|---|
| Jump | 횟수 | 서버의 실제 점프 |
| Sprint | 누적 시간 | 실제 달리기 구간 |
| Emote | 연속 유지 | Emote ActionState 시작·종료 |
| Pose | 연속 유지 | 요구 자세 유지 |

- 시도 시각은 Playing 시작 후 120 / 360 / 600초이고, 경기마다 2~3회 시도한다. 조건이 맞지 않는 회차는 다시 대기열에 넣지 않고 건너뛴다.
- 진행 이벤트는 `FDMQuestActionEvent`로 모아 `HandleQuestActionEvent`에 전달한다. 이 함수가 참여 여부·생존 상태·현재 퀘스트 Run의 유효성을 다시 검사한다. 입력을 눌렀다는 이유만으로 진행하지 않고, 서버의 실제 행동 Delegate를 기준으로 한다.
- 시간 충족형은 매 프레임 재지 않고, 목표 시각에 콜백되는 `DurationTargetTimerHandle`로 처리한다.
- 마감까지 못 끝내면 `FailedPendingCompletion`이 되고 캐릭터 주변에 3D 사이렌이 울린다. 부족한 행동을 마저 하면 `CompletedAfterFailure`로 사이렌이 꺼진다. 사이렌 상태는 Relevancy와 무관하게 전달되도록 복제 상태로 만들었다([퀘스트 사이렌](../Debugging/Gameplay/02_QuestSiren_Relevancy.md)).

## 랜덤 이벤트

| 이벤트 | 진행 | 대상 |
|---|---|---|
| 정전 | 3초 예고, 2초 전 조명 깜빡임, 10초 | 월드 조명·시야 표현 |
| 젤리 패닉 | NPC가 1~3초 간격으로 도주·점프·비명, 10초 | NPC 군중 |
| 입력 반전 | WASD 이동 입력만 반전, 10초 | 살아 있는 플레이어 |

- `TryScheduleEvent`는 시도 횟수, Playing 여부, 활성 이벤트·퀘스트를 확인한다. 같은 이벤트를 연속으로 고르지 않는다.
- 예고와 실제 효과는 다른 타이머다. 효과 시작 순간에 Phase·퀘스트·전원 생존 조건을 다시 확인하고, 하나라도 어긋나면 취소한다.
- 정전은 서버가 시작을 결정하고, `ADMBlackoutController`가 각 클라이언트에서 조명과 로컬 시야 조명을 처리한다. 서버 판단과 로컬 렌더 표현을 분리했다.

## 관련 코드

- [DMQuestDirector.cpp](../../Source/DontMelt/Private/Quests/DMQuestDirector.cpp) — `StartPlaying`, `TryStartQuest`, `HandleQuestActionEvent`
- [DMQuestProgressComponent.cpp](../../Source/DontMelt/Private/Quests/DMQuestProgressComponent.cpp) — `AddCountProgress`, `BeginDurationObservation`
- [DMRandomEventDirector.cpp](../../Source/DontMelt/Private/Events/DMRandomEventDirector.cpp) — `TryScheduleEvent`, `HandleWarningFinished`
- [DMBlackoutController.cpp](../../Source/DontMelt/Private/Events/DMBlackoutController.cpp) — `BeginPlay`

## 핵심 코드

```cpp
// HandleWarningFinished - 예고가 끝나 효과를 시작하기 직전에 조건을 다시 확인한다.
if (MatchGameState->GetCurrentPhase() != EDMMatchPhase::Playing)
{
    CancelScheduledEvent(TEXT("Match is no longer Playing"));
    return;
}

if (MatchGameState->HasCurrentQuest())
{
    CancelScheduledEvent(TEXT("Quest became active"));
    return;
}

if (!AreAllPlayersAlive())
{
    CancelScheduledEvent(TEXT("Not all players are alive at effect start"));
    return;
}
```

예고를 시작할 때 조건이 맞았어도, 3초 사이에 상황이 바뀔 수 있다. 효과를 적용하기 직전에 한 번 더 확인한다.

---

[Technical 목차](README.md)
