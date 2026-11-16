#pragma once

#include "CoreMinimal.h"
#include "Characters/DMNPCCharacter.h"
#include "DMAnonymousPoCNPCPawn.generated.h"

/** 서버에만 존재하는 실험용 NPC. 기존 NPC 전역 복제 설정은 건드리지 않는다. */
UCLASS()
class DONTMELT_API ADMAnonymousPoCNPCPawn : public ADMNPCCharacter
{
    GENERATED_BODY()

public:
    ADMAnonymousPoCNPCPawn(const FObjectInitializer& ObjectInitializer);
    virtual void Destroyed() override;
    virtual bool IsNetRelevantFor(
        const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation
    ) const override;
};
