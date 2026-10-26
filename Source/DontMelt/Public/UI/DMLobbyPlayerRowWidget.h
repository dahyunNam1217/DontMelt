
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "DMLobbyPlayerRowWidget.generated.h"

class ADMLobbyPlayerState;
class UTextBlock;

UCLASS()
class DONTMELT_API UDMLobbyPlayerRowWidget : public UUserWidget
{
    GENERATED_BODY()

public:
#pragma region Data Source

    void SetLobbyPlayerState(ADMLobbyPlayerState* InPlayerState);

#pragma endregion

protected:
#pragma region Lifecycle

    virtual void NativeDestruct() override;

#pragma endregion

private:
#pragma region Update

    void RefreshRow();
    void UnbindPlayerState();

    void HandleReadyChanged(ADMLobbyPlayerState* ChangedPlayerState, bool bNewReady);

    void HandleHostChanged(ADMLobbyPlayerState* ChangedPlayerState, bool bNewHost);

    void HandlePlayerNameChanged(ADMLobbyPlayerState* ChangedPlayerState);

#pragma endregion

#pragma region Widgets

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_PlayerName = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_Host = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_Ready = nullptr;

#pragma endregion

#pragma region Runtime

    TWeakObjectPtr<ADMLobbyPlayerState> BoundPlayerState;

#pragma endregion
};
