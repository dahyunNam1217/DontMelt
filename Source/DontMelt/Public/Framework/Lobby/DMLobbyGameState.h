// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameStateBase.h"
#include "DMLobbyGameState.generated.h"

/**
 * 모든 로비 참가자에게 공개 가능한 로비 데이터를 저장할 GameState.
 *
 * 1단계에서는 아직 로비 설정 Struct와 Replicated 변수를 만들지 않는다.
 */
UCLASS()
class DONTMELT_API ADMLobbyGameState : public AGameStateBase
{
    GENERATED_BODY()
};