// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/Lobby/DMLobbyPlayerController.h"

#include "Framework/Lobby/DMLobbyGameMode.h"
#include "Framework/Lobby/DMLobbyPlayerState.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(
    LogDMLobbyPlayerController,
    Log,
    All
);

void ADMLobbyPlayerController::RequestSetReady(
    const bool bNewReady
)
{
    if (!IsLocalController())
    {
        return;
    }

    const ADMLobbyPlayerState* LobbyPlayerState =
        GetPlayerState<ADMLobbyPlayerState>();

    UE_LOG(
        LogDMLobbyPlayerController,
        Log,
        TEXT(
            "Local ready request. "
            "PlayerName=%s, Requested=%s"
        ),
        LobbyPlayerState != nullptr
        ? *LobbyPlayerState->GetPlayerName()
        : TEXT("Invalid"),
        bNewReady ? TEXT("true") : TEXT("false")
    );

    ServerSetReady(bNewReady);
}

void ADMLobbyPlayerController::RequestStartMatch()
{
    if (!IsLocalController())
    {
        return;
    }

    const ADMLobbyPlayerState* LobbyPlayerState =
        GetPlayerState<ADMLobbyPlayerState>();

    UE_LOG(
        LogDMLobbyPlayerController,
        Log,
        TEXT(
            "Local start request. "
            "PlayerName=%s"
        ),
        LobbyPlayerState != nullptr
        ? *LobbyPlayerState->GetPlayerName()
        : TEXT("Invalid")
    );


    ServerRequestStartMatch();
}

void ADMLobbyPlayerController::DMReady()
{
    RequestSetReady(true);
}

void ADMLobbyPlayerController::DMUnready()
{
    RequestSetReady(false);
}

void ADMLobbyPlayerController::DMStartMatch()
{
    RequestStartMatch();
}


void ADMLobbyPlayerController::ServerSetReady_Implementation(
    const bool bNewReady
)
{
    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return;
    }

    ADMLobbyGameMode* LobbyGameMode =
        Cast<ADMLobbyGameMode>(World->GetAuthGameMode());

    if (LobbyGameMode == nullptr)
    {
        return;
    }

    LobbyGameMode->HandleReadyRequest(this, bNewReady);
}

void ADMLobbyPlayerController::
ServerRequestStartMatch_Implementation()
{
    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return;
    }

    ADMLobbyGameMode* LobbyGameMode =
        Cast<ADMLobbyGameMode>(World->GetAuthGameMode());

    if (LobbyGameMode == nullptr)
    {
        return;
    }

    LobbyGameMode->HandleStartMatchRequest(this);
}