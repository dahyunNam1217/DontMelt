#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "DMVoiceStatusWidget.generated.h"

class UImage;
class UDMVoiceChatSubsystem;

/**
 * 로컬 마이크 조작 상태만 표시한다.
 * 음성 송신 여부는 VoiceSubsystem이 소유한다.
 */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMVoiceStatusWidget : public UUserWidget
{
    GENERATED_BODY()

protected:
#pragma region Lifecycle

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

#pragma endregion

#pragma region Widgets

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> IMG_MicrophoneActive = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UImage> IMG_MicrophoneMuted = nullptr;

#pragma endregion

private:
#pragma region Voice Binding

    void RefreshVoiceStatus();
    void UnbindVoiceSubsystem();

    TWeakObjectPtr<UDMVoiceChatSubsystem> BoundVoiceSubsystem;

#pragma endregion
};