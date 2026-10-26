#include "Relic/DMRelicManager.h"

#include "Core/DMMatchConstants.h"

#include "Characters/DMPlayerCharacter.h"
#include "Characters/Components/DMInteractionComponent.h"

#include "Framework/Match/DMMatchPlayerState.h"

#include "Relic/DMBaseZone.h"
#include "Relic/DMRelic.h"
#include "Relic/DMRelicChest.h"
#include "Relic/DMRelicSpawnPoint.h"

#include "Core/DMMatchTypes.h"

#include "Data/DMRelicRuleDataAsset.h"

#include "Components/CapsuleComponent.h"

#include "TimerManager.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/WorldSettings.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMRelicManager, Log, All);

namespace
{
    /*
     * Relic은 최대 5개뿐이므로
     * 1초 주기 서버 검사는 충분히 가볍다.
     */
    constexpr float RelicRecoveryCheckInterval = 1.0f;

    /*
     * 정상적으로 배치된 Relic은
     * 마지막 Commit 위치에서 움직이지 않아야 한다.
     *
     * 50cm 이상 예상치 못한 이동이 발생하면
     * 비정상 이동으로 판단한다.
     */
    constexpr float RelicMaxUnexpectedDrift = 50.0f;

    /*
     * Relic 아래에 실제 WorldStatic 바닥이 있는지
     * 검사하기 위한 범위.
     */
    constexpr float RelicFloorTraceUpDistance = 50.0f;

    constexpr float RelicFloorTraceDownDistance = 300.0f;

    /*
     * KillZ에 너무 가까운 위치도
     * 정상 위치로 인정하지 않는다.
     */
    constexpr float RelicKillZSafetyMargin = 50.0f;
}

ADMRelicManager::ADMRelicManager()
{
    PrimaryActorTick.bCanEverTick = false;

    // 서버 전용 Manager.
    bReplicates = false;
}

#pragma region Initialization

bool ADMRelicManager::InitializeMatch(
    const int32 StartingPlayerCount,
    const int32 ExpectedTotalRelicCount,
    UDMRelicRuleDataAsset* InRelicRuleData,
    TSubclassOf<ADMRelic> InRelicClass,
    const TArray<ADMMatchPlayerState*>& MatchPlayers
)
{
    if (!ensureMsgf(HasAuthority(), TEXT("InitializeMatch failed: server authority is required.")))
    {
        return false;
    }

    if (bInitialized)
    {
        UE_LOG(LogDMRelicManager, Warning, TEXT("InitializeMatch rejected: RelicManager is already initialized."));

        return false;
    }

    if (!ensureMsgf(IsValid(InRelicRuleData), TEXT("InitializeMatch failed: RelicRuleData is unavailable.")))
    {
        return false;
    }

    RelicRuleData = InRelicRuleData;

    if (!ensureMsgf(InRelicClass, TEXT("InitializeMatch failed: RelicClass is not assigned.")))
    {
        return false;
    }

    if (!ensureMsgf(
            DMMatch::IsValidPlayerCount(StartingPlayerCount),
            TEXT("InitializeMatch failed: invalid StartingPlayerCount=%d"),
            StartingPlayerCount
        ))
    {
        return false;
    }

    if (!ensureMsgf(
            MatchPlayers.Num() == StartingPlayerCount,
            TEXT("InitializeMatch failed: player count mismatch. Snapshot=%d, Players=%d"),
            StartingPlayerCount,
            MatchPlayers.Num()
        ))
    {
        return false;
    }

    if (!ensureMsgf(
            ExpectedTotalRelicCount > 0,
            TEXT("InitializeMatch failed: ExpectedTotalRelicCount is invalid. Count=%d"),
            ExpectedTotalRelicCount
        ))
    {
        return false;
    }

    RelicClass = InRelicClass;

    if (!CacheWorldActors())
    {
        return false;
    }

    FString FailureReason;

    if (!ensureMsgf(
            ValidateMatchSetup(StartingPlayerCount, MatchPlayers, FailureReason),
            TEXT("Relic world setup validation failed: %s"),
            *FailureReason
        ))
    {
        return false;
    }

    if (!SpawnInitialRelics(MatchPlayers, ExpectedTotalRelicCount, FailureReason))
    {
        UE_LOG(LogDMRelicManager, Error, TEXT("Initial Relic spawn failed: %s"), *FailureReason);

        RollbackSpawnedRelics();

        return false;
    }

    bInitialized = true;

    /*
     * Match Relic 초기화가 끝난 뒤부터
     * 서버 위치 복구 감시를 시작한다.
     */
    StartRelicRecoveryMonitor();

    UE_LOG(
        LogDMRelicManager,
        Log,
        TEXT("RelicManager initialized. Players=%d, TotalRelics=%d"),
        StartingPlayerCount,
        Relics.Num()
    );

    return true;
}

bool ADMRelicManager::CacheWorldActors()
{
    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        UE_LOG(LogDMRelicManager, Error, TEXT("CacheWorldActors failed: World is unavailable."));

        return false;
    }

    RelicChests.Reset();
    RelicSpawnPoints.Reset();

    for (TActorIterator<ADMRelicChest> It(World); It; ++It)
    {
        ADMRelicChest* Chest = *It;

        if (IsValid(Chest))
        {
            RelicChests.Add(Chest);
        }
    }

    for (TActorIterator<ADMRelicSpawnPoint> It(World); It; ++It)
    {
        ADMRelicSpawnPoint* SpawnPoint = *It;

        if (IsValid(SpawnPoint))
        {
            RelicSpawnPoints.Add(SpawnPoint);
        }
    }

    UE_LOG(
        LogDMRelicManager,
        Log,
        TEXT("Relic world actors cached. Chests=%d, SpawnPoints=%d"),
        RelicChests.Num(),
        RelicSpawnPoints.Num()
    );

    return true;
}

bool ADMRelicManager::ValidateMatchSetup(
    const int32 StartingPlayerCount, const TArray<ADMMatchPlayerState*>& MatchPlayers, FString& OutFailureReason
) const
{
    if (MatchPlayers.Num() != StartingPlayerCount)
    {
        OutFailureReason = TEXT("Starting player count mismatch.");

        return false;
    }

    TSet<int32> UsedBaseSlots;

    for (const ADMMatchPlayerState* PlayerState : MatchPlayers)
    {
        if (!IsValid(PlayerState))
        {
            OutFailureReason = TEXT("Invalid MatchPlayerState.");

            return false;
        }

        if (!PlayerState->HasAssignedBaseSlot())
        {
            OutFailureReason =
                FString::Printf(TEXT("Player has no assigned BaseSlot. PlayerName=%s"), *PlayerState->GetPlayerName());

            return false;
        }

        const int32 BaseSlot = PlayerState->GetBaseSlot();

        if (!DMMatch::IsValidBaseSlot(BaseSlot))
        {
            OutFailureReason = FString::Printf(TEXT("Invalid BaseSlot. BaseSlot=%d"), BaseSlot);

            return false;
        }

        if (UsedBaseSlots.Contains(BaseSlot))
        {
            OutFailureReason = FString::Printf(TEXT("Duplicate player BaseSlot. BaseSlot=%d"), BaseSlot);

            return false;
        }

        UsedBaseSlots.Add(BaseSlot);

        ADMRelicChest* Chest = nullptr;

        if (!FindUniqueChestByBaseSlot(BaseSlot, Chest))
        {
            OutFailureReason =
                FString::Printf(TEXT("Exactly one RelicChest is required for participating BaseSlot=%d."), BaseSlot);

            return false;
        }

        ADMBaseZone* BaseZone = Chest->GetBaseZone();

        if (!IsValid(BaseZone))
        {
            OutFailureReason = FString::Printf(TEXT("Chest has no valid BaseZone. BaseSlot=%d"), BaseSlot);

            return false;
        }

        if (BaseZone->GetBaseSlot() != BaseSlot)
        {
            OutFailureReason = FString::Printf(
                TEXT("Chest/BaseZone BaseSlot mismatch. Chest=%d, BaseZone=%d"), BaseSlot, BaseZone->GetBaseSlot()
            );

            return false;
        }

        if (!BaseZone->IsActiveBase())
        {
            OutFailureReason = FString::Printf(TEXT("Starting BaseZone is inactive. BaseSlot=%d"), BaseSlot);

            return false;
        }
    }

    ADMRelicSpawnPoint* PublicSpawnPoint = nullptr;

    if (!FindUniquePublicStartPoint(PublicSpawnPoint))
    {
        OutFailureReason = TEXT("Exactly one PublicStart RelicSpawnPoint is required.");

        return false;
    }

    return true;
}

bool ADMRelicManager::SpawnInitialRelics(
    const TArray<ADMMatchPlayerState*>& MatchPlayers, const int32 ExpectedTotalRelicCount, FString& OutFailureReason
)
{
    TArray<ADMMatchPlayerState*> SortedPlayers = MatchPlayers;

    /*
     * RelicId가 PlayerArray 내부 순서에 영향을 받지 않도록
     * BaseSlot 순서로 정렬한다.
     */
    SortedPlayers.Sort(
        [](const ADMMatchPlayerState& A, const ADMMatchPlayerState& B)
        {
            return A.GetBaseSlot() < B.GetBaseSlot();
        }
    );

    for (ADMMatchPlayerState* PlayerState : SortedPlayers)
    {
        if (!IsValid(PlayerState))
        {
            OutFailureReason = TEXT("Invalid PlayerState during Relic spawn.");

            return false;
        }

        const int32 BaseSlot = PlayerState->GetBaseSlot();

        ADMRelicChest* Chest = nullptr;

        if (!FindUniqueChestByBaseSlot(BaseSlot, Chest))
        {
            OutFailureReason = FString::Printf(TEXT("Chest lookup failed during Relic spawn. BaseSlot=%d"), BaseSlot);

            return false;
        }

        ADMRelic* RegisteredRelic = SpawnRelic(Chest->GetActorTransform(), EDMRelicWorldState::Registered, BaseSlot);

        if (!IsValid(RegisteredRelic))
        {
            OutFailureReason = FString::Printf(TEXT("Registered Relic spawn failed. BaseSlot=%d"), BaseSlot);

            return false;
        }
    }

    ADMRelicSpawnPoint* PublicSpawnPoint = nullptr;

    if (!FindUniquePublicStartPoint(PublicSpawnPoint))
    {
        OutFailureReason = TEXT("PublicStart lookup failed during Relic spawn.");

        return false;
    }

    ADMRelic* PublicRelic =
        SpawnRelic(PublicSpawnPoint->GetActorTransform(), EDMRelicWorldState::PublicSpawn, INDEX_NONE);

    if (!IsValid(PublicRelic))
    {
        OutFailureReason = TEXT("Public Relic spawn failed.");

        return false;
    }

    if (Relics.Num() != ExpectedTotalRelicCount)
    {
        OutFailureReason = FString::Printf(
            TEXT("Spawned Relic count mismatch. Expected=%d, Actual=%d"), ExpectedTotalRelicCount, Relics.Num()
        );

        return false;
    }

    return true;
}

bool ADMRelicManager::FindUniqueChestByBaseSlot(const int32 BaseSlot, ADMRelicChest*& OutChest) const
{
    OutChest = nullptr;

    int32 MatchCount = 0;

    for (ADMRelicChest* Chest : RelicChests)
    {
        if (IsValid(Chest) && Chest->GetBaseSlot() == BaseSlot)
        {
            ++MatchCount;
            OutChest = Chest;
        }
    }

    if (MatchCount != 1)
    {
        OutChest = nullptr;

        return false;
    }

    return true;
}

bool ADMRelicManager::FindUniquePublicStartPoint(ADMRelicSpawnPoint*& OutSpawnPoint) const
{
    OutSpawnPoint = nullptr;

    int32 MatchCount = 0;

    for (ADMRelicSpawnPoint* SpawnPoint : RelicSpawnPoints)
    {
        if (IsValid(SpawnPoint) && SpawnPoint->GetPurpose() == EDMRelicSpawnPurpose::PublicStart)
        {
            ++MatchCount;
            OutSpawnPoint = SpawnPoint;
        }
    }

    /*
     * 공용 시작점은 정확히 하나만 사용한다.
     */
    if (MatchCount != 1)
    {
        OutSpawnPoint = nullptr;

        return false;
    }

    return true;
}

ADMRelic* ADMRelicManager::SpawnRelic(
    const FTransform& SpawnTransform, const EDMRelicWorldState InitialState, const int32 RegisteredBaseSlot
)
{
    if (!HasAuthority() || !RelicClass)
    {
        return nullptr;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return nullptr;
    }

    ADMRelic* NewRelic = World->SpawnActorDeferred<ADMRelic>(
        RelicClass, SpawnTransform, nullptr, nullptr, ESpawnActorCollisionHandlingMethod::AlwaysSpawn
    );

    if (!IsValid(NewRelic))
    {
        return nullptr;
    }

    const int32 NewRelicId = NextRelicId++;

    NewRelic->InitializeServer(NewRelicId, InitialState, RegisteredBaseSlot, SpawnTransform);

    NewRelic->FinishSpawning(SpawnTransform);

    Relics.Add(NewRelic);

    UE_LOG(
        LogDMRelicManager,
        Log,
        TEXT("Relic spawned. Id=%d, State=%s, BaseSlot=%d"),
        NewRelicId,
        *UEnum::GetValueAsString(InitialState),
        RegisteredBaseSlot
    );

    return NewRelic;
}

void ADMRelicManager::RollbackSpawnedRelics()
{
    for (ADMRelic* Relic : Relics)
    {
        if (IsValid(Relic))
        {
            Relic->Destroy();
        }
    }

    Relics.Reset();

    NextRelicId = 0;
}

#pragma endregion

#pragma region Recovery

void ADMRelicManager::StartRelicRecoveryMonitor()
{
    if (!HasAuthority())
    {
        return;
    }

    /*
     * 혹시 기존 Timer가 존재한다면
     * 중복 등록을 방지한다.
     */
    GetWorldTimerManager().ClearTimer(RelicRecoveryCheckTimer);

    GetWorldTimerManager().SetTimer(
        RelicRecoveryCheckTimer, this, &ADMRelicManager::CheckRelicsForRecovery, RelicRecoveryCheckInterval, true
    );

    UE_LOG(LogDMRelicManager, Log, TEXT("Relic recovery monitor started. Interval=%.2f"), RelicRecoveryCheckInterval);
}

bool ADMRelicManager::IsRelicWorldTransformValid(const FTransform& Transform, FString& OutFailureReason) const
{
    OutFailureReason.Reset();

    /*
     * NaN / Infinite 같은 완전히 잘못된 Transform 방어.
     */
    if (Transform.ContainsNaN())
    {
        OutFailureReason = TEXT("Transform contains invalid values.");

        return false;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        OutFailureReason = TEXT("World is unavailable.");

        return false;
    }

    const FVector Location = Transform.GetLocation();

    /*
     * =====================================
     * KillZ 검사
     * =====================================
     */
    AWorldSettings* WorldSettings = World->GetWorldSettings();

    if (IsValid(WorldSettings) && Location.Z <= WorldSettings->KillZ + RelicKillZSafetyMargin)
    {
        OutFailureReason = TEXT("Relic is below the safe KillZ range.");

        return false;
    }

    /*
     * =====================================
     * 바닥 존재 검사
     * =====================================
     *
     * 플레이어나 다른 Relic이 아니라
     * 실제 WorldStatic 지형만 검사한다.
     */
    const FVector TraceStart = Location + FVector(0.0f, 0.0f, RelicFloorTraceUpDistance);

    const FVector TraceEnd = Location - FVector(0.0f, 0.0f, RelicFloorTraceDownDistance);

    FCollisionObjectQueryParams ObjectQueryParams;

    ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldStatic);

    FCollisionQueryParams QueryParams;

    FHitResult FloorHit;

    const bool bHasValidFloor =
        World->LineTraceSingleByObjectType(FloorHit, TraceStart, TraceEnd, ObjectQueryParams, QueryParams);

    if (!bHasValidFloor)
    {
        OutFailureReason = TEXT("No reachable WorldStatic floor exists below the Relic.");

        return false;
    }

    return true;
}

bool ADMRelicManager::HasRelicMovedUnexpectedly(const ADMRelic* Relic, float& OutDistance) const
{
    OutDistance = 0.0f;

    if (!IsValid(Relic))
    {
        return false;
    }

    const FVector CurrentLocation = Relic->GetActorLocation();

    const FVector LastValidLocation = Relic->GetLastValidWorldTransform().GetLocation();

    if (CurrentLocation.ContainsNaN() || LastValidLocation.ContainsNaN())
    {
        return true;
    }

    OutDistance = FVector::Distance(CurrentLocation, LastValidLocation);

    return OutDistance > RelicMaxUnexpectedDrift;
}

bool ADMRelicManager::FindRecoveryTransform(
    const FVector& ReferenceLocation, FTransform& OutRecoveryTransform, FString& OutRecoverySource
) const
{
    ADMRelicSpawnPoint* NearestRecoveryPoint = nullptr;

    float BestDistanceSquared = TNumericLimits<float>::Max();

    /*
     * =====================================
     * 1순위:
     * 가장 가까운 Recovery SpawnPoint
     * =====================================
     */
    for (ADMRelicSpawnPoint* SpawnPoint : RelicSpawnPoints)
    {
        if (!IsValid(SpawnPoint))
        {
            continue;
        }

        if (SpawnPoint->GetPurpose() != EDMRelicSpawnPurpose::Recovery)
        {
            continue;
        }

        FString ValidationFailureReason;

        if (!IsRelicWorldTransformValid(SpawnPoint->GetActorTransform(), ValidationFailureReason))
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared(ReferenceLocation, SpawnPoint->GetActorLocation());

        if (DistanceSquared >= BestDistanceSquared)
        {
            continue;
        }

        BestDistanceSquared = DistanceSquared;

        NearestRecoveryPoint = SpawnPoint;
    }

    if (IsValid(NearestRecoveryPoint))
    {
        OutRecoveryTransform = NearestRecoveryPoint->GetActorTransform();

        OutRecoverySource = GetNameSafe(NearestRecoveryPoint);

        return true;
    }

    /*
     * =====================================
     * 2순위:
     * 중앙 공용 fallback
     * =====================================
     *
     * 현재 프로젝트에는 별도 CentralRecovery Enum이 없고
     * PublicStart가 Match마다 정확히 하나 존재하도록
     * 이미 검증되고 있다.
     *
     * 따라서 현재 구조에서는
     * PublicStart를 중앙 fallback으로 재사용한다.
     */
    ADMRelicSpawnPoint* PublicStartPoint = nullptr;

    if (!FindUniquePublicStartPoint(PublicStartPoint) || !IsValid(PublicStartPoint))
    {
        return false;
    }

    FString PublicStartFailureReason;

    if (!IsRelicWorldTransformValid(PublicStartPoint->GetActorTransform(), PublicStartFailureReason))
    {
        return false;
    }

    OutRecoveryTransform = PublicStartPoint->GetActorTransform();

    OutRecoverySource = GetNameSafe(PublicStartPoint);

    return true;
}

bool ADMRelicManager::RecoverWorldRelic(ADMRelic* Relic, const FString& RecoveryReason)
{
    if (!HasAuthority())
    {
        return false;
    }

    if (!IsValid(Relic))
    {
        return false;
    }

    const EDMRelicWorldState State = Relic->GetWorldState();

    /*
     * 월드에 실제로 놓여 있는 Relic만
     * 위치 복구 대상이다.
     *
     * Carried / Registered / DepositProtected는
     * 월드 위치를 플레이어가 직접 사용할 상태가 아니다.
     */
    if (State != EDMRelicWorldState::PublicSpawn && State != EDMRelicWorldState::Dropped)
    {
        return false;
    }

    const FVector ReferenceLocation = Relic->GetLastValidWorldTransform().GetLocation();

    FTransform RecoveryTransform;

    FString RecoverySource;

    if (!FindRecoveryTransform(ReferenceLocation, RecoveryTransform, RecoverySource))
    {
        UE_LOG(
            LogDMRelicManager,
            Error,
            TEXT("Relic recovery failed. RelicId=%d, Reason=%s"),
            Relic->GetRelicId(),
            *RecoveryReason
        );

        return false;
    }

    /*
     * 상태는 바꾸지 않는다.
     *
     * Dropped면 계속 Dropped,
     * PublicSpawn이면 계속 PublicSpawn이다.
     */
    Relic->SetActorTransform(RecoveryTransform, false, nullptr, ETeleportType::TeleportPhysics);

    /*
     * Recovery 위치는 Level에서 직접 배치한
     * 검증된 위치이므로 새로운 LastValid로 저장한다.
     */
    Relic->SetLastValidWorldTransform(RecoveryTransform);

    Relic->ForceNetUpdate();

    UE_LOG(
        LogDMRelicManager,
        Log,
        TEXT("Relic recovered. RelicId=%d, State=%s, Reason=%s, RecoverySource=%s, Location=%s"),
        Relic->GetRelicId(),
        *UEnum::GetValueAsString(Relic->GetWorldState()),
        *RecoveryReason,
        *RecoverySource,
        *RecoveryTransform.GetLocation().ToCompactString()
    );

    return true;
}

void ADMRelicManager::CheckRelicsForRecovery()
{
    if (!HasAuthority() || !bInitialized)
    {
        return;
    }

    for (ADMRelic* Relic : Relics)
    {
        if (!IsValid(Relic))
        {
            continue;
        }

        const EDMRelicWorldState State = Relic->GetWorldState();

        /*
         * 실제 World에 공개된 Relic만 검사한다.
         */
        if (State != EDMRelicWorldState::PublicSpawn && State != EDMRelicWorldState::Dropped)
        {
            continue;
        }

        /*
         * 현재 누군가 이 Relic을 상호작용 중이라면
         * 그 프레임에 강제 이동시키지 않는다.
         *
         * Interaction 검증이 먼저 취소된 다음
         * 다음 Recovery 검사에서 처리한다.
         */
        const TWeakObjectPtr<ADMRelic> RelicKey(Relic);

        if (InteractionReservations.Find(RelicKey) != nullptr)
        {
            continue;
        }

        /*
         * =====================================
         * 1. 현재 위치 자체 검증
         * =====================================
         */
        FString LocationFailureReason;

        if (!IsRelicWorldTransformValid(Relic->GetActorTransform(), LocationFailureReason))
        {
            RecoverWorldRelic(Relic, LocationFailureReason);

            continue;
        }

        /*
         * =====================================
         * 2. 비정상 이동 검사
         * =====================================
         */
        float UnexpectedMoveDistance = 0.0f;

        if (HasRelicMovedUnexpectedly(Relic, UnexpectedMoveDistance))
        {
            const FString RecoveryReason =
                FString::Printf(TEXT("Unexpected Relic movement. Distance=%.2f"), UnexpectedMoveDistance);

            RecoverWorldRelic(Relic, RecoveryReason);
        }
    }
}

#pragma endregion

#pragma region Queries

int32 ADMRelicManager::GetRegisteredRelicCount(const int32 BaseSlot) const
{
    int32 Count = 0;

    for (const ADMRelic* Relic : Relics)
    {
        if (!IsValid(Relic))
        {
            continue;
        }

        /*
         * 해당 Base에 등록된 Relic만 계산한다.
         */
        if (Relic->GetRegisteredBaseSlot() != BaseSlot)
        {
            continue;
        }

        const EDMRelicWorldState WorldState = Relic->GetWorldState();

        /*
         * Registered:
         * 보호 시간이 끝난 정상 등록 상태.
         *
         * DepositProtected:
         * 등록은 이미 완료됐지만
         * 아직 Steal만 금지된 상태.
         *
         * 둘 다 RegisteredRelicCount에는 포함한다.
         */
        if (WorldState != EDMRelicWorldState::Registered && WorldState != EDMRelicWorldState::DepositProtected)
        {
            continue;
        }

        ++Count;
    }

    return Count;
}

void ADMRelicManager::GetActiveRelicChests(TArray<ADMRelicChest*>& OutChests) const
{
    OutChests.Reset();
    OutChests.Reserve(RelicChests.Num());

    for (ADMRelicChest* Chest : RelicChests)
    {
        if (IsValid(Chest) && Chest->IsBaseActive())
        {
            OutChests.Add(Chest);
        }
    }
}

void ADMRelicManager::GetWorldVisibleRelics(TArray<ADMRelic*>& OutRelics) const
{
    OutRelics.Reset();
    OutRelics.Reserve(Relics.Num());

    for (ADMRelic* Relic : Relics)
    {
        if (!IsValid(Relic))
        {
            continue;
        }

        const EDMRelicWorldState WorldState = Relic->GetWorldState();

        if (WorldState != EDMRelicWorldState::PublicSpawn && WorldState != EDMRelicWorldState::Dropped)
        {
            continue;
        }

        OutRelics.Add(Relic);
    }
}

bool ADMRelicManager::BuildRelicRevealEntries(TArray<FDMRelicRevealEntry>& OutEntries) const
{
    OutEntries.Reset();

    if (!HasAuthority() || !bInitialized || Relics.IsEmpty())
    {
        return false;
    }

    OutEntries.Reserve(Relics.Num());

    for (ADMRelic* Relic : Relics)
    {
        if (!IsValid(Relic))
        {
            OutEntries.Reset();
            return false;
        }

        FVector RevealLocation = FVector::ZeroVector;

        switch (Relic->GetWorldState())
        {
        case EDMRelicWorldState::PublicSpawn:
        case EDMRelicWorldState::Dropped:
        case EDMRelicWorldState::Registered:
        case EDMRelicWorldState::DepositProtected:
            // 필드와 상자 유물은 실제 Actor의 현재 서버 위치를 사용한다.
            RevealLocation = Relic->GetActorLocation();
            break;

        case EDMRelicWorldState::Carried:
        {
            // 숨겨진 유물 Actor 위치가 아니라 운반 중인 Pawn 위치를 사용한다.
            ADMMatchPlayerState* Carrier = GetRelicCarrier(Relic);
            const APawn* CarrierPawn = IsValid(Carrier) ? Carrier->GetPawn() : nullptr;

            if (!IsValid(CarrierPawn))
            {
                OutEntries.Reset();
                return false;
            }

            RevealLocation = CarrierPawn->GetActorLocation();
            break;
        }

        default:
            OutEntries.Reset();
            return false;
        }

        if (RevealLocation.ContainsNaN())
        {
            OutEntries.Reset();
            return false;
        }

        FDMRelicRevealEntry Entry;
        Entry.MarkerId = Relic->GetRelicId();
        Entry.WorldLocation = RevealLocation;
        OutEntries.Add(Entry);
    }

    // 전부 수집되지 않았다면 부분 정보가 공개되지 않도록 실패 처리한다.
    if (OutEntries.Num() != Relics.Num())
    {
        OutEntries.Reset();
        return false;
    }

    return true;
}

ADMRelic* ADMRelicManager::FindRelicById(const int32 RelicId) const
{
    for (ADMRelic* Relic : Relics)
    {
        if (IsValid(Relic) && Relic->GetRelicId() == RelicId)
        {
            return Relic;
        }
    }

    return nullptr;
}

ADMRelic* ADMRelicManager::GetCarriedRelic(ADMMatchPlayerState* PlayerState) const
{
    if (!IsValid(PlayerState))
    {
        return nullptr;
    }

    const TWeakObjectPtr<ADMMatchPlayerState> Key(PlayerState);

    const TWeakObjectPtr<ADMRelic>* FoundRelic = CarrierToRelic.Find(Key);

    if (FoundRelic == nullptr || !FoundRelic->IsValid())
    {
        return nullptr;
    }

    return FoundRelic->Get();
}

ADMMatchPlayerState* ADMRelicManager::GetRelicCarrier(ADMRelic* Relic) const
{
    if (!IsValid(Relic))
    {
        return nullptr;
    }

    const TWeakObjectPtr<ADMRelic> Key(Relic);

    const TWeakObjectPtr<ADMMatchPlayerState>* FoundCarrier = RelicToCarrier.Find(Key);

    if (FoundCarrier == nullptr || !FoundCarrier->IsValid())
    {
        return nullptr;
    }

    return FoundCarrier->Get();
}

#pragma endregion

#pragma region Interaction

ADMRelic* ADMRelicManager::FindStealableRelicForBase(
    const int32 BaseSlot, const UDMInteractionComponent* RequestingComponent
) const
{
    for (ADMRelic* Relic : Relics)
    {
        if (!IsValid(Relic))
        {
            continue;
        }

        if (Relic->GetWorldState() != EDMRelicWorldState::Registered)
        {
            continue;
        }

        if (Relic->GetRegisteredBaseSlot() != BaseSlot)
        {
            continue;
        }

        const TWeakObjectPtr<ADMRelic> Key(Relic);

        const FDMRelicInteractionReservation* Reservation = InteractionReservations.Find(Key);

        if (Reservation != nullptr && Reservation->OwnerComponent.IsValid()
            && Reservation->OwnerComponent.Get() != RequestingComponent)
        {
            continue;
        }

        return Relic;
    }

    return nullptr;
}

bool ADMRelicManager::IsSamePlayerRepickupBlocked(
    ADMMatchPlayerState* PlayerState, ADMRelic* Relic, double& OutRemainingSeconds
) const
{
    OutRemainingSeconds = 0.0;

    if (!IsValid(PlayerState) || !IsValid(Relic))
    {
        return false;
    }

    const TWeakObjectPtr<ADMRelic> Key(Relic);

    const FDMRelicRepickupRestriction* Restriction = RepickupRestrictions.Find(Key);

    if (Restriction == nullptr)
    {
        return false;
    }

    if (!Restriction->BlockedPlayerState.IsValid() || Restriction->BlockedPlayerState.Get() != PlayerState)
    {
        return false;
    }

    const UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return false;
    }

    const double ServerNow = World->GetTimeSeconds();

    if (ServerNow >= Restriction->UnlockServerTime)
    {
        return false;
    }

    OutRemainingSeconds = Restriction->UnlockServerTime - ServerNow;

    return true;
}

FTransform ADMRelicManager::BuildDropTransform(
    const ADMPlayerCharacter* CarrierCharacter, const ADMRelic* Relic, const EDMRelicDropReason DropReason
) const
{
    check(IsValid(CarrierCharacter));
    check(IsValid(Relic));

    FTransform DropTransform = Relic->GetActorTransform();

    const UCapsuleComponent* Capsule = CarrierCharacter->GetCapsuleComponent();

    const float CapsuleHalfHeight = IsValid(Capsule) ? Capsule->GetScaledCapsuleHalfHeight() : 0.0f;

    FVector DropLocation = CarrierCharacter->GetActorLocation();

    /*
     * Manual Drop은 자기 Capsule과 겹치지 않도록
     * 캐릭터 앞쪽에 둔다.
     *
     * Death / Disconnect는
     * 현재 캐릭터 위치에 떨어뜨린다.
     */
    if (DropReason == EDMRelicDropReason::Manual)
    {
        constexpr float ManualDropForwardOffset = 100.0f;

        // 벽과 유물 사이에 남길 간격. 다시 주울 때 시야 검사가 벽에 막히지 않게 한다.
        constexpr float ManualDropWallClearance = 30.0f;

        const FVector Forward = CarrierCharacter->GetActorForwardVector();
        const FVector TraceStart = CarrierCharacter->GetActorLocation();
        const FVector TraceEnd = TraceStart + Forward * ManualDropForwardOffset;

        float DropForwardDistance = ManualDropForwardOffset;

        UWorld* World = GetWorld();

        if (IsValid(World))
        {
            FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DMRelicManualDropWallCheck), false, CarrierCharacter);
            QueryParams.AddIgnoredActor(Relic);

            FHitResult WallHit;

            // 앞이 막혀 있으면 벽 속이나 벽 뒤가 아닌 벽 앞에 내려놓는다.
            if (World->LineTraceSingleByChannel(WallHit, TraceStart, TraceEnd, ECC_Visibility, QueryParams))
            {
                // 캐릭터 뒤쪽으로는 넘어가지 않게 0 이상으로 제한한다.
                DropForwardDistance = FMath::Max(0.0f, WallHit.Distance - ManualDropWallClearance);
            }
        }

        DropLocation += Forward * DropForwardDistance;
    }

    constexpr float RelicHeightAboveFloor = 15.0f;

    DropLocation.Z -= FMath::Max(0.0f, CapsuleHalfHeight - RelicHeightAboveFloor);

    DropTransform.SetLocation(DropLocation);

    const FRotator DropRotation(0.0f, CarrierCharacter->GetActorRotation().Yaw, 0.0f);

    DropTransform.SetRotation(DropRotation.Quaternion());

    return DropTransform;
}

bool ADMRelicManager::IsManualDropOverlappingRelicChest(
    const FTransform& DropTransform, const ADMRelic* Relic, ADMRelicChest*& OutBlockingChest
) const
{
    OutBlockingChest = nullptr;

    if (!IsValid(Relic))
    {
        return false;
    }

    /*
     * 현재 Relic Actor의 실제 Bounds Extent를 사용한다.
     */
    FVector CurrentRelicBoundsOrigin;
    FVector RelicBoundsExtent;

    Relic->GetActorBounds(false, CurrentRelicBoundsOrigin, RelicBoundsExtent);

    /*
     * Actor Origin과 Bounds Origin이 다를 수도 있으므로
     * 현재 Transform 기준 Local Offset을 구한 뒤
     * Drop Transform에 다시 적용한다.
     */
    const FVector RelicBoundsLocalOffset =
        Relic->GetActorTransform().InverseTransformPosition(CurrentRelicBoundsOrigin);

    const FVector CandidateRelicBoundsOrigin = DropTransform.TransformPosition(RelicBoundsLocalOffset);

    /*
     * Chest에 너무 딱 붙여서 떨어뜨리는 것도 막기 위해
     * X/Y에 약간의 여유를 둔다.
     */
    constexpr float ChestDropSafetyMargin = 15.0f;

    FVector CandidateRelicExtent = RelicBoundsExtent;

    CandidateRelicExtent.X += ChestDropSafetyMargin;

    CandidateRelicExtent.Y += ChestDropSafetyMargin;

    const FBox CandidateRelicBox(
        CandidateRelicBoundsOrigin - CandidateRelicExtent,

        CandidateRelicBoundsOrigin + CandidateRelicExtent
    );

    /*
     * Chest는 최대 4개뿐이고
     * Q Drop 순간 한 번만 실행되므로
     * 단순 순회가 가장 적절하다.
     */
    for (const TObjectPtr<ADMRelicChest>& ChestPtr : RelicChests)
    {
        ADMRelicChest* Chest = ChestPtr.Get();

        if (!IsValid(Chest))
        {
            continue;
        }

        FVector ChestBoundsOrigin;
        FVector ChestBoundsExtent;

        Chest->GetActorBounds(false, ChestBoundsOrigin, ChestBoundsExtent);

        const FBox ChestBox(
            ChestBoundsOrigin - ChestBoundsExtent,

            ChestBoundsOrigin + ChestBoundsExtent
        );

        if (!CandidateRelicBox.Intersect(ChestBox))
        {
            continue;
        }

        OutBlockingChest = Chest;

        return true;
    }

    return false;
}

bool ADMRelicManager::ResolveInteractionRequest(
    const ADMPlayerCharacter* Interactor,
    AActor* TargetActor,
    const UDMInteractionComponent* RequestingComponent,
    const bool bRequestChestAction,
    EDMInteractionType& OutInteractionType,
    float& OutDuration,
    ADMRelic*& OutReservationRelic,
    FString& OutFailureReason
) const
{
    OutDuration = 0.0f;
    OutReservationRelic = nullptr;

    if (!IsValid(Interactor))
    {
        OutFailureReason = TEXT("Interactor is invalid.");

        return false;
    }

    if (!IsValid(TargetActor))
    {
        OutFailureReason = TEXT("TargetActor is invalid.");

        return false;
    }

    if (!IsValid(RelicRuleData))
    {
        OutFailureReason = TEXT("RelicRuleData is unavailable.");

        return false;
    }

    ADMMatchPlayerState* PlayerState = Interactor->GetPlayerState<ADMMatchPlayerState>();

    if (!IsValid(PlayerState) || !PlayerState->HasAssignedBaseSlot())
    {
        OutFailureReason = TEXT("Interactor MatchPlayerState is invalid.");

        return false;
    }

    /*
     * =====================================
     * World Relic
     *
     * PublicSpawn / Dropped
     * → PickupRelic
     * =====================================
     */
    ADMRelic* Relic = Cast<ADMRelic>(TargetActor);

    if (IsValid(Relic))
    {
        if (!Relics.Contains(Relic))
        {
            OutFailureReason = TEXT("Target Relic is not registered in RelicManager.");

            return false;
        }

        if (IsPlayerCarryingRelic(PlayerState))
        {
            OutFailureReason = TEXT("Player already carries a Relic.");

            return false;
        }

        const EDMRelicWorldState State = Relic->GetWorldState();

        if (State != EDMRelicWorldState::PublicSpawn && State != EDMRelicWorldState::Dropped)
        {
            OutFailureReason = TEXT("Relic is not currently pickable from the world.");

            return false;
        }

        /*
         * 같은 플레이어가 수동 Drop한 Relic을
         * 1초 안에 다시 줍는 것을 차단한다.
         */
        double RemainingRepickupSeconds = 0.0;

        if (IsSamePlayerRepickupBlocked(PlayerState, Relic, RemainingRepickupSeconds))
        {
            OutFailureReason =
                FString::Printf(TEXT("Same-player repickup delay is active. Remaining=%.2f"), RemainingRepickupSeconds);

            return false;
        }

        OutInteractionType = EDMInteractionType::PickupRelic;

        OutDuration = RelicRuleData->GetPickupDuration();

        OutReservationRelic = Relic;

        return true;
    }

    /*
     * =====================================
     * Chest
     * =====================================
     */
    ADMRelicChest* Chest = Cast<ADMRelicChest>(TargetActor);

    if (IsValid(Chest))
    {
        /*
         * Chest ↔ BaseZone 연결 검증
         */
        if (!Chest->HasValidBaseBinding())
        {
            OutFailureReason = TEXT("Chest/BaseZone binding is invalid.");

            return false;
        }

        /*
         * Inactive Base의 Chest는
         * 상호작용할 수 없다.
         */
        if (!Chest->IsBaseActive())
        {
            OutFailureReason = TEXT("Chest belongs to an inactive Base.");

            return false;
        }

        /*
         * =====================================
         * F
         * → InspectChest
         *
         * 상자 정보 확인만 하고
         * Timed Interaction은 만들지 않는다.
         * =====================================
         */
        if (!bRequestChestAction)
        {
            OutInteractionType = EDMInteractionType::InspectChest;

            OutDuration = 0.0f;

            OutReservationRelic = nullptr;

            return true;
        }

        const int32 ChestBaseSlot = Chest->GetBaseSlot();

        const int32 PlayerBaseSlot = PlayerState->GetBaseSlot();

        /*
         * =====================================
         * Space + 자기 Chest
         * → DepositRelic
         * =====================================
         */
        if (ChestBaseSlot == PlayerBaseSlot)
        {
            ADMRelic* CarriedRelic = GetCarriedRelic(PlayerState);

            /*
             * 운반 중인 Relic이 없으면
             * 등록할 수 없다.
             */
            if (!IsValid(CarriedRelic))
            {
                OutFailureReason = TEXT("Player is not carrying a Relic.");

                return false;
            }

            /*
             * Manager의 Carrier 관계뿐 아니라
             * 실제 Relic 상태도 Carried여야 한다.
             */
            if (CarriedRelic->GetWorldState() != EDMRelicWorldState::Carried)
            {
                OutFailureReason = TEXT("Carried Relic has invalid WorldState.");

                return false;
            }

            OutInteractionType = EDMInteractionType::DepositRelic;

            OutDuration = RelicRuleData->GetDepositDuration();

            /*
             * 등록 중에는 운반 중인 Relic을
             * Reservation 대상으로 사용한다.
             *
             * 따라서 같은 Relic에 대해
             * 다른 Interaction이 동시에 Commit되는 것을 막는다.
             */
            OutReservationRelic = CarriedRelic;

            return true;
        }

        /*
         * =====================================
         * Space + 상대 Chest
         * → StealRelic
         * =====================================
         */

        /*
         * 이미 Relic 하나를 운반 중이면
         * 추가 Relic을 훔칠 수 없다.
         */
        if (IsPlayerCarryingRelic(PlayerState))
        {
            OutFailureReason = TEXT("Player already carries a Relic.");

            return false;
        }

        /*
         * 상대 Chest에 등록되어 있는
         * Steal 가능한 Relic 하나를 찾는다.
         */
        ADMRelic* StealableRelic = FindStealableRelicForBase(ChestBaseSlot, RequestingComponent);

        if (!IsValid(StealableRelic))
        {
            OutFailureReason = TEXT("No stealable Relic exists in this Chest.");

            return false;
        }

        OutInteractionType = EDMInteractionType::StealRelic;

        /*
         * 현재 RuleData에는 별도의
         * StealDuration을 두지 않는다.
         *
         * Pickup / Steal 모두 2초 규칙이므로
         * PickupDuration을 공용으로 사용한다.
         */
        OutDuration = RelicRuleData->GetPickupDuration();

        OutReservationRelic = StealableRelic;

        return true;
    }

    /*
     * =====================================
     * Unsupported Target
     * =====================================
     */
    OutFailureReason = TEXT("Unsupported interaction target.");

    return false;
}

bool ADMRelicManager::TryAcquireInteractionReservation(
    ADMRelic* Relic, UDMInteractionComponent* OwnerComponent, const int32 RequestId, int32& OutReservationId
)
{
    OutReservationId = INDEX_NONE;

    if (!IsValid(Relic) || !IsValid(OwnerComponent))
    {
        return false;
    }

    const TWeakObjectPtr<ADMRelic> Key = Relic;

    if (FDMRelicInteractionReservation* Existing = InteractionReservations.Find(Key))
    {
        if (Existing->OwnerComponent.IsValid())
        {
            UE_LOG(
                LogDMRelicManager,
                Log,
                TEXT(
                    "Relic reservation rejected. RelicId=%d, ExistingReservationId=%d, ExistingRequestId=%d, IncomingRequestId=%d, ExistingOwner=%s, IncomingOwner=%s"
                ),
                Relic->GetRelicId(),
                Existing->ReservationId,
                Existing->RequestId,
                RequestId,
                *GetNameSafe(Existing->OwnerComponent.Get()),
                *GetNameSafe(OwnerComponent)
            );

            return false;
        }

        /*
         * Owner Component가 이미 사라진
         * stale Reservation은 여기서 제거한다.
         */
        InteractionReservations.Remove(Key);
    }

    if (NextReservationId <= 0)
    {
        NextReservationId = 1;
    }

    const int32 NewReservationId = NextReservationId++;

    FDMRelicInteractionReservation Reservation;

    Reservation.OwnerComponent = OwnerComponent;

    Reservation.ReservationId = NewReservationId;

    Reservation.RequestId = RequestId;

    InteractionReservations.Add(Key, Reservation);

    OutReservationId = NewReservationId;

    UE_LOG(
        LogDMRelicManager,
        Log,
        TEXT("Relic reservation acquired. RelicId=%d, ReservationId=%d, RequestId=%d, Owner=%s"),
        Relic->GetRelicId(),
        NewReservationId,
        RequestId,
        *GetNameSafe(OwnerComponent)
    );

    return true;
}

bool ADMRelicManager::IsInteractionReservationOwnedBy(
    ADMRelic* Relic, const UDMInteractionComponent* OwnerComponent, const int32 ReservationId
) const
{
    if (!IsValid(Relic) || !IsValid(OwnerComponent) || ReservationId == INDEX_NONE)
    {
        return false;
    }

    const TWeakObjectPtr<ADMRelic> Key = Relic;

    const FDMRelicInteractionReservation* Reservation = InteractionReservations.Find(Key);

    if (Reservation == nullptr)
    {
        return false;
    }

    return Reservation->OwnerComponent.Get() == OwnerComponent && Reservation->ReservationId == ReservationId;
}

void ADMRelicManager::ReleaseInteractionReservation(
    ADMRelic* Relic, UDMInteractionComponent* OwnerComponent, const int32 ReservationId
)
{
    if (!IsValid(Relic) || OwnerComponent == nullptr)
    {
        return;
    }

    const TWeakObjectPtr<ADMRelic> Key = Relic;

    const FDMRelicInteractionReservation* Reservation = InteractionReservations.Find(Key);

    if (Reservation == nullptr)
    {
        return;
    }

    if (Reservation->OwnerComponent.Get() != OwnerComponent || Reservation->ReservationId != ReservationId)
    {
        return;
    }

    const int32 ReleasedRequestId = Reservation->RequestId;

    InteractionReservations.Remove(Key);

    UE_LOG(
        LogDMRelicManager,
        Log,
        TEXT("Relic reservation released. RelicId=%d, ReservationId=%d, RequestId=%d, Owner=%s"),
        Relic->GetRelicId(),
        ReservationId,
        ReleasedRequestId,
        *GetNameSafe(OwnerComponent)
    );
}

bool ADMRelicManager::TryInspectChest(
    const ADMPlayerCharacter* Interactor,
    const ADMRelicChest* Chest,
    int32& OutBaseSlot,
    int32& OutRegisteredRelicCount,
    FString& OutFailureReason
) const
{
    OutBaseSlot = INDEX_NONE;

    OutRegisteredRelicCount = 0;

    if (!HasAuthority())
    {
        OutFailureReason = TEXT("Server authority is required.");

        return false;
    }

    if (!IsValid(Interactor))
    {
        OutFailureReason = TEXT("Interactor is invalid.");

        return false;
    }

    if (!IsValid(Chest))
    {
        OutFailureReason = TEXT("Chest is invalid.");

        return false;
    }

    if (!Chest->HasValidBaseBinding())
    {
        OutFailureReason = TEXT("Chest/BaseZone binding is invalid.");

        return false;
    }

    if (!Chest->IsBaseActive())
    {
        OutFailureReason = TEXT("Chest belongs to an inactive Base.");

        return false;
    }

    const int32 BaseSlot = Chest->GetBaseSlot();

    OutBaseSlot = BaseSlot;

    OutRegisteredRelicCount = GetRegisteredRelicCount(BaseSlot);

    UE_LOG(
        LogDMRelicManager,
        Log,
        TEXT("Chest inspected. BaseSlot=%d, RegisteredRelics=%d"),
        OutBaseSlot,
        OutRegisteredRelicCount
    );

    return true;
}

bool ADMRelicManager::TryCommitRelicAcquisition(
    ADMPlayerCharacter* Interactor,
    ADMRelic* Relic,
    const EDMInteractionType InteractionType,
    const ADMRelicChest* SourceChest,
    FString& OutFailureReason
)
{
    if (!HasAuthority())
    {
        OutFailureReason = TEXT("Server authority is required.");

        return false;
    }

    if (!bInitialized)
    {
        OutFailureReason = TEXT("RelicManager is not initialized.");

        return false;
    }

    if (!IsValid(Interactor))
    {
        OutFailureReason = TEXT("Interactor is invalid.");

        return false;
    }

    if (!IsValid(Relic) || !Relics.Contains(Relic))
    {
        OutFailureReason = TEXT("Relic is invalid or not registered.");

        return false;
    }

    ADMMatchPlayerState* PlayerState = Interactor->GetPlayerState<ADMMatchPlayerState>();

    if (!IsValid(PlayerState) || !PlayerState->HasAssignedBaseSlot())
    {
        OutFailureReason = TEXT("Interactor MatchPlayerState is invalid.");

        return false;
    }

    /*
     * 플레이어당 최대 하나.
     */
    if (IsPlayerCarryingRelic(PlayerState))
    {
        OutFailureReason = TEXT("Player already carries a Relic.");

        return false;
    }

    /*
     * 같은 Relic이 이미 다른 Carrier와 연결되어 있는지도
     * 최종 Commit 순간 다시 검사한다.
     */
    if (GetRelicCarrier(Relic) != nullptr)
    {
        OutFailureReason = TEXT("Relic already has a Carrier.");

        return false;
    }

    switch (InteractionType)
    {
    case EDMInteractionType::PickupRelic:
    {
        const EDMRelicWorldState State = Relic->GetWorldState();

        if (State != EDMRelicWorldState::PublicSpawn && State != EDMRelicWorldState::Dropped)
        {
            OutFailureReason = TEXT("Relic is no longer pickable from the world.");

            return false;
        }

        break;
    }

    case EDMInteractionType::StealRelic:
    {
        if (!IsValid(SourceChest))
        {
            OutFailureReason = TEXT("Steal source Chest is invalid.");

            return false;
        }

        if (!SourceChest->HasValidBaseBinding() || !SourceChest->IsBaseActive())
        {
            OutFailureReason = TEXT("Steal source Chest is unavailable.");

            return false;
        }

        if (SourceChest->GetBaseSlot() == PlayerState->GetBaseSlot())
        {
            OutFailureReason = TEXT("Cannot steal from own Chest.");

            return false;
        }

        if (Relic->GetWorldState() != EDMRelicWorldState::Registered)
        {
            OutFailureReason = TEXT("Relic is no longer Registered.");

            return false;
        }

        if (Relic->GetRegisteredBaseSlot() != SourceChest->GetBaseSlot())
        {
            OutFailureReason = TEXT("Relic no longer belongs to the source Chest.");

            return false;
        }

        break;
    }

    default:
    {
        OutFailureReason = TEXT("Unsupported acquisition type.");

        return false;
    }
    }

    /*
     * ==============================
     * 단 하나의 실제 Acquisition Commit
     * ==============================
     */

    const int32 PreviousBaseSlot = Relic->GetRegisteredBaseSlot();

    Relic->SetRegisteredBaseSlot(INDEX_NONE);

    Relic->SetWorldState(EDMRelicWorldState::Carried);

    RelicToCarrier.Add(Relic, PlayerState);

    CarrierToRelic.Add(PlayerState, Relic);

    /*
     * 이전 Drop의 재습득 제한은
     * 성공적으로 누군가 습득한 순간 더 이상 필요 없다.
     */
    RepickupRestrictions.Remove(Relic);

    /*
     * Owner HUD용 Projection.
     * Carrier 관계의 SSOT는 여전히 Manager Map이다.
     */
    PlayerState->SetCarryingRelicProjection(true);

    /*
     * 결과표용 누적 획득 수 집계.
     * Pickup·Steal 획득 Commit이 성공한 시점에만 1 증가한다.
     */
    PlayerState->IncrementAcquiredRelicCount();

    UE_LOG(
        LogDMRelicManager,
        Log,
        TEXT("Relic acquisition committed. RelicId=%d, Type=%s, Carrier=%s, PreviousBaseSlot=%d"),
        Relic->GetRelicId(),
        *UEnum::GetValueAsString(InteractionType),
        *GetNameSafe(PlayerState),
        PreviousBaseSlot
    );

    /*
     * Steal Commit이 실제로 성공한 뒤에만
     * Match 전체 알림을 발생시킨다.
     *
     * Pickup에서는 발생시키지 않는다.
     */
    if (InteractionType == EDMInteractionType::StealRelic)
    {
        RelicStolenDelegate.ExecuteIfBound(PlayerState);
    }

    return true;
}

bool ADMRelicManager::TryCommitRelicDeposit(
    ADMPlayerCharacter* Interactor, ADMRelic* Relic, const ADMRelicChest* DestinationChest, FString& OutFailureReason
)
{
    if (!HasAuthority())
    {
        OutFailureReason = TEXT("Server authority is required.");

        return false;
    }

    if (!bInitialized)
    {
        OutFailureReason = TEXT("RelicManager is not initialized.");

        return false;
    }

    if (!IsValid(Interactor))
    {
        OutFailureReason = TEXT("Interactor is invalid.");

        return false;
    }

    if (!IsValid(Relic))
    {
        OutFailureReason = TEXT("Relic is invalid.");

        return false;
    }

    if (!IsValid(DestinationChest))
    {
        OutFailureReason = TEXT("Destination Chest is invalid.");

        return false;
    }

    if (!DestinationChest->HasValidBaseBinding())
    {
        OutFailureReason = TEXT("Chest/BaseZone binding is invalid.");

        return false;
    }

    if (!DestinationChest->IsBaseActive())
    {
        OutFailureReason = TEXT("Chest belongs to an inactive Base.");

        return false;
    }

    ADMMatchPlayerState* PlayerState = Interactor->GetPlayerState<ADMMatchPlayerState>();

    if (!IsValid(PlayerState))
    {
        OutFailureReason = TEXT("MatchPlayerState is invalid.");

        return false;
    }

    /*
     * 등록 완료 순간 Alive여야 한다.
     */
    if (PlayerState->GetLifeState() != EDMPlayerLifeState::Alive)
    {
        OutFailureReason = TEXT("Deposit requires Alive state.");

        return false;
    }

    const int32 PlayerBaseSlot = PlayerState->GetBaseSlot();

    const int32 ChestBaseSlot = DestinationChest->GetBaseSlot();

    /*
     * 상대 Chest에 등록할 수 없다.
     */
    if (ChestBaseSlot != PlayerBaseSlot)
    {
        OutFailureReason = TEXT("Relic can only be deposited into the player's own Chest.");

        return false;
    }

    /*
     * Manager Carrier SSOT 검증.
     */
    if (GetCarriedRelic(PlayerState) != Relic)
    {
        OutFailureReason = TEXT("Player does not carry the requested Relic.");

        return false;
    }

    if (GetRelicCarrier(Relic) != PlayerState)
    {
        OutFailureReason = TEXT("Relic carrier relation is inconsistent.");

        return false;
    }

    if (Relic->GetWorldState() != EDMRelicWorldState::Carried)
    {
        OutFailureReason = TEXT("Relic is not in Carried state.");

        return false;
    }

    /*
     * =====================================
     * 실제 Deposit Commit
     * =====================================
     */

    RelicToCarrier.Remove(Relic);

    CarrierToRelic.Remove(PlayerState);

    /*
     * 등록된 Relic의 authoritative 위치도
     * Chest 위치로 이동한다.
     *
     * 이후 Steal / Reveal / Recovery에서도
     * 올바른 위치를 기준으로 사용할 수 있다.
     */
    const FTransform RegisteredTransform = DestinationChest->GetActorTransform();

    Relic->SetActorTransform(RegisteredTransform, false, nullptr, ETeleportType::TeleportPhysics);

    Relic->SetLastValidWorldTransform(RegisteredTransform);

    Relic->SetRegisteredBaseSlot(ChestBaseSlot);

    /*
     * 혹시 남아 있는 이전 Drop restriction 정리.
     */
    RepickupRestrictions.Remove(Relic);

    PlayerState->SetCarryingRelicProjection(false);

    /*
     * 등록 자체는 이 순간 완료된다.
     *
     * 하지만 등록 직후 일정 시간 동안은
     * 상대 플레이어가 다시 Steal할 수 없도록
     * DepositProtected 상태로 진입한다.
     */
    Relic->SetWorldState(EDMRelicWorldState::DepositProtected);

    /*
     * 서버에서 등록 보호 Timer를 시작한다.
     *
     * 보호 시간이 끝나면:
     *
     * DepositProtected
     *      ↓
     * Registered
     *
     * 로 전환된다.
     */
    StartDepositProtection(Relic);

    const int32 RegisteredRelicCount = GetRegisteredRelicCount(ChestBaseSlot);

    UE_LOG(
        LogDMRelicManager,
        Log,
        TEXT("Relic deposit committed. RelicId=%d, BaseSlot=%d, RegisteredRelics=%d, Depositor=%s"),
        Relic->GetRelicId(),
        ChestBaseSlot,
        RegisteredRelicCount,
        *GetNameSafe(PlayerState)
    );

    /*
     * Relic 상태 변경이 전부 Commit된 뒤에만
     * GameMode에게 Match 결과 재평가를 요청한다.
     *
     * 여기서 RelicManager가 직접
     * TryFinishMatch()를 호출하지 않는다.
     */
    RelicDepositCommittedDelegate.ExecuteIfBound();

    return true;
}

void ADMRelicManager::StartDepositProtection(ADMRelic* Relic)
{
    if (!HasAuthority())
    {
        return;
    }

    if (!IsValid(Relic) || !IsValid(RelicRuleData))
    {
        return;
    }

    /*
     * Deposit Commit 직후에만
     * 보호 Timer를 시작할 수 있다.
     */
    if (Relic->GetWorldState() != EDMRelicWorldState::DepositProtected)
    {
        return;
    }

    const float ProtectionDuration = RelicRuleData->GetDepositProtectionDuration();

    /*
     * RuleData에서 보호 시간이 0 이하라면
     * 보호 상태를 만들 필요가 없다.
     */
    if (ProtectionDuration <= 0.0f)
    {
        FinishDepositProtection(TWeakObjectPtr<ADMRelic>(Relic));

        return;
    }

    const TWeakObjectPtr<ADMRelic> RelicKey(Relic);

    /*
     * 동일 Relic에 기존 Timer가 혹시 남아 있다면
     * 제거하고 새 Timer를 사용한다.
     */
    FTimerHandle& TimerHandle = DepositProtectionTimers.FindOrAdd(RelicKey);

    GetWorldTimerManager().ClearTimer(TimerHandle);

    FTimerDelegate TimerDelegate;

    TimerDelegate.BindUObject(this, &ADMRelicManager::FinishDepositProtection, RelicKey);

    GetWorldTimerManager().SetTimer(TimerHandle, TimerDelegate, ProtectionDuration, false);

    UE_LOG(
        LogDMRelicManager,
        Log,
        TEXT("Relic deposit protection started. RelicId=%d, BaseSlot=%d, Duration=%.2f"),
        Relic->GetRelicId(),
        Relic->GetRegisteredBaseSlot(),
        ProtectionDuration
    );
}

void ADMRelicManager::FinishDepositProtection(TWeakObjectPtr<ADMRelic> RelicPtr)
{
    if (!HasAuthority())
    {
        return;
    }

    /*
     * Timer 관리 데이터부터 제거한다.
     *
     * 실제 Gameplay State는 Relic의 WorldState가
     * SSOT이므로 이 Map에는 상태를 저장하지 않는다.
     */
    DepositProtectionTimers.Remove(RelicPtr);

    ADMRelic* Relic = RelicPtr.Get();

    if (!IsValid(Relic))
    {
        return;
    }

    /*
     * Timer가 끝났더라도 현재 Relic 상태가
     * DepositProtected가 아니라면
     * 강제로 Registered로 덮어쓰지 않는다.
     */
    if (Relic->GetWorldState() != EDMRelicWorldState::DepositProtected)
    {
        return;
    }

    Relic->SetWorldState(EDMRelicWorldState::Registered);

    UE_LOG(
        LogDMRelicManager,
        Log,
        TEXT("Relic deposit protection expired. RelicId=%d, BaseSlot=%d, State=%s"),
        Relic->GetRelicId(),
        Relic->GetRegisteredBaseSlot(),
        *UEnum::GetValueAsString(Relic->GetWorldState())
    );
}

bool ADMRelicManager::TryDropCarriedRelic(
    ADMMatchPlayerState* CarrierPlayerState,
    ADMPlayerCharacter* CarrierCharacter,
    const EDMRelicDropReason DropReason,
    FString& OutFailureReason
)
{
    if (!HasAuthority())
    {
        OutFailureReason = TEXT("Server authority is required.");

        return false;
    }

    if (!bInitialized)
    {
        OutFailureReason = TEXT("RelicManager is not initialized.");

        return false;
    }

    if (!IsValid(RelicRuleData))
    {
        OutFailureReason = TEXT("RelicRuleData is unavailable.");

        return false;
    }

    if (!IsValid(CarrierPlayerState))
    {
        OutFailureReason = TEXT("Carrier PlayerState is invalid.");

        return false;
    }

    if (!IsValid(CarrierCharacter))
    {
        OutFailureReason = TEXT("Carrier Character is invalid.");

        return false;
    }

    /*
     * 수동 Q Drop은 살아 있는 플레이어만 가능.
     *
     * Death / Disconnect는 서버 생명주기 처리에서
     * 호출되므로 Alive 검사를 적용하지 않는다.
     */
    if (DropReason == EDMRelicDropReason::Manual && CarrierPlayerState->GetLifeState() != EDMPlayerLifeState::Alive)
    {
        OutFailureReason = TEXT("Manual Drop requires Alive state.");

        return false;
    }

    ADMRelic* Relic = GetCarriedRelic(CarrierPlayerState);

    if (!IsValid(Relic))
    {
        OutFailureReason = TEXT("Player is not carrying a Relic.");

        return false;
    }

    if (GetRelicCarrier(Relic) != CarrierPlayerState)
    {
        OutFailureReason = TEXT("Carrier map relation is inconsistent.");

        return false;
    }

    if (Relic->GetWorldState() != EDMRelicWorldState::Carried)
    {
        OutFailureReason = TEXT("Carried Relic has invalid WorldState.");

        return false;
    }

    FTransform FinalDropTransform = BuildDropTransform(CarrierCharacter, Relic, DropReason);

    FString DropLocationFailureReason;

    /*
     * 계산된 Drop 위치가 정상이라면 그대로 사용한다.
     *
     * 비정상이라면 Drop 자체를 실패시키는 것이 아니라
     * 검증된 Recovery SpawnPoint로 이동시킨다.
     */
    if (!IsRelicWorldTransformValid(FinalDropTransform, DropLocationFailureReason))
    {
        FVector RecoveryReferenceLocation = FinalDropTransform.GetLocation();

        /*
         * 계산된 위치 자체가 NaN이라면
         * Carrier 위치를 Recovery 검색 기준으로 사용한다.
         */
        if (RecoveryReferenceLocation.ContainsNaN())
        {
            RecoveryReferenceLocation = CarrierCharacter->GetActorLocation();
        }

        FString RecoverySource;

        if (!FindRecoveryTransform(RecoveryReferenceLocation, FinalDropTransform, RecoverySource))
        {
            OutFailureReason = FString::Printf(
                TEXT("Drop location is invalid and no RecoverySpawnPoint is available. Reason=%s"),
                *DropLocationFailureReason
            );

            return false;
        }

        UE_LOG(
            LogDMRelicManager,
            Log,
            TEXT(
                "Invalid Relic drop location replaced with RecoverySpawnPoint. RelicId=%d, DropReason=%s, InvalidReason=%s, RecoverySource=%s"
            ),
            Relic->GetRelicId(),
            *UEnum::GetValueAsString(DropReason),
            *DropLocationFailureReason,
            *RecoverySource
        );
    }

    /*
     * Q 수동 Drop은 Chest와 겹치는 위치에는
     * 허용하지 않는다.
     *
     * Drop을 실패시키므로 Carrier 관계와
     * Carry Projection은 그대로 유지된다.
     */
    if (DropReason == EDMRelicDropReason::Manual)
    {
        ADMRelicChest* BlockingChest = nullptr;

        if (IsManualDropOverlappingRelicChest(FinalDropTransform, Relic, BlockingChest))
        {
            OutFailureReason = TEXT("Manual Drop is blocked near a Relic Chest.");

            UE_LOG(
                LogDMRelicManager,
                Log,
                TEXT("Manual Relic Drop rejected: drop position overlaps Relic Chest. RelicId=%d, Chest=%s"),
                Relic->GetRelicId(),
                *GetNameSafe(BlockingChest)
            );

            return false;
        }
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        OutFailureReason = TEXT("World is unavailable.");

        return false;
    }

    /*
     * =====================================
     * 단 하나의 실제 Drop Commit
     * =====================================
     */

    RelicToCarrier.Remove(Relic);

    CarrierToRelic.Remove(CarrierPlayerState);

    Relic->SetRegisteredBaseSlot(INDEX_NONE);

    Relic->SetActorTransform(FinalDropTransform, false, nullptr, ETeleportType::TeleportPhysics);

    Relic->SetLastValidWorldTransform(FinalDropTransform);

    /*
     * 수동 Drop만 같은 플레이어 1초 재습득 제한.
     *
     * 사망자는 즉시 다시 주울 수 없고,
     * 연결 종료 플레이어도 존재하지 않으므로
     * 별도 restriction을 만들 필요가 없다.
     */
    if (DropReason == EDMRelicDropReason::Manual)
    {
        FDMRelicRepickupRestriction Restriction;

        Restriction.BlockedPlayerState = CarrierPlayerState;

        Restriction.UnlockServerTime = World->GetTimeSeconds() + RelicRuleData->GetSamePlayerRepickupDelay();

        RepickupRestrictions.Add(Relic, Restriction);
    }
    else
    {
        RepickupRestrictions.Remove(Relic);
    }

    CarrierPlayerState->SetCarryingRelicProjection(false);

    /*
     * 마지막에 Dropped로 변경하여
     * Transform/Carrier 상태 정리가 끝난 후
     * World에 다시 보이도록 한다.
     */
    Relic->SetWorldState(EDMRelicWorldState::Dropped);

    UE_LOG(
        LogDMRelicManager,
        Log,
        TEXT("Relic drop committed. RelicId=%d, Reason=%s, Carrier=%s, Location=%s"),
        Relic->GetRelicId(),
        *UEnum::GetValueAsString(DropReason),
        *GetNameSafe(CarrierPlayerState),
        *FinalDropTransform.GetLocation().ToCompactString()
    );

    return true;
}

#pragma endregion