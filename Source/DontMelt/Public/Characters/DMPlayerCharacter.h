// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/DMJellyCharacterBase.h"
#include "DMPlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;
class UDMCombatComponent;
class UDMInteractionComponent;
class UDMPoseComponent;

/**
 * 실제 플레이어가 조종하는 젤리 인간 Character.
 */


UCLASS()
class DONTMELT_API ADMPlayerCharacter : public ADMJellyCharacterBase
{
    GENERATED_BODY()

public:
    ADMPlayerCharacter();

protected:
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "DontMelt|Camera",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "DontMelt|Camera",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "DontMelt|Components",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UDMCombatComponent> CombatComponent;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "DontMelt|Components",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UDMInteractionComponent> InteractionComponent;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "DontMelt|Components",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UDMPoseComponent> PoseComponent;
};