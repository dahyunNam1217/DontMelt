
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "DMMainMenuWidget.generated.h"

class UWidget;
class UButton;
class UEditableTextBox;
class UComboBoxString;
class UTextBlock;
class UDMSettingsWidget;

/**
 * Title / Main Menu / Join 화면 관리.
 *
 * UI 표시 및 버튼 입력만 담당한다.
 * 레벨 이동과 네트워크 접속은
 * ADMFrontendPlayerController에 위임한다.
 */
UCLASS()
class DONTMELT_API UDMMainMenuWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
#pragma region Lifecycle

    virtual void NativeOnInitialized() override;
    virtual void NativeConstruct() override;

#pragma endregion

public:
    // 이미 생성된 메뉴를 MainMenuPanel로 되돌린다. NativeConstruct가 다시 불리지 않는 경우에 쓴다.
    void ShowMainMenuPanelForRecovery();

    // 온라인 방 생성 진행 상태와 결과를 표시한다.
    void SetHostCreationState(bool bInProgress, const FText& Message);

    // Steam 검색 결과를 순서대로 방 선택 목록에 반영한다.
    void SetSteamSessionOptions(const TArray<FString>& Options);

    // Steam 검색·참가 중 입력 상태와 안내 문구를 갱신한다.
    void SetSteamJoinBusy(bool bBusy, const FText& Message);

private:
#pragma region UI Actions

    UFUNCTION()
    void HandleTitleStartClicked();

    UFUNCTION()
    void HandleBackToTitleClicked();

    UFUNCTION()
    void HandleCreateLobbyClicked();

    UFUNCTION()
    void HandleOpenJoinClicked();

    UFUNCTION()
    void HandleRefreshSessionsClicked();

    UFUNCTION()
    void HandleJoinConfirmClicked();

    UFUNCTION()
    void HandleJoinBackClicked();

    UFUNCTION()
    void HandleQuitClicked();

    UFUNCTION()
    void HandleSettingsClicked();

    void HandleSettingsClosed();

#pragma endregion

#pragma region Presentation

    void ShowPanel(UWidget* ActivePanel);

    void SetJoinError(const FText& Message);

#pragma endregion

#pragma region Panels

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> TitlePanel = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> MainMenuPanel = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> JoinPanel = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMSettingsWidget> SettingsPanel = nullptr;

#pragma endregion

#pragma region Buttons

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_TitleStart = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_TitleQuit = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_CreateLobby = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_OpenJoin = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Settings = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Quit = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_BackToTitle = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_JoinConfirm = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_JoinBack = nullptr;

#pragma endregion

#pragma region Join Input

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UEditableTextBox> EDT_JoinAddress = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_JoinError = nullptr;

    // Steam 검색 결과에서 참가할 로비를 선택한다.
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> CB_SteamSessions = nullptr;

    // 현재 검색 결과를 다시 조회한다.
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_RefreshSessions = nullptr;

#pragma endregion

#pragma region Host Status

    // 메인 메뉴에서 방 생성 진행·오류를 표시한다.
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_HostStatus = nullptr;

#pragma endregion
};
