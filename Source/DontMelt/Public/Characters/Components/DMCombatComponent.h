// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DMCombatComponent.generated.h"

/**
 * 플레이어의 공격과 방어 판정을 담당할 컴포넌트.
 *
 * 1단계에서는 공격, 방어, 쿨다운, RPC를 구현하지 않는다.
 */


UCLASS( ClassGroup=(DontMelt), meta=(BlueprintSpawnableComponent) )
class DONTMELT_API UDMCombatComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UDMCombatComponent();


		
};
