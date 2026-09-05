// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "AIController.h"
#include "DMJellyAIController.generated.h"

/**
 * 젤리 NPC 전용 AIController.
 *
 * Behavior Tree 또는 StateTree 선택 전이므로
 * 현재 단계에서는 행동 자산과 런타임 상태를 소유하지 않는다.
 */


UCLASS()
class DONTMELT_API ADMJellyAIController : public AAIController
{
    GENERATED_BODY()
};