#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "DMAnimNotify_Footstep.generated.h"

/**
 * Jelly Character의 실제 발 디딤 Frame을
 * CharacterAudioComponent에 전달한다.
 *
 * Gameplay Event를 만들거나
 * Server RPC를 보내지 않는다.
 */
UCLASS(meta = (DisplayName = "DM Footstep"))
class DONTMELT_API UDMAnimNotify_Footstep : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(
        USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference
    ) override;
};