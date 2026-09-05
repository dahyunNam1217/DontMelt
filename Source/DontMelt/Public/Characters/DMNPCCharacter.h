// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/DMJellyCharacterBase.h"
#include "DMNPCCharacter.generated.h"

/**
 * NPC 젤리 인간 Character.
 *
 * 플레이어 전용 Combat, Interaction, Pose Component를 소유하지 않는다.
 */


UCLASS()
class DONTMELT_API ADMNPCCharacter : public ADMJellyCharacterBase
{
    GENERATED_BODY()

public:
    ADMNPCCharacter();
};