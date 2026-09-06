# Movement State / Action State 충돌 점검

> **기록 유형:** 상태 모델링 점검

## 1. 문제

모든 Player 상태를 하나의 Enum에 넣으면 서로 동시에 존재할 수 있는 상태를 표현하기 어렵다.

예:

```text
Idle
Walking
Running
Blocking
Attacking
```

이 구조는:

```text
Running + Blocking
```

같은 조합을 하나의 값으로 표현하지 못한다.

## 2. 발생 가능한 증상

```text
CurrentState = Running
        ↓
Block 입력
        ↓
CurrentState = Blocking
```

으로 바뀌면 "`현재 이동 중`"이라는 정보가 사라진다.

반대로 공격 상태를 표현하면서 이동 상태를 덮어쓰면 Movement/Animation/UI가 서로 다른 조건을 기대하게 될 수 있다.

## 3. 해결 방향

서로 독립 가능한 상태 축을 분리한다.

```text
Movement State
├─ Idle
├─ Walking
└─ Running

Action State
├─ None
├─ Blocking
└─ Attacking
```

필요하면 별도로:

```text
Life State
Status Effect State
```

를 둔다.

## 4. 모든 상태를 무조건 쪼개지는 않는다

State 축을 분리할 때도 실제 동시 존재 가능성을 기준으로 한다.

```text
서로 절대 동시에 존재할 수 없음
→ 하나의 Enum 후보

독립적으로 조합 가능
→ 별도 State Axis 후보
```

## 5. 디버깅 순서

상태가 의도치 않게 사라질 때:

```text
1. 어떤 상태를 Set했는가?
2. 같은 Enum에 있던 이전 정보가 덮어써졌는가?
3. 두 상태가 실제로 동시에 존재 가능한가?
4. 그렇다면 State Axis를 분리한다.
```

## 6. 기록 결론

상태 충돌을 단순 조건문 버그로 보지 않고, **상호 배타적인 상태와 조합 가능한 상태를 같은 Enum에 넣은 모델링 문제인지 먼저 확인**하는 기준을 만들었다.

## 관련 개념

`State Machine` `Enum` `Movement` `Action State` `State Modeling`
