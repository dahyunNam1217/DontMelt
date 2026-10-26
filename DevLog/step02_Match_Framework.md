# 2단계 — 로비·경기 Framework

## 07. Match 시스템 기본 구조 구현

2단계 Match 기반 시스템 구현을 시작했다. 이번 단계에서는 전투, 유물, NPC AI를 제외하고, 게임을 시작하고 끝낼 수 있는 **Match Framework 자체**에 집중했다.

구현 목표 흐름:

```text
2~4인 접속
    ↓
Player Spawn 완료
    ↓
Match 준비
    ↓
Match 시작
    ↓
Server 기준 Match Time 진행
    ↓
Match 종료
    ↓
GameState Replication
```

Match Phase를 명시적으로 관리하도록 구조를 잡았다.

대표 상태: `Waiting`, `Playing`, `Overtime`, `Finished`

**설계 포인트**

단순 Boolean 여러 개를 사용하는 것보다, Match 전체의 진행 단계를 명시적인 Phase(Enum)로 관리하는 방향을 선택했다.

## 08. Lobby Ready 및 Player 준비 상태 구현

플레이어가 Lobby에서 Match 시작 준비를 할 수 있도록 Ready 시스템을 구성했다. Ready 상태의 원본을 `ADMLobbyPlayerState`가 소유하도록 했다.

```text
Client
    ↓
Ready 요청
    ↓
Server
    ↓
LobbyPlayerState Ready 변경
    ↓
Replication
    ↓
다른 Client 확인
```

Host와 일반 Client가 서로 다른 값을 임의로 소유하지 않도록 구조를 정리했다.

**디버깅 / 검수**

Client가 Ready 상태를 직접 확정하지 않고, Server가 최종 상태 변경을 수행하는지 확인했다.

## 09. Player Spawn 완료 및 Match 시작 조건 검증

단순히 Client가 서버에 접속했다는 사실만으로 플레이어가 생존 중이라고 판단하지 않도록 게임 규칙을 명확하게 했다.

**확정 조건**: Player Character가 실제 Map에 Spawn 완료되어야 생존 플레이어로 처리한다.

다음 상태를 구분했다.

```text
Connection 완료
≠
Player Character Spawn 완료
≠
Match 참여 준비 완료
```

2~4명의 플레이어가 정상적으로 Spawn되고 Match 시작 조건을 만족했을 때만 게임을 시작하도록 Match Flow를 검수했다.

**문제 분석**

접속은 되어 있지만 Character가 아직 생성되지 않은 상태를 정상 플레이어로 계산할 경우, Match 시작 인원이나 생존 인원 계산이 잘못될 수 있으므로 이를 분리했다.

## 10. Server 기준 Match Timer 및 상태 전환 구현

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

**개발 방식 개선**

특정 게임 조건이 발생할 때까지 기다리는 대신, **Debug Command로 상태를 재현하는 테스트 구조**를 만들었다.

## 11. GameState Replication 검증

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

**검증 기준**

"Server 값이 정상이다"만 확인하지 않고, 다음 전체 과정을 확인했다.

```text
Server 상태 변경
    ↓
Replication
    ↓
Client 상태 확인
```

## 12. Listen Server 종료 및 Delegate 중복 호출 디버깅

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

**문제 분석**

한 번 발생해야 하는 이벤트가 여러 번 호출될 경우, "이벤트 로직 자체"보다 먼저 "Delegate가 중복 등록되었는가?"를 확인해야 한다는 것을 검증했다.

## 13. 2단계 최종 검수 및 이동·자세 단계 설계 시작

Match 기반 구현을 마무리하면서, 3단계로 넘어가기 전에 전체 코드를 다시 검수했다.

**검수 항목**

- 불필요한 반복문 여부
- 중복 Actor 검색 여부
- 불필요한 Tick 여부
- 불필요한 Replication 여부
- GameMode / GameState 책임 중복 여부
- PlayerState / Character 데이터 중복 여부
- Component 상태 중복 여부
- Server Authority 위반 여부
- Delegate Lifecycle
- Match State Replication

로그 사용량이 많아 별도의 Custom Log Header를 만드는 것도 검토했지만, 현재 규모에서는 추상화 계층을 추가할 실익이 크지 않다고 판단해 기존 `UE_LOG` 구조를 유지했다.

이후 3단계 이동·자세 구현에 진입하기 위한 설계를 시작했다. Player와 NPC 이동을 완전히 별개로 만드는 대신, 공통 Character Movement 구조를 사용할 수 있도록 기존 Architecture에 추가했다.

```text
Player
Input
    ↓
공통 Movement System

NPC
AI Decision
    ↓
공통 Movement System
```

Player와 NPC의 이동 명령을 발생시키는 주체는 다르지만, 실제 이동 처리 구조는 가능한 범위에서 공유하도록 설계했다.

3단계도 한 번에 구현하지 않고 `3-0, 3-1, 3-2, 3-3 ...` 형태로 세분화하여 진행하기로 했다.
