# Match Flow Console Command 검증

> **기록 유형:** 디버그 재현 도구 구축 / 상태 전환 검증

## 1. 문제

정상 Gameplay Flow만 사용하면 다음 상태를 테스트하기 위해 실제 조건이 충족될 때까지 기다려야 했다.

```text
Overtime
Finished
Abort
```

특정 Match State에서만 발생하는 문제를 반복해서 재현하기에는 비효율적이었다.

## 2. 해결

Match Flow를 직접 전환하거나 테스트할 수 있는 Console Command를 사용했다.

```text
DMReady
DMStartMatch
DMTestStartOvertime
DMTestFinishMatch
DMTestAbortMatch
```

## 3. 사용 목적

예:

```text
Playing
        ↓
DMTestStartOvertime
        ↓
Overtime 상태 진입 확인
```

또는:

```text
Playing
        ↓
DMTestFinishMatch
        ↓
Finished 상태 진입 확인
```

## 4. 디버깅 효과

Console Command를 이용하면 다음을 짧은 재현 경로로 확인할 수 있다.

```text
Phase 전환 자체
GameState 값 변경
Replication
UI 반응
Timer 종료 처리
Match 종료 처리
Abort 처리
```

즉 실제 승리 조건이나 제한 시간이 끝날 때까지 기다리지 않고, **문제가 발생하는 State로 직접 진입**할 수 있다.

## 5. 주의한 점

Debug Command가 Production Rule을 우회하는 기능이므로 다음을 구분한다.

```text
Debug Command
→ 상태 재현을 위한 Trigger

실제 Gameplay Rule
→ 정상 조건/권한 검증
```

Console Command가 작동했다는 사실만으로 실제 Match Rule이 정상이라고 판단하지 않는다.

## 6. 검증 순서

```text
Command 실행
        ↓
Server의 Phase 변경 확인
        ↓
GameState Replication 확인
        ↓
Client 상태 확인
        ↓
해당 Phase 전용 로직/UI 확인
```

## 7. 기록 결론

테스트 전용 Console Command를 만들어 **긴 정상 Gameplay Flow를 단축하고 상태 기반 버그를 빠르게 재현할 수 있는 디버깅 경로**를 확보했다.

## 관련 개념

`Console Command` `Match State Machine` `GameState` `Replication` `Reproduction`
