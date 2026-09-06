# UI / Gameplay State 불일치 점검

> **기록 유형:** 데이터 흐름 기반 UI 디버깅

## 1. 점검 대상

Widget이 Gameplay State의 별도 원본 값을 보관하고 있지 않은지 확인했다.

문제 구조:

```text
HealthComponent
└─ CurrentHealth = 80

Widget
└─ Health = 100
```

Widget 내부 값이 Gameplay State와 독립적으로 변하면 UI와 실제 게임 상태가 달라질 수 있다.

## 2. 정상 데이터 흐름

```text
Server Gameplay
        ↓
HealthComponent State 변경
        ↓
Replication / OnRep
        ↓
OnHealthChanged Delegate
        ↓
Owning Client Widget 갱신
```

UI는 State를 소유하기보다 **현재 State를 표현하는 Consumer**로 둔다.

## 3. 불일치 발생 시 확인 순서

```text
1. 원본 Gameplay 값은 정상인가?
2. Remote Client까지 Replication됐는가?
3. OnRep가 실행됐는가?
4. Delegate가 Broadcast됐는가?
5. Widget이 Bind돼 있는가?
6. Widget Update Function이 실행됐는가?
7. 표시 Conversion(FText 등)이 정상인가?
```

이렇게 Network → State → Event → Presentation 순으로 범위를 좁힌다.

## 4. Listen Server에서 주의

Host UI는 Server State를 같은 Process에서 즉시 볼 수 있어 Remote Client에서만 발생하는 Replication/UI 문제를 놓칠 수 있다.

따라서:

```text
Host Widget
Remote Client Widget
```

을 분리해서 확인한다.

## 5. Widget에 Local 표시 상태가 있을 수는 있다

다음과 같은 Presentation-only 값은 Widget이 소유할 수 있다.

```text
Animation Progress
현재 열린 Tab
Hover State
Local Transition State
```

문제는 Gameplay Fact 자체를 Widget이 별도 Source of Truth로 저장하는 경우다.

## 6. 기록 결론

UI 오류를 Widget 코드부터 수정하지 않고 **Gameplay State → Replication → Notification → Widget** 데이터 흐름을 단계적으로 추적하는 디버깅 기준을 정리했다.

## 관련 개념

`UI Synchronization` `OnRep` `Delegate` `HealthComponent` `UMG`
