#include "Animations/DMAnimNotify_Footstep.h"

#include "Characters/Components/DMCharacterAudioComponent.h"
#include "Characters/DMJellyCharacterBase.h"

#include "Components/SkeletalMeshComponent.h"

void UDMAnimNotify_Footstep::Notify(
    USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference
)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (MeshComp == nullptr)
    {
        return;
    }

    ADMJellyCharacterBase* OwnerCharacter = Cast<ADMJellyCharacterBase>(MeshComp->GetOwner());

    if (!IsValid(OwnerCharacter))
    {
        return;
    }

    UDMCharacterAudioComponent* AudioComponent = OwnerCharacter->GetCharacterAudioComponent();

    if (!IsValid(AudioComponent))
    {
        return;
    }

    AudioComponent->HandleFootstepNotify();
}