# 04 — 유물·상호작용

## 1. 객체와 데이터 소유권

| 객체 | 책임 |
|---|---|
| `ADMRelic` | 개별 `WorldState`, `RegisteredBaseSlot`, 월드 표현 |
| `ADMRelicManager` | 운반자 관계, 예약, Pickup/Drop/Steal/Deposit Commit, 등록 수 계산 |
| `ADMBaseZone` | BaseSlot, 활성 여부 |
| `ADMRelicChest` | 진영의 상호작용 지점 |
| `ADMRelicSpawnPoint` | 공용 유물 배치·복구 위치 |
| `UDMInteractionComponent` | 후보 탐색, 서버 상호작용 세션, 진행·취소 |

`bIsCarryingRelic`은 소유자 HUD용 표시값이고, 실제 운반 관계는 Manager가 갖는다. 상자와 GameState에 등록 수를 누적 저장하지 않고, 유물 상태를 조회해 계산한다.

## 2. 수량과 상태

| 시작 인원 | 전체 유물 | 승리 필요 수 |
|---:|---:|---:|
| 2 | 3 | 3 |
| 3 | 4 | 3 |
| 4 | 5 | 4 |

활성 진영마다 1개, 공용 1개를 둔다. `TotalRelicCount = StartingPlayerCount + 1`을 경기 시작 때 확정한다.

```text
PublicSpawn ─Pickup─→ Carried
Dropped ─Pickup─────→ Carried
Registered ─Steal───→ Carried
Carried ─Deposit────→ DepositProtected ─5초─→ Registered
Carried ─Drop/Death─→ Dropped
```

`Registered`와 `DepositProtected` 모두 등록 수에 포함한다. 보호 5초 동안은 훔칠 수 없다. 유물 하나를 둘이 들거나 한 명이 두 개를 드는 상태가 생기지 않도록 유물과 PlayerState의 관계를 검증한다.

## 3. 입력과 서버 요청

| 상황 | 조작 | 서버 확정 |
|---|---|---|
| 공용·드롭 유물 | F 2초 유지 | Pickup |
| 상자 접근 | F 누름 | 상자 스냅샷 응답 |
| 내 상자, 유물 운반 중 | Space 2초 유지 | Deposit |
| 상대 상자에 유물 있음 | Space 2초 유지 | Steal |
| 상자 닫기 | F 누름 | 로컬 Context 복귀 |
| 유물 버리기 | Q 누름 | Manual Drop |

상자를 여는 동작은 예약을 잡는 2초 행동이 아니다. 진행 시간과 서버 세션은 등록·탈취·바닥 습득에만 쓴다. 중단되면 로컬 진행바를 정리하고 예약을 푼다.

## 4. 후보 탐색

처음의 LineTrace 방식은 3인칭에서 바닥 유물을 정확히 내려다봐야 해서 조작이 어려웠다. 최종 후보 선택은 캐릭터 주변 Sphere 탐색, 수평 Facing, 거리, 카메라→대상 시야를 함께 쓴다. 기본 상호작용 거리는 250cm다. 이 탐색은 **안내용 예측**일 뿐 성공 판정이 아니다. 서버가 거리·시야·LifeState·자세·Phase·유물 상태·예약·운반자를 다시 검증한다. 시야 판정은 사이를 지나가는 Pawn을 차단으로 보지 않는다([LOS 문제](../../Debugging/Gameplay/03_Interaction_LOS_Pawn.md)).

## 5. 동시성과 실패 처리

- 상호작용마다 Request ID와 세션을 둔다. 유물 단위 예약으로 중복 완료를 막는다.
- 예약에 성공해도 완료가 보장되지 않는다. 진행 중 대상이 움직이거나, 진영이 비활성화되거나, 플레이어가 사망·기절하면 완료 요청을 거부한다.
- 입력을 놓은 취소, Phase 변경에 의한 시스템 취소, 조건 위반 실패를 구분한다.
- 사망 시 상호작용을 정리한 뒤 RelicManager에 Death Drop을 요청한다.
- 등록이 성공하면 서버는 유물 상태를 먼저 바꾼 다음 GameMode에 승리 평가를 요청한다.
- 드롭된 유물이 바닥이 없는 위치에 있으면 지정된 복구 지점으로 옮긴다. 임의 좌표에 만들지 않는다.

## 6. 네트워크와 UI

진영별 등록 수를 전역 복제하지 않는다. 상자를 연 플레이어에게만 서버 스냅샷으로 준다. 유물 위치 공개와 연장전 소개만 정해진 시간 동안 공개 데이터를 만든다.

## 7. 이후 연계

| 단계 | 연계 |
|---|---|
| 5단계 | 사망하면 상호작용 중단, 운반 유물 Drop |
| 8단계 | 선두 대상 스킬이 `GetRegisteredRelicCount()`로 서버 판단 |
| 10단계 | 상자 패널과 유물 HUD 중복 제거, 후반 위치 공개, 결과표의 등록 수·누적 획득 수 분리 |

관련 문서: [유물 상호작용 구현](../../Technical/06_Relic_Interaction.md)

[Stages 목차](README.md)
