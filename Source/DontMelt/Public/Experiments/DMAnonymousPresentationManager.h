#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "TimerManager.h"
#include "DMAnonymousPresentationManager.generated.h"

class APawn;
class ADMAnonymousPresentationActor;

/** 실험 소스와 공개 표현의 대응·수명만 관리하는 비복제 서버 Actor. */
UCLASS(NotBlueprintable)
class DONTMELT_API ADMAnonymousPresentationManager : public AActor
{
    GENERATED_BODY()

public:
    ADMAnonymousPresentationManager();
    bool RegisterSource(APawn* Pawn);
    void UnregisterSource(APawn* Pawn);

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    UFUNCTION()
    void HandleSourceDestroyed(AActor* Source);

    void SampleSources();
    void UpdatePresentation(APawn* Pawn, ADMAnonymousPresentationActor* Presentation);

    // 약한 참조는 소스나 표현의 수명을 연장하지 않으며 외부에 공개하지 않는다.
    TMap<TWeakObjectPtr<APawn>, TWeakObjectPtr<ADMAnonymousPresentationActor>> Presentations;
    FTimerHandle SampleTimer;
};
