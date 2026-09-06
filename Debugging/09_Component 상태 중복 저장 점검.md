# Component 간 동일 상태 중복 저장 점검

> **기록 유형:** State Consistency 점검

## 1. 점검 대상

Character와 여러 Gameplay Component가 같은 상태를 각각 독립적으로 저장하고 있지 않은지 점검했다.

문제 예:

```text
CombatComponent
└─ bIsStunned

StatusComponent
└─ bIsStunned

Character
└─ bIsStunned
```

## 2. 예상되는 문제

한 값을 갱신하고 다른 복사본을 놓치면:

```text
StatusComponent = Stunned
CombatComponent = Not Stunned
```

처럼 시스템별 판단이 달라질 수 있다.

결과:

```text
공격은 가능
이동은 불가능
UI는 정상 상태 표시
```

같은 일관성 문제가 생길 수 있다.

## 3. 해결 기준

동일한 Gameplay Fact는 하나의 State Owner가 관리한다.

예:

```text
StatusComponent
└─ bIsStunned ← Source of Truth

CombatComponent
└─ StatusComponent 조회 또는 Event 구독

Character
└─ State 결과에 반응
```

## 4. 직접 참조 vs Event

### 즉시 현재 상태가 필요

```text
StatusComponent->IsStunned()
```

### 상태가 변경될 때만 반응

```text
OnStatusChanged Delegate
```

처럼 사용 목적에 따라 조회와 Event를 구분한다.

## 5. 디버깅 순서

```text
1. 같은 상태 이름을 전체 검색
2. 몇 개 Class/Component가 Member로 보관하는지 확인
3. 값을 변경하는 함수 위치 확인
4. 원본 State Owner 결정
5. 다른 복사본 제거 또는 Read-only Cache로 명확히 구분
6. State 변경 로그를 한 곳에서 기록
```

## 6. 기록 결론

Component 분리는 코드 파일을 나누는 것보다 **각 State를 누가 소유하는지 명확히 하는 것**이 더 중요하다는 기준으로 중복 상태를 점검했다.

## 관련 개념

`Component Architecture` `Single Source of Truth` `Delegate` `State Ownership`
