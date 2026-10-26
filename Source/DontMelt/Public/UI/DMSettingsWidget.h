#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Types/SlateEnums.h"
#include "TimerManager.h"

#include "DMSettingsWidget.generated.h"

class UButton;
class UCheckBox;
class UComboBoxString;
class UTextBlock;
class UWidgetSwitcher;
class USlider;
class UWidget;

// 변경 확인 절차를 통과해 설정창을 실제로 닫아도 될 때만 Broadcast한다.
DECLARE_MULTICAST_DELEGATE(FDMOnSettingsClosed);

/**
 * 설정 화면에서 비교·적용 대상이 되는 값 묶음.
 *
 * 원본은 UGameUserSettings와 UDMLocalSettingsSubsystem이며,
 * 이 구조체는 설정창을 연 시점의 기준값과 현재 UI 입력값을 비교하기 위한 복사본이다.
 * 미구현인 언어·음성·키 설정은 포함하지 않는다.
 */
struct FDMSettingsValues
{
    // 그래픽 ComboBox는 표시 문자열을 그대로 비교한다.
    FString ResolutionOption;
    FString WindowModeOption;
    FString GraphicsQualityOption;
    FString FrameRateLimitOption;

    bool bVSyncEnabled = false;

    float MasterVolume = 1.0f;
    float SFXVolume = 1.0f;
    float MusicVolume = 1.0f;

    float MouseSensitivity = 1.0f;

    // 설정창의 임시 음성 모드 선택값이다.
    bool bVoiceActivationEnabled = false;

    // Slider 값은 부동소수점 오차를 허용해 비교한다.
    bool IsGraphicsEqual(const FDMSettingsValues& Other) const;
    bool IsLocalEqual(const FDMSettingsValues& Other) const;

    bool IsEqual(const FDMSettingsValues& Other) const
    {
        return IsGraphicsEqual(Other) && IsLocalEqual(Other);
    }
};

// 변경 확인창이 끝난 뒤 이어서 실행할 동작이다.
enum class EDMSettingsPendingAction : uint8
{
    None,
    SwitchCategory,
    CloseSettings
};

UCLASS()
class DONTMELT_API UDMSettingsWidget : public UUserWidget
{
    GENERATED_BODY()

public:
#pragma region Presentation

    // 저장된 값을 다시 불러오고 변경 상태와 확인창을 초기화한다.
    void OpenSettings();

    // 닫기 요청이다. 변경사항이 있으면 확인창을 먼저 표시한다.
    void RequestClose();

    /**
     * Esc 입력 처리.
     *
     * 확인창이 열려 있으면 확인창만 닫고, 아니면 닫기 요청으로 처리한다.
     * 설정창이 표시 중이 아니면 false를 반환한다.
     */
    bool HandleEscapeRequested();

    /**
     * 부모가 강제로 설정창을 숨길 때 사용한다.
     *
     * 매치 종료·네트워크 복구처럼 사용자 선택을 기다릴 수 없는 경우이며
     * 저장하지 않고 임시 변경사항과 확인창 상태만 버린다.
     * 닫힘 Delegate는 Broadcast하지 않는다.
     */
    void ForceDiscardPendingChanges();

    bool HasPendingChanges() const
    {
        return bHasPendingChanges;
    }

    // 변경 확인 절차가 끝나 실제로 닫아도 될 때 호출된다.
    FDMOnSettingsClosed& OnSettingsClosed()
    {
        return SettingsClosedDelegate;
    }

#pragma endregion

protected:
#pragma region Lifecycle

    virtual void NativeOnInitialized() override;

    virtual void NativeDestruct() override;

    // 타이틀·로비에서 설정창 내부에 포커스가 있을 때의 Esc 처리다.
    virtual FReply NativeOnKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

#pragma endregion

private:
#pragma region Graphics

    void RefreshGraphicsSettings();

    UFUNCTION()
    void HandleApplyClicked();

    UFUNCTION()
    void HandleResetClicked();

    UFUNCTION()
    void HandleBackClicked();

    // 현재 그래픽 UI 입력값을 UGameUserSettings에 반영한다. 사전 검증 실패 시 false.
    bool ApplyGraphicsValues(const FDMSettingsValues& Values);

    // 적용 전에 그래픽 입력값이 해석 가능한지 확인한다.
    bool ValidateGraphicsValues(const FDMSettingsValues& Values) const;

#pragma endregion

#pragma region Pending Changes

    // 저장된 설정을 UI에 다시 채우고 기준값으로 기록한다.
    void ReloadFromSavedSettings();

    // 현재 UI 입력값을 비교용 값으로 읽는다.
    FDMSettingsValues ReadValuesFromControls() const;

    // 기준값과 현재 UI 입력값을 비교해 변경 상태를 갱신한다.
    void UpdatePendingChangesState();

    // 변경된 값을 저장·적용한다. 실패하면 false를 반환하고 기준값을 유지한다.
    bool ApplyPendingChanges();

    // 지원 중인 설정을 기본값으로 UI에만 채운다. 저장하지 않는다.
    void ApplyDefaultValuesToControls();

    UFUNCTION()
    void HandleOptionSelectionChanged(FString SelectedItem, ESelectInfo::Type SelectionType);

    UFUNCTION()
    void HandleVolumeSliderChanged(float NewValue);

    UFUNCTION()
    void HandleVSyncChanged(bool bIsChecked);

#pragma endregion

#pragma region Confirmation

    void RequestCategory(int32 PageIndex);

    void ShowConfirmation(EDMSettingsPendingAction Action, int32 TargetPageIndex);

    void HideConfirmation();

    // 확인창에서 선택이 끝난 뒤 보류했던 탭 이동 또는 닫기를 실행한다.
    void ExecutePendingAction();

    // 확인 절차를 통과한 실제 닫기 처리다.
    void CompleteClose();

    bool IsConfirmationVisible() const;

    UFUNCTION()
    void HandleConfirmApplyClicked();

    UFUNCTION()
    void HandleDiscardChangesClicked();

    UFUNCTION()
    void HandleCancelConfirmationClicked();

#pragma endregion

#pragma region Video Confirmation

    bool RequiresVideoConfirmation(const FDMSettingsValues& Values) const;

    bool BeginVideoConfirmation(const FDMSettingsValues& Values);

    void UpdateVideoConfirmationCountdown();

    void EndVideoConfirmation(bool bRevert);

    UFUNCTION()
    void HandleKeepVideoModeClicked();

    UFUNCTION()
    void HandleRevertVideoModeClicked();

#pragma endregion

#pragma region Local Settings

    void RefreshLocalSettings();

    UFUNCTION()
    void HandleSensitivityPreviewChanged(float NewValue);

#pragma endregion

#pragma region Categories

    UFUNCTION()
    void HandleGraphicsClicked();

    UFUNCTION()
    void HandleAudioClicked();

    UFUNCTION()
    void HandleControlsClicked();

    UFUNCTION()
    void HandleLanguageClicked();

    UFUNCTION()
    void HandleVoiceClicked();

    void ShowCategory(int32 PageIndex);

#pragma endregion

#pragma region Category Widgets

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidgetSwitcher> SW_SettingsPages = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Graphics = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Audio = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Controls = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Language = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Voice = nullptr;

#pragma endregion

#pragma region Graphics Widgets

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> CB_Resolution = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> CB_WindowMode = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> CB_GraphicsQuality = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> CB_FrameRateLimit = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCheckBox> CHK_VSync = nullptr;

#pragma endregion

#pragma region Audio Widgets

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USlider> SL_MasterVolume = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USlider> SL_SFXVolume = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USlider> SL_MusicVolume = nullptr;

#pragma endregion

#pragma region Control Widgets

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USlider> SL_MouseSensitivity = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_MouseSensitivityValue = nullptr;

#pragma endregion

#pragma region Language Widgets

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> CB_Language = nullptr;

#pragma endregion

#pragma region Voice Widgets

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> CB_VoiceMode = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UComboBoxString> CB_InputDevice = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<USlider> SL_VoiceVolume = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_TestMicrophone = nullptr;

#pragma endregion

#pragma region Action Widgets

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Apply = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Reset = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Back = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_SettingsNotice = nullptr;

#pragma endregion

#pragma region Confirmation Widgets

    // 탭 이동과 설정창 닫기에 공통으로 재사용하는 변경 확인창이다.
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> ConfirmChangesPanel = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ConfirmTitle = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ConfirmDescription = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_ConfirmApply = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_DiscardChanges = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_CancelConfirmation = nullptr;

#pragma endregion

#pragma region Video Confirmation Widgets

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> VideoConfirmPanel = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_VideoCountdown = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_KeepVideoMode = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_RevertVideoMode = nullptr;

#pragma endregion

#pragma region Pending Change State

    // 설정창을 연 시점 또는 마지막 적용 시점의 값이다.
    FDMSettingsValues SavedValues;

    bool bHasPendingChanges = false;

    // UI를 코드로 채우는 동안 변경 이벤트를 무시한다.
    bool bIsPopulatingControls = false;

    // 확인창이 끝난 뒤 실행할 동작과 이동할 탭이다.
    // 확정 전에는 Widget Switcher의 Active Index를 바꾸지 않는다.
    EDMSettingsPendingAction PendingAction = EDMSettingsPendingAction::None;

    int32 PendingCategoryIndex = INDEX_NONE;

#pragma endregion

#pragma region Video Confirmation State

    // 변경된 화면 모드를 유지할지 확인하는 중이다.
    bool bVideoConfirmationActive = false;

    // 15초 뒤 이전 화면 설정으로 복원한다.
    double VideoConfirmationDeadline = 0.0;

    FTimerHandle VideoConfirmationTimerHandle;

#pragma endregion

#pragma region Delegates

    FDMOnSettingsClosed SettingsClosedDelegate;

#pragma endregion
};