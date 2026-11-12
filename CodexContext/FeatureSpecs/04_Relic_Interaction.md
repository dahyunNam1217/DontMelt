# 04. 유물·상자·서버 검증형 상호작용

## P2 승인 동기화 — 2026-11-13

이번 사용자 P2 요청의 명시 범위만 **설계 승인**으로 채택한다. 과거 승인으로 소급하지 않는다. 기준 HEAD는 `914173a`이며 구현 사실·설계 승인·에셋 적용·실행 검증은 별개다. 상세 근거와 미결정 사항은 [ImplementationStatus 10절](../ImplementationStatus.md#10-p2-승인-설계-및-현재-구현--2026-11-13)을 따른다.

**아래 기존 본문과의 관계:** 이 절 뒤의 기존 본문은 삭제하지 않고 이전 설계·조사 기록으로 보존한다. E1~E10과 충돌하는 값·완료/승인/미확인 표현은 이 절과 중앙10절이 대체한다. 특히90초·Walk300·NPC×10/40·NPC 발소리·3요소 이벤트 HUD·타임아웃 즉시 재요청은 최신 목표가 아니다. E1~E10 밖의 항목을 이번 승인으로 일괄 확정하지 않는다.

### E1 — 결과 통계와 순위 분리

**설계 승인:** 결과표 유물 열은 경기 중 누적 획득 횟수, 최종 순위는 종료 시 등록 유물 수를 사용하며 동률 공동 순위를 허용한다. 두 통계를 별도 관리하고 표시 변경으로 서버 승리 판정을 바꾸지 않는다.

**구현 확인 / 부분 구현:** `ADMRelicManager::TryCommitRelicAcquisition` → `ADMMatchPlayerState::IncrementAcquiredRelicCount` → `ADMMatchGameMode::BuildFinalPlayerResults`로 누적 통계를 보관한다. `FDMMatchResultEntry`의 `AcquiredRelicCount`와 `RelicCount`가 분리되어 있다. 그러나 `UDMPlayerResultRowWidget::SetResultEntry`는 현재 `RelicCount`를 표시하므로 목표 표시와 불일치한다. 성공 Pickup/Steal Commit마다 증가하는 것이 현재 동작이다.

책임·서버/Client 흐름·RPC/복제·UI·변경 예상 범위·미결정·검증 시나리오는 [중앙 기능 명세](../ImplementationStatus.md)의 E1 절을 함께 읽는다.

**검증 상태:** BP/DataAsset/Animation 실제 적용은 에셋 미검증, 빌드·PIE·Steam은 실행 미검증이다. 아래 완료 기준은 향후 검증 시나리오이며 통과 기록이 아니다. C++·WBP·Config 변경은 이번 문서 승인에 포함되지 않는다. [공통 게임 규칙](../GameplayRules.md) · [네트워크 정책](../NetworkPolicy.md)를 상호 참조한다.

---

## 데이터 소유권

| 요소 | 책임 |
|---|---|
| `ADMRelic` | WorldState, RegisteredBaseSlot, 월드의 개별 유물 상태 |
| `ADMRelicManager` | 스폰·운반 관계·예약·Pickup/Steal/Deposit/Drop Commit·등록 수 계산 |
| `UDMInteractionComponent` | 상호작용 후보/요청, Server session, Hold 검증/취소 |
| `ADMRelicChest` | 상자 접근/열기 인터랙션 지점 (등록 수 원본 아님) |
| `ADMBaseZone` / `ADMRelicSpawnPoint` | 진영/배치·복구 위치 |
| `ADMMatchPlayerState` | OwnerOnly `bIsCarryingRelic` 등 HUD 표시용 (원본 운반자는 Manager) |

## 수량 및 상태 머신

2/3/4인 경기의 전체 유물 **3/4/5개**, 등록 승리 목표 **3/3/4개**. 시작 인원 스냅샷으로 고정.

```text
PublicSpawn / Dropped → Carried → DepositProtected → Registered
Registered → Carried   [상대 상자 탈취]
Carried → Dropped      [수동 드롭 / 사망]
```

등록 보호 **5초**, 이때 탈취 불가이나 등록 수에는 포함. 운반 최대 1개.

## 요청 흐름

1. F로 필드/드롭 유물에 대해 Pickup 2초 Hold; F로 상자 열기는 즉시(2초 Hold 아님); Space Hold 2초로 Deposit/Steal; Q로 드롭.
2. 로컬 UI 후보는 Sphere+수평 Facing+거리(기본 250cm)+카메라 LOS로 표시. **안내는 판정 아님**.
3. `ServerRequestInteraction(Target, RequestId)`에서 유효성 확인. 과거·중복 RequestId 거절.
4. 유물 단위 예약(`TryAcquireInteractionReservation`) 후 시작/끝 **서버 시각을 소유자에게만** 전달, 진행바 로컬 계산.
5. 서버가 진행 중 **0.1초**마다 거리·시야·LifeState·ActionState·Phase·유물 상태·예약을 재검증.
6. Commit 직전에 다시 유효성 검사하고 상태 변경 → 예약/속도 제한 정리. 등록 성공 뒤 GameMode 종료 평가.

## 취소·실패

- 키 해제/모드 해제 = 사용자 취소, Phase 전환·경기 종료 = 시스템 취소, 거리·LOS·상태 위반 = 실패. **실패 페널티를 시스템 취소에 적용하지 않는다.**
- 상대 Pawn이 트레이스 사이에 끼어도 LOS 가림이 아님; 지형은 차단.
- 수동 Q Drop은 벽을 통과하지 않도록 전방 Visibility 검사.
- 유물 낙하 지점이 유효하지 않으면 **1초 주기 검사** 후 지정 복구 지점으로 이동.
- 사망 시 진행 세션·예약 해제 뒤 RelicManager Death Drop.
- 동시 요청 2개가 같은 유물을 들고 가지 못하도록 Manager 단일 예약과 최종 Commit 검사.

## 비공개 데이터

상대 운반자·각 진영 상자 등록 수는 전역 복제 금지. 상자를 연 소유 연결에만 시점 스냅샷 응답. 정규 종료 전 3초 위치 공개/연장전 5초 수 공개만 예외.

## 확인할 구현 후보

`Source/DontMelt/Private/Characters/Components/DMInteractionComponent.cpp`의 `ServerRequestInteraction_Implementation`, `ServerValidateActiveInteraction`; `.../Relic/DMRelicManager.cpp`의 `ResolveInteractionRequest`, `TryAcquireInteractionReservation`, `TryCommitRelicAcquisition`, `TryCommitRelicDeposit`, `TryDropCarriedRelic`.

## 완료 기준

- 같은 유물 동시 F Hold, 동일 RequestId 중복·취소·재전송.
- 등록 중 죽음/스턴/거리 이탈/Phase 종료, 등록 보호 5초/탈취, 벽 앞 드롭·지면 밖 복구.
- 2/3/4인 유물 수, 승리 등록 목표/중간 탈취, 상자 수 비공개 확인.
- Host/Client 지연 환경에서 서버 Commit과 진행바 성공/취소 일치.

[설계](../../Docs/Design/Stages/04_RelicInteraction.md) · [기술](../../Docs/Technical/06_Relic_Interaction.md) · [상호작용 버그 기록](../../Docs/Debugging/Gameplay/04_Interaction_PhaseChange.md)
