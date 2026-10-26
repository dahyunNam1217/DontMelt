#pragma once

#include "CoreMinimal.h"
#include "UObject/Interface.h"
#include "DMInteractable.generated.h"

UINTERFACE(BlueprintType)
class DONTMELT_API UDMInteractable : public UInterface
{
    GENERATED_BODY()
};

/**
 * InteractionComponent가 상호작용 후보로 취급할 수 있는 Actor용 Interface.
 *
 * 실제 상호작용 가능 여부와 서버 검증은
 * UDMInteractionComponent가 담당한다.
 */
class DONTMELT_API IDMInteractable
{
    GENERATED_BODY()
};