// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Core/DMMatchTypes.h"
#include "GameFramework/PlayerState.h"
#include "DMMatchPlayerState.generated.h"

class ADMMatchPlayerState;
class ADMMatchGameMode;

/**
 * 플레이어의 매치 생명주기 상태가 변경됐을 때 호출된다.
 *
 * 첫 번째 인자: 상태가 변경된 MatchPlayerState
 * 두 번째 인자: 새로운 LifeState
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(
    FDMOnMatchPlayerLifeStateChanged,
    ADMMatchPlayerState*,
    EDMPlayerLifeState
);

/**
 * 플레이어의 BaseSlot이 변경됐을 때 호출된다.
 *
 * 첫 번째 인자: 상태가 변경된 MatchPlayerState
 * 두 번째 인자: 새로운 BaseSlot
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(
    FDMOnMatchPlayerBaseSlotChanged,
    ADMMatchPlayerState*,
    int32
);

/**
 * Pawn 교체 이후에도 유지돼야 하는 플레이어별 매치 상태를 소유한다.
 *
 * LifeState와 BaseSlot의 단일 원본이며,
 * Character나 GameMode에 같은 데이터를 중복 저장하지 않는다.
 */
UCLASS()
class DONTMELT_API ADMMatchPlayerState : public APlayerState
{
    GENERATED_BODY()

    friend class ADMMatchGameMode;

public:
    virtual void GetLifetimeReplicatedProps(
        TArray<FLifetimeProperty>& OutLifetimeProps
    ) const override;

    /** 현재 플레이어의 매치 생명주기 상태를 반환한다. */
    UFUNCTION(BlueprintPure, Category = "DM|Match|Player")
    EDMPlayerLifeState GetLifeState() const
    {
        return LifeState;
    }

    /**
     * 현재 플레이어에게 할당된 BaseSlot을 반환한다.
     *
     * 아직 할당되지 않았다면 INDEX_NONE(-1)을 반환한다.
     */
    UFUNCTION(BlueprintPure, Category = "DM|Match|Player")
    int32 GetBaseSlot() const
    {
        return BaseSlot;
    }

    /** 현재 플레이어에게 유효한 BaseSlot이 할당됐는지 반환한다. */
    UFUNCTION(BlueprintPure, Category = "DM|Match|Player")
    bool HasAssignedBaseSlot() const
    {
        return BaseSlot != INDEX_NONE;
    }

    

    /** C++ UI 또는 Presenter가 LifeState 변경을 구독한다. */
    FDMOnMatchPlayerLifeStateChanged& OnLifeStateChanged()
    {
        return LifeStateChangedDelegate;
    }

    /** C++ UI 또는 Presenter가 BaseSlot 변경을 구독한다. */
    FDMOnMatchPlayerBaseSlotChanged& OnBaseSlotChanged()
    {
        return BaseSlotChangedDelegate;
    }

private:
    /**
     * 플레이어의 현재 매치 생명주기 상태.
     *
     * 실제 Pawn 스폰과 Possess가 검증되기 전까지 Respawning이다.
     */
    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_LifeState,
        BlueprintReadOnly,
        Category = "DM|Match|Player",
        meta = (AllowPrivateAccess = "true")
    )
    EDMPlayerLifeState LifeState =
        EDMPlayerLifeState::Respawning;

    /**
     * 플레이어에게 할당된 시작 진영 슬롯.
     *
     * INDEX_NONE은 아직 BaseSlot이 할당되지 않았다는 의미다.
     */
    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_BaseSlot,
        BlueprintReadOnly,
        Category = "DM|Match|Player",
        meta = (AllowPrivateAccess = "true")
    )
    int32 BaseSlot = INDEX_NONE;

    /**
     * 플레이어의 매치 생명주기 상태를 변경한다.
     *
     * 서버에서만 호출해야 한다.
     */
    void SetLifeState(EDMPlayerLifeState NewLifeState);

    /**
     * 플레이어의 BaseSlot을 변경한다.
     *
     * 서버에서만 호출해야 한다.
     * 허용값은 INDEX_NONE 또는 0~3이다.
     */
    void SetBaseSlot(int32 NewBaseSlot);

    /** 소유 클라이언트가 LifeState 복제값을 받으면 호출된다. */
    UFUNCTION()
    void OnRep_LifeState();

    /** 소유 클라이언트가 BaseSlot 복제값을 받으면 호출된다. */
    UFUNCTION()
    void OnRep_BaseSlot();

    /** 서버 Setter와 클라이언트 OnRep가 공유하는 알림 경로다. */
    void HandleLifeStateChanged();

    /** 서버 Setter와 클라이언트 OnRep가 공유하는 알림 경로다. */
    void HandleBaseSlotChanged();

    FDMOnMatchPlayerLifeStateChanged
        LifeStateChangedDelegate;

    FDMOnMatchPlayerBaseSlotChanged
        BaseSlotChangedDelegate;
};