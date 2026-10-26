#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "DMAnimNotify_AttackHit.generated.h"

/**
 * Attack Montage의 실제 타격 프레임을
 * CombatComponent에 전달한다.
 *
 * 실제 대상 판정은 수행하지 않는다.
 */
UCLASS(meta = (DisplayName = "DM Attack Hit"))
class DONTMELT_API UDMAnimNotify_AttackHit : public UAnimNotify
{
    GENERATED_BODY()

public:
    virtual void Notify(
        USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference
    ) override;
};