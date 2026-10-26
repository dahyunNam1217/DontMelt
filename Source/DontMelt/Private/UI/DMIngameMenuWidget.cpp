
#include "UI/DMIngameMenuWidget.h"

#include "UI/DMSettingsWidget.h"
#include "Framework/Match/DMMatchPlayerController.h"

#include "Components/Button.h"
#include "Components/Widget.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"

#pragma region Lifecycle

void UDMIngameMenuWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!ensure(
            MenuPanel && ConfirmLeavePanel && SettingsPanel && BTN_Resume && BTN_Settings && BTN_LeaveMatch
            && BTN_ConfirmLeave && BTN_CancelLeave
        ))
    {
        return;
    }

    BTN_Resume->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleResumeClicked);

    BTN_Settings->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleSettingsClicked);

    BTN_LeaveMatch->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleLeaveMatchClicked);

    BTN_ConfirmLeave->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleConfirmLeaveClicked);

    BTN_CancelLeave->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleCancelLeaveClicked);

    // 변경 확인 절차를 통과한 뒤에만 호출된다.
    SettingsPanel->OnSettingsClosed().AddUObject(this, &ThisClass::HandleSettingsClosed);
}

void UDMIngameMenuWidget::NativeConstruct()
{
    Super::NativeConstruct();

    ShowMainMenu();

    // 실제 표시 여부는 GameRoot가 관리한다.
    SetVisibility(ESlateVisibility::Collapsed);
}

void UDMIngameMenuWidget::NativeDestruct()
{
    // SettingsPanel은 이 위젯의 자식이라 수명이 같다.
    // 바인딩은 NativeOnInitialized에서 한 번만 하므로 여기서 해제하지 않는다.
    // 해제하면 같은 인스턴스가 다시 Construct될 때 닫힘 처리가 연결되지 않는다.

    Super::NativeDestruct();
}

FReply UDMIngameMenuWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape)
    {
        HandleBackNavigation();

        return FReply::Handled();
    }

    return Super::NativeOnPreviewKeyDown(InGeometry, InKeyEvent);
}

#pragma endregion

#pragma region Presentation

void UDMIngameMenuWidget::OpenMenu()
{
    ShowMainMenu();

    SetVisibility(ESlateVisibility::Visible);
}

void UDMIngameMenuWidget::ShowMainMenu()
{
    // 메뉴 재오픈·매치 종료로 인한 강제 닫기 등에서 남은 확인창과 임시 변경을 버린다.
    // 정상적인 닫기는 이미 확인 절차를 통과했으므로 버릴 변경사항이 없다.
    if (IsValid(SettingsPanel.Get()))
    {
        SettingsPanel->ForceDiscardPendingChanges();
    }

    if (IsValid(MenuPanel.Get()))
    {
        MenuPanel->SetVisibility(ESlateVisibility::Visible);
    }

    if (IsValid(ConfirmLeavePanel.Get()))
    {
        ConfirmLeavePanel->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (IsValid(SettingsPanel.Get()))
    {
        SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
    }
}

void UDMIngameMenuWidget::ShowLeaveConfirmation()
{
    if (IsValid(MenuPanel.Get()))
    {
        MenuPanel->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (IsValid(SettingsPanel.Get()))
    {
        SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (IsValid(ConfirmLeavePanel.Get()))
    {
        ConfirmLeavePanel->SetVisibility(ESlateVisibility::Visible);
    }
}

#pragma endregion

#pragma region Button Actions

void UDMIngameMenuWidget::HandleResumeClicked()
{
    ADMMatchPlayerController* Controller = Cast<ADMMatchPlayerController>(GetOwningPlayer());

    if (IsValid(Controller))
    {
        Controller->CloseIngameMenu();
    }
}

void UDMIngameMenuWidget::HandleSettingsClicked()
{
    if (!IsValid(SettingsPanel.Get()))
    {
        return;
    }

    MenuPanel->SetVisibility(ESlateVisibility::Collapsed);

    ConfirmLeavePanel->SetVisibility(ESlateVisibility::Collapsed);

    SettingsPanel->OpenSettings();
}

void UDMIngameMenuWidget::HandleSettingsClosed()
{
    ShowMainMenu();

    // Esc Preview 처리가 계속 이 위젯을 거치도록 포커스를 메뉴 안에 둔다.
    if (IsValid(BTN_Settings.Get()))
    {
        BTN_Settings->SetKeyboardFocus();
    }
}

void UDMIngameMenuWidget::HandleLeaveMatchClicked()
{
    ShowLeaveConfirmation();
}

void UDMIngameMenuWidget::HandleConfirmLeaveClicked()
{
    ADMMatchPlayerController* Controller = Cast<ADMMatchPlayerController>(GetOwningPlayer());

    if (IsValid(Controller))
    {
        Controller->LeaveMatchToTitle();
    }
}

void UDMIngameMenuWidget::HandleCancelLeaveClicked()
{
    ShowMainMenu();
}

void UDMIngameMenuWidget::HandleBackNavigation()
{
    if (IsValid(SettingsPanel.Get()) && SettingsPanel->GetVisibility() != ESlateVisibility::Collapsed)
    {
        // 바로 닫지 않고 설정창의 변경 확인 절차에 맡긴다.
        // 확인창이 열려 있으면 확인창만 닫힌다.
        SettingsPanel->HandleEscapeRequested();
        return;
    }

    if (IsValid(ConfirmLeavePanel.Get()) && ConfirmLeavePanel->GetVisibility() != ESlateVisibility::Collapsed)
    {
        HandleCancelLeaveClicked();
        return;
    }

    HandleResumeClicked();
}

#pragma endregion
