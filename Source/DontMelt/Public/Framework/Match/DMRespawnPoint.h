#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerStart.h"
#include "DMRespawnPoint.generated.h"

/**
 * 사망 후 Player Respawn에 사용하는
 * Level 배치 Spawn 후보.
 *
 * 초기 입장용 DM_Base_X PlayerStart와는
 * 별개의 책임을 가진다.
 */
UCLASS()
class DONTMELT_API ADMRespawnPoint : public APlayerStart
{
    GENERATED_BODY()

public:
    ADMRespawnPoint(const FObjectInitializer& ObjectInitializer);

    UFUNCTION(BlueprintPure, Category = "DM|Respawn")
    int32 GetBaseSlot() const
    {
        return BaseSlot;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Respawn")
    float GetWeight() const
    {
        return Weight;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Respawn")
    bool IsRespawnEnabled() const
    {
        return bEnabled;
    }

private:
    /**
     * 이 Point를 사용할 Player의 BaseSlot.
     */
    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "DM|Respawn",
        meta = (AllowPrivateAccess = "true", ClampMin = "-1", ClampMax = "3")
    )
    int32 BaseSlot = INDEX_NONE;

    /**
     * 같은 Base 안에서 선택될 상대 가중치.
     *
     * 모두 1이면 동일 확률이다.
     */
    UPROPERTY(
        EditInstanceOnly,
        BlueprintReadOnly,
        Category = "DM|Respawn",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0")
    )
    float Weight = 1.0f;

    /**
     * false인 Point는 Respawn 후보에서 제외한다.
     */
    UPROPERTY(EditInstanceOnly, BlueprintReadOnly, Category = "DM|Respawn", meta = (AllowPrivateAccess = "true"))
    bool bEnabled = true;
};