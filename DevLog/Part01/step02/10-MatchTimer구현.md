# 10일차 — Server 기준 Match Timer 및 상태 전환 구현

## 오늘 한 일

Match 시간 진행과 Phase 변경을 Server가 관리하도록 구성했다. Client 각자가 Match 시간을 별도로 계산하지 않고, Server 상태를 기준으로 동작하도록 했다.

특수 상태를 빠르게 테스트할 수 있도록 개발용 Console Command도 준비했다.

```text
DMReady
DMStartMatch
DMTestStartOvertime
DMTestFinishMatch
DMTestAbortMatch
```

예를 들어 실제 제한시간을 기다리지 않고 다음과 같이 강제 전환해서 동작을 확인할 수 있게 했다.

```text
Playing
    ↓
DMTestStartOvertime
    ↓
Overtime
```

## 복습 개념

- Server Authoritative Timer
- State Transition
- Console Command
- Runtime Debugging
- Edge Case Testing

## 오늘의 개발 방식 개선

특정 게임 조건이 발생할 때까지 기다리는 대신, **Debug Command로 상태를 재현하는 테스트 구조**를 만들었다.
