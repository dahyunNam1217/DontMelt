# 불필요한 Tick 사용 점검

> **기록 유형:** 성능 점검 / 이벤트 기반 구조 검토

## 1. 점검 대상

매 Frame 실행할 필요가 없는 Actor/Component에서 Tick이 활성화되어 있는지 확인했다.

예:

```cpp
PrimaryActorTick.bCanEverTick = false;
```

## 2. 판단 기준

Tick 자체를 "`나쁜 기능`"으로 보지 않는다.

다음 조건을 확인한다.

```text
매 Frame 값이 연속적으로 변하는가?
        ↓
Yes → Tick이 자연스러울 수 있음

특정 사건이 발생했을 때만 처리하면 되는가?
        ↓
Yes → Event/Delegate/Timer 후보
```

## 3. Tick이 필요한 예

```text
연속적인 Camera 보정
Custom Movement
매 Frame 시각적 Simulation
Frame 기반 Tracking
```

## 4. Tick이 불필요할 수 있는 예

```text
Health가 바뀌었는지 매 Frame 확인
Overlap 여부를 매 Frame 검색
Cooldown 종료 여부를 매 Frame 직접 감시
상태가 바뀌었는지 Polling
```

이 경우:

```text
Delegate
Overlap Event
Timer
OnRep
State Change Event
```

로 전환할 수 있다.

## 5. 성능 판단식

Actor 하나의 Tick이 작더라도 Actor 수가 늘어나면 누적된다.

```text
Tick Cost
≈
Tick 대상 수
×
Frame당 호출 횟수
×
각 Tick 작업 비용
```

따라서 NPC/Actor 수가 증가할 예정이라면 Tick Enabled Class를 정기적으로 점검한다.

## 6. 검증 방법

실제 최적화를 주장하려면:

```text
Tick 활성 Actor 수
Frame Time
Game Thread Time
```

을 Unreal Insights/Profiler 등으로 측정해야 한다.

현재 기록에는 전/후 성능 수치가 없으므로 "`Tick 제거로 N% 개선`" 같은 결과는 작성하지 않는다.

## 7. 기록 결론

필요 없는 Tick을 습관적으로 활성화하지 않고, **연속 Update인지 Event-driven Update인지 먼저 판단하는 성능 점검 기준**을 만들었다.

## 관련 개념

`Tick` `Timer` `Delegate` `Event-driven` `Performance`
