#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Events/DMRandomEventTypes.h"
#include "DMRandomEventSetDataAsset.generated.h"

/**
 * 현재 게임에서 사용할 Random Event 정의 목록.
 *
 * Runtime 상태는 보관하지 않는다.
 */
UCLASS(BlueprintType)
class DONTMELT_API UDMRandomEventSetDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    const TArray<FDMRandomEventDefinition>& GetEventDefinitions() const
    {
        return EventDefinitions;
    }

private:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Random Event", meta = (AllowPrivateAccess = "true"))
    TArray<FDMRandomEventDefinition> EventDefinitions;
};