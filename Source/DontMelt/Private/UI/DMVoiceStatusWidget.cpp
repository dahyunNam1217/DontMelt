#include "UI/DMVoiceStatusWidget.h"

#include "Voice/DMVoiceChatSubsystem.h"

#include "Components/Image.h"
#include "Engine/GameInstance.h"

#pragma region Lifecycle

void UDMVoiceStatusWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (!ensure(IMG_MicrophoneActive && IMG_MicrophoneMuted))
    {
        return;
    }

    // 표시만 담당하므로 마우스 입력을 차단하지 않는다.
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    UnbindVoiceSubsystem();

    UGameInstance* GameInstance = GetGameInstance();

    UDMVoiceChatSubsystem* Voice =
        IsValid(GameInstance) ? GameInstance->GetSubsystem<UDMVoiceChatSubsystem>() : nullptr;

    if (IsValid(Voice))
    {
        BoundVoiceSubsystem = Voice;

        // 이전 바인딩이 남아 있더라도 중복 구독하지 않는다.
        Voice->OnVoiceControlStateChanged().RemoveAll(this);

        Voice->OnVoiceControlStateChanged().AddUObject(this, &ThisClass::RefreshVoiceStatus);
    }

    // 최초 생성 시 현재 상태를 즉시 읽는다.
    RefreshVoiceStatus();
}

void UDMVoiceStatusWidget::NativeDestruct()
{
    UnbindVoiceSubsystem();

    Super::NativeDestruct();
}

#pragma endregion

#pragma region Voice Binding

void UDMVoiceStatusWidget::RefreshVoiceStatus()
{
    if (!IsValid(IMG_MicrophoneActive.Get()) || !IsValid(IMG_MicrophoneMuted.Get()))
    {
        return;
    }

    const UDMVoiceChatSubsystem* Voice = BoundVoiceSubsystem.Get();

    bool bShowActiveMic = false;
    bool bShowMutedMic = false;

    if (IsValid(Voice))
    {
        const bool bVoiceWorldActive = Voice->GetAppliedVoiceMode() != EDMVoiceMode::Disabled;

        if (bVoiceWorldActive)
        {
            if (Voice->IsVoiceActivationEnabled())
            {
                // 음성 감지 모드는 음소거 상태에서만 아이콘을 표시한다.
                bShowMutedMic = Voice->IsMicrophoneMuted();
            }
            else
            {
                // PTT 모드는 실제 송신 허용 시에만 표시한다.
                bShowActiveMic = Voice->IsNetworkVoiceTransmitting();
            }
        }
    }

    IMG_MicrophoneActive->SetVisibility(bShowActiveMic ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    IMG_MicrophoneMuted->SetVisibility(bShowMutedMic ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
}

void UDMVoiceStatusWidget::UnbindVoiceSubsystem()
{
    if (UDMVoiceChatSubsystem* Voice = BoundVoiceSubsystem.Get())
    {
        Voice->OnVoiceControlStateChanged().RemoveAll(this);
    }

    BoundVoiceSubsystem.Reset();
}

#pragma endregion