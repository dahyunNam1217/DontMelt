# 0단계 — 기획과 전체 책임 설계

## 01. 게임 핵심 기획 및 게임 규칙 확정

개인 프로젝트 **Don't Melt**의 전체 게임 구조와 핵심 플레이 규칙을 구체화했다.

게임의 기본 구조는 플레이어가 다수의 NPC 사이에서 NPC처럼 행동하며 다른 플레이어를 속이고, 유물을 확보하여 승리하는 방식으로 확정했다.

주요 규칙:

- 2~4인 멀티플레이
- Listen Server
- 서버 권한형 판정
- 유물 수집 및 등록
- 플레이어 사망 및 Respawn
- 공격 / 방어 / 회피
- 스킬 시스템
- 관전 시스템
- NPC 최대 약 40명
- 최종적으로 한 명의 승자 결정

전투 판정 규칙:

- 공격 범위 안의 대상 전체 판정
- Player와 NPC가 동시에 범위 안에 있으면 모두 공격 처리
- Player는 사망 판정, NPC는 Stun 처리
- 공격 Trace는 벽을 통과하지 않음
- 방어는 전방 공격만 방어
- 방어 성공 후에도 방어 지속시간 유지
- 동시 공격은 서버에 먼저 도착한 판정을 우선 처리

**결과**

구현 전에 애매했던 게임 규칙을 먼저 확정하여, 이후 코드에서 예외 처리 기준이 달라지는 문제를 방지했다.

## 02. 전체 개발 단계 및 구현 순서 설계

전체 게임을 한 번에 구현하지 않고, 기능 의존성에 따라 단계적으로 개발하기로 결정했다.

```text
0. 기획 및 전체 책임 설계
1. C++ 프로젝트 골격
2. Match 기반
3. 이동·자세
4. 유물
5. 전투·리스폰
6. NPC AI
7. 퀘스트·이벤트
8. 스킬
9. 관전
10. UI
11. Steam·음성
12. 최적화
```

각 단계가 완료되지 않은 상태에서 다음 기능을 미리 구현하지 않는 방식을 적용하기로 했다.

**설계 원칙**

```text
현재 필요한 기능
    ↓
필요한 책임 결정
    ↓
필요한 클래스 결정
    ↓
구현
    ↓
검증
    ↓
다음 단계
```

미래에 필요할 가능성만으로 클래스, Component, Enum, Struct 등을 미리 만들지 않는 방향으로 정리했다.

## 03. Unreal Gameplay Framework 책임 분리

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

**판단 기준**

데이터를 어디에 저장할지 결정할 때 다음 질문을 기준으로 사용했다.

> 이 데이터의 원본은 누가 소유해야 하는가?
> Character가 Destroy되어도 유지되어야 하는 데이터인가?
> Client 전체가 알아야 하는 데이터인가?

## 04. 프로젝트 최종 Architecture 확정

Gameplay Framework뿐만 아니라 Component 단위까지 책임을 구체적으로 분리했다.

```text
UDMGameInstance

Lobby
├─ ADMLobbyGameMode
├─ ADMLobbyGameState
└─ ADMLobbyPlayerState

Match
├─ ADMMatchGameMode
├─ ADMMatchGameState
└─ ADMMatchPlayerState

Player
├─ ADMPlayerController
├─ ADMBaseCharacter
└─ ADMPlayerCharacter

Component
├─ Health
├─ Character State
├─ Team
├─ Weapon
└─ Pose / Movement 관련 Component
```

Lobby와 Match의 PlayerState를 분리하고, Ready 정보의 Single Source of Truth를 확정했다.

- Ready 상태 → `ADMLobbyPlayerState`
- Lobby 설정 → `ADMLobbyGameState`
- Match 규칙 → `ADMMatchGameMode`
- 공개 Match 상태 → `ADMMatchGameState`

**검수한 내용**

다음 중복 여부를 집중적으로 검사했다.

- GameMode ↔ GameState
- PlayerState ↔ Character
- CombatComponent ↔ StatusComponent
- Movement State ↔ Action State
- Enum ↔ Gameplay Tag
- Gameplay Data ↔ UI
