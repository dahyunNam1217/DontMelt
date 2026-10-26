#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Relic/DMRelicTypes.h"
#include "DMRelicSpawnPoint.generated.h"

class USceneComponent;

/**
 * Relic의 시작/복구에 사용할 검증된 Level 위치.
 */
UCLASS()
class DONTMELT_API ADMRelicSpawnPoint : public AActor
{
    GENERATED_BODY()

public:
    ADMRelicSpawnPoint();

    UFUNCTION(BlueprintPure, Category = "DM|Relic|Spawn")
    EDMRelicSpawnPurpose GetPurpose() const
    {
        return Purpose;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Relic|Spawn")
    int32 GetBaseSlot() const
    {
        return BaseSlot;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Relic|Spawn")
    float GetWeight() const
    {
        return Weight;
    }

private:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DM|Relic|Spawn", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "DM|Relic|Spawn", meta = (AllowPrivateAccess = "true"))
    EDMRelicSpawnPurpose Purpose = EDMRelicSpawnPurpose::Recovery;

    /**
     * 특정 진영과 연결되는 SpawnPoint에만 사용한다.
     * 공용/일반 Recovery라면 INDEX_NONE.
     */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "DM|Relic|Spawn", meta = (AllowPrivateAccess = "true"))
    int32 BaseSlot = INDEX_NONE;

    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "DM|Relic|Spawn",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0")
    )
    float Weight = 1.0f;
};