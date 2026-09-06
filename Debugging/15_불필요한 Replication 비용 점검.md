# 불필요한 Replication 비용 점검

> **기록 유형:** 네트워크 비용 점검

## 1. 점검 대상

`Replicated`가 붙은 모든 Property가 실제 Remote Client에게 필요한 정보인지 확인한다.

질문:

```text
"다른 Connection이 이 값을 알아야 하는가?"
```

## 2. 필요한 Replication

예:

```text
Current Match Phase
Player Score
모든 Player에게 필요한 Objective State
Remote Character Gameplay State
```

이런 값은 Client가 현재 상태를 재구성해야 하므로 Replication 후보가 된다.

## 3. 불필요할 수 있는 Replication

예:

```text
Server 내부 판정용 임시 변수
Server-only AI Decision 중간값
로컬 함수 계산 Cache
Client가 절대 사용하지 않는 Rule State
```

이런 값을 무조건 Replicate하면 Network State Surface가 불필요하게 커진다.

## 4. 중요한 정확성

`Replicated` Property라고 해서 매 Frame 전체 값이 무조건 전송되는 것은 아니다.

Epic Property Replication 문서 기준으로 일반 Replicated Property는 **값이 변경됐을 때 Replication 대상이 되며**, 조건부 Replication도 사용할 수 있다.

예:

```cpp
DOREPLIFETIME_CONDITION(
    AMyActor,
    PrivateValue,
    COND_OwnerOnly
);
```

따라서 최적화 기준은:

```text
Replicated 개수만 줄이기
```

보다:

```text
누가 이 값을 필요로 하는가?
얼마나 자주 변경되는가?
어떤 Connection에만 필요한가?
```

가 더 정확하다.

## 5. 점검 항목

```text
[ ] 이 값이 Server-only인가?
[ ] 모든 Client가 필요한가?
[ ] Owning Client만 필요한가?
[ ] 값이 얼마나 자주 변경되는가?
[ ] RepNotify가 필요한가?
[ ] COND_OwnerOnly / COND_SkipOwner 등의 조건을 쓸 수 있는가?
```

## 6. 실제 성능 수치

현재 기록에는 Network Profiler 기준 전/후 Bandwidth 수치가 없다.

따라서:

```text
Replication을 줄여 Network Traffic을 크게 개선했다.
```

같은 결과는 주장하지 않는다.

이 문서는 **Replication Surface를 줄이기 위한 설계 점검 기록**으로 남긴다.

## 7. 기록 결론

Network State는 "`나중에 필요할 수도 있으니 전부 Replicate`"가 아니라 **Connection별 필요성, 변경 빈도, State Recovery 필요성**을 기준으로 선택한다.

## 관련 개념

`Replication` `Bandwidth` `Replication Condition` `COND_OwnerOnly` `GameState`
