# PlayerState / Character 데이터 중복 점검

> **기록 유형:** Respawn/Lifetime 기반 State Ownership 점검

## 1. 점검 대상

동일한 Player 데이터를 `PlayerState`와 `Character`가 각각 별도의 원본 값으로 보관하고 있지 않은지 확인했다.

## 2. 문제가 되는 구조

```text
PlayerState
└─ Score / Team / Ready / Persistent Match Data

Character
└─ 같은 값의 복사본
```

두 값을 독립적으로 변경하면 Respawn/재Possess 후 상태가 갈라질 수 있다.

예:

```text
PlayerState.Score = 10
Character.Score = 8
```

## 3. 판단 기준: Pawn Lifetime을 넘어 유지되는가?

### Respawn 이후에도 유지

```text
Score
Team
Ready
Player Name
Match-level Player Data
```

→ `PlayerState` 후보

### 현재 Pawn Instance에 종속

```text
Current Movement State
현재 Animation/Movement 관련 값
현재 Pawn Component State
```

→ `Character/Pawn` 후보

Epic Gameplay Framework에서도 PlayerState는 Player별로 모든 Machine에 공유해야 하는 Match 정보를 보관하는 용도로 사용한다.

## 4. 디버깅 순서

Respawn 이후 값이 이상할 때:

```text
1. PlayerState 값 확인
2. Old Character 값 확인
3. New Character 값 확인
4. Spawn/Possess 시 복사 로직 확인
5. 같은 데이터를 두 곳에서 원본으로 수정하는지 확인
```

## 5. Character가 PlayerState 값을 Cache할 수는 있는가?

필요하다면 표시/계산을 위해 값을 참조하거나 Cache할 수 있다.

다만:

```text
PlayerState와 Character 양쪽이
서로 독립적으로 수정되는 Source
```

가 되지 않도록 한다.

## 6. 기록 결론

Player 데이터의 저장 위치를 "`사용하기 편한 Class`"가 아니라 **Lifetime 기준으로 판단**해 Respawn 이후 State Divergence 가능성을 점검했다.

## 관련 개념

`PlayerState` `Character` `Respawn` `Possession` `State Ownership`
