// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DMJellyCharacterBase.generated.h"

class UDMCharacterStateComponent;
class UDMJellyPhysicsComponent;

/**
 * 플레이어와 NPC가 공유하는 젤리 인간 Character 기본 클래스.
 *
 * 공통 외형, 이동 기반, 상태 표현 및 물리 표현의 부착 지점만 제공한다.
 */

UCLASS(Abstract)
class DONTMELT_API ADMJellyCharacterBase : public ACharacter
{
	GENERATED_BODY()

public:
	
	ADMJellyCharacterBase();
protected:
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "DontMelt|Components",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UDMCharacterStateComponent> CharacterStateComponent;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "DontMelt|Components",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UDMJellyPhysicsComponent> JellyPhysicsComponent;

};
