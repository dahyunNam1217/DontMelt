# AI·이벤트

## NPC 인구 관리

NPC 수는 경기 시작 인원 × 배율(기본 15)로 정하고, `UDMNPCPopulationDirector`가 1~60명 범위에서 관리한다. 서버는 `DM_NPCSpawn` 태그가 붙은 스폰 지점을 모아 개수가 충분한지 먼저 검사한다.

생성 요청 수와 실제 성공 수는 다를 수 있다. 일부 스폰이 실패하면 이미 만든 NPC를 회수하고 초기화 실패로 처리해, 인원이 모자란 채로 경기가 시작되지 않게 했다.

스폰 지점은 순서를 섞어서 쓴다. 섞지 않으면 매 경기 같은 구역에 NPC가 몰리고, NPC가 없는 구역에 있는 캐릭터는 플레이어라는 것이 드러난다([스폰 편향](../../Docs/Debugging/Gameplay/12_NPC_Spawn_Shuffle.md)).

배율 상한은 성능을 보장하지 않는다. 실제 비용은 4인·NPC 40명 환경에서 따로 측정했다([NPC 성능 분석](../../Docs/Performance/01_NPC_Performance_Analysis.md)).

소스: [DMNPCPopulationDirector.cpp](../../Source/DontMelt/Private/AI/DMNPCPopulationDirector.cpp)

## 타이머 기반 상태 머신

NPC 행동은 Behavior Tree·StateTree와 AI Tick 없이 C++ 타이머 상태 머신으로 만들었다. NPC는 몇 초에 한 번 다음 행동을 고르면 충분해서 매 프레임 판단할 필요가 없었다.

```text
타이머 만료 → 가중치 기반 행동 선택 → Nav 이동 또는 동작 → 완료 콜백 → 다음 타이머
```

타이머를 써도 NPC 60명이 같은 순간에 판단하면 부하가 한 프레임에 몰린다. 그래서 첫 행동을 0.15~1.0초 사이 무작위 지연 뒤에 시작하고, 패닉 반응도 NPC마다 다른 지연을 둔다.

소스: [DMJellyAIController.cpp](../../Source/DontMelt/Private/Characters/DMJellyAIController.cpp)

## MoveToLocation과 완료 콜백

`MoveToLocation()`은 요청일 뿐 도착을 보장하지 않는다. NavMesh, 장애물, Pawn 상태에 따라 실패하거나 중간에 끊긴다.

주의할 점은 **이전 이동의 완료 콜백이 새 상태를 덮어쓰는 것**이다. 걷던 NPC가 패닉으로 도주를 시작했는데, 걷기 이동의 완료 콜백이 늦게 와서 Idle로 돌려버리는 식이다. 그래서 상태를 바꿀 때는 `StopMovement()`와 기존 타이머를 먼저 정리하고, `OnMoveCompleted`는 현재 상태가 이동 상태(`Moving`, `RelicApproachMoving`, `FleeMoving`, `PanicMoving`)일 때만 처리한다.

소스: [DMJellyAIController.cpp](../../Source/DontMelt/Private/Characters/DMJellyAIController.cpp)

## 행동 이벤트 라우팅

퀘스트가 점프·달리기·감정표현·자세 시스템에 직접 의존하면 기능이 늘 때마다 연결이 늘어난다. 각 시스템은 서버에서 실제 행동이 일어났을 때 `FDMQuestActionEvent`를 보내고, `HandleQuestActionEvent`가 참여 여부·생존 상태·현재 퀘스트 Run을 다시 검사해 진행도를 올린다.

입력을 눌렀다는 것만으로는 진행하지 않는다. 달리기 퀘스트도 키 입력이 아니라 서버에서 실제로 달린 구간만 센다.

진행도는 `UDMQuestProgressComponent`에 두고 `COND_OwnerOnly`로 복제한다. 누가 어떤 퀘스트를 하고 있는지가 새면 정체가 드러나기 때문이다.

소스: [DMQuestDirector.cpp](../../Source/DontMelt/Private/Quests/DMQuestDirector.cpp) · [DMQuestProgressComponent.cpp](../../Source/DontMelt/Private/Quests/DMQuestProgressComponent.cpp)

## 시간형 퀘스트 목표

진행 방식은 `EDMQuestProgressMode` 세 가지다.

| 모드 | 의미 | 퀘스트 |
|---|---|---|
| `Count` | 발생 횟수 | 점프 |
| `AccumulatedDuration` | 조건을 만족한 시간의 합 | 달리기 |
| `ContinuousDuration` | 끊기지 않고 유지한 시간 | 감정표현, 자세 |

"10초 달리기"와 "멈추지 않고 10초 유지"는 다르다. 시간형은 매 프레임 시간을 더하지 않고, 조건이 시작될 때 목표 시각에 맞춘 `DurationTargetTimerHandle`을 걸고 조건이 깨지면 취소한다.

소스: [DMQuestProgressComponent.cpp](../../Source/DontMelt/Private/Quests/DMQuestProgressComponent.cpp)

## 랜덤 이벤트와 정전 복구

랜덤 이벤트는 예고와 효과를 다른 타이머로 나눴다. 정전은 3초 예고 후 10초 동안 유지된다. 예고가 끝나 효과가 시작되는 순간 Phase·퀘스트·생존 조건을 다시 확인하고, 하나라도 어긋나면 취소한다. 예고 중에 경기가 끝날 수 있기 때문이다.

정전은 적용보다 복구가 중요했다. `ADMBlackoutController`는 정전 전에 각 조명의 원래 밝기와 표시 여부를 저장해 두고, 끝나면 그 값으로 되돌린다. 일괄로 밝기를 설정하면 레벨 디자인 때 정해 둔 조명 값이 사라진다.

서버는 이벤트 시작과 종료만 정하고, 조명 처리는 각 클라이언트가 로컬에서 한다. 조명 상태를 복제하지 않아도 된다.

소스: [DMRandomEventDirector.cpp](../../Source/DontMelt/Private/Events/DMRandomEventDirector.cpp) · [DMBlackoutController.cpp](../../Source/DontMelt/Private/Events/DMBlackoutController.cpp)

[DevLog 목차](../README.md)
