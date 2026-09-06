# Replication 등록 누락 점검

> **기록 유형:** 구조 점검 / 예방성 디버깅  
> **대상:** `ADMMatchGameState::PhaseState`

## 1. 점검 배경

멀티플레이 Match 상태를 `GameState`에 보관하면서 다음 변수를 Client까지 동기화해야 했다.

```cpp
UPROPERTY(Replicated)
EDMMatchPhase PhaseState;
```

단순히 `UPROPERTY(Replicated)`를 선언한 것만 보고 Replication 설정이 끝났다고 판단하지 않고, 실제 Lifetime Replication 등록까지 확인했다.

## 2. 점검 코드

```cpp
void ADMMatchGameState::GetLifetimeReplicatedProps(
    TArray<FLifetimeProperty>& OutLifetimeProps
) const
{
    Super::GetLifetimeReplicatedProps(
        OutLifetimeProps
    );

    DOREPLIFETIME(
        ADMMatchGameState,
        PhaseState
    );
}
```

## 3. 확인한 항목

```text
[ ] PhaseState가 Replicated Property인가?
[ ] GetLifetimeReplicatedProps()가 override되어 있는가?
[ ] Super::GetLifetimeReplicatedProps()를 호출하는가?
[ ] DOREPLIFETIME(ADMMatchGameState, PhaseState)가 있는가?
[ ] GameState Actor 자체가 Network Replication 대상인가?
[ ] 값을 Server Authority에서 변경하는가?
```

Epic의 Property Replication 구조에서도 `DOREPLIFETIME` 계열 Macro는 `GetLifetimeReplicatedProps()`에서 Property Replication Rule을 구성하는 방식으로 사용된다.

## 4. 원인 후보를 나누는 기준

Client에서 `PhaseState`가 갱신되지 않는다면 다음 순서로 범위를 좁힌다.

```text
Property 선언 문제
        ↓
Lifetime Replication 등록 문제
        ↓
Actor/Component Replication 설정 문제
        ↓
Server에서 실제 값이 변경됐는가?
        ↓
Client가 올바른 GameState Instance를 보고 있는가?
```

즉 `Replicated` Macro 하나만 확인하고 끝내지 않는다.

## 5. 검증 방법

Server에서 Phase를 변경한 뒤 Host와 Client 각각에서 현재 GameState 값을 확인한다.

```text
Server
PhaseState 변경
        ↓
Replication
        ↓
Client GameState
        ↓
PhaseState 확인
```

실제 Client 값이 Server와 일치해야 이 점검을 통과한 것으로 본다.

## 6. 기록 결론

이 항목은 **실제 Replication 누락 버그가 발생했다는 기록이 아니라**, Match State를 구현하면서 Replication 등록 누락 가능성을 사전에 점검한 기록이다.

### 재발 방지 체크리스트

```text
Replicated Property 추가
→ Actor/Component Replication 여부 확인
→ GetLifetimeReplicatedProps 확인
→ DOREPLIFETIME 계열 등록
→ Server에서 값 변경
→ Client 실제 값 검증
```

## 관련 개념

`Replication` `DOREPLIFETIME` `GameState` `Server Authority`
