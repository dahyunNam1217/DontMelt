#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"

class UWorld;

/** 실제 게임 객체를 사용하는 로컬 PIE 진입 정책. 플랫폼 ID를 생성하거나 저장하지 않는다. */
namespace DMDevPIEMatch
{
    bool IsEnabled(const UWorld* World, const FString& Options);
    bool IsLoopbackAddress(const FString& Address);

#if WITH_EDITOR && !UE_BUILD_SHIPPING
    bool BuildStartParameters(const UWorld* World, const FGameInstancePIEParameters& InParams,
        FGameInstancePIEParameters& OutParams, FString& OutError);
#endif
}
