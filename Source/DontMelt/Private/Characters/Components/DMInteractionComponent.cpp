#include "Characters/Components/DMInteractionComponent.h"

#include "Core/DMGameplayUtils.h"

#include "Characters/Components/DMCharacterStateComponent.h"
#include "Characters/Components/DMPoseComponent.h"
#include "Characters/DMPlayerCharacter.h"

#include "Framework/Match/DMMatchGameState.h"
#include "Framework/Match/DMMatchPlayerState.h"
#include "Framework/Match/DMMatchPlayerController.h"

#include "Interaction/DMInteractable.h"

#include "Relic/DMRelic.h"
#include "Relic/DMRelicManager.h"
#include "Relic/DMRelicChest.h"

#include "Engine/World.h"
#include "Engine/OverlapResult.h"
#include "EngineUtils.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMInteraction, Log, All);

namespace DMInteractionLineOfSight
{
    // 사이를 지나가는 Pawn을 건너뛰며 재검사할 최대 횟수.
    constexpr int32 MaxPawnSkipCount = 4;

    /**
     * Visibility 채널 LOS를 검사하되,
     * 사이를 지나가는 Pawn(플레이어·NPC)과 Pawn에 붙은 Actor(운반 중인 유물 등)는
     * 시야 차단으로 보지 않는다.
     *
     * 대상 Actor에 실제로 닿아야 성공하고, 지형에 막히면 실패한다.
     */
    bool HasVisibilityLineOfSightIgnoringPawns(
        const UWorld* World,
        const FVector& Start,
        const FVector& End,
        const AActor* TargetActor,
        FCollisionQueryParams QueryParams
    )
    {
        if (!IsValid(World) || !IsValid(TargetActor))
        {
            return false;
        }

        for (int32 SkipIndex = 0; SkipIndex <= MaxPawnSkipCount; ++SkipIndex)
        {
            FHitResult HitResult;

            const bool bHit = World->LineTraceSingleByChannel(HitResult, Start, End, ECC_Visibility, QueryParams);

            if (!bHit)
            {
                return false;
            }

            AActor* HitActor = HitResult.GetActor();

            if (HitActor == TargetActor)
            {
                return true;
            }

            if (!IsValid(HitActor))
            {
                return false;
            }

            const AActor* AttachParentActor = HitActor->GetAttachParentActor();

            const bool bIsPawnBlocker =
                HitActor->IsA<APawn>() || (IsValid(AttachParentActor) && AttachParentActor->IsA<APawn>());

            // 지형·구조물에 막힌 경우는 실패한다.
            if (!bIsPawnBlocker)
            {
                return false;
            }

            QueryParams.AddIgnoredActor(HitActor);
        }

        return false;
    }
}

#pragma region Lifecycle

UDMInteractionComponent::UDMInteractionComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    SetIsReplicatedByDefault(true);
}

void UDMInteractionComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION(UDMInteractionComponent, OwnerSession, COND_OwnerOnly);
}

void UDMInteractionComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    AActor* OwnerActor = GetOwner();

    if (OwnerActor != nullptr && OwnerActor->HasAuthority() && OwnerSession.IsActive())
    {
        EndServerInteraction(false, TEXT("Owner EndPlay"));
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CompletionTimerHandle);

        World->GetTimerManager().ClearTimer(ValidationTimerHandle);
    }

    Super::EndPlay(EndPlayReason);
}

#pragma endregion

#pragma region Public API

AActor* UDMInteractionComponent::GetLocalInteractionPromptCandidate()
{
    ADMPlayerCharacter* OwnerCharacter = GetOwnerPlayerCharacter();

    if (!IsValid(OwnerCharacter) || !OwnerCharacter->IsLocallyControlled() || IsInteractionInputLocked())
    {
        return nullptr;
    }

    // 같은 후보 선정 규칙을 사용하되 조회 로그는 남기지 않는다.
    return FindLocalInteractionCandidate(false);
}

void UDMInteractionComponent::RequestInteraction()
{
    ADMPlayerCharacter* OwnerCharacter = GetOwnerPlayerCharacter();

    if (!IsValid(OwnerCharacter) || !OwnerCharacter->IsLocallyControlled())
    {
        return;
    }

    if (IsInteractionInputLocked())
    {
        return;
    }

    AActor* Candidate = FindLocalInteractionCandidate();

    LocalFocusedActor = Candidate;

    if (!IsValid(Candidate))
    {
        UE_LOG(LogDMInteraction, Verbose, TEXT("[%s] No valid interaction candidate."), *GetNameSafe(OwnerCharacter));

        return;
    }

    SubmitLocalInteractionRequest(Candidate, false);
}

void UDMInteractionComponent::RequestChestAction(ADMRelicChest* Chest)
{
    ADMPlayerCharacter* OwnerCharacter = GetOwnerPlayerCharacter();

    if (!IsValid(OwnerCharacter) || !OwnerCharacter->IsLocallyControlled() || !IsValid(Chest))
    {
        return;
    }

    if (IsInteractionInputLocked())
    {
        return;
    }

    LocalFocusedActor = Chest;

    SubmitLocalInteractionRequest(Chest, true);
}

void UDMInteractionComponent::RequestCancelInteraction()
{
    if (!IsInteractionInputLocked() || bLocalCancelRequested)
    {
        return;
    }

    int32 RequestId = INDEX_NONE;

    if (OwnerSession.IsActive())
    {
        RequestId = OwnerSession.RequestId;
    }
    else
    {
        RequestId = PendingRequestId;
    }

    if (RequestId == INDEX_NONE)
    {
        return;
    }

    bLocalCancelRequested = true;

    ServerCancelInteraction(RequestId);
}

void UDMInteractionComponent::ForceCleanupForDeath()
{
    AActor* OwnerActor = GetOwner();

    if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
    {
        return;
    }

    if (!OwnerSession.IsActive())
    {
        return;
    }

    /*
     * 중요:
     *
     * EndServerInteractionAsFailure()를
     * 호출하지 않는다.
     *
     * 해당 함수는 Deposit 상황에서
     * Relic Drop까지 수행할 수 있기 때문이다.
     *
     * Death Pipeline에서는 Session /
     * Reservation만 정리하고
     * 실제 Death Drop은
     * RelicManager가 한 번만 수행한다.
     */
    EndServerInteraction(false, TEXT("Death cleanup"));
}

float UDMInteractionComponent::GetInteractionProgress01() const
{
    if (!OwnerSession.IsActive())
    {
        return 0.0f;
    }

    const double Duration = OwnerSession.EndServerTime - OwnerSession.StartServerTime;

    if (Duration <= 0.0)
    {
        return 1.0f;
    }

    const double CurrentTime = DMGameplay::GetServerTimeSeconds(this);

    return static_cast<float>(FMath::Clamp((CurrentTime - OwnerSession.StartServerTime) / Duration, 0.0, 1.0));
}

#pragma endregion

#pragma region Local Candidate

void UDMInteractionComponent::SubmitLocalInteractionRequest(AActor* TargetActor, const bool bRequestChestAction)
{
    if (!IsValid(TargetActor) || IsInteractionInputLocked())
    {
        return;
    }

    if (NextLocalRequestId <= 0)
    {
        NextLocalRequestId = 1;
    }

    const int32 RequestId = NextLocalRequestId++;

    PendingRequestId = RequestId;

    bLocalInteractionRequestPending = true;

    bLocalCancelRequested = false;

    ServerRequestInteraction(TargetActor, RequestId, bRequestChestAction);
}

AActor* UDMInteractionComponent::FindLocalInteractionCandidate(const bool bWriteLog)
{
    ADMPlayerCharacter* OwnerCharacter = GetOwnerPlayerCharacter();

    if (!IsValid(OwnerCharacter))
    {
        return nullptr;
    }

    APlayerController* PlayerController = Cast<APlayerController>(OwnerCharacter->GetController());

    if (!IsValid(PlayerController))
    {
        return nullptr;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return nullptr;
    }

    FVector ViewLocation;
    FRotator ViewRotation;

    PlayerController->GetPlayerViewPoint(ViewLocation, ViewRotation);

    /*
     * Interaction 후보 선정에서는
     * Camera Pitch를 사용하지 않는다.
     *
     * 따라서 바닥에 놓인 Relic을 상호작용하기 위해
     * 화면을 정확히 아래로 내릴 필요가 없다.
     */
    FVector ViewForward2D = ViewRotation.Vector();

    ViewForward2D.Z = 0.0f;

    if (!ViewForward2D.Normalize())
    {
        return nullptr;
    }

    FCollisionObjectQueryParams ObjectQueryParams;

    ObjectQueryParams.AddObjectTypesToQuery(ECC_WorldDynamic);

    FCollisionQueryParams OverlapQueryParams(SCENE_QUERY_STAT(DMInteractionCandidateOverlap), false);

    OverlapQueryParams.AddIgnoredActor(OwnerCharacter);

    TArray<FOverlapResult> OverlapResults;

    World->OverlapMultiByObjectType(
        OverlapResults,
        OwnerCharacter->GetActorLocation(),
        FQuat::Identity,
        ObjectQueryParams,
        FCollisionShape::MakeSphere(InteractionRange),
        OverlapQueryParams
    );

    TSet<AActor*> ProcessedActors;

    AActor* BestCandidate = nullptr;

    float BestScore = -BIG_NUMBER;

    for (const FOverlapResult& OverlapResult : OverlapResults)
    {
        AActor* CandidateActor = OverlapResult.GetActor();

        if (!IsValid(CandidateActor) || CandidateActor == OwnerCharacter)
        {
            continue;
        }

        if (ProcessedActors.Contains(CandidateActor))
        {
            continue;
        }

        ProcessedActors.Add(CandidateActor);

        if (!CandidateActor->GetClass()->ImplementsInterface(UDMInteractable::StaticClass()))
        {
            continue;
        }

        FVector TargetOrigin;
        FVector TargetExtent;

        CandidateActor->GetActorBounds(false, TargetOrigin, TargetExtent);

        const float Distance = FVector::Distance(OwnerCharacter->GetActorLocation(), TargetOrigin);

        if (Distance > InteractionRange)
        {
            continue;
        }

        /*
         * 높이 차이는 후보 방향 판정에서 제거한다.
         *
         * 바닥 Relic / Chest 모두
         * "플레이어 앞에 있느냐"만 검사한다.
         */
        FVector ToTarget2D = TargetOrigin - OwnerCharacter->GetActorLocation();

        ToTarget2D.Z = 0.0f;

        float FacingDot = 1.0f;

        if (!ToTarget2D.IsNearlyZero())
        {
            ToTarget2D.Normalize();

            FacingDot = FVector::DotProduct(ViewForward2D, ToTarget2D);
        }

        if (FacingDot < InteractionFacingDotThreshold)
        {
            continue;
        }

        /*
         * 전방 범위에 있다고 해서
         * 벽 뒤의 Actor까지 선택하면 안 된다.
         *
         * 후보 위치까지 실제 Visibility LOS를 검사한다.
         */
        FCollisionQueryParams LineOfSightQueryParams(SCENE_QUERY_STAT(DMLocalInteractionLOS), false);

        LineOfSightQueryParams.AddIgnoredActor(OwnerCharacter);

        // 서버 검증(HasServerLineOfSightTo)과 같은 규칙으로
        // 사이를 지나가는 Pawn은 시야 차단으로 보지 않는다.
        const bool bHasLineOfSight = DMInteractionLineOfSight::HasVisibilityLineOfSightIgnoringPawns(
            World, ViewLocation, TargetOrigin, CandidateActor, LineOfSightQueryParams
        );

        if (!bHasLineOfSight)
        {
            continue;
        }

        /*
         * 정면에 가까운 대상을 우선하고,
         * 비슷한 방향이면 가까운 대상을 우선한다.
         */
        const float DistanceScore = 1.0f - FMath::Clamp(Distance / InteractionRange, 0.0f, 1.0f);

        const float Score = FacingDot * 2.0f + DistanceScore;

        if (Score <= BestScore)
        {
            continue;
        }

        BestScore = Score;

        BestCandidate = CandidateActor;
    }

    if (!IsValid(BestCandidate))
    {
        if (bWriteLog)
        {
            UE_LOG(LogDMInteraction, Verbose, TEXT("Interaction candidate search: No valid candidate."));
        }

        return nullptr;
    }

    if (bWriteLog)
    {
        UE_LOG(LogDMInteraction, Log, TEXT("Interaction candidate accepted. Actor=%s"), *GetNameSafe(BestCandidate));
    }

    return BestCandidate;
}

#pragma endregion

#pragma region Server Request

void UDMInteractionComponent::ServerRequestInteraction_Implementation(
    AActor* TargetActor, const int32 RequestId, const bool bRequestChestAction
)
{
    ADMPlayerCharacter* OwnerCharacter = GetOwnerPlayerCharacter();

    if (!IsValid(OwnerCharacter) || !OwnerCharacter->HasAuthority())
    {
        return;
    }

    /*
     * 같은 Request 또는 과거 Request의 재사용 방지.
     */
    if (RequestId <= 0 || RequestId <= LastServerProcessedRequestId)
    {
        UE_LOG(
            LogDMInteraction,
            Verbose,
            TEXT(
                "Interaction request rejected: duplicate or stale RequestId. IncomingRequestId=%d, LastProcessedRequestId=%d, Owner=%s"
            ),
            RequestId,
            LastServerProcessedRequestId,
            *GetNameSafe(OwnerCharacter)
        );

        ClientRejectInteraction(RequestId);

        return;
    }

    LastServerProcessedRequestId = RequestId;

    if (OwnerSession.IsActive())
    {
        UE_LOG(
            LogDMInteraction,
            Log,
            TEXT(
                "Interaction request rejected: another Interaction is already active. IncomingRequestId=%d, ActiveRequestId=%d, ActiveType=%s"
            ),
            RequestId,
            OwnerSession.RequestId,
            *UEnum::GetValueAsString(OwnerSession.InteractionType)
        );

        ClientRejectInteraction(RequestId);

        return;
    }

    FString FailureReason;

    if (!ValidateCommonServerRules(TargetActor, FailureReason))
    {
        UE_LOG(LogDMInteraction, Log, TEXT("Interaction rejected. RequestId=%d, Reason=%s"), RequestId, *FailureReason);

        ClientRejectInteraction(RequestId);

        return;
    }

    ADMRelicManager* RelicManager = ResolveRelicManager();

    if (!IsValid(RelicManager))
    {
        ClientRejectInteraction(RequestId);

        return;
    }

    EDMInteractionType InteractionType = EDMInteractionType::PickupRelic;

    float Duration = 0.0f;

    ADMRelic* ReservationRelic = nullptr;

    if (!RelicManager->ResolveInteractionRequest(
            OwnerCharacter,
            TargetActor,
            this,
            bRequestChestAction,
            InteractionType,
            Duration,
            ReservationRelic,
            FailureReason
        ))
    {
        UE_LOG(
            LogDMInteraction,
            Log,
            TEXT("Interaction target rejected. RequestId=%d, Reason=%s"),
            RequestId,
            *FailureReason
        );

        ClientRejectInteraction(RequestId);

        return;
    }

    UDMCharacterStateComponent* CharacterStateComponent =
        OwnerCharacter->FindComponentByClass<UDMCharacterStateComponent>();

    if (!IsValid(CharacterStateComponent) || !CharacterStateComponent->CanPerformAction(EDMActionRequest::Interact))
    {
        ClientRejectInteraction(RequestId);

        return;
    }

    if (InteractionType == EDMInteractionType::InspectChest)
    {
        ADMRelicChest* Chest = Cast<ADMRelicChest>(TargetActor);

        if (!IsValid(Chest))
        {
            ClientRejectInteraction(RequestId);

            return;
        }

        int32 BaseSlot = INDEX_NONE;

        int32 RegisteredRelicCount = 0;

        if (!RelicManager->TryInspectChest(OwnerCharacter, Chest, BaseSlot, RegisteredRelicCount, FailureReason))
        {
            UE_LOG(
                LogDMInteraction, Log, TEXT("InspectChest rejected. RequestId=%d, Reason=%s"), RequestId, *FailureReason
            );

            ClientRejectInteraction(RequestId);

            return;
        }

        ADMMatchPlayerController* MatchPlayerController =
            Cast<ADMMatchPlayerController>(OwnerCharacter->GetController());

        if (!IsValid(MatchPlayerController))
        {
            ClientRejectInteraction(RequestId);

            return;
        }

        MatchPlayerController->ClientOpenChestInteraction(Chest, BaseSlot, RegisteredRelicCount);

        /*
         * InspectChest는 Hold Interaction이 아니므로
         * OwnerSession을 생성하지 않는다.
         *
         * 최초 F 요청의 Local Pending만 종료한다.
         */
        ClientFinishInteraction(RequestId);

        UE_LOG(
            LogDMInteraction,
            Log,
            TEXT("InspectChest accepted. RequestId=%d, BaseSlot=%d, RegisteredRelics=%d"),
            RequestId,
            BaseSlot,
            RegisteredRelicCount
        );

        return;
    }

    int32 ReservationId = INDEX_NONE;

    if (IsValid(ReservationRelic))
    {
        if (!RelicManager->TryAcquireInteractionReservation(ReservationRelic, this, RequestId, ReservationId))
        {
            UE_LOG(
                LogDMInteraction,
                Log,
                TEXT("Interaction rejected: Relic is already reserved. RequestId=%d, RelicId=%d"),
                RequestId,
                ReservationRelic->GetRelicId()
            );

            ClientRejectInteraction(RequestId);

            return;
        }
    }

    if (!CharacterStateComponent->TryBeginAction(EDMActionState::Interact))
    {
        if (IsValid(ReservationRelic))
        {
            RelicManager->ReleaseInteractionReservation(ReservationRelic, this, ReservationId);
        }

        ClientRejectInteraction(RequestId);

        return;
    }

    BeginServerInteraction(TargetActor, InteractionType, Duration, ReservationRelic, ReservationId, RequestId);
}

void UDMInteractionComponent::ServerCancelInteraction_Implementation(const int32 RequestId)
{
    if (!OwnerSession.IsActive())
    {
        return;
    }

    if (OwnerSession.RequestId != RequestId)
    {
        return;
    }

    EndServerInteraction(false, TEXT("Canceled by owner input"));
}

void UDMInteractionComponent::ClientRejectInteraction_Implementation(const int32 RequestId)
{
    if (PendingRequestId != RequestId)
    {
        return;
    }

    PendingRequestId = INDEX_NONE;

    bLocalInteractionRequestPending = false;

    bLocalCancelRequested = false;
}

void UDMInteractionComponent::ClientFinishInteraction_Implementation(const int32 RequestId)
{
    /*
     * 아주 짧은 Interaction은
     * OwnerSession의 Active 상태가 Client에 Replicate되기 전에
     * 서버에서 완료될 수 있다.
     *
     * 따라서 Interaction 종료는 Replication에만 의존하지 않고
     * 서버가 명시적으로 Owner Client에게 알려
     * Local Pending 상태를 해제한다.
     */
    if (PendingRequestId != RequestId)
    {
        return;
    }

    PendingRequestId = INDEX_NONE;

    bLocalInteractionRequestPending = false;

    bLocalCancelRequested = false;
}

#pragma endregion

#pragma region Server Validation

bool UDMInteractionComponent::ValidateCommonServerRules(AActor* TargetActor, FString& OutFailureReason) const
{
    const ADMPlayerCharacter* OwnerCharacter = GetOwnerPlayerCharacter();

    if (!IsValid(OwnerCharacter) || !OwnerCharacter->HasAuthority())
    {
        OutFailureReason = TEXT("Invalid server owner.");

        return false;
    }

    if (!IsValid(TargetActor))
    {
        OutFailureReason = TEXT("Target is invalid.");

        return false;
    }

    if (!TargetActor->GetClass()->ImplementsInterface(UDMInteractable::StaticClass()))
    {
        OutFailureReason = TEXT("Target is not interactable.");

        return false;
    }

    const ADMMatchPlayerState* MatchPlayerState = OwnerCharacter->GetPlayerState<ADMMatchPlayerState>();

    if (!IsValid(MatchPlayerState))
    {
        OutFailureReason = TEXT("MatchPlayerState is unavailable.");

        return false;
    }

    if (MatchPlayerState->GetLifeState() != EDMPlayerLifeState::Alive)
    {
        OutFailureReason = TEXT("Player is not Alive.");

        return false;
    }

    const UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        OutFailureReason = TEXT("World is unavailable.");

        return false;
    }

    const ADMMatchGameState* MatchGameState = World->GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        OutFailureReason = TEXT("MatchGameState is unavailable.");

        return false;
    }

    const EDMMatchPhase Phase = MatchGameState->GetCurrentPhase();

    if (Phase != EDMMatchPhase::Playing && Phase != EDMMatchPhase::Overtime)
    {
        OutFailureReason = TEXT("Interaction is not allowed in the current MatchPhase.");

        return false;
    }

    const UDMPoseComponent* PoseComponent = OwnerCharacter->FindComponentByClass<UDMPoseComponent>();

    if (IsValid(PoseComponent) && !PoseComponent->IsActionAllowedByPose(EDMActionRequest::Interact))
    {
        OutFailureReason = TEXT("Current Pose blocks interaction.");

        return false;
    }

    FVector TargetOrigin;
    FVector TargetExtent;

    TargetActor->GetActorBounds(false, TargetOrigin, TargetExtent);

    const float DistanceSquared = FVector::DistSquared(OwnerCharacter->GetActorLocation(), TargetOrigin);

    if (DistanceSquared > FMath::Square(InteractionRange))
    {
        OutFailureReason = TEXT("Target is outside interaction range.");

        return false;
    }

    if (!HasServerLineOfSightTo(TargetActor))
    {
        OutFailureReason = TEXT("Line of sight is blocked.");

        return false;
    }

    return true;
}

bool UDMInteractionComponent::HasServerLineOfSightTo(AActor* TargetActor) const
{
    const ADMPlayerCharacter* OwnerCharacter = GetOwnerPlayerCharacter();

    if (!IsValid(OwnerCharacter) || !IsValid(TargetActor))
    {
        return false;
    }

    FVector EyeLocation;
    FRotator EyeRotation;

    OwnerCharacter->GetActorEyesViewPoint(EyeLocation, EyeRotation);

    FVector TargetOrigin;
    FVector TargetExtent;

    TargetActor->GetActorBounds(false, TargetOrigin, TargetExtent);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DMServerInteractionLOS), false);

    QueryParams.AddIgnoredActor(OwnerCharacter);

    /*
     * 진행 중 상호작용은 0.1초마다 이 검사를 반복한다.
     * 사이를 지나가는 플레이어·NPC 때문에 상호작용이 실패하고
     * Deposit 유물이 Drop되지 않도록 Pawn은 시야 차단으로 보지 않는다.
     */
    return DMInteractionLineOfSight::HasVisibilityLineOfSightIgnoringPawns(
        GetWorld(), EyeLocation, TargetOrigin, TargetActor, QueryParams
    );
}

void UDMInteractionComponent::ServerValidateActiveInteraction()
{
    if (!OwnerSession.IsActive())
    {
        return;
    }

    FString FailureReason;

    if (!ValidateCommonServerRules(OwnerSession.TargetActor, FailureReason))
    {
        EndServerInteractionAsFailure(FailureReason);

        return;
    }

    ADMRelicManager* RelicManager = ResolveRelicManager();

    if (!IsValid(RelicManager))
    {
        EndServerInteractionAsFailure(TEXT("RelicManager is unavailable."));

        return;
    }

    EDMInteractionType ResolvedType = EDMInteractionType::PickupRelic;

    float ResolvedDuration = 0.0f;

    ADMRelic* ResolvedReservationRelic = nullptr;

    const bool bChestActionRequest = OwnerSession.InteractionType == EDMInteractionType::StealRelic
        || OwnerSession.InteractionType == EDMInteractionType::DepositRelic;

    if (!RelicManager->ResolveInteractionRequest(
            GetOwnerPlayerCharacter(),
            OwnerSession.TargetActor,
            this,
            bChestActionRequest,
            ResolvedType,
            ResolvedDuration,
            ResolvedReservationRelic,
            FailureReason
        ))
    {
        EndServerInteractionAsFailure(FailureReason);

        return;
    }

    /*
     * 시작할 때와 Interaction 의미가
     * 중간에 달라졌다면 취소한다.
     */
    if (ResolvedType != OwnerSession.InteractionType)
    {
        EndServerInteraction(false, TEXT("Interaction type changed during execution."));

        return;
    }

    ADMRelic* ReservedRelic = ActiveReservedRelic.Get();

    if (IsValid(ReservedRelic))
    {
        if (ResolvedReservationRelic != ReservedRelic
            || !RelicManager->IsInteractionReservationOwnedBy(ReservedRelic, this, ActiveReservationId))
        {
            EndServerInteraction(false, TEXT("Interaction reservation is no longer valid."));

            return;
        }
    }
}

void UDMInteractionComponent::ServerHandleInteractionCompleted()
{
    if (!OwnerSession.IsActive())
    {
        return;
    }

    ADMPlayerCharacter* OwnerCharacter = GetOwnerPlayerCharacter();

    if (!IsValid(OwnerCharacter))
    {
        EndServerInteractionAsFailure(TEXT("OwnerCharacter is invalid."));

        return;
    }

    FString FailureReason;

    if (!ValidateCommonServerRules(OwnerSession.TargetActor, FailureReason))
    {
        EndServerInteractionAsFailure(FailureReason);

        return;
    }

    ADMRelicManager* RelicManager = ResolveRelicManager();

    if (!IsValid(RelicManager))
    {
        EndServerInteractionAsFailure(TEXT("RelicManager is unavailable."));

        return;
    }

    EDMInteractionType ResolvedType = EDMInteractionType::PickupRelic;

    float ResolvedDuration = 0.0f;

    ADMRelic* ResolvedReservationRelic = nullptr;

    const bool bChestActionRequest = OwnerSession.InteractionType == EDMInteractionType::StealRelic
        || OwnerSession.InteractionType == EDMInteractionType::DepositRelic;

    /*
     * 완료 순간 Target 상태를 한 번 더 검사한다.
     */
    if (!RelicManager->ResolveInteractionRequest(
            OwnerCharacter,
            OwnerSession.TargetActor,
            this,
            bChestActionRequest,
            ResolvedType,
            ResolvedDuration,
            ResolvedReservationRelic,
            FailureReason
        ))
    {
        EndServerInteractionAsFailure(FailureReason);

        return;
    }

    if (ResolvedType != OwnerSession.InteractionType)
    {
        EndServerInteractionAsFailure(TEXT("Interaction type changed before completion."));

        return;
    }

    ADMRelic* ReservedRelic = ActiveReservedRelic.Get();

    if (IsValid(ReservedRelic))
    {
        if (ResolvedReservationRelic != ReservedRelic
            || !RelicManager->IsInteractionReservationOwnedBy(ReservedRelic, this, ActiveReservationId))
        {
            EndServerInteractionAsFailure(TEXT("Reservation validation failed at completion."));

            return;
        }
    }

    switch (OwnerSession.InteractionType)
    {
    case EDMInteractionType::PickupRelic:
    {
        ADMRelic* TargetRelic = Cast<ADMRelic>(OwnerSession.TargetActor);

        if (!IsValid(TargetRelic))
        {
            EndServerInteractionAsFailure(TEXT("Pickup target Relic is invalid."));

            return;
        }

        if (!RelicManager->TryCommitRelicAcquisition(
                OwnerCharacter, TargetRelic, EDMInteractionType::PickupRelic, nullptr, FailureReason
            ))
        {
            EndServerInteractionAsFailure(FailureReason);

            return;
        }

        UE_LOG(
            LogDMInteraction,
            Log,
            TEXT("PickupRelic committed. RequestId=%d, RelicId=%d"),
            OwnerSession.RequestId,
            TargetRelic->GetRelicId()
        );

        break;
    }

    case EDMInteractionType::StealRelic:
    {
        ADMRelicChest* Chest = Cast<ADMRelicChest>(OwnerSession.TargetActor);

        ADMRelic* StealRelic = ActiveReservedRelic.Get();

        if (!IsValid(Chest) || !IsValid(StealRelic))
        {
            EndServerInteractionAsFailure(TEXT("Steal target or reserved Relic is invalid."));

            return;
        }

        if (!RelicManager->TryCommitRelicAcquisition(
                OwnerCharacter, StealRelic, EDMInteractionType::StealRelic, Chest, FailureReason
            ))
        {
            EndServerInteractionAsFailure(FailureReason);

            return;
        }

        UE_LOG(
            LogDMInteraction,
            Log,
            TEXT("StealRelic committed. RequestId=%d, RelicId=%d, ChestBaseSlot=%d"),
            OwnerSession.RequestId,
            StealRelic->GetRelicId(),
            Chest->GetBaseSlot()
        );

        break;
    }

    case EDMInteractionType::DepositRelic:
    {
        ADMRelicChest* Chest = Cast<ADMRelicChest>(OwnerSession.TargetActor);

        ADMRelic* DepositRelic = ActiveReservedRelic.Get();

        if (!IsValid(Chest) || !IsValid(DepositRelic))
        {
            EndServerInteractionAsFailure(TEXT("Deposit Chest or reserved Relic is invalid."));

            return;
        }

        if (!RelicManager->TryCommitRelicDeposit(OwnerCharacter, DepositRelic, Chest, FailureReason))
        {
            EndServerInteractionAsFailure(FailureReason);

            return;
        }

        UE_LOG(
            LogDMInteraction,
            Log,
            TEXT("DepositRelic committed. RequestId=%d, RelicId=%d, ChestBaseSlot=%d"),
            OwnerSession.RequestId,
            DepositRelic->GetRelicId(),
            Chest->GetBaseSlot()
        );

        break;
    }

    default:
    {
        EndServerInteractionAsFailure(TEXT("Interaction type is not implemented in this stage."));

        return;
    }
    }

    EndServerInteraction(true, TEXT("Completed"));
}

#pragma endregion

#pragma region Session

void UDMInteractionComponent::BeginServerInteraction(
    AActor* TargetActor,
    const EDMInteractionType InteractionType,
    const float Duration,
    ADMRelic* ReservedRelic,
    const int32 ReservationId,
    const int32 RequestId
)
{
    check(GetOwner() != nullptr && GetOwner()->HasAuthority());

    ActiveReservedRelic = ReservedRelic;

    ActiveReservationId = ReservationId;

    const double StartTime = DMGameplay::GetServerTimeSeconds(this);

    OwnerSession.TargetActor = TargetActor;

    OwnerSession.InteractionType = InteractionType;

    OwnerSession.RequestId = RequestId;

    OwnerSession.StartServerTime = StartTime;

    OwnerSession.EndServerTime = StartTime + FMath::Max(0.0f, Duration);

    /*
     * Listen Server의 로컬 Player는
     * RepNotify를 기다리지 않으므로
     * 서버 로컬에서도 즉시 알린다.
     */
    InteractionSessionChangedDelegate.Broadcast(OwnerSession);

    SetServerMovementLocked(true);

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            CompletionTimerHandle,
            this,
            &ThisClass::ServerHandleInteractionCompleted,
            FMath::Max(0.01f, Duration),
            false
        );

        World->GetTimerManager().SetTimer(
            ValidationTimerHandle, this, &ThisClass::ServerValidateActiveInteraction, ServerValidationInterval, true
        );
    }

    if (AActor* OwnerActor = GetOwner())
    {
        OwnerActor->ForceNetUpdate();
    }

    ADMPlayerCharacter* OwnerCharacter = GetOwnerPlayerCharacter();

    if (IsValid(OwnerCharacter) && OwnerCharacter->IsLocallyControlled())
    {
        PendingRequestId = INDEX_NONE;

        bLocalInteractionRequestPending = false;

        bLocalCancelRequested = false;
    }

    UE_LOG(
        LogDMInteraction,
        Log,
        TEXT("Interaction started. RequestId=%d, Type=%s, Duration=%.2f, Target=%s, ReservationId=%d"),
        RequestId,
        *UEnum::GetValueAsString(InteractionType),
        Duration,
        *GetNameSafe(TargetActor),
        ReservationId
    );
}

void UDMInteractionComponent::EndServerInteractionAsFailure(const FString& Reason)
{
    if (!OwnerSession.IsActive())
    {
        return;
    }

    /*
     * Match가 이미 끝났거나 상호작용이 허용되지 않는 Phase로 전환된 경우에는
     * 실패 Gameplay 처리(DepositFailure Drop)를 만들지 않는다.
     *
     * Interaction Session만 정리한다.
     */
    UWorld* World = GetWorld();

    if (IsValid(World))
    {
        const ADMMatchGameState* MatchGameState = World->GetGameState<ADMMatchGameState>();

        if (IsValid(MatchGameState))
        {
            const EDMMatchPhase Phase = MatchGameState->GetCurrentPhase();

            /*
             * Playing/Overtime 이외의 Phase(OvertimeIntro, Finished, Aborted 등)에서의 실패는
             * 플레이어 귀책이 아닌 Phase 전환에 의한 것이므로
             * 유물 Drop 없이 상호작용만 취소한다.
             *
             * 사망은 ForceCleanupForDeath(), 접속 종료는 GameMode Logout 경로에서 별도로 처리한다.
             */
            if (Phase != EDMMatchPhase::Playing && Phase != EDMMatchPhase::Overtime)
            {
                EndServerInteraction(false, TEXT("Interaction cancelled by MatchPhase change."));

                return;
            }
        }
    }

    /*
     * Deposit 진행 중 서버 검증 실패는
     * Carry 상태로 되돌리는 것이 아니라
     * 현재 위치에 Relic을 Drop한다.
     */
    if (OwnerSession.InteractionType == EDMInteractionType::DepositRelic)
    {
        ADMPlayerCharacter* OwnerCharacter = GetOwnerPlayerCharacter();

        ADMMatchPlayerState* PlayerState =
            IsValid(OwnerCharacter) ? OwnerCharacter->GetPlayerState<ADMMatchPlayerState>() : nullptr;

        ADMRelicManager* RelicManager = ResolveRelicManager();

        if (IsValid(OwnerCharacter) && IsValid(PlayerState) && IsValid(RelicManager)
            && RelicManager->IsPlayerCarryingRelic(PlayerState))
        {
            EDMRelicDropReason DropReason = EDMRelicDropReason::DepositFailure;

            /*
             * LifeState에 맞는 DropReason을 남긴다.
             */
            if (PlayerState->GetLifeState() == EDMPlayerLifeState::Disconnected)
            {
                DropReason = EDMRelicDropReason::Disconnect;
            }
            else if (PlayerState->GetLifeState() != EDMPlayerLifeState::Alive)
            {
                DropReason = EDMRelicDropReason::Death;
            }

            FString DropFailureReason;

            if (!RelicManager->TryDropCarriedRelic(PlayerState, OwnerCharacter, DropReason, DropFailureReason))
            {
                UE_LOG(LogDMInteraction, Warning, TEXT("Deposit failure Drop failed. Reason=%s"), *DropFailureReason);
            }
        }
    }

    EndServerInteraction(false, Reason);
}

void UDMInteractionComponent::EndServerInteraction(const bool bCompleted, const FString& Reason)
{
    if (!OwnerSession.IsActive())
    {
        return;
    }

    const int32 FinishedRequestId = OwnerSession.RequestId;

    const EDMInteractionType FinishedType = OwnerSession.InteractionType;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(CompletionTimerHandle);

        World->GetTimerManager().ClearTimer(ValidationTimerHandle);
    }

    ADMRelic* ReservedRelic = ActiveReservedRelic.Get();

    if (IsValid(ReservedRelic))
    {
        if (ADMRelicManager* RelicManager = ResolveRelicManager())
        {
            RelicManager->ReleaseInteractionReservation(ReservedRelic, this, ActiveReservationId);
        }
    }

    if (ADMPlayerCharacter* OwnerCharacter = GetOwnerPlayerCharacter())
    {
        if (UDMCharacterStateComponent* CharacterStateComponent =
                OwnerCharacter->FindComponentByClass<UDMCharacterStateComponent>())
        {
            CharacterStateComponent->EndAction(EDMActionState::Interact);
        }
    }

    SetServerMovementLocked(false);

    ActiveReservedRelic.Reset();

    ActiveReservationId = INDEX_NONE;

    /*
     * OwnerSession Replication보다 Interaction이 먼저 종료될 수 있으므로
     * Owner Client의 Local Pending 상태를 명시적으로 정리한다.
     */
    ClientFinishInteraction(FinishedRequestId);

    OwnerSession.Reset();

    InteractionSessionChangedDelegate.Broadcast(OwnerSession);

    if (AActor* OwnerActor = GetOwner())
    {
        OwnerActor->ForceNetUpdate();
    }

    ADMPlayerCharacter* OwnerCharacter = GetOwnerPlayerCharacter();

    if (IsValid(OwnerCharacter) && OwnerCharacter->IsLocallyControlled())
    {
        PendingRequestId = INDEX_NONE;

        bLocalInteractionRequestPending = false;

        bLocalCancelRequested = false;
    }

    UE_LOG(
        LogDMInteraction,
        Log,
        TEXT("Interaction ended. RequestId=%d, Type=%s, Completed=%s, Reason=%s"),
        FinishedRequestId,
        *UEnum::GetValueAsString(FinishedType),
        bCompleted ? TEXT("true") : TEXT("false"),
        *Reason
    );
}

void UDMInteractionComponent::OnRep_OwnerSession()
{
    if (OwnerSession.IsActive())
    {
        if (PendingRequestId == OwnerSession.RequestId)
        {
            PendingRequestId = INDEX_NONE;

            bLocalInteractionRequestPending = false;

            bLocalCancelRequested = false;
        }
    }
    else
    {
        PendingRequestId = INDEX_NONE;

        bLocalInteractionRequestPending = false;

        bLocalCancelRequested = false;
    }

    /*
     * OwnerSession 자체가 UI Read Model이다.
     *
     * HUD는 별도의 진행 상태를 저장하지 않고
     * 이 이벤트를 통해 현재 Session을 읽는다.
     */
    InteractionSessionChangedDelegate.Broadcast(OwnerSession);
}

#pragma endregion

#pragma region Helpers

ADMPlayerCharacter* UDMInteractionComponent::GetOwnerPlayerCharacter() const
{
    return Cast<ADMPlayerCharacter>(GetOwner());
}

ADMRelicManager* UDMInteractionComponent::ResolveRelicManager()
{
    if (CachedRelicManager.IsValid())
    {
        return CachedRelicManager.Get();
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return nullptr;
    }

    for (TActorIterator<ADMRelicManager> It(World); It; ++It)
    {
        ADMRelicManager* Manager = *It;

        if (IsValid(Manager))
        {
            CachedRelicManager = Manager;

            return Manager;
        }
    }

    return nullptr;
}

void UDMInteractionComponent::SetServerMovementLocked(const bool bLocked)
{
    ADMPlayerCharacter* OwnerCharacter = GetOwnerPlayerCharacter();

    if (!IsValid(OwnerCharacter) || !OwnerCharacter->HasAuthority())
    {
        return;
    }

    UCharacterMovementComponent* MovementComponent = OwnerCharacter->GetCharacterMovement();

    if (!IsValid(MovementComponent))
    {
        return;
    }

    if (bLocked)
    {
        if (!bMovementModeSaved)
        {
            SavedMovementMode = static_cast<uint8>(MovementComponent->MovementMode);

            SavedCustomMovementMode = MovementComponent->CustomMovementMode;

            bMovementModeSaved = true;
        }

        OwnerCharacter->ClearSprintRequest();

        MovementComponent->StopMovementImmediately();

        MovementComponent->DisableMovement();

        return;
    }

    if (!bMovementModeSaved)
    {
        return;
    }

    MovementComponent->SetMovementMode(static_cast<EMovementMode>(SavedMovementMode), SavedCustomMovementMode);

    bMovementModeSaved = false;
}

#pragma endregion