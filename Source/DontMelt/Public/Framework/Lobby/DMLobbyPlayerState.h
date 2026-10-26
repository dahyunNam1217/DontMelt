#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "DMLobbyPlayerState.generated.h"

class ADMLobbyGameMode;
class ADMLobbyPlayerState;

DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnLobbyReadyChanged, ADMLobbyPlayerState*, bool);

DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnLobbyHostChanged, ADMLobbyPlayerState*, bool);

DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnLobbyPlayerNameChanged, ADMLobbyPlayerState*);

/**
 * 로비 참가자 한 명의 공개 상태를 소유한다.
 *
 * Ready와 Host 상태의 단일 원본이며,
 * LobbyGameMode는 이 값을 저장하지 않고 조회·검증만 수행한다.
 */
UCLASS()
class DONTMELT_API ADMLobbyPlayerState : public APlayerState
{
    GENERATED_BODY()

    friend class ADMLobbyGameMode;

public:
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    virtual void OnRep_PlayerName() override;
    virtual void SetPlayerName(const FString& S) override;

    FDMOnLobbyPlayerNameChanged& OnPlayerNameChanged()
    {
        return PlayerNameChangedDelegate;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Lobby|Player")
    bool IsReady() const
    {
        return bIsReady;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Lobby|Player")
    bool IsHost() const
    {
        return bIsHost;
    }

    FDMOnLobbyReadyChanged& OnReadyChanged()
    {
        return ReadyChangedDelegate;
    }

    FDMOnLobbyHostChanged& OnHostChanged()
    {
        return HostChangedDelegate;
    }

private:
    void SetReadyState(bool bNewReady);
    void SetHostState(bool bNewHost);

    UFUNCTION()
    void OnRep_IsReady();

    UFUNCTION()
    void OnRep_IsHost();

    void HandleReadyStateChanged();
    void HandleHostStateChanged();

    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_IsReady,
        BlueprintReadOnly,
        Category = "DM|Lobby|Player",
        meta = (AllowPrivateAccess = "true")
    )
    bool bIsReady = false;

    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_IsHost,
        BlueprintReadOnly,
        Category = "DM|Lobby|Player",
        meta = (AllowPrivateAccess = "true")
    )
    bool bIsHost = false;

    FDMOnLobbyReadyChanged ReadyChangedDelegate;
    FDMOnLobbyHostChanged HostChangedDelegate;
    FDMOnLobbyPlayerNameChanged PlayerNameChangedDelegate;
};
