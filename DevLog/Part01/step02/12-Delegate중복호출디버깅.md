# 12일차 — Listen Server 종료 및 Delegate 중복 호출 디버깅

## 오늘 한 일

Host가 Listen Server를 종료했을 때 Client가 정상적으로 연결 종료를 감지하고 기본 Map으로 복귀하는지 테스트했다.

확인 로그:

```text
Connection to the Listen Server ended.
Returning the Client to the default map.
```

또한 재접속 이후 Delegate가 중복 등록되는 문제를 검사했다.

**재현 과정**

```text
Client → Host 접속
    ↓
Match 시작
    ↓
Host 종료
    ↓
Client 복귀
    ↓
Host 재접속
    ↓
Match 재시작
    ↓
Host 다시 종료
```

각 Host 종료마다 Disconnect 관련 로그가 **정확히 한 번씩** 발생하는지 확인했다.

**Delegate 관리**

`NetworkFailureDelegate`의 Lifecycle을 추적하고, 필요 시 `FDelegateHandle`을 이용해 등록 상태를 관리했다.

```text
Delegate 등록
    ↓
FDelegateHandle 저장
    ↓
IsValid 확인
    ↓
Remove
    ↓
Reset
```

PIE만으로 확인하기 어려운 Network 종료 상황은 독립적인 `-game` 프로세스 및 로그 파일을 활용하는 방법도 검토했다.

## 복습 개념

- Network Failure
- Listen Server Disconnect
- Delegate
- Delegate Binding
- `FDelegateHandle`
- `Remove`
- `Reset`
- Object Lifecycle
- Standalone Multiplayer Testing
- Runtime Log Analysis

## 오늘의 디버깅 포인트

한 번 발생해야 하는 이벤트가 여러 번 호출될 경우, "이벤트 로직 자체"보다 먼저 "Delegate가 중복 등록되었는가?"를 확인해야 한다는 것을 검증했다.
