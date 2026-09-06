# GameMode / GameState 상태 중복 점검

> **기록 유형:** State Consistency 점검 / 구조적 버그 예방

## 1. 점검 배경

같은 Match Phase를 `GameMode`와 `GameState`가 각각 독립적으로 보관하면 두 값이 서로 다른 시점에 변경될 위험이 있다.

문제 구조:

```text
GameMode
└─ CurrentPhase

GameState
└─ CurrentPhase
```

## 2. 발생 가능한 증상

```text
GameMode = Finished
GameState = Playing
```

같은 상태가 생기면:

```text
Server Rule은 종료됨
Client UI는 계속 Playing 표시
```

처럼 서로 다른 시스템이 다른 사실을 기준으로 동작할 수 있다.

## 3. 책임 분리 기준

```text
GameMode
→ Server-only Rule / Transition 조건 / 승패 판정

GameState
→ Server가 변경하고
  모든 Client에게 공유해야 하는 현재 Match State
```

GameMode는 Phase를 판단하고 변경을 요청하되, `PhaseState` 자체의 Single Source of Truth는 `GameState`에 두는 구조로 점검한다.

Epic의 Gameplay Framework에서도 GameMode는 Server Rule을, GameState는 Client까지 공유할 Game-wide State를 담당한다.

## 4. 점검 순서

상태 불일치가 의심될 때:

```text
1. CurrentPhase가 몇 군데 선언돼 있는가?
2. 실제 값을 변경하는 함수는 몇 개인가?
3. GameMode와 GameState가 각각 값을 보관하는가?
4. UI는 어느 값을 읽는가?
5. Server Rule은 어느 값을 읽는가?
```

## 5. 해결 기준

```text
GameMode
TryStartMatch()
TryFinishMatch()
EvaluateWinner()

        ↓

GameState
SetMatchPhase()
PhaseState
```

처럼 Rule과 State Storage를 분리한다.

## 6. 기록 결론

동일한 Match Fact를 여러 Framework Class가 동시에 소유하지 않도록 **Match Phase의 Source of Truth를 하나로 유지하는 구조적 점검**을 수행했다.

## 관련 개념

`GameMode` `GameState` `Single Source of Truth` `Match Phase` `State Consistency`
