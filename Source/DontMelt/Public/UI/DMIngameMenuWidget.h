
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"

#include "DMIngameMenuWidget.generated.h"

class UWidget;
class UButton;
class UDMSettingsWidget;
struct FGeometry;
struct FKeyEvent;

/**
 * 인게임 Esc 메뉴의 로컬 표시와 버튼 입력만 담당한다.
 * 매치 정지 및 서버 Gameplay 상태는 변경하지 않는다.
 */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMIngameMenuWidget : public UUserWidget
{
    GENERATED_BODY()

public:
#pragma region Presentation

    // 메뉴를 열 때 초기 페이지로 복귀한다.
    void OpenMenu();

#pragma endregion

protected:
#pragma region Lifecycle

    virtual void NativeOnInitialized() override;

    virtual void NativeConstruct() override;

    virtual void NativeDestruct() override;

    virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

#pragma endregion

private:
#pragma region Button Actions

    UFUNCTION()
    void HandleResumeClicked();

    UFUNCTION()
    void HandleSettingsClicked();

    UFUNCTION()
    void HandleLeaveMatchClicked();

    UFUNCTION()
    void HandleConfirmLeaveClicked();

    UFUNCTION()
    void HandleCancelLeaveClicked();

    void HandleSettingsClosed();

    void HandleBackNavigation();

#pragma endregion

#pragma region Presentation Helpers

    void ShowMainMenu();

    void ShowLeaveConfirmation();

#pragma endregion

#pragma region Panels

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> MenuPanel = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> ConfirmLeavePanel = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMSettingsWidget> SettingsPanel = nullptr;

#pragma endregion

#pragma region Buttons

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Resume = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Settings = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_LeaveMatch = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_ConfirmLeave = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_CancelLeave = nullptr;

#pragma endregion
};
