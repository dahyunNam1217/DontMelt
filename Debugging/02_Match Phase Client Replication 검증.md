# Match Phase Client Replication 검증

> **기록 유형:** 런타임 네트워크 검증  
> **대상:** `ADMMatchGameState::PhaseState`

## 1. 검증 목적

Server에서 Match Phase를 정상적으로 변경하더라도 그것만으로 Multiplayer 상태 동기화가 정상이라고 판단할 수 없다.

검증 목표는 다음 흐름 전체였다.

```text
Server Phase 변경
        ↓
GameState Replication
        ↓
각 Client의 GameState
        ↓
동일한 Phase 확인
```

## 2. 사용한 Console Command

```text
getall DMMatchGameState PhaseState
```

각 PIE/실행 Instance에서 Console을 열어 현재 `DMMatchGameState`의 `PhaseState`를 확인하는 방식으로 사용했다.

확인 대상 Phase:

```text
Waiting
Playing
Overtime
Finished
```

※ 위 값은 Match Flow에서 확인해야 하는 상태 목록이며, 이 문서에서는 당시 각 회차의 Raw Console Output을 보존하고 있지 않다.

## 3. 판정 기준

### 정상

```text
Server PhaseState == Client PhaseState
```

### 비정상

```text
Server = Playing
Client = Waiting
```

처럼 동일 시점에 상태가 다르게 관찰되는 경우.

## 4. 상태가 다를 때 확인 순서

```text
1. PhaseState가 Replicated인가?
2. GetLifetimeReplicatedProps에 등록됐는가?
3. 값을 Server에서 변경했는가?
4. Client가 같은 Match의 GameState를 확인하고 있는가?
5. Phase 변경 직후 Replication이 도달할 시간을 고려했는가?
6. 다른 Local/Debug 변수와 PhaseState를 혼동하지 않았는가?
```

## 5. 왜 GameState를 기준으로 확인했는가

`GameMode`는 Server에만 존재하지만 `GameState`는 Server와 Client에 존재하며 Match-wide State를 공유하는 용도다.

따라서 `PhaseState`처럼 모든 Player가 알아야 하는 현재 Match 상태는 `GameState`에서 확인하는 것이 구조적으로 맞다.

## 6. 기록 결론

이 검증은 **Server의 상태 변경 코드만 확인하지 않고 Client의 실제 Runtime State까지 확인하는 습관**을 만들기 위한 네트워크 디버깅 기록이다.

Raw Log가 남아 있지 않기 때문에 특정 회차에서 `"Playing이 정확히 N회 확인됐다"` 같은 수치는 추가하지 않는다.

## 재사용 체크리스트

```text
Server 값 확인
        ↓
Client 값 확인
        ↓
같은 Actor/State를 보고 있는지 확인
        ↓
RepNotify/UI까지 이어질 경우 마지막 Consumer까지 확인
```

## 관련 개념

`GameState` `Replication` `Runtime Inspection` `PIE` `getall`
