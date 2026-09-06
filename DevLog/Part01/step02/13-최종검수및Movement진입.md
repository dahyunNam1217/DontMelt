# 13일차 — 2단계 최종 검수 및 이동·자세 단계 설계 시작

## 오늘 한 일

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

## 복습 개념

- Code Review
- Performance Review
- Network Optimization
- Over-Abstraction
- CharacterMovementComponent
- Player Input
- AI Movement
- Shared Character Architecture
- Code Reuse
- Regression Testing

## 현재 진행 상태

```text
0단계 — 게임 기획 / 전체 책임 설계     ✅ 완료
1단계 — C++ 프로젝트 골격             ✅ 완료
2단계 — Match 기반                    ✅ 구현 및 최종 검수 완료
3단계 — 이동·자세                     🟡 진입 / 구현 설계 진행
4단계 — 유물                          ⬜ 미진행
5단계 — 전투·리스폰                   ⬜ 미진행
6단계 — NPC AI                        ⬜ 미진행
7단계 — 퀘스트·이벤트                 ⬜ 미진행
8단계 — 스킬                          ⬜ 미진행
9단계 — 관전                          ⬜ 미진행
10단계 — UI                           ⬜ 미진행
11단계 — Steam·음성                   ⬜ 미진행
12단계 — 최적화                       ⬜ 미진행
```
