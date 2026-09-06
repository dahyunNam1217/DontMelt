# 11일차 — GameState Replication 검증

## 오늘 한 일

Server에서 변경한 Match State가 실제 모든 Client에게 전달되는지 검증했다. Replicated Property가 올바르게 등록되었는지 확인했다.

```cpp
UPROPERTY(Replicated)
EDMMatchPhase PhaseState;
```

`GetLifetimeReplicatedProps()`에서 Replication 대상으로 등록했다.

```cpp
DOREPLIFETIME(ADMMatchGameState, PhaseState);
```

**Runtime 검증**

콘솔에서 다음 명령을 사용했다.

```text
getall DMMatchGameState PhaseState
```

각 PIE Instance의 `PhaseState`를 비교하여 Server와 Client의 상태가 일치하는지 확인했다.

## 복습 개념

- Unreal Replication
- `UPROPERTY(Replicated)`
- `GetLifetimeReplicatedProps`
- `FLifetimeProperty`
- `DOREPLIFETIME`
- Server → Client Replication

## 오늘의 검증 기준

"Server 값이 정상이다"만 확인하지 않고, 다음 전체 과정을 확인했다.

```text
Server 상태 변경
    ↓
Replication
    ↓
Client 상태 확인
```
