#include "Characters/Components/DMCharacterMovementComponent.h"
#if WITH_EDITOR
#include "Development/DMNPCMovementDiagnostics.h"
#include "Characters/DMNPCCharacter.h"
#include "Characters/DMJellyAIController.h"
#include "Navigation/PathFollowingComponent.h"
#include "Engine/World.h"
#include "Components/PrimitiveComponent.h"
#endif

#include "Characters/DMJellyCharacterBase.h"
#include "Characters/Components/DMCharacterStateComponent.h"
#include "Core/DMGameplayTags.h"

#pragma region Network Prediction Internals

namespace
{
    constexpr float SlowMovementSpeedMultiplier = 0.5f;

    class FSavedMove_DMCharacter : public FSavedMove_Character
    {
    public:
        using Super = FSavedMove_Character;

        FSavedMove_DMCharacter()
            : bSavedWantsToSprint(false)
        {
        }

        virtual void Clear() override
        {
            Super::Clear();

            bSavedWantsToSprint = false;
        }

        virtual uint8 GetCompressedFlags() const override
        {
            uint8 Result = Super::GetCompressedFlags();

            if (bSavedWantsToSprint)
            {
                Result |= FSavedMove_Character::FLAG_Custom_0;
            }

            return Result;
        }

        virtual bool
        CanCombineWith(const FSavedMovePtr& NewMove, ACharacter* InCharacter, float MaxDelta) const override
        {
            const FSavedMove_DMCharacter* NewDMMove = static_cast<const FSavedMove_DMCharacter*>(NewMove.Get());

            if (NewDMMove == nullptr || bSavedWantsToSprint != NewDMMove->bSavedWantsToSprint)
            {
                return false;
            }

            return Super::CanCombineWith(NewMove, InCharacter, MaxDelta);
        }

        virtual void SetMoveFor(
            ACharacter* Character,
            float InDeltaTime,
            const FVector& NewAccel,
            FNetworkPredictionData_Client_Character& ClientData
        ) override
        {
            Super::SetMoveFor(Character, InDeltaTime, NewAccel, ClientData);

            const UDMCharacterMovementComponent* MovementComponent =
                Cast<UDMCharacterMovementComponent>(Character != nullptr ? Character->GetCharacterMovement() : nullptr);

            bSavedWantsToSprint = MovementComponent != nullptr && MovementComponent->IsSprintRequested();
        }

        virtual void PrepMoveFor(ACharacter* Character) override
        {
            Super::PrepMoveFor(Character);

            UDMCharacterMovementComponent* MovementComponent =
                Cast<UDMCharacterMovementComponent>(Character != nullptr ? Character->GetCharacterMovement() : nullptr);

            if (MovementComponent == nullptr)
            {
                return;
            }

            MovementComponent->SetSprintRequested(bSavedWantsToSprint);
        }

    private:
        uint8 bSavedWantsToSprint : 1;
    };

    class FNetworkPredictionData_Client_DMCharacter : public FNetworkPredictionData_Client_Character
    {
    public:
        using Super = FNetworkPredictionData_Client_Character;

        explicit FNetworkPredictionData_Client_DMCharacter(const UCharacterMovementComponent& ClientMovement)
            : Super(ClientMovement)
        {
        }

        virtual FSavedMovePtr AllocateNewMove() override
        {
            return FSavedMovePtr(new FSavedMove_DMCharacter());
        }
    };
}

#pragma endregion

#pragma region Sprint

void UDMCharacterMovementComponent::SetSprintRequested(const bool bRequested)
{
    bWantsToSprint = bRequested;
}

bool UDMCharacterMovementComponent::IsSprintRequested() const
{
    return bWantsToSprint;
}

bool UDMCharacterMovementComponent::IsEmoteMovementLocked() const
{
    const ADMJellyCharacterBase* Character = Cast<ADMJellyCharacterBase>(CharacterOwner);
    const UDMCharacterStateComponent* State = IsValid(Character) ? Character->GetCharacterStateComponent() : nullptr;
    return IsValid(State) && State->GetActionState() == EDMActionState::Emote;
}

bool UDMCharacterMovementComponent::CanAttemptJump() const
{
    return !IsEmoteMovementLocked() && Super::CanAttemptJump();
}

FVector UDMCharacterMovementComponent::ConstrainAnimRootMotionVelocity(
    const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const
{
    FVector velocity = Super::ConstrainAnimRootMotionVelocity(RootMotionVelocity, CurrentVelocity);
    if (IsEmoteMovementLocked())
    {
        velocity.X = 0.0;
        velocity.Y = 0.0;
        if (IsMovingOnGround())
        {
            velocity.Z = 0.0;
        }
    }
    return velocity;
}

float UDMCharacterMovementComponent::GetMaxSpeed() const
{
    if (IsEmoteMovementLocked())
    {
        return 0.0f;
    }

    const float DefaultMaxSpeed = Super::GetMaxSpeed();

    const ADMJellyCharacterBase* JellyCharacter = Cast<ADMJellyCharacterBase>(CharacterOwner);

    const UDMCharacterStateComponent* StateComponent =
        JellyCharacter != nullptr ? JellyCharacter->GetCharacterStateComponent() : nullptr;

    /*
     * Status.Stunned는
     * CharacterStateComponent가 SSOT다.
     *
     * 서버에서도 MaxSpeed가 0이므로
     * Client의 이동 입력 여부와 무관하게
     * Stun 중 실제 이동을 허용하지 않는다.
     */
    if (JellyCharacter != nullptr)
    {
        if (StateComponent != nullptr && StateComponent->HasStatus(DMGameplayTags::Status_Stunned))
        {
            return 0.0f;
        }
    }

    float DesiredMaxSpeed = DefaultMaxSpeed;

    if (bWantsToSprint && JellyCharacter != nullptr && JellyCharacter->CanSprint())
    {
        switch (MovementMode)
        {
        case MOVE_Walking:
        case MOVE_NavWalking:
        case MOVE_Falling:
            DesiredMaxSpeed = JellyCharacter->GetSprintSpeed();
            break;

        default:
            break;
        }
    }

    // 서버와 소유 클라이언트가 같은 OwnerOnly 상태를 사용해 이동 예측을 일치시킨다.
    if (StateComponent != nullptr && StateComponent->HasStatus(DMGameplayTags::Status_SlowMovement))
    {
        DesiredMaxSpeed *= SlowMovementSpeedMultiplier;
    }

    return DesiredMaxSpeed;
}

void UDMCharacterMovementComponent::OnMovementUpdated(
    const float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity
)
{
    Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);
#if WITH_EDITOR
    DMNPCMovementDiagnostics::RecordMovement(this, DeltaSeconds);
#endif

    /*
     * Quest-relevant sprint events are server-only.
     *
     * NPCs currently have no listener for this delegate,
     * so skip the extra evaluation when nobody is listening.
     */
    if (CharacterOwner == nullptr || !CharacterOwner->HasAuthority()
        || !AuthoritativeSprintStateChangedDelegate.IsBound())
    {
        return;
    }

    const bool bIsSprintingNow = IsAuthoritativeSprintActive();

    if (bWasAuthoritativeSprinting == bIsSprintingNow)
    {
        return;
    }

    bWasAuthoritativeSprinting = bIsSprintingNow;

    AuthoritativeSprintStateChangedDelegate.Broadcast(bIsSprintingNow);
}

bool UDMCharacterMovementComponent::IsAuthoritativeSprintActive() const
{
    if (!bWantsToSprint)
    {
        return false;
    }

    const ADMJellyCharacterBase* JellyCharacter = Cast<ADMJellyCharacterBase>(CharacterOwner);

    if (JellyCharacter == nullptr || !JellyCharacter->CanSprint())
    {
        return false;
    }

    /*
     * Sprint Quest counts actual ground running.
     * Holding Shift while standing still does not count.
     */
    if (MovementMode != MOVE_Walking && MovementMode != MOVE_NavWalking)
    {
        return false;
    }

    const FVector HorizontalVelocity(Velocity.X, Velocity.Y, 0.0f);

    return !HorizontalVelocity.IsNearlyZero(1.0f);
}

#pragma endregion

#pragma region Network Prediction

void UDMCharacterMovementComponent::UpdateFromCompressedFlags(const uint8 Flags)
{
    Super::UpdateFromCompressedFlags(Flags);

    bWantsToSprint = !IsEmoteMovementLocked() && (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
}

FNetworkPredictionData_Client* UDMCharacterMovementComponent::GetPredictionData_Client() const
{
    if (ClientPredictionData == nullptr)
    {
        UDMCharacterMovementComponent* MutableThis = const_cast<UDMCharacterMovementComponent*>(this);

        MutableThis->ClientPredictionData = new FNetworkPredictionData_Client_DMCharacter(*this);
    }

    return ClientPredictionData;
}

#pragma endregion
#if WITH_EDITOR
void UDMCharacterMovementComponent::OnCharacterStuckInGeometry(const FHitResult* Hit)
{
    DMNPCMovementDiagnostics::RecordStuck(this, Hit);
    Super::OnCharacterStuckInGeometry(Hit);
}
#endif

#if WITH_EDITOR
void UDMCharacterMovementComponent::HandleImpact(const FHitResult& Hit, const float TimeSlice, const FVector& MoveDelta)
{
    // 부모 처리 전 충돌 정보를 관측하되 모든 경우 부모를 정확히 한 번 호출한다.
    if (ADMJellyAIController::IsDetailedNPCDiagnosticsEnabled() && IsValid(CharacterOwner)
        && CharacterOwner->HasAuthority() && CharacterOwner->IsA<ADMNPCCharacter>()
        && GetWorld() && GetWorld()->IsGameWorld() && GetWorld()->GetNetMode() != NM_Client)
    {
        const double Now = GetWorld()->GetTimeSeconds();
        if (NPCDiagnosticLastImpactTime < 0.0 || Now - NPCDiagnosticLastImpactTime >= 1.0)
        {
            const ADMJellyAIController* AI = Cast<ADMJellyAIController>(CharacterOwner->GetController());
            const UPathFollowingComponent* Path = AI ? AI->GetPathFollowingComponent() : nullptr;
            const bool bHasRequest = Path && Path->GetCurrentRequestId().IsValid();
            UE_LOG(LogTemp, Log,
                TEXT("[NPCDetail] Impact World=%s Time=%.6f NPC=%s HasRequest=%d Request=%u HitActor=%s HitComponent=%s Location=%s ImpactPoint=%s ImpactNormal=%s Normal=%s Blocking=%d StartPenetrating=%d Depth=%.3f Pos=%s Velocity=%s Mode=%d TimeSlice=%.6f MoveDelta=%s Suppressed=%u"),
                *GetNameSafe(GetWorld()), Now, *GetNameSafe(CharacterOwner), bHasRequest,
                bHasRequest ? uint32(Path->GetCurrentRequestId()) : 0, *GetPathNameSafe(Hit.GetActor()),
                *GetPathNameSafe(Hit.GetComponent()), *Hit.Location.ToString(), *Hit.ImpactPoint.ToString(),
                *Hit.ImpactNormal.ToString(), *Hit.Normal.ToString(), int32(Hit.bBlockingHit),
                int32(Hit.bStartPenetrating), Hit.PenetrationDepth, *CharacterOwner->GetActorLocation().ToString(),
                *Velocity.ToString(), int32(MovementMode), TimeSlice, *MoveDelta.ToString(), NPCDiagnosticSuppressedImpacts);
            NPCDiagnosticLastImpactTime = Now;
            NPCDiagnosticSuppressedImpacts = 0;
        }
        else
        {
            ++NPCDiagnosticSuppressedImpacts;
        }
    }
    else
    {
        NPCDiagnosticLastImpactTime = -1.0;
        NPCDiagnosticSuppressedImpacts = 0;
    }
    Super::HandleImpact(Hit, TimeSlice, MoveDelta);
}
#endif
