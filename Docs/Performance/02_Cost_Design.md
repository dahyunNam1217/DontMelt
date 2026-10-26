# 시스템별 비용 관리 설계

NPC가 많은 Listen Server 게임에서 비용이 커지는 지점마다 어떤 선택을 했는지 정리했다. 각 선택에는 **무엇을 줄였는지와 무엇은 여전히 남는지**를 함께 적었다.

## NPC 인구와 반응 대상 제한

`ADMMatchGameMode::TryFinalizeStartingRoster()`에서 시작 인원과 배율로 NPC 수를 한 번 확정한다.

```cpp
ResolvedNPCMultiplierSnapshot = MatchRuleData->DefaultNPCMultiplier;
ResolvedNPCCountSnapshot = StartingPlayerCountSnapshot * ResolvedNPCMultiplierSnapshot;
```

- 스폰 지점은 `CollectSpawnPoints()`에서 한 번 수집한다. 이후 오인 공격 반응(`HandleWrongTargetAttack()`)은 월드를 다시 검색하지 않고 등록된 `RegisteredNPCs`(`TWeakObjectPtr` 목록)를 순회한다.
- 직접 맞은 NPC는 항상 반응하고, 주변 목격자는 `FVector::DistSquared2D()`로 반경을 검사해 거리순으로 `MaxNearbyResponderCount`(기본 6명)까지만 반응시킨다. 반응 반경 기본값은 1200cm다.

```cpp
const float ReactionRadiusSquared = FMath::Square(ReactionRadius);
const int32 NearbyReactionLimit = FMath::Min(MaxNearbyResponderCount, NearbyCandidates.Num());
```

상한은 **반응 실행 수**를 제한한다. 후보 탐색은 등록 NPC 수 N, 직접 타격 수 H에 대해 O(N × H), 후보 정렬은 O(M log M)으로 남는다. NPC 60명 규모에서는 문제가 되지 않는 크기다.

## AI 행동 스케줄링

`ADMJellyAIController`는 `PrimaryActorTick.bCanEverTick = false`이고, 행동은 타이머와 `OnMoveCompleted()` 콜백으로 이어진다.

| 상황 | 대기·재시도 값 | 의미 |
|---|---|---|
| 최초 행동 | 0.15~1.00초 | 같은 프레임에 생성된 NPC들의 첫 경로 요청 분산 |
| 이동 종료 후 | 0.15~0.40초 | 도착 직후 재요청 방지 |
| 경로 탐색 실패 | 0.50초 | 같은 프레임 무한 재시도 방지 |
| 점프 후 행동 | 0.60초 | 연속 동작 간 간격 |

새 상태로 바꿀 때는 `StopMovement()`와 기존 타이머를 먼저 정리해, 이전 이동의 완료 콜백이 새 상태를 덮어쓰지 않게 했다. 단, `bCanEverTick = false`는 AIController 자체의 Tick을 끄는 것이지 PathFollowing·CharacterMovement의 프레임 작업까지 없애는 것은 아니다.

## 복제 범위

| 데이터 | 복제 정책 | 이유 |
|---|---|---|
| 공격·방어 쿨다운 종료 서버 시각 | `COND_OwnerOnly` | 본인 HUD와 입력 판정에만 필요 |
| 스킬 쿨다운, 상호작용 세션, 퀘스트 진행도 | `COND_OwnerOnly` | 개인 정보 |
| `ActiveStatusTags`, 투명화 타이밍 | `COND_OwnerOnly` | 다른 상태까지 노출되지 않게 |
| `bStunPresentationActive`, `bInvisibilityPresentationActive` | 일반 복제 | 관찰자에게 필요한 최소 표현 상태만 따로 |
| 매치 Phase·공개 이벤트·결과 | GameState 일반 복제 + RepNotify | 참가자 공통 정보 |
| `ADMRelicManager` | `bReplicates = false` | 서버 전용 관리 객체 |
| 발소리 AudioComponent | 비복제 | AnimNotify로 각자 로컬 재생 |

남은 시간과 쿨다운은 **종료 서버 시각**만 복제하고, 클라이언트가 `GetServerWorldTimeSeconds()`와의 차이로 계산한다. 매초 값을 보내거나 RPC로 묻지 않는다.

NPC는 `SetReplicates(true)`, `SetReplicateMovement(true)`를 쓴다. NPC가 많을 때는 **이동 복제**가 가장 큰 네트워크 비용 후보이고, [NPC 성능 분석](01_NPC_Performance_Analysis.md)에서 `NetUpdateFrequency` 조정을 후보로 정리했다.

## 이동 예측에 Sprint 의도 통합

달리기 전환을 별도 Reliable RPC로 보내지 않고, CharacterMovement의 예측 경로에 넣었다.

```cpp
// FSavedMove_DMCharacter::GetCompressedFlags()
if (bSavedWantsToSprint)
{
    Result |= FSavedMove_Character::FLAG_Custom_0;
}
```

- `CanCombineWith()`는 Sprint 의도가 다른 SavedMove의 병합을 거부한다. 입력 전환 프레임을 잘못 합치지 않기 위한 선택이라, 달리기 토글이 잦으면 이동 전송량은 오히려 늘 수 있다.
- 다른 클라이언트의 달리기 애니메이션은 Sprint bool을 추가 복제하지 않고 관측 속도로 판단한다. 대신 패킷 손실 시 잠시 표현이 어긋날 수 있다.

## Tick 대신 타이머

| 클래스 | 설정 | 대신 쓰는 흐름 |
|---|---|---|
| `ADMJellyAIController` | Tick 비활성화 | 이동 완료 콜백·행동 타이머 |
| `ADMRelicManager`, `ADMRelic` | Tick 비활성화 | 서버 Commit, 1초 주기 위치 복구 검사 |
| `UDMCombatComponent` | Tick 비활성화 | 타격 Notify, 쿨다운 종료 시각 |
| `UDMInteractionComponent` | Tick 비활성화 | 완료 타이머(1회) + 검증 타이머(0.1초) |
| `UDMCharacterStateComponent` | Tick 비활성화 | 상태 변경·만료 타이머, `EndPlay`에서 일괄 정리 |
| `UDMSkillComponent`, `UDMPoseComponent` | Tick 비활성화 | 요청·상태 변경·타이머 |
| `UDMJellyPhysicsComponent`, `UDMCharacterAudioComponent` | Tick 비활성화 | 사망 이벤트, AnimNotify |
| `ADMBlackoutController` | Tick 비활성화 | 이벤트 상태 Delegate, 바인딩 실패 시에만 0.25초 재시도 |

타이머도 공짜는 아니다. 0.1초 타이머는 초당 약 10회 실행된다. 그래서 상호작용 검증 타이머는 **진행 중인 상호작용에만** 돌고, 끝나면 바로 해제한다. 젤리 AnimInstance의 `NativeUpdateAnimation()`처럼 매 프레임 계산이 필요한 곳은 Tick 대상으로 남겼다. 프로젝트 전체를 Tick-free라고 보지 않는다.

## UI 갱신

상태가 바뀔 때는 Delegate·OnRep으로 갱신하고, **시간이 흐르는 표시만** 필요한 동안 타이머로 갱신한다.

| 위젯 | 갱신 간격 | 대상 |
|---|---|---|
| `UDMGameplayHUDWidget` | 0.25초 | 경기 남은 시간 |
| `UDMSkillBarWidget`, `UDMCombatHUDWidget` | 0.10초 | 활성 쿨다운 |
| `UDMQuestHUDWidget`, `UDMGameRootWidget` | 0.20초 | 퀘스트, 상호작용 안내 |
| `UDMRandomEventHUDWidget` | 0.10초 | 이벤트 진행 |
| `UDMChestPanelWidget`, `UDMRelicRevealHUDWidget` | 0.05초 | 상자 진행, 유물 공개 연출 |
| `UDMMatchResultWidget`, `UDMSpectatorHUDWidget` | 0.25초 | 결과·관전 시간 |

```cpp
// UDMSkillBarWidget::RefreshCooldowns() - 쿨다운 중인 슬롯이 없으면 타이머를 멈춘다.
if (!bSlot1Cooling && !bSlot2Cooling && !bSlot3Cooling)
{
    StopDisplayTimer();
    return;
}
```

이 타이머들은 동시에 모두 돌지 않는다. 표시 조건이 끝나거나 `NativeDestruct()`가 오면 해제한다.

## 젤리 애니메이션과 사망 Ragdoll

- 평소의 젤리 흔들림은 AnimInstance가 스프링 7개로 계산하는 **시각 파라미터**다. 물리 시뮬레이션도, 복제도 없다. 위치가 크게 튀거나 업데이트 간격이 0.25초를 넘으면 스프링 상태만 초기화해 뼈가 튀는 것을 막는다.
- 사망 Ragdoll은 사망 이벤트에서만 켠다. Capsule 충돌을 끄고 Mesh는 `WorldStatic`·`WorldDynamic`만 Block하게 해, 시체가 다른 Pawn이나 전투 Visibility 트레이스를 방해하지 않는다. 시작 신호만 Reliable Multicast로 보내고 뼈별 자세는 동기화하지 않는다.
- 남는 비용은 **NPC 수만큼의 AnimInstance 평가**와 동시 사망 시의 물리 Body 수다. 측정 항목은 [프로파일링 계획](03_Profiling_Plan.md)에 넣었다.

## 서버 전용 관리자와 액터 캐시

`ADMRelicManager`는 Tick과 복제를 모두 끈 서버 전용 Actor다. `CacheWorldActors()`가 초기화 때 상자와 유물 스폰 지점을 한 번 수집해 보관한다. 상호작용 컴포넌트는 찾은 매니저를 `CachedRelicManager`로 보관하고, 캐시가 무효해졌을 때만 다시 찾는다. 이 캐시는 상자와 스폰 지점이 런타임에 추가되지 않는다는 전제를 둔다.

## 승패 판정은 단순 순회

승패 확인은 최대 4명의 참가자를 이벤트 시점에만 순회한다. 이 규모에서 캐시나 별도 자료구조를 추가하면 데이터 일관성 관리 비용만 늘어난다. 대신 `TInlineAllocator<DMMatch::MaxPlayers>`로 힙 할당 없이 목록을 만든다.

---

[Performance 목차](README.md)
