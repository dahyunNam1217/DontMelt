#include "Characters/Components/DMCharacterStateComponent.h"

#include "Core/DMGameplayTags.h"
#include "Core/DMMatchTypes.h"
#include "Framework/Match/DMMatchPlayerState.h"
#include "Characters/Components/DMPoseComponent.h"
#include "Characters/Components/DMCharacterAudioComponent.h"
#include "Characters/Components/DMCharacterVFXComponent.h"
#include "Characters/DMJellyCharacterBase.h"

#include "Engine/World.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "TimerManager.h"
#include "Net/UnrealNetwork.h"

#pragma region Lifecycle

UDMCharacterStateComponent::UDMCharacterStateComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    SetIsReplicatedByDefault(true);
}

void UDMCharacterStateComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(UDMCharacterStateComponent, ActionState);

    DOREPLIFETIME_CONDITION(UDMCharacterStateComponent, ActiveStatusTags, COND_OwnerOnly);

    // 투명화 시간은 효과를 받은 소유 플레이어에게만 전달한다.
    DOREPLIFETIME_CONDITION(UDMCharacterStateComponent, InvisibleTiming, COND_OwnerOnly);

    // 스턴 표현용 bool은 관찰자에게도 복제한다.
    // 실제 상태 태그의 OwnerOnly 정책은 변경하지 않는다.
    DOREPLIFETIME(UDMCharacterStateComponent, bStunPresentationActive);

    // 투명화 표현용 bool도 관찰자에게 복제한다. 실제 상태 태그는 계속 OwnerOnly다.
    DOREPLIFETIME(UDMCharacterStateComponent, bInvisibilityPresentationActive);
}

void UDMCharacterStateComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearAllTimersForObject(this);
    }

    StatusExpiryTimers.Empty();

    Super::EndPlay(EndPlayReason);
}

#pragma endregion

#pragma region Action State

bool UDMCharacterStateComponent::CanPerformAction(const EDMActionRequest ActionRequest) const
{
    if (!IsLifeStateAllowed())
    {
        return false;
    }

    if (!IsStatusAllowedForAction(ActionRequest))
    {
        return false;
    }

    /*
     * Move / Sprint / Jump는 주요 ActionState 자체가 아니다.
     *
     * 기획에서 이동 금지가 명확한
     * Interact / FakeDeath에 대해서만
     * 현재 이동 입력을 차단한다.
     *
     * Attack / Defense 중 이동 가능 여부는
     * 아직 기획상 확정되지 않았으므로
     * 여기서 임의로 제한하지 않는다.
     */
    switch (ActionRequest)
    {
    case EDMActionRequest::Move:
    case EDMActionRequest::Sprint:
    case EDMActionRequest::Jump:

        return ActionState != EDMActionState::Interact && ActionState != EDMActionState::FakeDeath;

    default:
        break;
    }

    /*
     * 주요 Gameplay Action은 상호 배타.
     */
    if (ActionState != EDMActionState::None)
    {
        return false;
    }

    return true;
}

bool UDMCharacterStateComponent::TryBeginAction(const EDMActionState NewActionState)
{
    AActor* OwnerActor = GetOwner();

    if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
    {
        return false;
    }

    if (NewActionState == EDMActionState::None)
    {
        return false;
    }

    if (ActionState != EDMActionState::None)
    {
        return false;
    }

    ActionState = NewActionState;

    OwnerActor->ForceNetUpdate();

    return true;
}

void UDMCharacterStateComponent::EndAction(const EDMActionState ExpectedActionState)
{
    AActor* OwnerActor = GetOwner();

    if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
    {
        return;
    }

    if (ActionState != ExpectedActionState)
    {
        return;
    }

    const bool bEndingEmote = ActionState == EDMActionState::Emote;

    const FName EndedEmoteId = bEndingEmote ? ActiveEmoteId : NAME_None;

    ActionState = EDMActionState::None;

    if (bEndingEmote && !EndedEmoteId.IsNone())
    {
        ActiveEmoteId = NAME_None;

        MulticastEndEmote(EndedEmoteId);

        AuthoritativeEmoteStateChangedDelegate.Broadcast(false, EndedEmoteId);
    }

    OwnerActor->ForceNetUpdate();
}

#pragma endregion

#pragma region Emote

bool UDMCharacterStateComponent::CanRequestEmote(const FName EmoteId) const
{
    if (EmoteId.IsNone())
    {
        return false;
    }

    if (!CanPerformAction(EDMActionRequest::Emote))
    {
        return false;
    }

    const AActor* OwnerActor = GetOwner();

    if (OwnerActor == nullptr)
    {
        return false;
    }

    const UDMPoseComponent* PoseComponent = OwnerActor->FindComponentByClass<UDMPoseComponent>();

    if (PoseComponent != nullptr && !PoseComponent->IsActionAllowedByPose(EDMActionRequest::Emote))
    {
        return false;
    }

    return true;
}

void UDMCharacterStateComponent::RequestEmote_Implementation(const FName EmoteId)
{
    AActor* OwnerActor = GetOwner();

    if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
    {
        return;
    }

    if (!CanRequestEmote(EmoteId))
    {
        return;
    }

    MulticastPresentEmote(EmoteId);
}

void UDMCharacterStateComponent::RequestBeginEmote_Implementation(const FName EmoteId)
{
    AActor* OwnerActor = GetOwner();

    if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
    {
        return;
    }

    if (!CanRequestEmote(EmoteId))
    {
        return;
    }

    if (!TryBeginAction(EDMActionState::Emote))
    {
        return;
    }

    ActiveEmoteId = EmoteId;

    MulticastPresentEmote(EmoteId);

    AuthoritativeEmoteStateChangedDelegate.Broadcast(true, EmoteId);
}

void UDMCharacterStateComponent::RequestEndEmote_Implementation()
{
    AActor* OwnerActor = GetOwner();

    if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
    {
        return;
    }

    if (ActionState != EDMActionState::Emote)
    {
        return;
    }

    EndAction(EDMActionState::Emote);
}

void UDMCharacterStateComponent::MulticastPresentEmote_Implementation(const FName EmoteId)
{
    LastPresentedEmoteId = EmoteId;

    OnEmotePresentationRequested.Broadcast(EmoteId);
}

void UDMCharacterStateComponent::MulticastEndEmote_Implementation(const FName EmoteId)
{
    // 복제된 ActionState로 이미 종료한 이모트라면 다시 종료하지 않는다.
    if (!GetOwner()->HasAuthority() && LastPresentedEmoteId != EmoteId)
    {
        return;
    }

    LastPresentedEmoteId = NAME_None;

    OnEmotePresentationEnded.Broadcast(EmoteId);
}

void UDMCharacterStateComponent::OnRep_ActionState(const EDMActionState PreviousActionState)
{
    // 종료 RPC가 유실되어도 Emote 상태가 끝났다면 연출을 정리한다.
    if (PreviousActionState == EDMActionState::Emote && ActionState != EDMActionState::Emote
        && !LastPresentedEmoteId.IsNone())
    {
        const FName EndedEmoteId = LastPresentedEmoteId;

        LastPresentedEmoteId = NAME_None;

        OnEmotePresentationEnded.Broadcast(EndedEmoteId);
    }

    ReplicatedActionStateChangedDelegate.Broadcast(PreviousActionState, ActionState);
}

#pragma endregion

#pragma region Status

bool UDMCharacterStateComponent::HasStatus(const FGameplayTag& StatusTag) const
{
    if (!StatusTag.IsValid())
    {
        return false;
    }

    return ActiveStatusTags.HasTagExact(StatusTag);
}

bool UDMCharacterStateComponent::ApplyTimedStatus(const FGameplayTag& StatusTag, const float Duration)
{
    AActor* OwnerActor = GetOwner();

    if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
    {
        return false;
    }

    if (!StatusTag.IsValid() || Duration <= 0.0f)
    {
        return false;
    }

    if (HasStatus(StatusTag))
    {
        return false;
    }

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return false;
    }

    // 투명화일 때만 서버 기준 종료 시각을 기록한다.
    if (StatusTag.MatchesTagExact(DMGameplayTags::Status_Invisible))
    {
        InvisibleTiming.StartServerTime = static_cast<double>(World->GetTimeSeconds());

        InvisibleTiming.EndServerTime = InvisibleTiming.StartServerTime + static_cast<double>(Duration);
    }

    ActiveStatusTags.AddTag(StatusTag);

    // Listen Server의 로컬 UI에도 변경을 전달한다.
    StatusChangedDelegate.Broadcast();

    /*
     * Status 원본이 실제로 추가된 이후
     * 필요한 Presentation을 전달한다.
     */
    HandleStatusPresentationChanged(StatusTag);

    if (StatusTag.MatchesTagExact(DMGameplayTags::Status_Stunned))
    {
        ADMJellyCharacterBase* OwnerCharacter = Cast<ADMJellyCharacterBase>(OwnerActor);

        if (OwnerCharacter != nullptr)
        {
            /*
             * Stun 시작 순간 기존 Sprint 의도를 제거한다.
             */
            OwnerCharacter->ClearSprintRequest();

            /*
             * Jump 버튼 유지 상태를 해제한다.
             */
            OwnerCharacter->StopJumping();

            /*
             * 현재 남아 있는 이동 Velocity 제거.
             */
            if (UCharacterMovementComponent* MovementComponent = OwnerCharacter->GetCharacterMovement())
            {
                MovementComponent->StopMovementImmediately();
            }
        }
    }

    FTimerHandle& TimerHandle = StatusExpiryTimers.FindOrAdd(StatusTag);

    FTimerDelegate TimerDelegate;

    TimerDelegate.BindUObject(this, &ThisClass::HandleStatusExpired, StatusTag);

    World->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, Duration, false);

    OwnerActor->ForceNetUpdate();

    return true;
}

void UDMCharacterStateComponent::OnRep_ActiveStatusTags()
{
    // 클라이언트가 새 상태이상을 받았으므로 UI에 알린다.
    StatusChangedDelegate.Broadcast();
}

void UDMCharacterStateComponent::OnRep_InvisibleTiming()
{
    // 시간 정보가 변경되면 기존 상태 HUD 갱신 경로를 재사용한다.
    StatusChangedDelegate.Broadcast();
}

bool UDMCharacterStateComponent::RemoveStatus(const FGameplayTag& StatusTag)
{
    AActor* OwnerActor = GetOwner();

    if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
    {
        return false;
    }

    if (!StatusTag.IsValid() || !HasStatus(StatusTag))
    {
        return false;
    }

    if (FTimerHandle* TimerHandle = StatusExpiryTimers.Find(StatusTag))
    {
        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().ClearTimer(*TimerHandle);
        }

        StatusExpiryTimers.Remove(StatusTag);
    }

    ActiveStatusTags.RemoveTag(StatusTag);

    // 투명화가 취소되면 시간 정보도 초기화한다.
    if (StatusTag.MatchesTagExact(DMGameplayTags::Status_Invisible))
    {
        InvisibleTiming = FDMInvisibleTiming();
    }

    // 서버에서 상태가 제거되었음을 로컬 UI에 알린다.
    StatusChangedDelegate.Broadcast();

    HandleStatusPresentationChanged(StatusTag);

    OwnerActor->ForceNetUpdate();

    return true;
}

void UDMCharacterStateComponent::HandleStatusExpired(const FGameplayTag StatusTag)
{
    AActor* OwnerActor = GetOwner();

    if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
    {
        return;
    }

    StatusExpiryTimers.Remove(StatusTag);

    if (!HasStatus(StatusTag))
    {
        return;
    }

    ActiveStatusTags.RemoveTag(StatusTag);

    // 서버 타이머로 투명화가 만료된 경우에도 시간 정보를 정리한다.
    if (StatusTag.MatchesTagExact(DMGameplayTags::Status_Invisible))
    {
        InvisibleTiming = FDMInvisibleTiming();
    }

    // 서버 타이머에 의해 상태가 만료된 경우에도 UI를 갱신한다.
    StatusChangedDelegate.Broadcast();

    HandleStatusPresentationChanged(StatusTag);

    OwnerActor->ForceNetUpdate();
}

#pragma endregion

void UDMCharacterStateComponent::HandleStatusPresentationChanged(const FGameplayTag& StatusTag)
{
    // 스턴 원본 상태에서 관찰자용 표현값을 계산한다.
    if (StatusTag.MatchesTagExact(DMGameplayTags::Status_Stunned))
    {
        const bool bPreviousActive = bStunPresentationActive;
        bStunPresentationActive = HasStatus(DMGameplayTags::Status_Stunned);

        // 서버에서는 RepNotify가 자동 실행되지 않으므로 직접 적용한다.
        ApplyStunPresentation();

        // false → true 전이에서만 시작음을 재생한다.
        if (!bPreviousActive && bStunPresentationActive)
        {
            if (const ADMJellyCharacterBase* OwnerCharacter = Cast<ADMJellyCharacterBase>(GetOwner()))
            {
                if (UDMCharacterAudioComponent* AudioComponent = OwnerCharacter->GetCharacterAudioComponent())
                {
                    AudioComponent->PlayStunSound();
                }
            }
        }
    }

    // 투명화 표현 상태 동기화
    if (StatusTag.MatchesTagExact(DMGameplayTags::Status_Invisible))
    {
        // Gameplay 원본 상태에서 관찰자용 표현값을 계산한다.
        bInvisibilityPresentationActive = HasStatus(DMGameplayTags::Status_Invisible);

        // RepNotify는 서버에서 자동 호출되지 않으므로 직접 적용한다.
        ApplyInvisibilityPresentation(bInvisibilityPresentationActive);
    }
}


void UDMCharacterStateComponent::OnRep_StunPresentationActive(const bool bPreviousActive)
{
    // 클라이언트는 기존 복제된 표현값만 사용한다.
    ApplyStunPresentation();

    // 시작 시 한 번만 울린다. 기존 스턴 VFX와 종료 처리는 변경하지 않는다.
    if (!bPreviousActive && bStunPresentationActive)
    {
        if (const ADMJellyCharacterBase* OwnerCharacter = Cast<ADMJellyCharacterBase>(GetOwner()))
        {
            if (UDMCharacterAudioComponent* AudioComponent = OwnerCharacter->GetCharacterAudioComponent())
            {
                AudioComponent->PlayStunSound();
            }
        }
    }
}

void UDMCharacterStateComponent::ApplyStunPresentation()
{
    ADMJellyCharacterBase* OwnerCharacter = Cast<ADMJellyCharacterBase>(GetOwner());

    if (!IsValid(OwnerCharacter))
    {
        return;
    }

    UDMCharacterVFXComponent* VFXComponent = OwnerCharacter->GetCharacterVFXComponent();

    if (!IsValid(VFXComponent))
    {
        return;
    }

    // Gameplay 판정 없이 현재 표현값에 맞춰 시작 또는 종료한다.
    VFXComponent->SetStunEffectActive(bStunPresentationActive);
}

void UDMCharacterStateComponent::OnRep_InvisibilityPresentationActive()
{
    // 멀티캐스트와 같은 값을 다시 적용해도 결과는 같다.
    ApplyInvisibilityPresentation(bInvisibilityPresentationActive);
}

void UDMCharacterStateComponent::ApplyInvisibilityPresentation(const bool bInvisible)
{
    ADMJellyCharacterBase* OwnerCharacter = Cast<ADMJellyCharacterBase>(GetOwner());

    if (!IsValid(OwnerCharacter))
    {
        return;
    }

    USkeletalMeshComponent* MeshComponent = OwnerCharacter->GetMesh();

    if (IsValid(MeshComponent))
    {
        if (bInvisible)
        {
            // 첫 투명화 적용 시 현재 머티리얼을 슬롯별로 저장한다.
            if (OriginalMeshMaterials.IsEmpty())
            {
                const int32 MaterialCount = MeshComponent->GetNumMaterials();

                for (int32 Index = 0; Index < MaterialCount; ++Index)
                {
                    OriginalMeshMaterials.Add(MeshComponent->GetMaterial(Index));
                }
            }

            // 현재 로컬 플레이어가 조작하는 캐릭터인지 확인한다.
            const bool bOwnerView = OwnerCharacter->IsLocallyControlled();

            UMaterialInterface* TargetMaterial =
                bOwnerView ? OwnerInvisibilityMaterial.Get() : ObserverInvisibilityMaterial.Get();

            if (IsValid(TargetMaterial))
            {
                const int32 MaterialCount = FMath::Min(OriginalMeshMaterials.Num(), MeshComponent->GetNumMaterials());

                for (int32 Index = 0; Index < MaterialCount; ++Index)
                {
                    // 같은 머티리얼이면 불필요한 변경을 수행하지 않는다.
                    if (MeshComponent->GetMaterial(Index) != TargetMaterial)
                    {
                        MeshComponent->SetMaterial(Index, TargetMaterial);
                    }
                }
            }
            else
            {
                UE_LOG(
                    LogTemp,
                    Error,
                    TEXT("[%s] Invisibility material is not assigned. OwnerView=%d"),
                    *GetNameSafe(OwnerCharacter),
                    bOwnerView ? 1 : 0
                );
            }
        }
        else
        {
            // 투명화 이전에 사용하던 모든 슬롯을 복원한다.
            const int32 MaterialCount = FMath::Min(OriginalMeshMaterials.Num(), MeshComponent->GetNumMaterials());

            for (int32 Index = 0; Index < MaterialCount; ++Index)
            {
                UMaterialInterface* OriginalMaterial = OriginalMeshMaterials[Index].Get();

                if (MeshComponent->GetMaterial(Index) != OriginalMaterial)
                {
                    MeshComponent->SetMaterial(Index, OriginalMaterial);
                }
            }

            // 다음 투명화에서 새로운 원본을 저장하도록 초기화한다.
            OriginalMeshMaterials.Reset();
        }
    }

    // 기존 투명화 발소리 표현은 그대로 유지한다.
    if (UDMCharacterAudioComponent* AudioComponent = OwnerCharacter->GetCharacterAudioComponent())
    {
        AudioComponent->SetInvisibilityPresentationActive(bInvisible);
    }
}

#pragma region Validation

bool UDMCharacterStateComponent::IsLifeStateAllowed() const
{
    const APawn* OwnerPawn = Cast<APawn>(GetOwner());

    if (OwnerPawn == nullptr)
    {
        return true;
    }

    const ADMMatchPlayerState* MatchPlayerState = Cast<ADMMatchPlayerState>(OwnerPawn->GetPlayerState());

    /*
     * NPC에는 MatchPlayerState가 없으므로
     * LifeState 제한을 적용하지 않는다.
     */
    if (MatchPlayerState == nullptr)
    {
        return true;
    }

    return MatchPlayerState->GetLifeState() == EDMPlayerLifeState::Alive;
}

bool UDMCharacterStateComponent::IsStatusAllowedForAction(const EDMActionRequest ActionRequest) const
{
    if (HasStatus(DMGameplayTags::Status_Stunned))
    {
        switch (ActionRequest)
        {
        case EDMActionRequest::Move:
        case EDMActionRequest::Sprint:
        case EDMActionRequest::Jump:
        case EDMActionRequest::Attack:
        case EDMActionRequest::Defense:
        case EDMActionRequest::Interact:
        case EDMActionRequest::Skill:
        case EDMActionRequest::Emote:
            return false;

        case EDMActionRequest::PoseChange:
            break;

        default:
            return false;
        }
    }

    if (HasStatus(DMGameplayTags::Status_SpawnProtected))
    {
        switch (ActionRequest)
        {
        case EDMActionRequest::Attack:
        case EDMActionRequest::Defense:
        case EDMActionRequest::Interact:
        case EDMActionRequest::Skill:
            return false;

        default:
            break;
        }
    }

    if (HasStatus(DMGameplayTags::Status_Invisible))
    {
        /*
         * Invisibility 중에는 공격 / 방어 /
         * Pose 변경 / Emote를 직접 수행할 수 없다.
         *
         * Attack은 예외적으로 CombatComponent에서
         * "투명 해제 → 기존 공격 검증" 순서로 처리한다.
         *
         * 즉 이 정책 자체에서는 Attack을 차단 상태로 유지한다.
         */
        switch (ActionRequest)
        {
        case EDMActionRequest::Attack:
        case EDMActionRequest::Defense:
        case EDMActionRequest::PoseChange:
        case EDMActionRequest::Emote:
            return false;

        default:
            break;
        }
    }

    return true;
}

#pragma endregion
