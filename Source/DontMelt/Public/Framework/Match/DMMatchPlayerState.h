#pragma once

#include "CoreMinimal.h"
#include "Core/DMMatchTypes.h"
#include "GameFramework/PlayerState.h"
#include "DMMatchPlayerState.generated.h"

class ADMMatchGameMode;
class ADMRelicManager;
class ADMMatchPlayerState;
class UDMQuestProgressComponent;
class UDMSkillComponent;

DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnMatchPlayerLifeStateChanged, ADMMatchPlayerState*, EDMPlayerLifeState);

DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnMatchPlayerBaseSlotChanged, ADMMatchPlayerState*, int32);

DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnMatchPlayerCarryStateChanged, ADMMatchPlayerState*, bool);

DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnMatchPlayerRematchReadyChanged, ADMMatchPlayerState*, bool);

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
    friend class ADMRelicManager;

    friend class UDMRespawnCoordinator;

public:
    ADMMatchPlayerState();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "DM|Match|Player")
    EDMPlayerLifeState GetLifeState() const
    {
        return LifeState;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Match|Player")
    int32 GetBaseSlot() const
    {
        return BaseSlot;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Match|Player")
    bool HasAssignedBaseSlot() const
    {
        return BaseSlot != INDEX_NONE;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Match|Player")
    bool IsCarryingRelic() const
    {
        return bIsCarryingRelic;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Match|Quest")
    UDMQuestProgressComponent* GetQuestProgressComponent() const
    {
        return QuestProgressComponent;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Match|Skill")
    UDMSkillComponent* GetSkillComponent() const
    {
        return SkillComponent;
    }

    FDMOnMatchPlayerCarryStateChanged& OnCarryStateChanged()
    {
        return CarryStateChangedDelegate;
    }

    FDMOnMatchPlayerLifeStateChanged& OnLifeStateChanged()
    {
        return LifeStateChangedDelegate;
    }

    FDMOnMatchPlayerBaseSlotChanged& OnBaseSlotChanged()
    {
        return BaseSlotChangedDelegate;
    }

    FDMOnMatchPlayerRematchReadyChanged& OnRematchReadyChanged()
    {
        return RematchReadyChangedDelegate;
    }

    // 서버에서 복제된 현재 재경기 준비 상태를 반환한다.
    bool IsRematchReady() const
    {
        return bIsRematchReady;
    }

private:
    void SetLifeState(EDMPlayerLifeState NewLifeState);

    void SetBaseSlot(int32 NewBaseSlot);

    void SetRematchReady(bool bNewReady);

    void IncrementKillCount();

    int32 GetKillCount() const
    {
        return KillCount;
    }

    // 서버에서 유물 획득(Pickup/Steal) Commit이 성공했을 때 호출한다.
    void IncrementAcquiredRelicCount();

    int32 GetAcquiredRelicCount() const
    {
        return AcquiredRelicCount;
    }

    UFUNCTION()
    void OnRep_IsRematchReady();

    void HandleRematchReadyChanged();

    UFUNCTION()
    void OnRep_LifeState();

    UFUNCTION()
    void OnRep_BaseSlot();

    void SetCarryingRelicProjection(bool bNewIsCarryingRelic);

    UFUNCTION()
    void OnRep_IsCarryingRelic();

    void HandleCarryStateChanged();

    void HandleLifeStateChanged();
    void HandleBaseSlotChanged();

    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_LifeState,
        BlueprintReadOnly,
        Category = "DM|Match|Player",
        meta = (AllowPrivateAccess = "true")
    )
    EDMPlayerLifeState LifeState = EDMPlayerLifeState::Respawning;

    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_BaseSlot,
        BlueprintReadOnly,
        Category = "DM|Match|Player",
        meta = (AllowPrivateAccess = "true")
    )
    int32 BaseSlot = INDEX_NONE;

    /**
     * Owner HUD용 Carry 상태 Projection.
     *
     * Carrier 관계의 권위 원본은
     * ADMRelicManager의 Carrier Map이다.
     */
    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_IsCarryingRelic,
        BlueprintReadOnly,
        Category = "DM|Match|Player",
        meta = (AllowPrivateAccess = "true")
    )
    bool bIsCarryingRelic = false;

    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_IsRematchReady,
        BlueprintReadOnly,
        Category = "DM|Match|Player",
        meta = (AllowPrivateAccess = "true")
    )
    bool bIsRematchReady = false;

    // 서버에서만 사용하는 통계이므로 복제하지 않는다.
    int32 KillCount = 0;

    // 결과표용 누적 유물 획득 수. 서버 전용이며 결과 Snapshot으로만 전달한다.
    int32 AcquiredRelicCount = 0;

    /**
     * Pawn Respawn과 무관하게 유지되는
     * 개인 Quest Progress Component.
     *
     * Component 내부 QuestProgress만 OwnerOnly 복제한다.
     * Component 포인터 자체를 별도로 Replicate하지 않는다.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DM|Match|Quest", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UDMQuestProgressComponent> QuestProgressComponent;

    /**
     * Pawn Respawn과 관계없이 유지되는
     * Player Skill Runtime Component.
     *
     * Skill cooldown의 SSOT이며
     * Character에 동일 cooldown 값을 저장하지 않는다.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DM|Match|Skill", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UDMSkillComponent> SkillComponent;

    FDMOnMatchPlayerCarryStateChanged CarryStateChangedDelegate;

    FDMOnMatchPlayerLifeStateChanged LifeStateChangedDelegate;

    FDMOnMatchPlayerBaseSlotChanged BaseSlotChangedDelegate;

    FDMOnMatchPlayerRematchReadyChanged RematchReadyChangedDelegate;
};
