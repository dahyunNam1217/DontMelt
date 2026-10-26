#include "Core/DMGameplayUtils.h"

#include "EnhancedInputSubsystems.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "Framework/Match/DMMatchPlayerState.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/PlayerState.h"

namespace DMGameplay
{
    double GetServerTimeSeconds(const UObject* WorldContextObject)
    {
        const UWorld* World = IsValid(WorldContextObject) ? WorldContextObject->GetWorld() : nullptr;

        if (!IsValid(World))
        {
            return 0.0;
        }

        const AGameStateBase* GameState = World->GetGameState();

        return IsValid(GameState) ? GameState->GetServerWorldTimeSeconds() : World->GetTimeSeconds();
    }

    FDMMatchPlayerStateList GetMatchPlayerStates(const AGameStateBase* GameState)
    {
        FDMMatchPlayerStateList Result;

        if (!IsValid(GameState))
        {
            return Result;
        }

        for (APlayerState* PlayerState : GameState->PlayerArray)
        {
            ADMMatchPlayerState* MatchPlayerState = Cast<ADMMatchPlayerState>(PlayerState);

            if (IsValid(MatchPlayerState))
            {
                Result.Add(MatchPlayerState);
            }
        }

        return Result;
    }

    UEnhancedInputLocalPlayerSubsystem* GetLocalInputSubsystem(const APlayerController* PlayerController)
    {
        if (!IsValid(PlayerController) || !PlayerController->IsLocalController())
        {
            return nullptr;
        }

        ULocalPlayer* LocalPlayer = PlayerController->GetLocalPlayer();

        return IsValid(LocalPlayer) ? LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>() : nullptr;
    }
}
