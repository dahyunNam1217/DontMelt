
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"

#include "DMLobbyGameState.generated.h"

DECLARE_MULTICAST_DELEGATE(FDMOnLobbyDataChanged);

UCLASS()
class DONTMELT_API ADMLobbyGameState : public AGameStateBase
{
    GENERATED_BODY()

public:
#pragma region Lifecycle

    virtual void AddPlayerState(APlayerState* PlayerState) override;

    virtual void RemovePlayerState(APlayerState* PlayerState) override;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

#pragma region Lobby Rules

    void SetLobbyPlayerLimits(int32 InMinPlayerCount, int32 InMaxPlayerCount);

    int32 GetMinPlayerCount() const
    {
        return MinPlayerCount;
    }

    int32 GetMaxPlayerCount() const
    {
        return MaxPlayerCount;
    }

#pragma endregion

#pragma region Delegates

    FDMOnLobbyDataChanged& OnLobbyDataChanged()
    {
        return LobbyDataChangedDelegate;
    }

#pragma endregion

private:
#pragma region RepNotify

    UFUNCTION()
    void OnRep_LobbyPlayerLimits();

#pragma endregion

#pragma region Replicated Data

    UPROPERTY(ReplicatedUsing = OnRep_LobbyPlayerLimits)
    int32 MinPlayerCount = 0;

    UPROPERTY(ReplicatedUsing = OnRep_LobbyPlayerLimits)
    int32 MaxPlayerCount = 0;

#pragma endregion

#pragma region Delegates

    FDMOnLobbyDataChanged LobbyDataChangedDelegate;

#pragma endregion
};
