
#include "Framework/Lobby/DMLobbyGameState.h"

#include "Net/UnrealNetwork.h"

#pragma region Lifecycle

void ADMLobbyGameState::AddPlayerState(APlayerState* PlayerState)
{
    const int32 PreviousCount = PlayerArray.Num();

    Super::AddPlayerState(PlayerState);

    if (PlayerArray.Num() != PreviousCount)
    {
        LobbyDataChangedDelegate.Broadcast();
    }
}

void ADMLobbyGameState::RemovePlayerState(APlayerState* PlayerState)
{
    const int32 PreviousCount = PlayerArray.Num();

    Super::RemovePlayerState(PlayerState);

    if (PlayerArray.Num() != PreviousCount)
    {
        LobbyDataChangedDelegate.Broadcast();
    }
}

void ADMLobbyGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ADMLobbyGameState, MinPlayerCount);

    DOREPLIFETIME(ADMLobbyGameState, MaxPlayerCount);
}

#pragma endregion

#pragma region Lobby Rules

void ADMLobbyGameState::SetLobbyPlayerLimits(const int32 InMinPlayerCount, const int32 InMaxPlayerCount)
{
    if (!HasAuthority())
    {
        return;
    }

    if (MinPlayerCount == InMinPlayerCount && MaxPlayerCount == InMaxPlayerCount)
    {
        return;
    }

    MinPlayerCount = InMinPlayerCount;
    MaxPlayerCount = InMaxPlayerCount;

    LobbyDataChangedDelegate.Broadcast();

    ForceNetUpdate();
}

#pragma endregion

#pragma region RepNotify

void ADMLobbyGameState::OnRep_LobbyPlayerLimits()
{
    LobbyDataChangedDelegate.Broadcast();
}

#pragma endregion
