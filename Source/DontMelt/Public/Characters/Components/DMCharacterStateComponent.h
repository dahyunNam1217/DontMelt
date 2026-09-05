// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DMCharacterStateComponent.generated.h"

/**
 * 플레이어와 NPC의 주요 행동 상태 및 동시 상태효과를 관리할 컴포넌트.
 *
 * 1단계에서는 상태 데이터와 상태 변경 함수를 만들지 않는다.
 * 현재 목적은 Character 계층의 소유 관계와 부착 위치를 확정하는 것이다.
 */


UCLASS( ClassGroup=(DontMelt), meta=(BlueprintSpawnableComponent) )
class DONTMELT_API UDMCharacterStateComponent : public UActorComponent
{
	GENERATED_BODY()

public:	
	// Sets default values for this component's properties
	UDMCharacterStateComponent();


};
