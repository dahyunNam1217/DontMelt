
#include "UI/DMLobbyPlayerRowWidget.h"

#include "Framework/Lobby/DMLobbyPlayerState.h"
#include "Components/TextBlock.h"

#pragma region Data Source

void UDMLobbyPlayerRowWidget::SetLobbyPlayerState(ADMLobbyPlayerState* InPlayerState)
{
    if (BoundPlayerState.Get() == InPlayerState)
    {
        RefreshRow();
        return;
    }

    UnbindPlayerState();

    BoundPlayerState = InPlayerState;

    if (IsValid(InPlayerState))
    {
        InPlayerState->OnReadyChanged().AddUObject(this, &UDMLobbyPlayerRowWidget::HandleReadyChanged);

        InPlayerState->OnHostChanged().AddUObject(this, &UDMLobbyPlayerRowWidget::HandleHostChanged);

        InPlayerState->OnPlayerNameChanged().AddUObject(this, &UDMLobbyPlayerRowWidget::HandlePlayerNameChanged);
    }

    RefreshRow();
}

#pragma endregion

#pragma region Lifecycle

void UDMLobbyPlayerRowWidget::NativeDestruct()
{
    UnbindPlayerState();

    Super::NativeDestruct();
}

#pragma endregion

#pragma region Update

void UDMLobbyPlayerRowWidget::RefreshRow()
{
    ADMLobbyPlayerState* PlayerState = BoundPlayerState.Get();

    if (!IsValid(PlayerState) || !IsValid(TXT_PlayerName.Get()) || !IsValid(TXT_Host.Get())
        || !IsValid(TXT_Ready.Get()))
    {
        return;
    }

    TXT_PlayerName->SetText(FText::FromString(PlayerState->GetPlayerName()));

    TXT_Host->SetText(PlayerState->IsHost() ? FText::FromString(TEXT("HOST")) : FText::GetEmpty());

    TXT_Ready->SetText(
        PlayerState->IsReady() ? FText::FromString(TEXT("준비 완료")) : FText::FromString(TEXT("대기 중"))
    );
}

void UDMLobbyPlayerRowWidget::UnbindPlayerState()
{
    ADMLobbyPlayerState* PlayerState = BoundPlayerState.Get();

    if (IsValid(PlayerState))
    {
        PlayerState->OnReadyChanged().RemoveAll(this);
        PlayerState->OnHostChanged().RemoveAll(this);
        PlayerState->OnPlayerNameChanged().RemoveAll(this);
    }

    BoundPlayerState.Reset();
}

void UDMLobbyPlayerRowWidget::HandleReadyChanged(ADMLobbyPlayerState* ChangedPlayerState, bool bNewReady)
{
    RefreshRow();
}

void UDMLobbyPlayerRowWidget::HandleHostChanged(ADMLobbyPlayerState* ChangedPlayerState, bool bNewHost)
{
    RefreshRow();
}

void UDMLobbyPlayerRowWidget::HandlePlayerNameChanged(ADMLobbyPlayerState* ChangedPlayerState)
{
    RefreshRow();
}

#pragma endregion
