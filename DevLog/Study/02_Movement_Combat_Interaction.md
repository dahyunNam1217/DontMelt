# 이동·전투·상호작용

## GetMaxSpeed로 이동 제한

이동 속도 제한은 입력 차단이 아니라 `GetMaxSpeed()`에서 한다. 클라이언트 예측과 서버 재생이 같은 함수를 거치므로 양쪽 결과가 같다.

Don't Melt에서는 `Status.Stunned`가 있으면 0을 반환하고, 달리기 요청이 있을 때만 달리기 속도를 쓴다. 입력만 막으면 서버 쪽에서는 여전히 움직일 수 있고, 반대로 서버만 막으면 클라이언트가 움직였다가 당겨진다.

`OnMovementUpdated()`에서는 "달리기 키를 누르고 있는가"와 "실제로 빠르게 움직이고 있는가"를 나눠 본다. 벽에 막혀 있으면 요청이 있어도 속도는 0이다.

소스: [DMCharacterMovementComponent.cpp](../../Source/DontMelt/Private/Characters/Components/DMCharacterMovementComponent.cpp)

## AnimNotify 시점의 서버 히트 판정

몽타주 재생과 피해 확정은 다른 일이다. 원격 클라이언트는 입력 반응을 위해 몽타주를 먼저 재생하지만, 피해는 서버에서 재생한 몽타주의 `UDMAnimNotify_AttackHit` 시점에 판정한다.

| 단계 | 담당 |
|---|---|
| 판정 시점 | 서버 몽타주의 AnimNotify |
| 후보 수집 | `ECC_Pawn` 오버랩 |
| 재검사 | 거리·전방 각도·월드 지형 차폐 |
| 확정 | 스폰 보호·방어 각도·방어 횟수 확인 후 사망 또는 NPC 반응 Commit |

Notify가 두 번 오거나 몽타주가 중간에 끊겨도 피해가 두 번 들어가지 않도록 `AttackSequence`와 `LastResolvedAttackHitSequence`를 비교한다.

소스: [DMAnimNotify_AttackHit.cpp](../../Source/DontMelt/Private/Animations/DMAnimNotify_AttackHit.cpp) · [DMCombatComponent.cpp](../../Source/DontMelt/Private/Characters/Components/DMCombatComponent.cpp)

## 상호작용 거리와 시야

화면에 유물이 보인다는 것은 서버가 허용할 근거가 아니다. 서버는 서버 좌표로 거리를 재고, 트레이스로 벽에 가려졌는지 다시 본다.

트레이스는 채널 응답과 무시할 Actor 설정에 따라 결과가 달라진다. 시야 판정에서 다른 Pawn이 상자를 가리는 문제를 고칠 때, 오브젝트 타입 트레이스로 바꾸는 1차 안은 채널 응답을 무시해 상자 자체가 가림막이 되는 회귀를 만들었다. 반영 전에 발견해서 폐기하고, 채널 트레이스를 유지한 채 Pawn만 무시하도록 고쳤다([LOS 문제](../../Docs/Debugging/Gameplay/03_Interaction_LOS_Pawn.md)).

소스: [DMInteractionComponent.cpp](../../Source/DontMelt/Private/Characters/Components/DMInteractionComponent.cpp)

## Hold 상호작용 세션

누르고 있어야 끝나는 상호작용은 시작·진행·완료·취소를 하나의 세션으로 묶었다. `UDMInteractionComponent`는 `OwnerSession`(시작·종료 서버 시각 포함)을 소유자에게만 복제하고, 서버에서 0.1초 주기 재검증과 완료 타이머를 돌린다.

1. 클라이언트가 `RequestId`와 함께 시작을 요청한다.
2. 서버가 대상·상태를 확인하고 세션을 만든다.
3. 진행 중 조건이 깨지거나 키를 떼면 취소한다.
4. 완료 시 서버가 유물 상태를 바꾼다.

진행바가 100%가 됐다는 것은 완료 근거가 아니다. 완료는 서버 타이머가 정한다.

소스: [DMInteractionComponent.cpp](../../Source/DontMelt/Private/Characters/Components/DMInteractionComponent.cpp)

## 예약과 Commit

두 명이 같은 유물을 거의 동시에 주우면 둘 다 성공하면 안 된다. `ADMRelicManager`가 유물 단위 예약을 관리한다.

| 단계 | 의미 |
|---|---|
| Reserve | 먼저 요청한 한 명이 유물을 점유 |
| Validate | 0.1초마다 거리·시야·예약 유효성 재검사 |
| Commit | 완료 직전 다시 확인한 뒤 실제 소유권 변경 |
| Release | 성공·실패·취소 모두 예약 해제 |

예약에 성공했다고 완료가 보장되지 않는다. 시작 조건과 완료 조건을 따로 검사하는 것이 핵심이었다.

소스: [DMRelicManager.cpp](../../Source/DontMelt/Private/Relic/DMRelicManager.cpp)

## 유물 상태와 Deferred Spawn

유물은 `EDMRelicWorldState`(`PublicSpawn`, `Carried`, `Dropped`, `Registered`, `DepositProtected`)로 상태마다 허용 행동을 다르게 둔다.

새 유물은 `SpawnActorDeferred()`로 만들고, `InitializeServer()`로 ID와 초기 상태를 넣은 뒤 `FinishSpawning()`을 호출한다. `BeginPlay`와 첫 복제 시점에 ID와 상태가 이미 채워져 있으므로, 값이 비어 있는 유물이 잠깐이라도 월드에 나오지 않는다.

소스: [DMRelicManager.cpp](../../Source/DontMelt/Private/Relic/DMRelicManager.cpp) · [DMRelic.cpp](../../Source/DontMelt/Private/Relic/DMRelic.cpp)

## 스킬 대상 선택

클라이언트는 대상 PlayerState를 보내지 않고 **선택 정책**만 보낸다. 대상을 직접 보내게 하면 원하는 상대를 고를 수 있고, 누가 플레이어인지도 드러난다.

| 정책 | 서버 처리 |
|---|---|
| `RandomEligible` | 자격 있는 후보 중 무작위 1명 |
| `LeadingEligible` | 등록 유물이 가장 많은 후보, 동률이면 그중 무작위 |

후보에서 NPC·자기 자신·사망자·스폰 보호 대상을 뺀다. 후보가 없거나 사용이 실패하면 쿨다운을 소비하지 않는다.

소스: [DMSkillComponent.cpp](../../Source/DontMelt/Private/Skills/DMSkillComponent.cpp)

## 상태 효과와 Gameplay Tag

스턴·투명화·입력 반전은 `UE_DEFINE_GAMEPLAY_TAG`로 정의한 태그(`Status.Stunned` 등)로 표현하고, `UDMCharacterStateComponent`가 원본을 가진다. 이동·전투·스킬·상호작용은 이 컴포넌트에 태그를 묻기만 한다.

태그는 상태 이름일 뿐 지속 시간과 해제 시점을 관리하지 않는다. 종료 시각과 해제(만료·사망·리스폰)는 서버 코드가 따로 처리하고, 만료 콜백에서는 액터와 현재 상태가 아직 유효한지 다시 확인한다. `ActiveStatusTags`는 소유자에게만 복제하고, 다른 사람에게 보여야 하는 스턴·투명화 연출은 표현용 bool로 따로 복제한다.

소스: [DMGameplayTags.cpp](../../Source/DontMelt/Private/Core/DMGameplayTags.cpp) · [DMCharacterStateComponent.cpp](../../Source/DontMelt/Private/Characters/Components/DMCharacterStateComponent.cpp)

[DevLog 목차](../README.md)
