# Server Authority 위반 점검

> **기록 유형:** 네트워크 구조 점검 / 예방성 디버깅

## 1. 점검 대상

Client가 Match Phase와 같은 Authoritative Gameplay State를 직접 변경하는 경로가 없는지 점검했다.

문제가 되는 구조:

```cpp
PhaseState =
    EDMMatchPhase::Playing;
```

가 Remote Client의 Local Copy에서 직접 실행되는 경우.

## 2. 정상 데이터 흐름

```text
Client Input / Request
        ↓
Owning Actor의 Server RPC 등
        ↓
Server
요청 유효성 / Gameplay Rule 확인
        ↓
Authoritative State 변경
        ↓
GameState / Actor Replication
        ↓
Clients
```

Server RPC는 Actor Ownership의 영향을 받으므로 "`아무 Actor에서 Server RPC를 호출하면 된다`"고 보지 않는다.

## 3. Client가 상태를 직접 바꿀 때의 문제

Remote Client가 자신의 Local Copy만 변경하면:

```text
Server State와 불일치
다른 Client와 불일치
다음 Server Replication에 의해 다시 덮어써질 수 있음
Game Rule 우회 가능성
```

이 생길 수 있다.

## 4. 점검 항목

```text
[ ] PhaseState Setter가 Server에서만 호출되는가?
[ ] Client Input은 Request 형태로 Server에 전달되는가?
[ ] Server가 조건을 다시 검증하는가?
[ ] Client RPC/Multicast를 State Source of Truth로 사용하고 있지 않은가?
[ ] Host에서만 정상 동작하는 구조가 아닌가?
```

## 5. Listen Server에서 특히 주의할 점

Listen Server Host는:

```text
Server Authority
+
Local Player
```

역할을 동시에 가지므로 Host에서만 테스트하면 Remote Client의 잘못된 Authority 경로를 놓치기 쉽다.

따라서 반드시 Remote Client Instance에서도 확인한다.

## 6. 기록 결론

이 항목은 특정 Authority Bug 발생 기록이 아니라, Match State 구현 과정에서 **Authoritative State 변경 경로가 Server에 집중되어 있는지 점검한 기록**이다.

## 관련 개념

`Server Authority` `Ownership` `Server RPC` `Replication` `Listen Server`
