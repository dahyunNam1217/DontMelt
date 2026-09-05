// Fill out your copyright notice in the Description page of Project Settings.


#include "Framework/Match/DMMatchPlayerController.h"

#include "Framework/Match/DMMatchGameMode.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(
    LogDMMatchPlayerController,
    Log,
    All
);

void ADMMatchPlayerController::
DMTestFinishMatch()
{
    if (!HasAuthority())
    {
        UE_LOG(
            LogDMMatchPlayerController,
            Warning,
            TEXT(
                "DMTestFinishMatch rejected: "
                "only the Listen Server host "
                "may execute this command."
            )
        );

        return;
    }

    UWorld* World = GetWorld();

    ADMMatchGameMode* MatchGameMode =
        IsValid(World)
        ? World->GetAuthGameMode<
        ADMMatchGameMode
        >()
        : nullptr;

    if (!IsValid(MatchGameMode))
    {
        UE_LOG(
            LogDMMatchPlayerController,
            Error,
            TEXT(
                "DMTestFinishMatch failed: "
                "MatchGameMode is unavailable."
            )
        );

        return;
    }

    MatchGameMode->TryFinishMatch(
        EDMMatchEndReason::RegularTimeVictory
    );
}

void ADMMatchPlayerController::
DMTestAbortMatch()
{
    if (!HasAuthority())
    {
        UE_LOG(
            LogDMMatchPlayerController,
            Warning,
            TEXT(
                "DMTestAbortMatch rejected: "
                "only the Listen Server host "
                "may execute this command."
            )
        );

        return;
    }

    UWorld* World = GetWorld();

    ADMMatchGameMode* MatchGameMode =
        IsValid(World)
        ? World->GetAuthGameMode<
        ADMMatchGameMode
        >()
        : nullptr;

    if (!IsValid(MatchGameMode))
    {
        UE_LOG(
            LogDMMatchPlayerController,
            Error,
            TEXT(
                "DMTestAbortMatch failed: "
                "MatchGameMode is unavailable."
            )
        );

        return;
    }

    MatchGameMode->TryAbortMatch(
        EDMMatchEndReason::HostLeft
    );
}

void ADMMatchPlayerController::
DMTestStartOvertime()
{
    if (!HasAuthority())
    {
        UE_LOG(
            LogDMMatchPlayerController,
            Warning,
            TEXT(
                "DMTestStartOvertime rejected: "
                "only the Listen Server host "
                "may execute this command."
            )
        );

        return;
    }

    UWorld* World = GetWorld();

    ADMMatchGameMode* MatchGameMode =
        IsValid(World)
        ? World->GetAuthGameMode<
        ADMMatchGameMode
        >()
        : nullptr;

    if (!IsValid(MatchGameMode))
    {
        UE_LOG(
            LogDMMatchPlayerController,
            Error,
            TEXT(
                "DMTestStartOvertime failed: "
                "MatchGameMode is unavailable."
            )
        );

        return;
    }

    MatchGameMode->TryStartOvertimeIntro();
}

void ADMMatchPlayerController::
DMTestFinishOvertime()
{
    if (!HasAuthority())
    {
        UE_LOG(
            LogDMMatchPlayerController,
            Warning,
            TEXT(
                "DMTestFinishOvertime rejected: "
                "only the Listen Server host "
                "may execute this command."
            )
        );

        return;
    }

    UWorld* World = GetWorld();

    ADMMatchGameMode* MatchGameMode =
        IsValid(World)
        ? World->GetAuthGameMode<
        ADMMatchGameMode
        >()
        : nullptr;

    if (!IsValid(MatchGameMode))
    {
        UE_LOG(
            LogDMMatchPlayerController,
            Error,
            TEXT(
                "DMTestFinishOvertime failed: "
                "MatchGameMode is unavailable."
            )
        );

        return;
    }

    MatchGameMode->TryFinishMatch(
        EDMMatchEndReason::OvertimeVictory
    );
}

