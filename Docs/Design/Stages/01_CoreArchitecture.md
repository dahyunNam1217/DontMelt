# 01 — 기본 클래스와 데이터 소유권

## 클래스 계층

```text
Frontend / Lobby / Match
├─ GameMode    규칙 검증
├─ GameState   공개 상태
├─ PlayerState 참가자 수명 상태
└─ Controller  입력·UI·소유자 요청

JellyCharacterBase
├─ PlayerCharacter
└─ NPCCharacter
```

Lobby와 Match는 GameMode·GameState·PlayerState·Controller를 서로 재사용하지 않고 따로 둔다. Character에는 이동과 Pawn 수명의 기능을 두고, 리스폰해도 남아야 하는 상태는 PlayerState에 둔다.

## 유지할 원칙

| 원칙 | 의미 |
|---|---|
| Server Authority | 성공·실패의 최종 Commit은 서버에서 한다 |
| Single Source of Truth | 하나의 상태를 여러 bool·Enum에 중복 저장하지 않는다 |
| Composition | 수명과 책임이 실제로 다를 때만 Component로 나눈다 |
| DataAsset | 경기 수치와 반복 콘텐츠를 코드에서 분리한다 |
| Minimal Replication | 서버 내부 상태와 숨겨야 할 정보를 복제하지 않는다 |
| Event Driven | 가능하면 Timer·Delegate·RepNotify를 쓴다 |

## 파일과 의존성

`Source/DontMelt/Public`과 `Private` 아래에 같은 기능별 폴더를 둔다. `*.generated.h`는 헤더의 마지막 include다. 순환 참조가 생기면 단방향 호출 경계를 다시 정하고, 필요한 곳은 전방 선언으로 푼다.

## 이 단계의 작업

기능이 없는 Component가 Tick하지 않도록 `bCanEverTick = false`로 시작했고, 필요 없는 `BeginPlay`·`TickComponent`는 만들지 않았다. Development Editor 빌드, Lobby·Match 테스트 맵 진입, Listen Server + 여러 PIE Client 접속, Player·NPC 생성, Component 부착을 확인한 뒤 다음 단계로 넘어갔다.

현재 전체 책임은 [시스템 아키텍처 설계](../SystemArchitecture.md)에 정리했다.

[Stages 목차](README.md)
