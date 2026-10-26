#pragma once

#include "CoreMinimal.h"
#include "DMRelicTypes.generated.h"

/**
 * 유물 하나의 서버 권위 월드 상태.
 *
 * 동시에 하나의 상태만 존재한다.
 */
UENUM(BlueprintType)
enum class EDMRelicWorldState : uint8
{
    PublicSpawn UMETA(DisplayName = "Public Spawn"),
    Carried UMETA(DisplayName = "Carried"),
    Dropped UMETA(DisplayName = "Dropped"),
    Registered UMETA(DisplayName = "Registered"),
    DepositProtected UMETA(DisplayName = "Deposit Protected")
};

/**
 * Carried Relic이 Dropped 상태로 전환된 원인.
 *
 * Drop 자체는 InteractionType이 아니다.
 */
UENUM(BlueprintType)
enum class EDMRelicDropReason : uint8
{
    Manual UMETA(DisplayName = "Manual"),
    Death UMETA(DisplayName = "Death"),
    Disconnect UMETA(DisplayName = "Disconnect"),
    DepositFailure UMETA(DisplayName = "Deposit Failure")
};

/**
 * 공통 상호작용 파이프라인에서 구분할 상호작용 종류.
 */
UENUM(BlueprintType)
enum class EDMInteractionType : uint8
{
    InspectChest UMETA(DisplayName = "Inspect Chest"),
    StealRelic UMETA(DisplayName = "Steal Relic"),
    PickupRelic UMETA(DisplayName = "Pickup Relic"),
    DepositRelic UMETA(DisplayName = "Deposit Relic")
};

/**
 * 진영의 현재 활성 상태.
 */
UENUM(BlueprintType)
enum class EDMBaseActivityState : uint8
{
    Active UMETA(DisplayName = "Active"),
    Inactive UMETA(DisplayName = "Inactive")
};

/**
 * 사전에 배치된 Relic SpawnPoint의 용도.
 */
UENUM(BlueprintType)
enum class EDMRelicSpawnPurpose : uint8
{
    PublicStart UMETA(DisplayName = "Public Start"),
    Recovery UMETA(DisplayName = "Recovery"),
    DisconnectRecovery UMETA(DisplayName = "Disconnect Recovery")
};