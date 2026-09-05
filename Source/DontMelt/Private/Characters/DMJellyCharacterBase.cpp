

#include "Characters/DMJellyCharacterBase.h"

#include "Characters/Components/DMCharacterStateComponent.h"
#include "Characters/Components/DMJellyPhysicsComponent.h"
#include "GameFramework/CharacterMovementComponent.h"


ADMJellyCharacterBase::ADMJellyCharacterBase()
{
    CharacterStateComponent =
        CreateDefaultSubobject<UDMCharacterStateComponent>(
            TEXT("CharacterStateComponent")
        );

    JellyPhysicsComponent =
        CreateDefaultSubobject<UDMJellyPhysicsComponent>(
            TEXT("JellyPhysicsComponent")
        );

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    UCharacterMovementComponent* CharacterMovementComponent =
        GetCharacterMovement();

    if (CharacterMovementComponent != nullptr)
    {
        CharacterMovementComponent ->bOrientRotationToMovement = true;
    }
}