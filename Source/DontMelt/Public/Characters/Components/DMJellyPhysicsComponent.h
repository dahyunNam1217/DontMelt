// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DMJellyPhysicsComponent.generated.h"

/**
 * 젤리 캐릭터의 부분 물리, 가짜 죽음, 실제 사망 래그돌 표현을
 * 담당할 컴포넌트.
 *
 * 게임플레이 상태의 원본은 소유하지 않는다.
 */


UCLASS( ClassGroup=(DontMelt), meta=(BlueprintSpawnableComponent) )
class DONTMELT_API UDMJellyPhysicsComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UDMJellyPhysicsComponent();


};
