# Don't Melt

UE5 C++ 기반 **2\~4인 Listen Server 멀티플레이 프로젝트**입니다.

이 프로젝트의 핵심 목표는 기능을 빠르게 늘리는 것이 아니라, 멀티플레이 게임에서 중요한 **Authority, 상태 소유권, Replication 범위, Gameplay Framework 책임**을 명확하게 설계한 뒤 구현하는 것입니다.

\---

## Engineering Goals

* **Server Authority**를 기준으로 게임플레이 판정 구조 설계
* 동일 상태를 여러 객체가 저장하지 않도록 **Single Source of Truth** 유지
* GameMode / GameState / PlayerState / Controller / Character의 **Lifetime과 책임 분리**
* 필요 이상의 네트워크 데이터 전송을 피하는 **Minimal Replication**
* Character 기능을 Component 단위로 나누는 **Composition 기반 구조**
* 불필요한 Tick과 Polling 대신 **Event Driven** 흐름 우선 검토
* 기능 추가 전 상태 소유자와 네트워크 공개 범위를 먼저 정의

\---

## Gameplay Framework Responsibility

|클래스|주요 책임|
|-|-|
|`ADMMatchGameMode`|서버 전용 매치 진행, Spawn / Death / Respawn, 결과 판정|
|`ADMMatchGameState`|모든 참여자에게 공개되는 매치 상태의 Replication|
|`ADMMatchPlayerState`|플레이어 단위의 지속·공유 데이터|
|`ADMMatchPlayerController`|입력, 로컬 UI 흐름, 특정 Client 대상 응답|
|`ADMCharacter` + Components|실제 Character 행동과 기능 실행|

설계 원칙과 예외 기준은 [최종 아키텍처 설계서](./Docs/최종%20아키텍처%20설계서.md)에 정리했습니다.

\---

## Source Structure

```text
Source/DontMelt/
├─ Public/
│  ├─ Characters/
│  ├─ Core/
│  ├─ Data/
│  └─ Framework/
│
└─ Private/
   ├─ Characters/
   ├─ Core/
   ├─ Data/
   └─ Framework/
```

Character 기능은 한 클래스에 모두 넣기보다 역할별 Component로 분리합니다.

```text
Characters/Components/
├─ DMCharacterStateComponent
├─ DMCombatComponent
├─ DMInteractionComponent
├─ DMJellyPhysicsComponent
└─ DMPoseComponent
```

\---

## Network Design

### Authority

서버가 최종 판정해야 하는 상태와 Client가 표현만 담당할 상태를 구분합니다.

### Replication

`UPROPERTY(Replicated)`를 많이 사용하는 것이 목표가 아니라, **다른 Client가 실제로 알아야 하는 상태만 공개**하는 것을 기준으로 합니다.

### State Ownership

설계·디버깅 시 아래 네 가지를 먼저 확인합니다.

1. **Owner** — 이 데이터의 원본은 어디인가?
2. **Authority** — 누가 값을 변경할 수 있는가?
3. **Lifetime** — Pawn 교체 / Respawn / Map 이동 후에도 남아야 하는가?
4. **Replication Scope** — Server only / Owner only / Everyone 중 누가 알아야 하는가?

\---

## Verification \& Debugging

단순 오류 해결뿐 아니라 구조적 위험을 검증 대상으로 기록합니다.

* Replication 등록 누락
* Match Phase Client Replication
* Listen Server 종료 후 Client 복귀
* Delegate 중복 등록 Regression Test
* Server Authority 위반 점검
* GameMode / GameState 상태 중복
* PlayerState / Character 데이터 중복
* Component 간 상태 중복
* Movement State / Action State 충돌
* 불필요한 Tick / 반복 탐색 / Replication 비용 점검

➡️ [Debugging Index](./Debugging/README.md)

\---

## Documentation

|문서|내용|
|-|-|
|[최종 기획서](./Docs/최종기획서.md)|게임 규칙과 기능 요구사항|
|[최종 아키텍처 설계서](./Docs/최종%20아키텍처%20설계서.md)|책임·의존성·상태 소유권·네트워크 설계|
|[개발 계획표](./Docs/개발%20계획표.md)|단계별 구현 및 검증 계획|
|[DevLog](./DevLog/README.md)|실제 구현 진행 기록|
|[Debugging](./Debugging/README.md)|오류·검증·회귀 테스트 기록|

\---

## What I Want This Repository to Show

이 저장소는 “멀티플레이 기능을 만들어 봤다”는 결과보다 다음 역량을 보여주는 것을 목표로 합니다.

* UE Gameplay Framework의 역할을 구분해서 설계하는 능력
* Server / Client의 책임을 구분하는 사고방식
* 상태 중복과 의존성 문제를 사전에 발견하는 구조적 사고
* 로그·재현 절차·검증 기준을 통해 원인을 좁히는 디버깅 방식
* 설계 문서와 실제 코드를 연결해 설명하는 능력

