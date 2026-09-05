// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerState.h"
#include "DMLobbyPlayerState.generated.h"

/**
 * 로비 참가자 한 명의 공개 상태를 저장할 PlayerState.
 *
 * Ready와 Host 상태는 로비 구현 단계에 추가한다.
 */




class ADMLobbyGameMode;


/**
 * 로비 참가자의 준비 상태가 변경됐을 때 호출된다.
 *
 * 첫 번째 인자: 상태가 변경된 LobbyPlayerState
 * 두 번째 인자: 새로운 준비 상태
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(
    FDMOnLobbyReadyChanged,
    ADMLobbyPlayerState*,
    bool
);

/**
 * 로비 참가자의 방장 상태가 변경됐을 때 호출된다.
 *
 * 첫 번째 인자: 상태가 변경된 LobbyPlayerState
 * 두 번째 인자: 새로운 방장 상태
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(
    FDMOnLobbyHostChanged,
    ADMLobbyPlayerState*,
    bool
);

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
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    /** 현재 플레이어가 준비 상태인지 반환한다. */
    UFUNCTION(BlueprintPure, Category = "DM|Lobby|Player")
    bool IsReady() const
    {
        return bIsReady;
    }

    /** 현재 플레이어가 방장인지 반환한다. */
    UFUNCTION(BlueprintPure, Category = "DM|Lobby|Player")
    bool IsHost() const
    {
        return bIsHost;
    }



    /** C++ UI 또는 Presenter가 Ready 변경을 구독할 때 사용한다. */
    FDMOnLobbyReadyChanged& OnReadyChanged()
    {
        return ReadyChangedDelegate;
    }

    /** C++ UI 또는 Presenter가 Host 변경을 구독할 때 사용한다. */
    FDMOnLobbyHostChanged& OnHostChanged()
    {
        return HostChangedDelegate;
    }

private:
    /**
     * 로비 준비 상태.
     *
     * 모든 로비 참가자가 확인해야 하므로 전체 복제한다.
     */
    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_IsReady,
        BlueprintReadOnly,
        Category = "DM|Lobby|Player",
        meta = (AllowPrivateAccess = "true")
    )
    bool bIsReady = false;

    /**
     * 로비 방장 상태.
     *
     * 모든 로비 참가자가 확인해야 하므로 전체 복제한다.
     */
    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_IsHost,
        BlueprintReadOnly,
        Category = "DM|Lobby|Player",
        meta = (AllowPrivateAccess = "true")
    )
    bool bIsHost = false;

    /**
     * 준비 상태를 변경한다.
     *
     * 서버에서만 호출해야 한다.
     * 클라이언트 입력은 이후 PlayerController의 Server RPC를 거친다.
     */
    void SetReadyState(bool bNewReady);

    /**
     * 방장 상태를 변경한다.
     *
     * 서버의 LobbyGameMode만 호출하는 것을 원칙으로 한다.
     */
    void SetHostState(bool bNewHost);

    /** 클라이언트가 Ready 복제값을 수신했을 때 호출된다. */
    UFUNCTION()
    void OnRep_IsReady();

    /** 클라이언트가 Host 복제값을 수신했을 때 호출된다. */
    UFUNCTION()
    void OnRep_IsHost();

    /** 서버 Setter와 클라이언트 OnRep가 공유하는 Ready 알림 경로다. */
    void HandleReadyStateChanged();

    /** 서버 Setter와 클라이언트 OnRep가 공유하는 Host 알림 경로다. */
    void HandleHostStateChanged();

    FDMOnLobbyReadyChanged ReadyChangedDelegate;
    FDMOnLobbyHostChanged HostChangedDelegate;




};

