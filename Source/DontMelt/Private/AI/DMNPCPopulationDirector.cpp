#include "AI/DMNPCPopulationDirector.h"

#include "Characters/DMNPCCharacter.h"
#include "Characters/DMJellyAIController.h"
#include "Framework/Match/DMMatchGameMode.h"

#include "Engine/TargetPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMNPCPopulationDirector, Log, All);

namespace DMNPCPopulationDirectorPrivate
{
    const FName NPCSpawnPointTag = TEXT("DM_NPCSpawn");

    struct FReactionCandidate
    {
        ADMNPCCharacter* NPC = nullptr;

        float DistanceSquared = 0.0f;
    };
}

#pragma region Initialization

bool UDMNPCPopulationDirector::InitializePopulation(
    const int32 InTargetNPCCount, TSubclassOf<ADMNPCCharacter> InNPCClass
)
{
    ADMMatchGameMode* MatchGameMode = GetTypedOuter<ADMMatchGameMode>();

    if (!ensureMsgf(
            IsValid(MatchGameMode) && MatchGameMode->HasAuthority(),
            TEXT("InitializePopulation failed: valid authoritative MatchGameMode outer is required.")
        ))
    {
        return false;
    }

    if (!ensureMsgf(
            InTargetNPCCount > 0 && InTargetNPCCount <= 60,
            TEXT("InitializePopulation failed: invalid TargetNPCCount=%d"),
            InTargetNPCCount
        ))
    {
        return false;
    }

    if (!ensureMsgf(InNPCClass, TEXT("InitializePopulation failed: NPCClass is invalid.")))
    {
        return false;
    }

    if (RegisteredNPCs.Num() > 0)
    {
        UE_LOG(
            LogDMNPCPopulationDirector,
            Warning,
            TEXT("InitializePopulation rejected: population is already initialized.")
        );

        return false;
    }

    UWorld* World = MatchGameMode->GetWorld();

    if (!IsValid(World))
    {
        return false;
    }

    TArray<ATargetPoint*> SpawnPoints;

    if (!CollectSpawnPoints(SpawnPoints))
    {
        return false;
    }

    // 인원이 적어도 매 경기 같은 구역에만 NPC가 몰리지 않도록 스폰 지점 순서를 섞는다.
    for (int32 Index = SpawnPoints.Num() - 1; Index > 0; --Index)
    {
        SpawnPoints.Swap(Index, FMath::RandRange(0, Index));
    }

    if (!ensureMsgf(
            SpawnPoints.Num() >= InTargetNPCCount,
            TEXT("InitializePopulation failed: not enough NPC spawn points. Required=%d, Found=%d"),
            InTargetNPCCount,
            SpawnPoints.Num()
        ))
    {
        return false;
    }

    TargetNPCCount = InTargetNPCCount;

    RegisteredNPCs.Reserve(TargetNPCCount);

    FActorSpawnParameters SpawnParameters;

    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AdjustIfPossibleButAlwaysSpawn;

    for (int32 NPCIndex = 0; NPCIndex < TargetNPCCount; ++NPCIndex)
    {
        ATargetPoint* SpawnPoint = SpawnPoints[NPCIndex];

        if (!ensureMsgf(
                IsValid(SpawnPoint), TEXT("InitializePopulation failed: invalid SpawnPoint. Index=%d"), NPCIndex
            ))
        {
            ShutdownPopulation();

            return false;
        }

        ADMNPCCharacter* SpawnedNPC =
            World->SpawnActor<ADMNPCCharacter>(InNPCClass, SpawnPoint->GetActorTransform(), SpawnParameters);

        if (!IsValid(SpawnedNPC))
        {
            UE_LOG(
                LogDMNPCPopulationDirector,
                Error,
                TEXT("InitializePopulation failed: NPC spawn failed. Index=%d, SpawnPoint=%s"),
                NPCIndex,
                *GetNameSafe(SpawnPoint)
            );

            ShutdownPopulation();

            return false;
        }

        RegisteredNPCs.Add(SpawnedNPC);
    }

    const int32 RegisteredCount = GetRegisteredNPCCount();

    if (RegisteredCount != TargetNPCCount)
    {
        UE_LOG(
            LogDMNPCPopulationDirector,
            Error,
            TEXT("InitializePopulation failed: registry count mismatch. Target=%d, Registered=%d"),
            TargetNPCCount,
            RegisteredCount
        );

        ShutdownPopulation();

        return false;
    }

    UE_LOG(
        LogDMNPCPopulationDirector,
        Log,
        TEXT("NPC population initialized. Target=%d, Registered=%d, AvailableSpawnPoints=%d"),
        TargetNPCCount,
        RegisteredCount,
        SpawnPoints.Num()
    );

    return true;
}

void UDMNPCPopulationDirector::ShutdownPopulation()
{
    DestroyRegisteredNPCs();

    TargetNPCCount = 0;
}

#pragma endregion

#pragma region Query

int32 UDMNPCPopulationDirector::GetRegisteredNPCCount() const
{
    int32 ValidNPCCount = 0;

    for (const TWeakObjectPtr<ADMNPCCharacter>& NPCPointer : RegisteredNPCs)
    {
        if (NPCPointer.IsValid())
        {
            ++ValidNPCCount;
        }
    }

    return ValidNPCCount;
}

#pragma endregion

#pragma region Reaction

void UDMNPCPopulationDirector::HandleWrongTargetAttack(
    const FVector& AttackOrigin,
    const TArray<ADMNPCCharacter*>& HitNPCTargets,
    const float ReactionRadius,
    const int32 MaxNearbyResponderCount,
    const float FleeDistance
)
{
    ADMMatchGameMode* MatchGameMode = GetTypedOuter<ADMMatchGameMode>();

    if (!IsValid(MatchGameMode) || !MatchGameMode->HasAuthority())
    {
        return;
    }

    if (AttackOrigin.ContainsNaN() || ReactionRadius <= 0.0f || MaxNearbyResponderCount < 0 || FleeDistance <= 0.0f)
    {
        return;
    }

    TArray<ADMNPCCharacter*> ValidHitNPCs;

    TSet<ADMNPCCharacter*> DirectHitNPCSet;

    for (ADMNPCCharacter* HitNPC : HitNPCTargets)
    {
        if (!IsValid(HitNPC) || DirectHitNPCSet.Contains(HitNPC))
        {
            continue;
        }

        DirectHitNPCSet.Add(HitNPC);

        ValidHitNPCs.Add(HitNPC);
    }

    if (ValidHitNPCs.Num() <= 0)
    {
        return;
    }

    /*
     * 직접 맞은 NPC는
     * Radius / MaxResponder 제한과 무관하게
     * 반드시 반응한다.
     */
    int32 DirectReactionCount = 0;

    for (ADMNPCCharacter* HitNPC : ValidHitNPCs)
    {
        if (DispatchWrongTargetReaction(HitNPC, AttackOrigin, FleeDistance))
        {
            ++DirectReactionCount;
        }
    }

    /*
     * 직접 맞은 NPC 외 주변 NPC 후보.
     *
     * World 전체를 찾지 않고
     * Director Registry만 순회한다.
     */
    TArray<DMNPCPopulationDirectorPrivate::FReactionCandidate> NearbyCandidates;

    const float ReactionRadiusSquared = FMath::Square(ReactionRadius);

    for (const TWeakObjectPtr<ADMNPCCharacter>& NPCPointer : RegisteredNPCs)
    {
        ADMNPCCharacter* NPC = NPCPointer.Get();

        if (!IsValid(NPC) || DirectHitNPCSet.Contains(NPC))
        {
            continue;
        }

        float NearestHitDistanceSquared = TNumericLimits<float>::Max();

        for (const ADMNPCCharacter* HitNPC : ValidHitNPCs)
        {
            const float DistanceSquared = FVector::DistSquared2D(NPC->GetActorLocation(), HitNPC->GetActorLocation());

            NearestHitDistanceSquared = FMath::Min(NearestHitDistanceSquared, DistanceSquared);
        }

        if (NearestHitDistanceSquared > ReactionRadiusSquared)
        {
            continue;
        }

        DMNPCPopulationDirectorPrivate::FReactionCandidate Candidate;

        Candidate.NPC = NPC;

        Candidate.DistanceSquared = NearestHitDistanceSquared;

        NearbyCandidates.Add(Candidate);
    }

    /*
     * 가장 가까운 목격자부터 선택한다.
     *
     * 랜덤보다 반응이 자연스럽고
     * 결과가 Debug하기 쉽다.
     */
    NearbyCandidates.Sort(
        [](const DMNPCPopulationDirectorPrivate::FReactionCandidate& A,
           const DMNPCPopulationDirectorPrivate::FReactionCandidate& B)
        {
            return A.DistanceSquared < B.DistanceSquared;
        }
    );

    const int32 NearbyReactionLimit = FMath::Min(MaxNearbyResponderCount, NearbyCandidates.Num());

    int32 NearbyReactionCount = 0;

    for (int32 Index = 0; Index < NearbyReactionLimit; ++Index)
    {
        ADMNPCCharacter* NPC = NearbyCandidates[Index].NPC;

        if (DispatchWrongTargetReaction(NPC, AttackOrigin, FleeDistance))
        {
            ++NearbyReactionCount;
        }
    }

    UE_LOG(
        LogDMNPCPopulationDirector,
        Log,
        TEXT("Wrong-target NPC reaction dispatched. Direct=%d, Nearby=%d, NearbyCandidates=%d, Radius=%.1f"),
        DirectReactionCount,
        NearbyReactionCount,
        NearbyCandidates.Num(),
        ReactionRadius
    );
}

bool UDMNPCPopulationDirector::DispatchWrongTargetReaction(
    ADMNPCCharacter* NPCCharacter, const FVector& AttackOrigin, const float FleeDistance
) const
{
    if (!IsValid(NPCCharacter))
    {
        return false;
    }

    ADMJellyAIController* AIController = Cast<ADMJellyAIController>(NPCCharacter->GetController());

    if (!IsValid(AIController))
    {
        return false;
    }

    AIController->HandleWrongTargetReaction(AttackOrigin, FleeDistance);

    return true;
}

void UDMNPCPopulationDirector::HandlePanicEvent()
{
    ADMMatchGameMode* MatchGameMode = GetTypedOuter<ADMMatchGameMode>();

    if (!IsValid(MatchGameMode) || !MatchGameMode->HasAuthority())
    {
        return;
    }

    int32 PanicDispatchCount = 0;

    for (const TWeakObjectPtr<ADMNPCCharacter>& NPCPointer : RegisteredNPCs)
    {
        ADMNPCCharacter* NPC = NPCPointer.Get();

        if (!IsValid(NPC))
        {
            continue;
        }

        ADMJellyAIController* AIController = Cast<ADMJellyAIController>(NPC->GetController());

        if (!IsValid(AIController))
        {
            continue;
        }

        AIController->HandlePanicReaction();

        ++PanicDispatchCount;
    }

    UE_LOG(
        LogDMNPCPopulationDirector,
        Log,
        TEXT("NPC Panic event dispatched. Registered=%d, Dispatched=%d"),
        GetRegisteredNPCCount(),
        PanicDispatchCount
    );
}

void UDMNPCPopulationDirector::HandleNearbyPlayerDeath(
    const FVector& DeathLocation,
    const float ReactionRadius,
    const int32 MaxResponderCount,
    const float MockDuration
)
{
    const ADMMatchGameMode* MatchGameMode = GetTypedOuter<ADMMatchGameMode>();

    if (!IsValid(MatchGameMode) || !MatchGameMode->HasAuthority() || DeathLocation.ContainsNaN()
        || ReactionRadius <= 0.0f || MaxResponderCount <= 0 || MockDuration <= 0.0f)
    {
        return;
    }

    // NPC는 기존 Registry에서만 검색한다. 월드 전체 Actor 검색은 하지 않는다.
    TArray<DMNPCPopulationDirectorPrivate::FReactionCandidate> Candidates;
    const float RadiusSquared = FMath::Square(ReactionRadius);

    for (const TWeakObjectPtr<ADMNPCCharacter>& NPCPointer : RegisteredNPCs)
    {
        ADMNPCCharacter* NPC = NPCPointer.Get();
        if (!IsValid(NPC))
        {
            continue;
        }

        const float DistanceSquared = FVector::DistSquared2D(NPC->GetActorLocation(), DeathLocation);
        if (DistanceSquared > RadiusSquared)
        {
            continue;
        }

        DMNPCPopulationDirectorPrivate::FReactionCandidate Candidate;
        Candidate.NPC = NPC;
        Candidate.DistanceSquared = DistanceSquared;
        Candidates.Add(Candidate);
    }

    Candidates.Sort([](const DMNPCPopulationDirectorPrivate::FReactionCandidate& A,
                       const DMNPCPopulationDirectorPrivate::FReactionCandidate& B)
    {
        return A.DistanceSquared < B.DistanceSquared;
    });

    int32 StartedCount = 0;

    for (const DMNPCPopulationDirectorPrivate::FReactionCandidate& Candidate : Candidates)
    {
        if (StartedCount >= MaxResponderCount)
        {
            break;
        }

        ADMNPCCharacter* NPC = Candidate.NPC;
        if (!IsValid(NPC))
        {
            continue;
        }

        ADMJellyAIController* AIController = Cast<ADMJellyAIController>(NPC->GetController());
        if (IsValid(AIController) && AIController->TryStartDeathMockingReaction(DeathLocation, MockDuration))
        {
            ++StartedCount;
        }
    }

    UE_LOG(
        LogDMNPCPopulationDirector,
        Log,
        TEXT("NPC death mocking dispatched. Candidates=%d, Started=%d, Radius=%.1f"),
        Candidates.Num(), StartedCount, ReactionRadius
    );
}

#pragma endregion

#pragma region Spawn

bool UDMNPCPopulationDirector::CollectSpawnPoints(TArray<ATargetPoint*>& OutSpawnPoints) const
{
    OutSpawnPoints.Reset();

    const ADMMatchGameMode* MatchGameMode = GetTypedOuter<ADMMatchGameMode>();

    if (!IsValid(MatchGameMode))
    {
        return false;
    }

    UWorld* World = MatchGameMode->GetWorld();

    if (!IsValid(World))
    {
        return false;
    }

    for (TActorIterator<ATargetPoint> Iterator(World); Iterator; ++Iterator)
    {
        ATargetPoint* TargetPoint = *Iterator;

        if (IsValid(TargetPoint) && TargetPoint->ActorHasTag(DMNPCPopulationDirectorPrivate::NPCSpawnPointTag))
        {
            OutSpawnPoints.Add(TargetPoint);
        }
    }

    if (!ensureMsgf(
            OutSpawnPoints.Num() > 0,
            TEXT("No NPC spawn TargetPoints found. Required Actor Tag=%s"),
            *DMNPCPopulationDirectorPrivate::NPCSpawnPointTag.ToString()
        ))
    {
        return false;
    }

    return true;
}

void UDMNPCPopulationDirector::DestroyRegisteredNPCs()
{
    for (const TWeakObjectPtr<ADMNPCCharacter>& NPCPointer : RegisteredNPCs)
    {
        ADMNPCCharacter* NPC = NPCPointer.Get();

        if (IsValid(NPC))
        {
            NPC->Destroy();
        }
    }

    RegisteredNPCs.Reset();
}

#pragma endregion