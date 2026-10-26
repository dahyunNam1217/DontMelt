# 07 — 퀘스트·랜덤 이벤트

## 1. 시스템 분리

`UDMQuestDirector`가 전역 퀘스트 일정·선택·마감·순위를, `UDMQuestProgressComponent`(MatchPlayerState 소유)가 개인 진행도를, `UDMRandomEventDirector`가 이벤트 일정·예고·효과·종료를 맡는다. GameState에는 공개할 정보만 둔다.

```text
ADMMatchGameMode
├─ UDMQuestDirector → MatchPlayerState / UDMQuestProgressComponent
├─ UDMRandomEventDirector → Blackout / Panic / InputReverse
└─ ADMMatchGameState → 공개 퀘스트·이벤트 상태
```

두 Director는 동시에 실행되지 않는다. 같은 프레임에 시작을 시도해도 실행 조건에서 하나를 배제한다. 연장전 소개·종료·중단으로 넘어가면 진행 중인 타이머와 효과를 정리한다. 진행도를 매 Tick 계산하지 않는다.

## 2. 퀘스트 네 종류

| 퀘스트 | 진행 방식 | 진행 조건 |
|---|---|---|
| Jump | 횟수 | 서버의 실제 점프 |
| Sprint | 누적 시간 | 실제 달리기 구간 |
| Emote | 연속 유지 | Emote ActionState 유지 |
| Pose | 연속 유지 | 요구 자세 유지 |

초기 테스트 데이터는 점프 3회, 달리기 3초, 감정표현 3초, 자세 3초이고 제한시간은 각각 20초다. 입력을 눌렀다는 이유로 진행하지 않고, 서버의 실제 행동 Delegate로 이벤트를 받는다.

## 3. 참가자와 마감

퀘스트 시도 시각은 Playing 시작 후 120 / 360 / 600초이고, 경기마다 2~3회 시도한다. 조건이 맞지 않는 시도는 다시 대기열에 넣지 않고 건너뛴다. 시작 조건은 Playing Phase, 다른 퀘스트·이벤트 없음, 유효한 참가자와 개인 진행 컴포넌트다.

마감까지 못 끝내면 `FailedPendingCompletion`이 된다. 실패해도 퀘스트를 바로 없애지 않고, 남은 행동을 마치면 `CompletedAfterFailure`로 경고를 끝낸다. 제한시간 안에 끝낸 플레이어는 서버 완료 시각 순서로 순위를 매긴다.

## 4. 실패 페널티와 보상

처음에는 반복 실패음과 1등 30%·2등 10% 스킬 쿨다운 회복 보상을 설계했다. 8단계에서는 보상 정책이 확정되지 않아 연결을 보류했고, 10단계에서 **보상을 없애고 미완료 캐릭터 주변에 3D 사이렌을 울리는 방식**으로 확정했다. 지금은 퀘스트 결과가 스킬 쿨다운에 영향을 주지 않는다.

## 5. 랜덤 이벤트

| 이벤트 | 처리 |
|---|---|
| Blackout | 서버가 시작, 클라이언트가 로컬 조명·화면 표현 |
| Panic | NPC Director에 전달, NPC마다 지연을 둔 반응 |
| InputReverse | 플레이어에게 `Status.InputReversed` 적용·해제 |

예고와 종료 시각은 공개 상태로 보낸다. 화면 효과와 조명은 서버 판정을 대신하지 않는다. 같은 이벤트를 연속으로 고르지 않고, 조건이 안 맞으면 대기열 없이 건너뛴다.

## 6. 이 단계의 정리

연장전 전환(`TryStartOvertimeIntro()`)에서 `ShutdownQuestSystem()`이 두 번 호출되던 것을 한 번으로 정리했다. 10단계에서는 퀘스트 HUD, 이벤트 이름·설명·진행 바, 실패 사이렌 안내를 붙였다.

관련 문서: [퀘스트·이벤트 구현](../../Technical/09_Quest_RandomEvent.md)

[Stages 목차](README.md)
