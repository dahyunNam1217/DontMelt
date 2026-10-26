#include "UI/DMSettingsWidget.h"

#include "Core/DMGameInstance.h"
#include "Core/DMLocalSettingsSubsystem.h"

#include "Components/Slider.h"
#include "Components/Button.h"
#include "Components/CheckBox.h"
#include "Components/ComboBoxString.h"
#include "Components/TextBlock.h"
#include "Components/WidgetSwitcher.h"
#include "Voice/DMVoiceChatSubsystem.h"

#include "HAL/PlatformTime.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameUserSettings.h"
#include "Input/Events.h"
#include "InputCoreTypes.h"
#include "Kismet/KismetSystemLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMSettings, Log, All);

namespace
{
    // Slider 비교 허용 오차다.
    constexpr float SettingsSliderTolerance = 0.001f;

    // 기본값은 SaveGame 기본값과 동일하게 유지한다.
    const TCHAR* DefaultWindowModeOption = TEXT("테두리 없는 창");
    const TCHAR* DefaultGraphicsQualityOption = TEXT("보통");
    const TCHAR* DefaultFrameRateLimitOption = TEXT("60");
    constexpr bool bDefaultVSyncEnabled = false;
    constexpr float DefaultVolume = 1.0f;
    constexpr float DefaultMouseSensitivity = 1.0f;
}

#pragma region Settings Values

bool FDMSettingsValues::IsGraphicsEqual(const FDMSettingsValues& Other) const
{
    return ResolutionOption == Other.ResolutionOption && WindowModeOption == Other.WindowModeOption
        && GraphicsQualityOption == Other.GraphicsQualityOption && FrameRateLimitOption == Other.FrameRateLimitOption
        && bVSyncEnabled == Other.bVSyncEnabled;
}

bool FDMSettingsValues::IsLocalEqual(const FDMSettingsValues& Other) const
{
    return FMath::IsNearlyEqual(MasterVolume, Other.MasterVolume, SettingsSliderTolerance)
        && FMath::IsNearlyEqual(SFXVolume, Other.SFXVolume, SettingsSliderTolerance)
        && FMath::IsNearlyEqual(MusicVolume, Other.MusicVolume, SettingsSliderTolerance)
        && FMath::IsNearlyEqual(MouseSensitivity, Other.MouseSensitivity, SettingsSliderTolerance)
        && bVoiceActivationEnabled == Other.bVoiceActivationEnabled;
}

#pragma endregion

#pragma region Lifecycle

void UDMSettingsWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (!ensure(
            SW_SettingsPages && BTN_Graphics && BTN_Audio && BTN_Controls && BTN_Language && BTN_Voice && CB_Resolution
            && CB_WindowMode && CB_GraphicsQuality && CB_FrameRateLimit && CHK_VSync && BTN_Apply && BTN_Reset
            && BTN_Back && SL_MasterVolume && SL_SFXVolume && SL_MusicVolume && SL_MouseSensitivity
            && TXT_MouseSensitivityValue && CB_Language && CB_VoiceMode && CB_InputDevice && SL_VoiceVolume
            && BTN_TestMicrophone && TXT_SettingsNotice && ConfirmChangesPanel && TXT_ConfirmTitle
            && TXT_ConfirmDescription && BTN_ConfirmApply && BTN_DiscardChanges && VideoConfirmPanel
            && TXT_VideoCountdown && BTN_KeepVideoMode && BTN_RevertVideoMode && BTN_CancelConfirmation
        ))
    {
        return;
    }

    BTN_Graphics->OnClicked.AddUniqueDynamic(this, &UDMSettingsWidget::HandleGraphicsClicked);

    BTN_Audio->OnClicked.AddUniqueDynamic(this, &UDMSettingsWidget::HandleAudioClicked);

    BTN_Controls->OnClicked.AddUniqueDynamic(this, &UDMSettingsWidget::HandleControlsClicked);

    BTN_Language->OnClicked.AddUniqueDynamic(this, &UDMSettingsWidget::HandleLanguageClicked);

    BTN_Voice->OnClicked.AddUniqueDynamic(this, &UDMSettingsWidget::HandleVoiceClicked);

    BTN_Apply->OnClicked.AddUniqueDynamic(this, &UDMSettingsWidget::HandleApplyClicked);

    BTN_KeepVideoMode->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleKeepVideoModeClicked);

    BTN_RevertVideoMode->OnClicked.AddUniqueDynamic(this, &ThisClass::HandleRevertVideoModeClicked);

    VideoConfirmPanel->SetVisibility(ESlateVisibility::Collapsed);

    BTN_Reset->OnClicked.AddUniqueDynamic(this, &UDMSettingsWidget::HandleResetClicked);

    BTN_Back->OnClicked.AddUniqueDynamic(this, &UDMSettingsWidget::HandleBackClicked);

    SL_MouseSensitivity->OnValueChanged.AddUniqueDynamic(this, &UDMSettingsWidget::HandleSensitivityPreviewChanged);

    // 입력값이 바뀌면 기준값과 비교해 변경 상태만 갱신한다.
    CB_Resolution->OnSelectionChanged.AddUniqueDynamic(this, &UDMSettingsWidget::HandleOptionSelectionChanged);

    CB_WindowMode->OnSelectionChanged.AddUniqueDynamic(this, &UDMSettingsWidget::HandleOptionSelectionChanged);

    CB_GraphicsQuality->OnSelectionChanged.AddUniqueDynamic(this, &UDMSettingsWidget::HandleOptionSelectionChanged);

    CB_FrameRateLimit->OnSelectionChanged.AddUniqueDynamic(this, &UDMSettingsWidget::HandleOptionSelectionChanged);

    CB_VoiceMode->OnSelectionChanged.AddUniqueDynamic(this, &UDMSettingsWidget::HandleOptionSelectionChanged);

    CHK_VSync->OnCheckStateChanged.AddUniqueDynamic(this, &UDMSettingsWidget::HandleVSyncChanged);

    SL_MasterVolume->OnValueChanged.AddUniqueDynamic(this, &UDMSettingsWidget::HandleVolumeSliderChanged);

    SL_SFXVolume->OnValueChanged.AddUniqueDynamic(this, &UDMSettingsWidget::HandleVolumeSliderChanged);

    SL_MusicVolume->OnValueChanged.AddUniqueDynamic(this, &UDMSettingsWidget::HandleVolumeSliderChanged);

    // 확인창은 하나만 두고 탭 이동과 닫기에 공통으로 사용한다.
    BTN_ConfirmApply->OnClicked.AddUniqueDynamic(this, &UDMSettingsWidget::HandleConfirmApplyClicked);

    BTN_DiscardChanges->OnClicked.AddUniqueDynamic(this, &UDMSettingsWidget::HandleDiscardChangesClicked);

    BTN_CancelConfirmation->OnClicked.AddUniqueDynamic(this, &UDMSettingsWidget::HandleCancelConfirmationClicked);

    ConfirmChangesPanel->SetVisibility(ESlateVisibility::Collapsed);

    // 타이틀·로비에서 Esc를 받기 위해 포커스를 받을 수 있게 한다.
    SetIsFocusable(true);

    // 번역 데이터가 아직 없으므로 한국어만 표시한다.
    CB_Language->ClearOptions();
    CB_Language->AddOption(TEXT("한국어"));
    CB_Language->SetSelectedOption(TEXT("한국어"));
    CB_Language->SetIsEnabled(false);

    // 지원하는 두 음성 모드만 제공한다.
    CB_VoiceMode->ClearOptions();

    CB_VoiceMode->AddOption(TEXT("눌러서 말하기"));
    CB_VoiceMode->AddOption(TEXT("음성 감지"));

    CB_VoiceMode->SetSelectedOption(TEXT("눌러서 말하기"));

    CB_VoiceMode->SetIsEnabled(true);

    CB_InputDevice->ClearOptions();
    CB_InputDevice->AddOption(TEXT("음성 기능 준비 중"));
    CB_InputDevice->SetSelectedIndex(0);
    CB_InputDevice->SetIsEnabled(false);

    SL_VoiceVolume->SetValue(1.0f);
    SL_VoiceVolume->SetIsEnabled(false);

    BTN_TestMicrophone->SetIsEnabled(false);
}

void UDMSettingsWidget::NativeDestruct()
{
    // 위젯이 제거되면 확인되지 않은 화면 변경을 복원한다.
    if (bVideoConfirmationActive)
    {
        EndVideoConfirmation(true);
    }

    Super::NativeDestruct();
}

#pragma endregion

#pragma region Presentation

void UDMSettingsWidget::OpenSettings()
{
    // 이전에 남은 확인창·보류 동작은 버리고 저장값에서 다시 시작한다.
    HideConfirmation();

    ReloadFromSavedSettings();

    ShowCategory(0);
    SetVisibility(ESlateVisibility::Visible);

    if (TXT_SettingsNotice)
    {
        TXT_SettingsNotice->SetText(FText::GetEmpty());
    }

    // 이전 화면의 버튼이 Collapsed되어도 Esc가 설정창에 전달되도록 한다.
    SetKeyboardFocus();
}

void UDMSettingsWidget::RequestClose()
{
    if (bVideoConfirmationActive)
    {
        return;
    }

    if (IsConfirmationVisible())
    {
        return;
    }

    UpdatePendingChangesState();

    if (!bHasPendingChanges)
    {
        CompleteClose();
        return;
    }

    ShowConfirmation(EDMSettingsPendingAction::CloseSettings, INDEX_NONE);
}

bool UDMSettingsWidget::HandleEscapeRequested()
{
    if (GetVisibility() == ESlateVisibility::Collapsed || GetVisibility() == ESlateVisibility::Hidden)
    {
        return false;
    }

    if (bVideoConfirmationActive)
    {
        HandleRevertVideoModeClicked();
        return true;
    }

    // 확인창 표시 중 Esc는 확인창만 닫고 현재 탭에 머문다.
    if (IsConfirmationVisible())
    {
        HandleCancelConfirmationClicked();
        return true;
    }

    RequestClose();
    return true;
}

void UDMSettingsWidget::ForceDiscardPendingChanges()
{
    if (bVideoConfirmationActive)
    {
        EndVideoConfirmation(true);
    }

    // 저장하지 않는다. 다음 OpenSettings에서 저장값을 다시 불러온다.
    HideConfirmation();

    bHasPendingChanges = false;
}

FReply UDMSettingsWidget::NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    if (InKeyEvent.GetKey() == EKeys::Escape && HandleEscapeRequested())
    {
        return FReply::Handled();
    }

    return Super::NativeOnKeyDown(InGeometry, InKeyEvent);
}

#pragma endregion

#pragma region Local Settings

void UDMSettingsWidget::RefreshLocalSettings()
{
    UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;

    if (!IsValid(GameInstance))
    {
        return;
    }

    const UDMLocalSettingsSubsystem* LocalSettings = GameInstance->GetSubsystem<UDMLocalSettingsSubsystem>();

    if (!IsValid(LocalSettings))
    {
        return;
    }

    SL_MasterVolume->SetValue(LocalSettings->GetMasterVolume());

    SL_SFXVolume->SetValue(LocalSettings->GetSFXVolume());

    SL_MusicVolume->SetValue(LocalSettings->GetMusicVolume());

    SL_MouseSensitivity->SetValue(LocalSettings->GetMouseSensitivity());

    // 저장된 음성 모드를 설정 ComboBox에 표시한다.
    CB_VoiceMode->SetSelectedOption(
        LocalSettings->GetVoiceActivationEnabled() ? TEXT("음성 감지") : TEXT("눌러서 말하기")
    );

    HandleSensitivityPreviewChanged(SL_MouseSensitivity->GetValue());
}

void UDMSettingsWidget::HandleSensitivityPreviewChanged(float NewValue)
{
    if (!IsValid(TXT_MouseSensitivityValue.Get()))
    {
        return;
    }

    TXT_MouseSensitivityValue->SetText(FText::FromString(FString::Printf(TEXT("%.2f"), NewValue)));

    // 감도 표시와 별개로 변경 상태를 갱신한다.
    UpdatePendingChangesState();
}

#pragma endregion

#pragma region Graphics

void UDMSettingsWidget::RefreshGraphicsSettings()
{
    UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;

    if (!IsValid(Settings))
    {
        return;
    }

    // 현재 장치가 지원하는 전체 화면 해상도를 사용한다.
    CB_Resolution->ClearOptions();

    TArray<FIntPoint> Resolutions;

    UKismetSystemLibrary::GetSupportedFullscreenResolutions(Resolutions);

    for (const FIntPoint& Resolution : Resolutions)
    {
        if (Resolution.X <= 0 || Resolution.Y <= 0)
        {
            continue;
        }

        const FString Option = FString::Printf(TEXT("%d x %d"), Resolution.X, Resolution.Y);

        CB_Resolution->AddOption(Option);
    }

    const FIntPoint CurrentResolution = Settings->GetScreenResolution();

    const FString CurrentOption = FString::Printf(TEXT("%d x %d"), CurrentResolution.X, CurrentResolution.Y);

    if (CurrentResolution.X > 0 && CurrentResolution.Y > 0)
    {
        if (CB_Resolution->FindOptionIndex(CurrentOption) == INDEX_NONE)
        {
            CB_Resolution->AddOption(CurrentOption);
        }

        CB_Resolution->SetSelectedOption(CurrentOption);
    }

    // 화면 모드.
    CB_WindowMode->ClearOptions();
    CB_WindowMode->AddOption(TEXT("전체 화면"));
    CB_WindowMode->AddOption(TEXT("테두리 없는 창"));
    CB_WindowMode->AddOption(TEXT("창 모드"));

    switch (Settings->GetFullscreenMode())
    {
    case EWindowMode::Fullscreen:
        CB_WindowMode->SetSelectedOption(TEXT("전체 화면"));
        break;

    case EWindowMode::WindowedFullscreen:
        CB_WindowMode->SetSelectedOption(TEXT("테두리 없는 창"));
        break;

    default:
        CB_WindowMode->SetSelectedOption(TEXT("창 모드"));
        break;
    }

    // 그래픽 품질.
    CB_GraphicsQuality->ClearOptions();
    CB_GraphicsQuality->AddOption(TEXT("낮음"));
    CB_GraphicsQuality->AddOption(TEXT("보통"));
    CB_GraphicsQuality->AddOption(TEXT("높음"));
    CB_GraphicsQuality->AddOption(TEXT("매우 높음"));
    CB_GraphicsQuality->AddOption(TEXT("사용자 지정"));

    const int32 Quality = Settings->GetOverallScalabilityLevel();

    switch (Quality)
    {
    case 0:
        CB_GraphicsQuality->SetSelectedOption(TEXT("낮음"));
        break;

    case 1:
        CB_GraphicsQuality->SetSelectedOption(TEXT("보통"));
        break;

    case 2:
        CB_GraphicsQuality->SetSelectedOption(TEXT("높음"));
        break;

    case 3:
        CB_GraphicsQuality->SetSelectedOption(TEXT("매우 높음"));
        break;

    default:
        CB_GraphicsQuality->SetSelectedOption(TEXT("사용자 지정"));
        break;
    }

    // FPS 제한.
    CB_FrameRateLimit->ClearOptions();
    CB_FrameRateLimit->AddOption(TEXT("30"));
    CB_FrameRateLimit->AddOption(TEXT("60"));
    CB_FrameRateLimit->AddOption(TEXT("120"));
    CB_FrameRateLimit->AddOption(TEXT("144"));
    CB_FrameRateLimit->AddOption(TEXT("무제한"));

    const float CurrentFPS = Settings->GetFrameRateLimit();

    if (CurrentFPS <= 0.0f)
    {
        CB_FrameRateLimit->SetSelectedOption(TEXT("무제한"));
    }
    else
    {
        const FString FPSOption = FString::FromInt(FMath::RoundToInt(CurrentFPS));

        if (CB_FrameRateLimit->FindOptionIndex(FPSOption) == INDEX_NONE)
        {
            CB_FrameRateLimit->AddOption(FPSOption);
        }

        CB_FrameRateLimit->SetSelectedOption(FPSOption);
    }

    CHK_VSync->SetIsChecked(Settings->IsVSyncEnabled());
}

void UDMSettingsWidget::HandleApplyClicked()
{
    // 탭과 관계없이 변경된 지원 설정 전체를 한 번에 적용한다.
    if (ApplyPendingChanges())
    {
        TXT_SettingsNotice->SetText(FText::FromString(TEXT("설정을 적용했습니다.")));
    }
}

bool UDMSettingsWidget::ValidateGraphicsValues(const FDMSettingsValues& Values) const
{
    const UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;

    if (!IsValid(Settings))
    {
        return false;
    }

    // 현재 해상도를 읽지 못해 선택이 비어 있으면 해상도만 변경하지 않는다.
    if (Values.ResolutionOption.IsEmpty())
    {
        return true;
    }

    FString WidthString;
    FString HeightString;

    if (!Values.ResolutionOption.Split(TEXT(" x "), &WidthString, &HeightString))
    {
        return false;
    }

    return FCString::Atoi(*WidthString) > 0 && FCString::Atoi(*HeightString) > 0;
}

bool UDMSettingsWidget::ApplyGraphicsValues(const FDMSettingsValues& Values)
{
    UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;

    if (!IsValid(Settings))
    {
        return false;
    }

    // 해상도.
    FString WidthString;
    FString HeightString;

    if (Values.ResolutionOption.Split(TEXT(" x "), &WidthString, &HeightString))
    {
        const int32 Width = FCString::Atoi(*WidthString);
        const int32 Height = FCString::Atoi(*HeightString);

        if (Width > 0 && Height > 0)
        {
            Settings->SetScreenResolution(FIntPoint(Width, Height));
        }
    }

    // 화면 모드.
    const FString& Mode = Values.WindowModeOption;

    if (Mode == TEXT("전체 화면"))
    {
        Settings->SetFullscreenMode(EWindowMode::Fullscreen);
    }
    else if (Mode == TEXT("테두리 없는 창"))
    {
        Settings->SetFullscreenMode(EWindowMode::WindowedFullscreen);
    }
    else if (Mode == TEXT("창 모드"))
    {
        Settings->SetFullscreenMode(EWindowMode::Windowed);
    }

    // 그래픽 품질.
    const FString& Quality = Values.GraphicsQualityOption;

    if (Quality == TEXT("낮음"))
    {
        Settings->SetOverallScalabilityLevel(0);
    }
    else if (Quality == TEXT("보통"))
    {
        Settings->SetOverallScalabilityLevel(1);
    }
    else if (Quality == TEXT("높음"))
    {
        Settings->SetOverallScalabilityLevel(2);
    }
    else if (Quality == TEXT("매우 높음"))
    {
        Settings->SetOverallScalabilityLevel(3);
    }

    // 사용자 지정이면 기존 개별 품질을 유지한다.

    // FPS 제한.
    const FString& FPS = Values.FrameRateLimitOption;

    const float Limit = FPS == TEXT("무제한") ? 0.0f : FCString::Atof(*FPS);

    Settings->SetFrameRateLimit(Limit);

    // 수직 동기화.
    Settings->SetVSyncEnabled(Values.bVSyncEnabled);

    // 설정 적용 및 저장.
    Settings->ApplySettings(false);

    UE_LOG(LogDMSettings, Log, TEXT("Graphics settings applied."));

    return true;
}

void UDMSettingsWidget::HandleResetClicked()
{
    // 지원 중인 설정 전체를 UI에서만 기본값으로 바꾼다.
    // 저장은 적용 버튼 또는 변경 확인창의 적용에서만 수행한다.
    ApplyDefaultValuesToControls();

    UpdatePendingChangesState();

    TXT_SettingsNotice->SetText(FText::FromString(
        bHasPendingChanges ? TEXT("기본값을 선택했습니다. 적용을 눌러 저장하세요.") : TEXT("이미 기본값입니다.")
    ));
}

void UDMSettingsWidget::HandleBackClicked()
{
    // 바로 닫지 않고 변경 확인 절차를 거친다.
    RequestClose();
}

#pragma endregion

#pragma region Categories

void UDMSettingsWidget::ShowCategory(int32 PageIndex)
{
    if (!IsValid(SW_SettingsPages.Get()))
    {
        return;
    }

    if (PageIndex < 0 || PageIndex >= SW_SettingsPages->GetChildrenCount())
    {
        return;
    }

    SW_SettingsPages->SetActiveWidgetIndex(PageIndex);

    // 적용·초기화는 탭이 아니라 지원 설정 전체를 대상으로 한다.
    // 적용 버튼은 변경사항이 있을 때만 활성화한다.
    BTN_Apply->SetIsEnabled(bHasPendingChanges);

    BTN_Reset->SetIsEnabled(true);

    if (!IsValid(TXT_SettingsNotice.Get()))
    {
        return;
    }

    switch (PageIndex)
    {
    case 3:
        TXT_SettingsNotice->SetText(FText::FromString(TEXT("현재 한국어만 지원합니다.")));
        break;

    case 4:
        TXT_SettingsNotice->SetText(
            FText::FromString(TEXT("음성 모드: PTT는 V키로 송신 전환, 음성 감지는 M키로 음소거 전환"))
        );
        break;

    default:
        TXT_SettingsNotice->SetText(FText::GetEmpty());
        break;
    }
}

void UDMSettingsWidget::HandleGraphicsClicked()
{
    RequestCategory(0);
}

void UDMSettingsWidget::HandleAudioClicked()
{
    RequestCategory(1);
}

void UDMSettingsWidget::HandleControlsClicked()
{
    RequestCategory(2);
}

void UDMSettingsWidget::HandleLanguageClicked()
{
    RequestCategory(3);
}

void UDMSettingsWidget::HandleVoiceClicked()
{
    RequestCategory(4);
}

#pragma endregion

#pragma region Pending Changes

void UDMSettingsWidget::ReloadFromSavedSettings()
{
    {
        // 코드로 UI를 채우는 동안 발생하는 변경 이벤트는 무시한다.
        TGuardValue<bool> PopulateGuard(bIsPopulatingControls, true);

        RefreshGraphicsSettings();
        RefreshLocalSettings();
    }

    // 실제 저장값이 UI에 표시된 형태 그대로를 기준값으로 기록한다.
    SavedValues = ReadValuesFromControls();

    bHasPendingChanges = false;

    if (IsValid(BTN_Apply.Get()))
    {
        BTN_Apply->SetIsEnabled(false);
    }
}

FDMSettingsValues UDMSettingsWidget::ReadValuesFromControls() const
{
    FDMSettingsValues Values;

    if (!IsValid(CB_Resolution.Get()) || !IsValid(CB_WindowMode.Get()) || !IsValid(CB_GraphicsQuality.Get())
        || !IsValid(CB_FrameRateLimit.Get()) || !IsValid(CHK_VSync.Get()) || !IsValid(SL_MasterVolume.Get())
        || !IsValid(SL_SFXVolume.Get()) || !IsValid(SL_MusicVolume.Get()) || !IsValid(SL_MouseSensitivity.Get())
        || !IsValid(CB_VoiceMode.Get()))
    {
        return Values;
    }

    Values.ResolutionOption = CB_Resolution->GetSelectedOption();
    Values.WindowModeOption = CB_WindowMode->GetSelectedOption();
    Values.GraphicsQualityOption = CB_GraphicsQuality->GetSelectedOption();
    Values.FrameRateLimitOption = CB_FrameRateLimit->GetSelectedOption();
    Values.bVSyncEnabled = CHK_VSync->IsChecked();

    Values.MasterVolume = SL_MasterVolume->GetValue();
    Values.SFXVolume = SL_SFXVolume->GetValue();
    Values.MusicVolume = SL_MusicVolume->GetValue();

    // 저장 시 Subsystem이 적용하는 범위와 같은 기준으로 비교한다.
    Values.MouseSensitivity = FMath::Clamp(SL_MouseSensitivity->GetValue(), 0.1f, 3.0f);

    Values.bVoiceActivationEnabled = CB_VoiceMode->GetSelectedOption() == TEXT("음성 감지");

    return Values;
}

void UDMSettingsWidget::UpdatePendingChangesState()
{
    if (bIsPopulatingControls)
    {
        return;
    }

    // 플래그를 누적하지 않고 매번 기준값과 비교한다.
    // 원래 값으로 되돌리면 자동으로 변경 상태가 해제된다.
    bHasPendingChanges = !SavedValues.IsEqual(ReadValuesFromControls());

    if (IsValid(BTN_Apply.Get()))
    {
        BTN_Apply->SetIsEnabled(bHasPendingChanges);
    }
}

bool UDMSettingsWidget::ApplyPendingChanges()
{
    UpdatePendingChangesState();

    if (!bHasPendingChanges)
    {
        return true;
    }

    const FDMSettingsValues PendingValues = ReadValuesFromControls();

    const bool bGraphicsChanged = !SavedValues.IsGraphicsEqual(PendingValues);

    const bool bLocalChanged = !SavedValues.IsLocalEqual(PendingValues);

    // 1) 실패할 수 있는 검사를 먼저 끝내 일부만 적용되는 상황을 줄인다.
    if (bGraphicsChanged && !ValidateGraphicsValues(PendingValues))
    {
        TXT_SettingsNotice->SetText(FText::FromString(TEXT("그래픽 설정을 적용할 수 없습니다.")));

        return false;
    }

    // 해상도 또는 화면 모드 변경은 먼저 15초 확인을 받는다.
    if (bGraphicsChanged && !bVideoConfirmationActive && RequiresVideoConfirmation(PendingValues))
    {
        if (!BeginVideoConfirmation(PendingValues))
        {
            TXT_SettingsNotice->SetText(FText::FromString(TEXT("화면 설정을 미리 적용할 수 없습니다.")));
        }

        // 아직 최종 저장을 하지 않았다.
        return false;
    }

    UGameInstance* GameInstance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr;

    // 2) 음량·감도 저장. 실패하면 메모리 원본을 이전 값으로 되돌린다.
    if (bLocalChanged)
    {
        UDMLocalSettingsSubsystem* LocalSettings =
            IsValid(GameInstance) ? GameInstance->GetSubsystem<UDMLocalSettingsSubsystem>() : nullptr;

        if (!IsValid(LocalSettings))
        {
            TXT_SettingsNotice->SetText(FText::FromString(TEXT("로컬 설정을 찾을 수 없습니다.")));

            return false;
        }

        const float PreviousMaster = LocalSettings->GetMasterVolume();
        const float PreviousSFX = LocalSettings->GetSFXVolume();
        const float PreviousMusic = LocalSettings->GetMusicVolume();
        const float PreviousSensitivity = LocalSettings->GetMouseSensitivity();
        const bool bPreviousVoiceActivation = LocalSettings->GetVoiceActivationEnabled();

        LocalSettings->SetAudioVolumes(PendingValues.MasterVolume, PendingValues.SFXVolume, PendingValues.MusicVolume);

        LocalSettings->SetMouseSensitivity(PendingValues.MouseSensitivity);

        // 음성 모드도 다른 로컬 설정과 같은 SaveGame에 저장한다.
        LocalSettings->SetVoiceActivationEnabled(PendingValues.bVoiceActivationEnabled);

        if (!LocalSettings->SaveSettings())
        {
            // 디스크 저장에 실패한 값이 메모리에만 남지 않도록 복원한다.
            LocalSettings->SetAudioVolumes(PreviousMaster, PreviousSFX, PreviousMusic);

            LocalSettings->SetMouseSensitivity(PreviousSensitivity);

            // 저장 실패 시 음성 모드의 메모리 원본도 복원한다.
            LocalSettings->SetVoiceActivationEnabled(bPreviousVoiceActivation);

            TXT_SettingsNotice->SetText(FText::FromString(TEXT("설정 저장에 실패했습니다.")));

            return false;
        }

        if (UDMGameInstance* DMGameInstance = Cast<UDMGameInstance>(GameInstance))
        {
            DMGameInstance->ApplyLocalAudioSettings(GetWorld());
        }

        // 디스크 저장에 성공한 다음 실제 음성 시스템에 적용한다.
        if (UDMVoiceChatSubsystem* Voice = GameInstance->GetSubsystem<UDMVoiceChatSubsystem>())
        {
            Voice->SetVoiceActivationEnabled(LocalSettings->GetVoiceActivationEnabled());
        }
    }

    // 3) 그래픽 적용. 검사를 통과한 값만 전달한다.
    if (bGraphicsChanged && !ApplyGraphicsValues(PendingValues))
    {
        TXT_SettingsNotice->SetText(FText::FromString(TEXT("그래픽 설정 적용에 실패했습니다.")));

        // 이미 저장한 음량·감도는 기준값에 반영한다.
        ReloadFromSavedSettings();

        return false;
    }

    // 4) 실제 적용된 값으로 기준값을 다시 기록한다.
    ReloadFromSavedSettings();

    // 장치가 요청과 다른 그래픽 값을 사용하게 되었다면 안내한다.
    if (bGraphicsChanged && !SavedValues.IsGraphicsEqual(PendingValues))
    {
        TXT_SettingsNotice->SetText(FText::FromString(TEXT("일부 그래픽 설정이 현재 장치에 맞게 조정되었습니다.")));
    }

    return true;
}

void UDMSettingsWidget::ApplyDefaultValuesToControls()
{
    TGuardValue<bool> PopulateGuard(bIsPopulatingControls, true);

    // 해상도는 장치마다 지원 목록이 다르므로 현재 선택을 유지한다.
    CB_WindowMode->SetSelectedOption(DefaultWindowModeOption);
    CB_GraphicsQuality->SetSelectedOption(DefaultGraphicsQualityOption);
    CB_FrameRateLimit->SetSelectedOption(DefaultFrameRateLimitOption);
    CHK_VSync->SetIsChecked(bDefaultVSyncEnabled);

    SL_MasterVolume->SetValue(DefaultVolume);
    SL_SFXVolume->SetValue(DefaultVolume);
    SL_MusicVolume->SetValue(DefaultVolume);

    SL_MouseSensitivity->SetValue(DefaultMouseSensitivity);

    // Slider SetValue는 변경 이벤트를 보내지 않으므로 표시를 직접 갱신한다.
    HandleSensitivityPreviewChanged(SL_MouseSensitivity->GetValue());

    // 음성 모드의 기본값은 PTT다.
    CB_VoiceMode->SetSelectedOption(TEXT("눌러서 말하기"));
}

void UDMSettingsWidget::HandleOptionSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType)
{
    UpdatePendingChangesState();
}

void UDMSettingsWidget::HandleVolumeSliderChanged(float NewValue)
{
    UpdatePendingChangesState();
}

void UDMSettingsWidget::HandleVSyncChanged(bool bIsChecked)
{
    UpdatePendingChangesState();
}

#pragma endregion

#pragma region Confirmation

void UDMSettingsWidget::RequestCategory(int32 PageIndex)
{
    if (!IsValid(SW_SettingsPages.Get()) || IsConfirmationVisible() || bVideoConfirmationActive)
    {
        return;
    }

    if (SW_SettingsPages->GetActiveWidgetIndex() == PageIndex)
    {
        return;
    }

    UpdatePendingChangesState();

    if (!bHasPendingChanges)
    {
        ShowCategory(PageIndex);
        return;
    }

    // 확정 전에는 Active Index를 바꾸지 않고 이동할 탭만 기억한다.
    ShowConfirmation(EDMSettingsPendingAction::SwitchCategory, PageIndex);
}

void UDMSettingsWidget::ShowConfirmation(const EDMSettingsPendingAction Action, const int32 TargetPageIndex)
{
    if (!IsValid(ConfirmChangesPanel.Get()))
    {
        return;
    }

    PendingAction = Action;
    PendingCategoryIndex = TargetPageIndex;

    TXT_ConfirmTitle->SetText(FText::FromString(TEXT("변경사항 확인")));

    TXT_ConfirmDescription->SetText(FText::FromString(TEXT("변경된 설정이 있습니다. 적용하시겠습니까?")));

    BTN_ConfirmApply->SetIsEnabled(true);

    // Visible이어야 뒤쪽 설정 위젯의 클릭을 막는다.
    ConfirmChangesPanel->SetVisibility(ESlateVisibility::Visible);

    BTN_ConfirmApply->SetKeyboardFocus();
}

void UDMSettingsWidget::HideConfirmation()
{
    PendingAction = EDMSettingsPendingAction::None;
    PendingCategoryIndex = INDEX_NONE;

    if (IsValid(ConfirmChangesPanel.Get()))
    {
        ConfirmChangesPanel->SetVisibility(ESlateVisibility::Collapsed);
    }
}

bool UDMSettingsWidget::IsConfirmationVisible() const
{
    return IsValid(ConfirmChangesPanel.Get()) && ConfirmChangesPanel->GetVisibility() == ESlateVisibility::Visible;
}

void UDMSettingsWidget::ExecutePendingAction()
{
    // 숨기기 전에 값을 보관한다. HideConfirmation이 보류 상태를 초기화한다.
    const EDMSettingsPendingAction Action = PendingAction;
    const int32 TargetPageIndex = PendingCategoryIndex;

    HideConfirmation();

    switch (Action)
    {
    case EDMSettingsPendingAction::SwitchCategory:
        ShowCategory(TargetPageIndex);
        SetKeyboardFocus();
        break;

    case EDMSettingsPendingAction::CloseSettings:
        CompleteClose();
        break;

    case EDMSettingsPendingAction::None:
    default:
        SetKeyboardFocus();
        break;
    }
}

void UDMSettingsWidget::CompleteClose()
{
    HideConfirmation();

    // 실제 화면 전환과 포커스 복구는 각 부모 위젯이 담당한다.
    SettingsClosedDelegate.Broadcast();
}

void UDMSettingsWidget::HandleConfirmApplyClicked()
{
    if (!ApplyPendingChanges())
    {
        // 영상 설정 확인창으로 전환한 경우 실패로 처리하지 않는다.
        if (bVideoConfirmationActive)
        {
            return;
        }

        TXT_ConfirmDescription->SetText(TXT_SettingsNotice->GetText());

        return;
    }

    ExecutePendingAction();
}

void UDMSettingsWidget::HandleDiscardChangesClicked()
{
    // 저장하지 않고 UI 입력값을 저장값으로 되돌린다.
    ReloadFromSavedSettings();

    ExecutePendingAction();
}

void UDMSettingsWidget::HandleCancelConfirmationClicked()
{
    // 확인창만 닫고 현재 탭과 입력값을 유지한다.
    HideConfirmation();

    SetKeyboardFocus();
}

#pragma endregion

#pragma region Video Confirmation

bool UDMSettingsWidget::RequiresVideoConfirmation(const FDMSettingsValues& Values) const
{
    return Values.ResolutionOption != SavedValues.ResolutionOption
        || Values.WindowModeOption != SavedValues.WindowModeOption;
}

bool UDMSettingsWidget::BeginVideoConfirmation(const FDMSettingsValues& Values)
{
    UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr;

    UWorld* World = GetWorld();

    if (!IsValid(Settings) || !IsValid(World) || !IsValid(VideoConfirmPanel.Get()) || bVideoConfirmationActive)
    {
        return false;
    }

    FIntPoint NewResolution = Settings->GetScreenResolution();

    if (!Values.ResolutionOption.IsEmpty())
    {
        FString WidthString;
        FString HeightString;

        if (!Values.ResolutionOption.Split(TEXT(" x "), &WidthString, &HeightString))
        {
            return false;
        }

        NewResolution = FIntPoint(FCString::Atoi(*WidthString), FCString::Atoi(*HeightString));

        if (NewResolution.X <= 0 || NewResolution.Y <= 0)
        {
            return false;
        }
    }

    EWindowMode::Type NewWindowMode = EWindowMode::Windowed;

    if (Values.WindowModeOption == TEXT("전체 화면"))
    {
        NewWindowMode = EWindowMode::Fullscreen;
    }
    else if (Values.WindowModeOption == TEXT("테두리 없는 창"))
    {
        NewWindowMode = EWindowMode::WindowedFullscreen;
    }
    else if (Values.WindowModeOption != TEXT("창 모드"))
    {
        return false;
    }

    // 현재 정상 화면을 마지막 확인된 화면 모드로 기록한다.
    Settings->ConfirmVideoMode();

    bVideoConfirmationActive = true;

    // 저장하지 않고 해상도와 화면 모드만 임시 적용한다.
    Settings->SetScreenResolution(NewResolution);
    Settings->SetFullscreenMode(NewWindowMode);
    Settings->ApplyResolutionSettings(false);

    // 기존 변경 확인창에서 진입했다면 해당 창만 숨긴다.
    // 이동할 탭/닫기 요청 상태는 아직 유지해야 한다.
    if (IsValid(ConfirmChangesPanel.Get()))
    {
        ConfirmChangesPanel->SetVisibility(ESlateVisibility::Collapsed);
    }

    VideoConfirmPanel->SetVisibility(ESlateVisibility::Visible);

    VideoConfirmationDeadline = FPlatformTime::Seconds() + 15.0;

    World->GetTimerManager().SetTimer(
        VideoConfirmationTimerHandle, this, &ThisClass::UpdateVideoConfirmationCountdown, 0.25f, true
    );

    UpdateVideoConfirmationCountdown();

    BTN_KeepVideoMode->SetKeyboardFocus();

    return true;
}

void UDMSettingsWidget::UpdateVideoConfirmationCountdown()
{
    if (!bVideoConfirmationActive)
    {
        return;
    }

    const double Remaining = VideoConfirmationDeadline - FPlatformTime::Seconds();

    if (Remaining <= 0.0)
    {
        HandleRevertVideoModeClicked();
        return;
    }

    const int32 Seconds = FMath::CeilToInt(Remaining);

    TXT_VideoCountdown->SetText(
        FText::FromString(FString::Printf(TEXT("%d초 후 이전 화면 설정으로 복원됩니다."), Seconds))
    );
}

void UDMSettingsWidget::EndVideoConfirmation(const bool bRevert)
{
    if (!bVideoConfirmationActive)
    {
        return;
    }

    bVideoConfirmationActive = false;

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(VideoConfirmationTimerHandle);
    }

    if (bRevert)
    {
        if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
        {
            // 마지막으로 확인했던 해상도와 화면 모드를 복구한다.
            Settings->RevertVideoMode();
            Settings->ApplyResolutionSettings(false);
        }
    }

    VideoConfirmPanel->SetVisibility(ESlateVisibility::Collapsed);

    VideoConfirmationDeadline = 0.0;
}

void UDMSettingsWidget::HandleKeepVideoModeClicked()
{
    if (!bVideoConfirmationActive)
    {
        return;
    }

    // 확인을 누른 뒤에만 실제 설정 전체를 적용·저장한다.
    if (!ApplyPendingChanges())
    {
        EndVideoConfirmation(true);
        ReloadFromSavedSettings();
        HideConfirmation();

        TXT_SettingsNotice->SetText(FText::FromString(TEXT("설정 적용에 실패하여 이전 화면으로 복원했습니다.")));

        SetKeyboardFocus();
        return;
    }

    if (UGameUserSettings* Settings = GEngine ? GEngine->GetGameUserSettings() : nullptr)
    {
        Settings->ConfirmVideoMode();
        Settings->SaveSettings();
    }

    EndVideoConfirmation(false);

    // 탭 전환이나 설정창 닫기 요청이 있었다면 이어서 실행한다.
    ExecutePendingAction();
}

void UDMSettingsWidget::HandleRevertVideoModeClicked()
{
    if (!bVideoConfirmationActive)
    {
        return;
    }

    EndVideoConfirmation(true);

    // 임시 입력값을 버리고 이전 저장값으로 복원한다.
    ReloadFromSavedSettings();
    HideConfirmation();

    TXT_SettingsNotice->SetText(FText::FromString(TEXT("이전 화면 설정으로 복원했습니다.")));

    SetKeyboardFocus();
}

#pragma endregion
