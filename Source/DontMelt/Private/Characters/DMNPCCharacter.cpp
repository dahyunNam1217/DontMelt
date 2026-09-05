// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/DMNPCCharacter.h"

#include "Characters/DMJellyAIController.h"
#include "Engine/EngineTypes.h"

ADMNPCCharacter::ADMNPCCharacter()
{
    AIControllerClass = ADMJellyAIController::StaticClass();

    AutoPossessAI =
        EAutoPossessAI::PlacedInWorldOrSpawned;
}