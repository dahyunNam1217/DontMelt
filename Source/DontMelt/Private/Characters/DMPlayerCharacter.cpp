// Fill out your copyright notice in the Description page of Project Settings.


#include "Characters/DMPlayerCharacter.h"

#include "Camera/CameraComponent.h"
#include "Characters/Components/DMCombatComponent.h"
#include "Characters/Components/DMInteractionComponent.h"
#include "Characters/Components/DMPoseComponent.h"
#include "GameFramework/SpringArmComponent.h"

ADMPlayerCharacter::ADMPlayerCharacter()
{
    CameraBoom =
        CreateDefaultSubobject<USpringArmComponent>(
            TEXT("CameraBoom")
        );

    CameraBoom->SetupAttachment(GetRootComponent());
    CameraBoom->bUsePawnControlRotation = true;

    FollowCamera =
        CreateDefaultSubobject<UCameraComponent>(
            TEXT("FollowCamera")
        );

    FollowCamera->SetupAttachment(
        CameraBoom,
        USpringArmComponent::SocketName
    );

    FollowCamera->bUsePawnControlRotation = false;

    CombatComponent =
        CreateDefaultSubobject<UDMCombatComponent>(
            TEXT("CombatComponent")
        );

    InteractionComponent =
        CreateDefaultSubobject<UDMInteractionComponent>(
            TEXT("InteractionComponent")
        );

    PoseComponent =
        CreateDefaultSubobject<UDMPoseComponent>(
            TEXT("PoseComponent")
        );
}