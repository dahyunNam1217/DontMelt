# 3일차 — Unreal Gameplay Framework 책임 분리

## 오늘 한 일

Unreal Gameplay Framework를 기준으로 게임 시스템의 책임을 분리했다. 특히 여러 객체가 동일한 데이터를 중복으로 관리하지 않도록 설계를 검수했다.

**GameMode**
- 게임 규칙 판정
- 승리 조건 판단
- Match Phase 전환 결정
- Server Only

**GameState**
- 현재 Match Phase
- 남은 시간
- 공유 Match 데이터
- Server → Client Replication

**PlayerState**
- 플레이어 단위 지속 데이터
- 생존 상태
- 통계
- 진영
- Respawn 이후에도 유지되어야 하는 정보

**Character**
- 현재 Pawn에 종속된 상태
- Movement
- Animation
- 현재 Character Runtime 상태

## 복습 개념

- Unreal Gameplay Framework
- GameMode
- GameState
- PlayerState
- PlayerController
- Character / Pawn
- Data Ownership
- Single Source of Truth

## 오늘의 판단 기준

데이터를 어디에 저장할지 결정할 때 다음 질문을 기준으로 사용했다.

> 이 데이터의 원본은 누가 소유해야 하는가?
> Character가 Destroy되어도 유지되어야 하는 데이터인가?
> Client 전체가 알아야 하는 데이터인가?
