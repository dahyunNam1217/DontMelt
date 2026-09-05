// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DMPoseComponent.generated.h"


UCLASS( ClassGroup=(DontMelt), meta=(BlueprintSpawnableComponent) )
class DONTMELT_API UDMPoseComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UDMPoseComponent();

	
};
