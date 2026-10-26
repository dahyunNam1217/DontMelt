
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "DMLobbyWidget.generated.h"

class ADMLobbyGameState;
class ADMLobbyPlayerController;
class ADMLobbyPlayerState;
class UDMLobbyPlayerRowWidget;
class UVerticalBox;
class UTextBlock;
class UButton;
class UDMSettingsWidget;

UCLASS()
class DONTMELT_API UDMLobbyWidget : public UUserWidget
{
    GENERATED_BODY()

public:
#pragma region Initialization

    void InitializeLobby(ADMLobbyPlayerController* InController);

    void RefreshLobby();

    // 로컬 UI가 유효한 LobbyGameState를 구독하고 있는지 확인한다.
    bool IsGameStateBound() const;

#pragma endregion

protected:
#pragma region Lifecycle

    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;

#pragma endregion

private:
#pragma region Data Binding

    void TryBindGameState();
    void UnbindGameState();

    void BindPlayerStates();
    void UnbindPlayerStates();

    void HandleLobbyDataChanged();

    void HandlePlayerReadyChanged(ADMLobbyPlayerState* ChangedPlayerState, bool bNewReady);

    void HandlePlayerHostChanged(ADMLobbyPlayerState* ChangedPlayerState, bool bNewHost);

#pragma endregion

#pragma region Presentation

    void RefreshPlayerList();
    void RefreshControls();

#pragma endregion

#pragma region Button Actions

    UFUNCTION()
    void HandleReadyClicked();

    UFUNCTION()
    void HandleStartMatchClicked();

    UFUNCTION()
    void HandleLeaveLobbyClicked();

    UFUNCTION()
    void HandleSettingsClicked();

    void HandleSettingsClosed();

#pragma endregion

#pragma region Widget Configuration

    UPROPERTY(EditDefaultsOnly, Category = "DM|Lobby|UI")
    TSubclassOf<UDMLobbyPlayerRowWidget> PlayerRowWidgetClass;

#pragma endregion

#pragma region Bind Widgets

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> VB_PlayerList = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_PlayerCount = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_RuleSummary = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_LobbyNotice = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ReadyButton = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Ready = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_StartMatch = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_LeaveLobby = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Settings = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMSettingsWidget> SettingsPanel = nullptr;

#pragma endregion

#pragma region Runtime References

    TWeakObjectPtr<ADMLobbyPlayerController> BoundController;

    TWeakObjectPtr<ADMLobbyGameState> BoundGameState;

    TArray<TWeakObjectPtr<ADMLobbyPlayerState>> BoundPlayerStates;

#pragma endregion
};
