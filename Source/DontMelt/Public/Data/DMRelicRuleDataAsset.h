#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DMRelicRuleDataAsset.generated.h"

/**
 * 유물 시스템의 불변 규칙과 밸런스 값을 정의한다.
 *
 * 런타임 Relic 상태를 저장하지 않는다.
 */
UCLASS(BlueprintType)
class DONTMELT_API UDMRelicRuleDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    UDMRelicRuleDataAsset();

    /**
     * 매치 시작 인원에 대응하는
     * 전체 유물 수와 승리 필요 유물 수를 조회한다.
     *
     * 유효한 규칙이 없으면 false를 반환한다.
     */
    bool TryGetRelicCountRule(int32 StartingPlayerCount, int32& OutTotalRelicCount, int32& OutRequiredRelicCount) const;

    float GetPickupDuration() const
    {
        return PickupDuration;
    }

    float GetDepositDuration() const
    {
        return DepositDuration;
    }

    float GetDepositProtectionDuration() const
    {
        return DepositProtectionDuration;
    }

    float GetSamePlayerRepickupDelay() const
    {
        return SamePlayerRepickupDelay;
    }

private:
#pragma region Count Rules

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Relic|Count", meta = (AllowPrivateAccess = "true"))
    TMap<int32, int32> TotalRelicCountByPlayerCount;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Relic|Count", meta = (AllowPrivateAccess = "true"))
    TMap<int32, int32> RequiredRelicCountByPlayerCount;

#pragma endregion

#pragma region Interaction Rules

    /** 유물 습득에 필요한 시간. */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Relic|Interaction",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0")
    )
    float PickupDuration = 2.0f;

    /** 유물 등록에 필요한 시간. */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Relic|Interaction",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0")
    )
    float DepositDuration = 2.0f;

    /** 등록 완료 후 유물을 다시 탈취할 수 없도록 보호하는 시간. */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Relic|Interaction",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0")
    )
    float DepositProtectionDuration = 5.0f;

    /** 수동 드롭한 플레이어의 동일 유물 재습득 제한 시간. */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Relic|Interaction",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0")
    )
    float SamePlayerRepickupDelay = 1.0f;

#pragma endregion
};