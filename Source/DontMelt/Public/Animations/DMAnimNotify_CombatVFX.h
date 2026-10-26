
#pragma once

#include "CoreMinimal.h"
#include "Animation/AnimNotifies/AnimNotify.h"
#include "DMAnimNotify_CombatVFX.generated.h"

/**
 * 공격·방어 Montage에서 로컬 VFX 및 SFX 연출을 요청한다.
 *
 * 공격 판정, 방어 승인, Gameplay 상태 변경은 수행하지 않는다.
 * 실제 재생은 CharacterVFXComponent와 CharacterAudioComponent가 담당한다.
 */
UCLASS(meta = (DisplayName = "DM Combat VFX"))
class DONTMELT_API UDMAnimNotify_CombatVFX : public UAnimNotify
{
    GENERATED_BODY()

public:
#pragma region Anim Notify

    virtual void Notify(
        USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference
    ) override;

#pragma endregion

private:
#pragma region Presentation Configuration

    /**
     * false: 공격 VFX 및 SFX
     * true: 방어 VFX 및 SFX
     *
     * Montage에 배치한 Notify의 설정값이며
     * 현재 Gameplay ActionState를 저장하지 않는다.
     */
    UPROPERTY(EditAnywhere, Category = "DontMelt|VFX")
    bool bUseDefenseEffect = false;

#pragma endregion
};
