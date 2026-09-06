# Listen Server 종료 후 Client 복귀 검증

> **기록 유형:** 실제 Connection 종료 시나리오 검증

## 1. 검증 대상

Listen Server Host가 종료됐을 때 Remote Client가 끊어진 Match World에 남거나 정지하지 않고 안전한 기본 Map/Lobby로 복귀하는지 확인했다.

## 2. 확인 로그

```text
Connection to the Listen Server ended.
Returning the Client to the default map.
```

이 로그를 기준으로 Connection 종료 처리 경로가 실행되는지 확인했다.

## 3. 기대 흐름

```text
Listen Server 종료
        ↓
Client Connection 종료 감지
        ↓
Network Failure / Disconnect 처리
        ↓
Client-side Match 상태 정리
        ↓
Default Map / Lobby 복귀
```

Unreal에서는 Network Failure가 Engine 수준의 Network Failure Event로 전달될 수 있으며, `ConnectionLost` 같은 Failure Type을 구분할 수 있다.

## 4. 확인 포인트

```text
[ ] Client 화면이 멈추지 않는가?
[ ] 종료된 Server World에 계속 남지 않는가?
[ ] Map 복귀가 실행되는가?
[ ] 복귀 로직이 한 번만 실행되는가?
[ ] 재접속 후 다시 종료해도 동일하게 동작하는가?
```

## 5. 구조적 확인

Network Failure의 발생 주체와 처리 위치를 구분한다.

```text
Engine
→ Network Failure Event 발생

GameInstance 등 장수명 객체
→ Failure Event를 구독하고
   프로젝트 복귀 정책 실행
```

GameInstance를 사용한다면 "`GameInstance가 Failure를 발생시킨다`"가 아니라 **장수명 Subscriber/Policy 위치**로 본다.

## 6. 남아 있는 검증 과제

현재 보존된 기록에는 다음 로그가 있다.

```text
Connection to the Listen Server ended.
Returning the Client to the default map.
```

하지만 `FailureType`, `ErrorString`, `NetDriver`, 재접속 회차별 Raw Log는 별도로 보존되어 있지 않다.

향후에는 다음을 추가한다.

```text
FailureType
ErrorString
World
NetMode
Reconnect Count
```

## 7. 기록 결론

Host 종료라는 비정상 Connection 종료 상황을 별도 시나리오로 두고 **Client Recovery Path가 실제로 실행되는지 로그와 Map 복귀로 검증**했다.

## 관련 개념

`Listen Server` `ConnectionLost` `Network Failure` `GameInstance` `Client Recovery`
