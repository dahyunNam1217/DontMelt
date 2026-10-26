#include "Framework/Match/DMMatchPlayerState.h"

#include "Core/DMMatchConstants.h"

#include "Core/DMGameInstance.h"
#include "Engine/World.h"
#include "Quests/DMQuestProgressComponent.h"
#include "Skills/DMSkillComponent.h"
#include "Net/UnrealNetwork.h"

ADMMatchPlayerState::ADMMatchPlayerState()
{
    QuestProgressComponent = CreateDefaultSubobject<UDMQuestProgressComponent>(TEXT("QuestProgressComponent"));

    SkillComponent = CreateDefaultSubobject<UDMSkillComponent>(TEXT("SkillComponent"));
}

void ADMMatchPlayerState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION(ADMMatchPlayerState, LifeState, COND_OwnerOnly);

    DOREPLIFETIME_CONDITION(ADMMatchPlayerState, BaseSlot, COND_OwnerOnly);

    DOREPLIFETIME_CONDITION(ADMMatchPlayerState, bIsCarryingRelic, COND_OwnerOnly);

    DOREPLIFETIME(ADMMatchPlayerState, bIsRematchReady);
}

void ADMMatchPlayerState::SetLifeState(const EDMPlayerLifeState NewLifeState)
{
    if (!HasAuthority())
    {
        return;
    }

    if (LifeState == NewLifeState)
    {
        return;
    }

    LifeState = NewLifeState;
    HandleLifeStateChanged();
}

void ADMMatchPlayerState::SetBaseSlot(const int32 NewBaseSlot)
{
    if (!HasAuthority())
    {
        return;
    }

    const bool bIsValidBaseSlot = NewBaseSlot == INDEX_NONE || DMMatch::IsValidBaseSlot(NewBaseSlot);

    if (!bIsValidBaseSlot)
    {
        return;
    }

    if (BaseSlot == NewBaseSlot)
    {
        return;
    }

    BaseSlot = NewBaseSlot;
    HandleBaseSlotChanged();
}

void ADMMatchPlayerState::SetRematchReady(const bool bNewReady)
{
    if (!HasAuthority() || bIsRematchReady == bNewReady)
    {
        return;
    }

    bIsRematchReady = bNewReady;

    HandleRematchReadyChanged();
    ForceNetUpdate();
}

void ADMMatchPlayerState::IncrementKillCount()
{
    if (!HasAuthority())
    {
        return;
    }

    ++KillCount;
}

void ADMMatchPlayerState::IncrementAcquiredRelicCount()
{
    // 서버에서 확정된 획득만 집계한다.
    if (!HasAuthority())
    {
        return;
    }

    ++AcquiredRelicCount;
}

void ADMMatchPlayerState::OnRep_IsRematchReady()
{
    HandleRematchReadyChanged();
}

void ADMMatchPlayerState::HandleRematchReadyChanged()
{
    RematchReadyChangedDelegate.Broadcast(this, bIsRematchReady);
}

void ADMMatchPlayerState::SetCarryingRelicProjection(const bool bNewIsCarryingRelic)
{
    if (!HasAuthority())
    {
        return;
    }

    if (bIsCarryingRelic == bNewIsCarryingRelic)
    {
        return;
    }

    bIsCarryingRelic = bNewIsCarryingRelic;

    HandleCarryStateChanged();

    ForceNetUpdate();
}

void ADMMatchPlayerState::OnRep_IsCarryingRelic()
{
    HandleCarryStateChanged();
}

void ADMMatchPlayerState::HandleCarryStateChanged()
{
    CarryStateChangedDelegate.Broadcast(this, bIsCarryingRelic);
}

void ADMMatchPlayerState::OnRep_LifeState()
{
    HandleLifeStateChanged();
}

void ADMMatchPlayerState::OnRep_BaseSlot()
{
    HandleBaseSlotChanged();
}

void ADMMatchPlayerState::HandleLifeStateChanged()
{
    LifeStateChangedDelegate.Broadcast(this, LifeState);
}

void ADMMatchPlayerState::HandleBaseSlotChanged()
{
    UWorld* World = GetWorld();

    UDMGameInstance* GameInstance = IsValid(World) ? Cast<UDMGameInstance>(World->GetGameInstance()) : nullptr;

    if (IsValid(GameInstance))
    {
        GameInstance->RefreshLocalMatchIdentity(World);
    }

    BaseSlotChangedDelegate.Broadcast(this, BaseSlot);
}