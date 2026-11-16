#pragma once

#include "CoreMinimal.h"
#include "GameFramework/GameModeBase.h"
#include "DMAnonymousPoCGameMode.generated.h"

class ADMAnonymousPresentationManager;
class ADMAnonymousPoCNPCPawn;

/** 명시적인 맵 Override로만 실행하는 독립 실험. 본 게임의 매치 흐름은 연결하지 않는다. */
UCLASS()
class DONTMELT_API ADMAnonymousPoCGameMode : public AGameModeBase
{
    GENERATED_BODY()

public:
    ADMAnonymousPoCGameMode();
    virtual void StartPlay() override;
    virtual void PreLogin(
        const FString& Options, const FString& Address, const FUniqueNetIdRepl& UniqueId, FString& ErrorMessage
    ) override;
    virtual void HandleStartingNewPlayer_Implementation(APlayerController* NewPlayer) override;
    virtual void Logout(AController* Exiting) override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

protected:
    UPROPERTY(EditDefaultsOnly, Category = "DM|Experiment")
    FTransform NPCSpawnTransform = FTransform(FRotator::ZeroRotator, FVector(400.0f, 0.0f, 100.0f));

private:
    bool EnsurePresentationManager();
    void RegisterPlayer(APlayerController* Controller);

    UPROPERTY(Transient)
    TObjectPtr<ADMAnonymousPresentationManager> PresentationManager;

    TWeakObjectPtr<ADMAnonymousPoCNPCPawn> NPCPawn;
};
