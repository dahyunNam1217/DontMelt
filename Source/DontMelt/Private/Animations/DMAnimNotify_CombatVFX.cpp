
#include "Animations/DMAnimNotify_CombatVFX.h"

#include "Characters/DMJellyCharacterBase.h"
#include "Characters/Components/DMCharacterVFXComponent.h"
#include "Characters/Components/DMCharacterAudioComponent.h"

#include "Components/SkeletalMeshComponent.h"

#pragma region Anim Notify

void UDMAnimNotify_CombatVFX::Notify(
    USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference
)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (!IsValid(MeshComp))
    {
        return;
    }

    // 현재 애니메이션을 재생하는 캐릭터를 확인한다.
    ADMJellyCharacterBase* OwnerCharacter = Cast<ADMJellyCharacterBase>(MeshComp->GetOwner());

    if (!IsValid(OwnerCharacter))
    {
        return;
    }

    // 다른 SkeletalMesh에서 발생한 Notify는 처리하지 않는다.
    if (OwnerCharacter->GetMesh() != MeshComp)
    {
        return;
    }

    UDMCharacterVFXComponent* VFXComponent = OwnerCharacter->GetCharacterVFXComponent();
    UDMCharacterAudioComponent* AudioComponent = OwnerCharacter->GetCharacterAudioComponent();

    // VFX와 SFX는 서로 독립적으로 처리한다.
    // 한쪽 컴포넌트가 없어도 다른 연출의 실행을 막지 않는다.
    if (bUseDefenseEffect)
    {
        if (IsValid(VFXComponent))
        {
            VFXComponent->PlayDefenseEffect();
        }

        if (IsValid(AudioComponent))
        {
            AudioComponent->PlayDefenseSound();
        }
    }
    else
    {
        if (IsValid(VFXComponent))
        {
            VFXComponent->PlayAttackEffect();
        }

        if (IsValid(AudioComponent))
        {
            AudioComponent->PlayAttackSound();
        }
    }
}

#pragma endregion
