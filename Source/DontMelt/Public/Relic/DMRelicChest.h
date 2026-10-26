#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/DMInteractable.h"
#include "DMRelicChest.generated.h"

class ADMBaseZone;
class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;

/**
 * 한 진영의 Relic 상호작용 Endpoint.
 *
 * 유물 개수와 Base 활성 상태를 직접 소유하지 않는다.
 */
UCLASS()
class DONTMELT_API ADMRelicChest : public AActor, public IDMInteractable
{
    GENERATED_BODY()

public:
    ADMRelicChest();

    UFUNCTION(BlueprintPure, Category = "DM|Relic|Chest")
    int32 GetBaseSlot() const
    {
        return BaseSlot;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Relic|Chest")
    ADMBaseZone* GetBaseZone() const
    {
        return BaseZone;
    }

    /**
     * Chest와 BaseZone 설정이 서로 일치하는지 검사한다.
     */
    bool HasValidBaseBinding() const;

    /**
     * 이 Chest가 속한 Base가 현재 Active인지 반환한다.
     *
     * ActivityState를 Chest에 복사하지 않고
     * BaseZone을 직접 조회한다.
     */
    bool IsBaseActive() const;

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DM|Relic|Chest", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DM|Relic|Chest", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UStaticMeshComponent> ChestMesh;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DM|Relic|Chest", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UBoxComponent> InteractionBounds;

    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "DM|Relic|Chest",
        meta = (AllowPrivateAccess = "true", ClampMin = "0")
    )
    int32 BaseSlot = INDEX_NONE;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "DM|Relic|Chest", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<ADMBaseZone> BaseZone;
};