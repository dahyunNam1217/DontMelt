// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DMInteractionComponent.generated.h"

/**
 * 플레이어의 상호작용 후보 탐색과 상호작용 진행을 담당할 컴포넌트.
 *
 * 유물 규칙과 유물 상태의 원본은 소유하지 않는다.
 */


UCLASS( ClassGroup=(DontMelt), meta=(BlueprintSpawnableComponent) )
class DONTMELT_API UDMInteractionComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UDMInteractionComponent();


};
