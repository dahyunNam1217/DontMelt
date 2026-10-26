#pragma once

#include "CoreMinimal.h"
#include "Relic/DMRelicTypes.h"
#include "DMInteractionTypes.generated.h"

class AActor;

/**
 * 소유 플레이어에게만 공개되는
 * 현재 Interaction 진행 정보.
 *
 * 실제 성공 여부의 권위는 서버에 있다.
 */
USTRUCT(BlueprintType)
struct DONTMELT_API FDMInteractionSession
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DM|Interaction")
    TObjectPtr<AActor> TargetActor = nullptr;

    UPROPERTY(BlueprintReadOnly, Category = "DM|Interaction")
    EDMInteractionType InteractionType = EDMInteractionType::PickupRelic;

    UPROPERTY(BlueprintReadOnly, Category = "DM|Interaction")
    int32 RequestId = INDEX_NONE;

    UPROPERTY(BlueprintReadOnly, Category = "DM|Interaction")
    double StartServerTime = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "DM|Interaction")
    double EndServerTime = 0.0;

    bool IsActive() const
    {
        return RequestId != INDEX_NONE;
    }

    void Reset()
    {
        TargetActor = nullptr;
        InteractionType = EDMInteractionType::PickupRelic;

        RequestId = INDEX_NONE;
        StartServerTime = 0.0;
        EndServerTime = 0.0;
    }
};