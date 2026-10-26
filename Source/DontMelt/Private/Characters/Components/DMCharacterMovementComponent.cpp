#include "Characters/Components/DMCharacterMovementComponent.h"

#include "Characters/DMJellyCharacterBase.h"
#include "Characters/Components/DMCharacterStateComponent.h"
#include "Core/DMGameplayTags.h"

#pragma region Network Prediction Internals

namespace
{
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

float UDMCharacterMovementComponent::GetMaxSpeed() const
{
    const float DefaultMaxSpeed = Super::GetMaxSpeed();

    const ADMJellyCharacterBase* JellyCharacter = Cast<ADMJellyCharacterBase>(CharacterOwner);

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
        const UDMCharacterStateComponent* StateComponent = JellyCharacter->GetCharacterStateComponent();

        if (StateComponent != nullptr && StateComponent->HasStatus(DMGameplayTags::Status_Stunned))
        {
            return 0.0f;
        }
    }

    if (!bWantsToSprint)
    {
        return DefaultMaxSpeed;
    }

    if (JellyCharacter == nullptr || !JellyCharacter->CanSprint())
    {
        return DefaultMaxSpeed;
    }

    switch (MovementMode)
    {
    case MOVE_Walking:
    case MOVE_NavWalking:
    case MOVE_Falling:

        return JellyCharacter->GetSprintSpeed();

    default:

        return DefaultMaxSpeed;
    }
}

void UDMCharacterMovementComponent::OnMovementUpdated(
    const float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity
)
{
    Super::OnMovementUpdated(DeltaSeconds, OldLocation, OldVelocity);

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

    bWantsToSprint = (Flags & FSavedMove_Character::FLAG_Custom_0) != 0;
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