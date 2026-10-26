
#include "UI/DMMainMenuWidget.h"

#include "UI/DMSettingsWidget.h"

#include "Framework/Frontend/DMFrontendPlayerController.h"

#include "Core/DMGameInstance.h"

#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/ComboBoxString.h"
#include "Components/Widget.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMMainMenu, Log, All);

#pragma region Lifecycle

void UDMMainMenuWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!IsValid(TitlePanel.Get()) || !IsValid(MainMenuPanel.Get()) || !IsValid(JoinPanel.Get())
        || !IsValid(BTN_TitleStart.Get()) || !IsValid(BTN_TitleQuit.Get()) || !IsValid(BTN_CreateLobby.Get())
        || !IsValid(BTN_OpenJoin.Get()) || !IsValid(BTN_Quit.Get()) || !IsValid(BTN_BackToTitle.Get())
        || !IsValid(BTN_JoinConfirm.Get()) || !IsValid(BTN_JoinBack.Get()) || !IsValid(SettingsPanel.Get())
        || !IsValid(BTN_Settings.Get()) || !IsValid(EDT_JoinAddress.Get()) || !IsValid(TXT_JoinError.Get())
        || !IsValid(CB_SteamSessions.Get()) || !IsValid(BTN_RefreshSessions.Get()) || !IsValid(TXT_HostStatus.Get()))
    {
        UE_LOG(LogDMMainMenu, Error, TEXT("Required main menu widgets are missing."));
        return;
    }

    BTN_TitleStart->OnClicked.AddUniqueDynamic(this, &UDMMainMenuWidget::HandleTitleStartClicked);

    BTN_TitleQuit->OnClicked.AddUniqueDynamic(this, &UDMMainMenuWidget::HandleQuitClicked);

    BTN_CreateLobby->OnClicked.AddUniqueDynamic(this, &UDMMainMenuWidget::HandleCreateLobbyClicked);

    BTN_OpenJoin->OnClicked.AddUniqueDynamic(this, &UDMMainMenuWidget::HandleOpenJoinClicked);

    BTN_RefreshSessions->OnClicked.AddUniqueDynamic(this, &UDMMainMenuWidget::HandleRefreshSessionsClicked);

    BTN_Quit->OnClicked.AddUniqueDynamic(this, &UDMMainMenuWidget::HandleQuitClicked);

    BTN_BackToTitle->OnClicked.AddUniqueDynamic(this, &UDMMainMenuWidget::HandleBackToTitleClicked);

    BTN_JoinConfirm->OnClicked.AddUniqueDynamic(this, &UDMMainMenuWidget::HandleJoinConfirmClicked);

    BTN_JoinBack->OnClicked.AddUniqueDynamic(this, &UDMMainMenuWidget::HandleJoinBackClicked);

    BTN_Settings->OnClicked.AddUniqueDynamic(this, &UDMMainMenuWidget::HandleSettingsClicked);

    // 변경 확인 절차를 통과한 뒤에만 호출된다.
    SettingsPanel->OnSettingsClosed().AddUObject(this, &UDMMainMenuWidget::HandleSettingsClosed);
}

void UDMMainMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    SetJoinError(FText::GetEmpty());

    // Steam 참가 화면에서는 개발용 IP 입력을 숨긴다.
    if (IsValid(EDT_JoinAddress.Get()))
    {
        EDT_JoinAddress->SetVisibility(ESlateVisibility::Collapsed);
    }

    SetSteamSessionOptions(TArray<FString>());
    SetSteamJoinBusy(false, FText::GetEmpty());

    SetHostCreationState(false, FText::GetEmpty());

    UDMGameInstance* GameInstance = Cast<UDMGameInstance>(GetGameInstance());

    const bool bOpenMainMenu = IsValid(GameInstance) && GameInstance->ConsumeOpenMainMenuPanelOnNextEntry();

    ShowPanel(bOpenMainMenu ? MainMenuPanel.Get() : TitlePanel.Get());
}

#pragma endregion

#pragma region UI Actions

void UDMMainMenuWidget::HandleTitleStartClicked()
{
    ShowPanel(MainMenuPanel.Get());
}

void UDMMainMenuWidget::HandleBackToTitleClicked()
{
    ShowPanel(TitlePanel.Get());
}

void UDMMainMenuWidget::HandleCreateLobbyClicked()
{
    ADMFrontendPlayerController* Controller = Cast<ADMFrontendPlayerController>(GetOwningPlayer());

    if (!IsValid(Controller))
    {
        UE_LOG(LogDMMainMenu, Warning, TEXT("Frontend PlayerController is invalid."));
        return;
    }

    if (!Controller->HostLobby())
    {
        UE_LOG(LogDMMainMenu, Warning, TEXT("Failed to request lobby hosting."));
    }
}

void UDMMainMenuWidget::HandleOpenJoinClicked()
{
    SetSteamSessionOptions(TArray<FString>());
    SetSteamJoinBusy(false, FText::GetEmpty());

    ShowPanel(JoinPanel.Get());

    ADMFrontendPlayerController* Controller = Cast<ADMFrontendPlayerController>(GetOwningPlayer());

    if (!IsValid(Controller))
    {
        SetJoinError(FText::FromString(TEXT("메뉴 컨트롤러를 찾을 수 없습니다.")));
        return;
    }

    // 방 참가 화면에 들어오면 한 번 검색한다.
    Controller->FindSteamLobbies();
}

void UDMMainMenuWidget::HandleRefreshSessionsClicked()
{
    ADMFrontendPlayerController* Controller = Cast<ADMFrontendPlayerController>(GetOwningPlayer());

    if (!IsValid(Controller))
    {
        SetJoinError(FText::FromString(TEXT("메뉴 컨트롤러를 찾을 수 없습니다.")));
        return;
    }

    Controller->FindSteamLobbies();
}

void UDMMainMenuWidget::HandleJoinConfirmClicked()
{
    ADMFrontendPlayerController* Controller = Cast<ADMFrontendPlayerController>(GetOwningPlayer());

    if (!IsValid(Controller))
    {
        SetJoinError(FText::FromString(TEXT("메뉴 컨트롤러를 찾을 수 없습니다.")));
        return;
    }

    if (!IsValid(CB_SteamSessions.Get()))
    {
        return;
    }

    const int32 SelectedIndex = CB_SteamSessions->GetSelectedIndex();

    if (SelectedIndex == INDEX_NONE)
    {
        SetJoinError(FText::FromString(TEXT("참가할 Steam 로비를 선택해주세요.")));
        return;
    }

    Controller->JoinSteamLobby(SelectedIndex);
}

void UDMMainMenuWidget::HandleJoinBackClicked()
{
    SetSteamSessionOptions(TArray<FString>());
    SetSteamJoinBusy(false, FText::GetEmpty());

    ShowPanel(MainMenuPanel.Get());
}

void UDMMainMenuWidget::HandleQuitClicked()
{
    ADMFrontendPlayerController* Controller = Cast<ADMFrontendPlayerController>(GetOwningPlayer());

    if (IsValid(Controller))
    {
        Controller->QuitGame();
    }
}

void UDMMainMenuWidget::HandleSettingsClicked()
{
    if (!IsValid(SettingsPanel.Get()))
    {
        return;
    }

    ShowPanel(SettingsPanel.Get());
    SettingsPanel->OpenSettings();
}

void UDMMainMenuWidget::HandleSettingsClosed()
{
    ShowPanel(MainMenuPanel.Get());

    // 설정창에 있던 포커스를 원래 메뉴 버튼으로 돌려놓는다.
    if (IsValid(BTN_Settings.Get()))
    {
        BTN_Settings->SetKeyboardFocus();
    }
}

#pragma endregion

#pragma region Presentation

void UDMMainMenuWidget::ShowMainMenuPanelForRecovery()
{
    // 오류 복구는 사용자 확인을 기다리지 않는다. 저장하지 않고 변경사항을 버린다.
    if (IsValid(SettingsPanel.Get()))
    {
        SettingsPanel->ForceDiscardPendingChanges();
    }

    SetJoinError(FText::GetEmpty());

    SetHostCreationState(false, FText::GetEmpty());

    SetSteamSessionOptions(TArray<FString>());
    SetSteamJoinBusy(false, FText::GetEmpty());

    ShowPanel(MainMenuPanel.Get());
}

void UDMMainMenuWidget::ShowPanel(UWidget* ActivePanel)
{
    if (!IsValid(ActivePanel))
    {
        return;
    }

    if (IsValid(TitlePanel.Get()))
    {
        TitlePanel->SetVisibility(
            TitlePanel.Get() == ActivePanel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed
        );
    }

    if (IsValid(MainMenuPanel.Get()))
    {
        MainMenuPanel->SetVisibility(
            MainMenuPanel.Get() == ActivePanel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed
        );
    }

    if (IsValid(JoinPanel.Get()))
    {
        JoinPanel->SetVisibility(
            JoinPanel.Get() == ActivePanel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed
        );
    }

    if (IsValid(SettingsPanel.Get()))
    {
        SettingsPanel->SetVisibility(
            SettingsPanel.Get() == ActivePanel ? ESlateVisibility::Visible : ESlateVisibility::Collapsed
        );
    }
}

void UDMMainMenuWidget::SetJoinError(const FText& Message)
{
    if (!IsValid(TXT_JoinError.Get()))
    {
        return;
    }

    TXT_JoinError->SetText(Message);

    TXT_JoinError->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UDMMainMenuWidget::SetHostCreationState(const bool bInProgress, const FText& Message)
{
    // 비동기 요청 중에는 중복 생성과 다른 화면 이동을 막는다.
    if (IsValid(BTN_CreateLobby.Get()))
    {
        BTN_CreateLobby->SetIsEnabled(!bInProgress);
    }

    if (IsValid(BTN_OpenJoin.Get()))
    {
        BTN_OpenJoin->SetIsEnabled(!bInProgress);
    }

    if (IsValid(BTN_Settings.Get()))
    {
        BTN_Settings->SetIsEnabled(!bInProgress);
    }

    if (IsValid(BTN_BackToTitle.Get()))
    {
        BTN_BackToTitle->SetIsEnabled(!bInProgress);
    }

    if (IsValid(BTN_Quit.Get()))
    {
        BTN_Quit->SetIsEnabled(!bInProgress);
    }

    if (!IsValid(TXT_HostStatus.Get()))
    {
        return;
    }

    TXT_HostStatus->SetText(Message);

    TXT_HostStatus->SetVisibility(Message.IsEmpty() ? ESlateVisibility::Collapsed : ESlateVisibility::HitTestInvisible);
}

void UDMMainMenuWidget::SetSteamSessionOptions(const TArray<FString>& Options)
{
    if (!IsValid(CB_SteamSessions.Get()))
    {
        return;
    }

    // 기존 화면 목록만 초기화한다.
    CB_SteamSessions->ClearOptions();

    for (const FString& Option : Options)
    {
        CB_SteamSessions->AddOption(Option);
    }

    if (!Options.IsEmpty())
    {
        CB_SteamSessions->SetSelectedIndex(0);
    }
    else
    {
        CB_SteamSessions->ClearSelection();
    }

    if (IsValid(BTN_JoinConfirm.Get()))
    {
        BTN_JoinConfirm->SetIsEnabled(!Options.IsEmpty());
    }
}

void UDMMainMenuWidget::SetSteamJoinBusy(const bool bBusy, const FText& Message)
{
    if (IsValid(BTN_RefreshSessions.Get()))
    {
        BTN_RefreshSessions->SetIsEnabled(!bBusy);
    }

    if (IsValid(BTN_JoinBack.Get()))
    {
        BTN_JoinBack->SetIsEnabled(!bBusy);
    }

    if (IsValid(CB_SteamSessions.Get()))
    {
        CB_SteamSessions->SetIsEnabled(!bBusy);
    }

    if (IsValid(BTN_JoinConfirm.Get()))
    {
        const bool bHasSelection =
            IsValid(CB_SteamSessions.Get()) && CB_SteamSessions->GetSelectedIndex() != INDEX_NONE;

        BTN_JoinConfirm->SetIsEnabled(!bBusy && bHasSelection);
    }

    // 기존 텍스트를 검색 진행·결과·오류 표시로 재사용한다.
    SetJoinError(Message);
}

#pragma endregion
