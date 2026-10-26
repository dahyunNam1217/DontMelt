#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Relic/DMRelicTypes.h"
#include "DMBaseZone.generated.h"

class ADMMatchGameMode;
class UBoxComponent;
class USceneComponent;

/**
 * 진영 하나의 월드 영역과 Active/Inactive 상태를 소유한다.
 */
UCLASS()
class DONTMELT_API ADMBaseZone : public AActor
{
    GENERATED_BODY()

    friend class ADMMatchGameMode;

public:
    ADMBaseZone();

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    UFUNCTION(BlueprintPure, Category = "DM|Relic|Base")
    int32 GetBaseSlot() const
    {
        return BaseSlot;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Relic|Base")
    EDMBaseActivityState GetActivityState() const
    {
        return ActivityState;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Relic|Base")
    bool IsActiveBase() const
    {
        return ActivityState == EDMBaseActivityState::Active;
    }

private:
    void SetActivityState(EDMBaseActivityState NewState);

    UFUNCTION()
    void OnRep_ActivityState();

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DM|Relic|Base", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DM|Relic|Base", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBoxComponent> BaseBounds;

    /**
     * 이 월드 Zone이 나타내는 고정 진영 슬롯.
     *
     * PlayerState BaseSlot의 복사본이 아니다.
     */
    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "DM|Relic|Base",
        meta = (AllowPrivateAccess = "true", ClampMin = "0")
    )
    int32 BaseSlot = INDEX_NONE;

    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_ActivityState,
        BlueprintReadOnly,
        Category = "DM|Relic|Base",
        meta = (AllowPrivateAccess = "true")
    )
    EDMBaseActivityState ActivityState = EDMBaseActivityState::Active;
};