
#include "Characters/DMNPCCharacter.h"

#include "Characters/DMJellyAIController.h"
#include "Characters/Components/DMCharacterVFXComponent.h"
#include "Engine/EngineTypes.h"

ADMNPCCharacter::ADMNPCCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetReplicates(true);
    SetReplicateMovement(true);

    AIControllerClass = ADMJellyAIController::StaticClass();

    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;
}

#pragma region NPC Alert Presentation

void ADMNPCCharacter::MulticastPlayAlertEffect_Implementation()
{
    // 서버에서 확정한 일회성 반응을 로컬 Niagara로 표시한다.
    UDMCharacterVFXComponent* VFXComponent = GetCharacterVFXComponent();

    if (!IsValid(VFXComponent))
    {
        return;
    }

    VFXComponent->PlayNPCAlertEffect();
}

#pragma endregion
