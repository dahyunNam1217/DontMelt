# Enum / Gameplay Tag 동일 상태 중복 점검

> **기록 유형:** 상태 표현 방식 점검

## 1. 점검 대상

같은 Gameplay Fact를 Enum과 Gameplay Tag가 각각 독립적으로 표현하고 있지 않은지 확인했다.

예:

```cpp
EDMPlayerLifeState::Dead
```

동시에:

```text
State.Player.Dead
```

## 2. 문제 구조

둘 다 원본 상태로 사용되면:

```text
Enum = Dead
GameplayTag = 없음
```

또는:

```text
Enum = Alive
GameplayTag = State.Player.Dead
```

같은 모순이 발생할 수 있다.

## 3. 판단 기준

동일한 사실이라면 **원본 표현을 하나 선택**한다.

예:

```text
Player Life State
→ EDMPlayerLifeState를 Source of Truth

Gameplay Tag
→ 다른 시스템 연동이 필요하면
  Source에서 파생/동기화
```

또는 프로젝트 전체가 Gameplay Tag 중심 State System을 사용한다면 반대로 Tag를 Source로 둘 수 있다.

핵심은 "`Enum이 좋다 / Tag가 좋다`"가 아니라 **양쪽이 독립적으로 원본이 되지 않는 것**이다.

## 4. 서로 다른 역할이면 같이 사용할 수 있다

예:

```text
Enum
→ 상호 배타적인 큰 Life State

Gameplay Tags
→ 동시에 여러 개 존재할 수 있는 Effect/Condition
```

이라면 두 시스템이 서로 다른 Fact를 표현하므로 중복이 아니다.

## 5. 디버깅 순서

```text
1. 문제가 되는 Gameplay Fact 이름 정의
2. Enum과 Tag 중 어디에 저장되는지 확인
3. 양쪽 Update 함수 확인
4. 하나가 다른 하나에서 파생되는지 확인
5. 독립 Update 경로가 있으면 통합
```

## 6. 기록 결론

상태 시스템 확장 과정에서 Enum과 Gameplay Tag가 모두 존재한다는 이유만으로 문제라고 보지 않고, **동일한 Gameplay Fact를 둘이 독립적으로 소유하는 경우만 중복 상태로 판단**한다.

## 관련 개념

`Enum` `Gameplay Tag` `State Model` `Single Source of Truth`
