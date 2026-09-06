# 7일차 — Match 시스템 기본 구조 구현

## 오늘 한 일

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

## 복습 개념

- Match State Machine
- Enum
- Server Game Rule
- Match Lifecycle
- State Transition

## 설계 포인트

단순 Boolean 여러 개를 사용하는 것보다, Match 전체의 진행 단계를 명시적인 Phase(Enum)로 관리하는 방향을 선택했다.
