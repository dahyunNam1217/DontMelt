#include "Framework/Lobby/DMLobbyPlayerController.h"

#include "Core/DMGameplayUtils.h"

#include "Framework/Lobby/DMLobbyGameMode.h"
#include "Framework/Lobby/DMLobbyPlayerState.h"

#include "Input/DMInputConfigDataAsset.h"

#include "UI/DMLobbyWidget.h"

#include "Core/DMGameInstance.h"
#include "Voice/DMVoiceChatSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/SoftObjectPath.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputAction.h"
#include "InputMappingContext.h"
#include "Engine/LocalPlayer.h"
#include "Engine/World.h"
#include "GameFramework/GameModeBase.h"
#include "GameFramework/GameStateBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMLobbyPlayerController, Log, All);

#pragma region Lifecycle

void ADMLobbyPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);

    if (!IsValid(EnhancedInput) || !IsValid(VoiceInputConfig.Get()))
    {
        UE_LOG(LogDMLobbyPlayerController, Warning, TEXT("Lobby voice input is not configured."));
        return;
    }

    // PTT 모드의 V 키 홀드
    if (IsValid(VoiceInputConfig->PushToTalkAction.Get()))
    {
        EnhancedInput->BindAction(
            VoiceInputConfig->PushToTalkAction.Get(), ETriggerEvent::Started, this, &ThisClass::InputPushToTalkStarted
        );
        EnhancedInput->BindAction(
            VoiceInputConfig->PushToTalkAction.Get(), ETriggerEvent::Completed, this, &ThisClass::InputPushToTalkStopped
        );
        EnhancedInput->BindAction(
            VoiceInputConfig->PushToTalkAction.Get(), ETriggerEvent::Canceled, this, &ThisClass::InputPushToTalkStopped
        );
    }

    // 음성 감지 모드의 M 키 음소거 토글
    if (IsValid(VoiceInputConfig->ToggleMicrophoneMuteAction.Get()))
    {
        EnhancedInput->BindAction(
            VoiceInputConfig->ToggleMicrophoneMuteAction.Get(),
            ETriggerEvent::Started,
            this,
            &ThisClass::InputToggleMicrophoneMute
        );
    }
}

void ADMLobbyPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!IsLocalController())
    {
        return;
    }

    // 로비 화면이 GameInstance 맵 로딩보다 늦게 준비되는 경우 음악을 재확인한다.
    if (UDMGameInstance* DMInstance = Cast<UDMGameInstance>(GetGameInstance()))
    {
        DMInstance->RefreshMusicForWorld(GetWorld());
    }

    if (IsValid(VoiceInputConfig.Get()) && IsValid(VoiceInputConfig->VoiceMappingContext.Get()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = DMGameplay::GetLocalInputSubsystem(this))
        {
            FModifyContextOptions ModifyOptions;
            ModifyOptions.bIgnoreAllPressedKeysUntilRelease = true;

            InputSubsystem->RemoveMappingContext(VoiceInputConfig->VoiceMappingContext.Get(), ModifyOptions);

            InputSubsystem->AddMappingContext(VoiceInputConfig->VoiceMappingContext.Get(), 10, ModifyOptions);
        }
    }

    // 로비 UI 생성 결과와 관계없이 로컬 음성 정책을 적용한다.
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UDMVoiceChatSubsystem* Voice = GameInstance->GetSubsystem<UDMVoiceChatSubsystem>())
        {
            Voice->EnterLobbyVoice();
        }
    }

    if (!ensureMsgf(LobbyWidgetClass, TEXT("LobbyWidgetClass is not assigned.")))
    {
        return;
    }

    LobbyWidget = CreateWidget<UDMLobbyWidget>(this, LobbyWidgetClass);

    if (!IsValid(LobbyWidget.Get()))
    {
        return;
    }

    LobbyWidget->AddToViewport();
    // 로비 UI를 유지하면서 음성 키 입력도 허용한다.
    FInputModeGameAndUI InputMode;

    InputMode.SetWidgetToFocus(LobbyWidget->TakeWidget());

    InputMode.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);

    SetInputMode(InputMode);
    bShowMouseCursor = true;
    bEnableClickEvents = true;

    LobbyWidget->InitializeLobby(this);

    // 처음 생성된 UI가 GameState를 찾지 못했다면 재시도한다.
    ScheduleLobbyGameStateRetry();
}

void ADMLobbyPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (IsLocalController() && IsValid(VoiceInputConfig.Get()) && IsValid(VoiceInputConfig->VoiceMappingContext.Get()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = DMGameplay::GetLocalInputSubsystem(this))
        {
            InputSubsystem->RemoveMappingContext(VoiceInputConfig->VoiceMappingContext.Get());
        }
    }

    // 이전 로비에서 사용한 정책을 종료한다.
    // 실제 Provider 객체는 GameInstance 수명 동안 유지한다.
    if (IsLocalController())
    {
        if (UGameInstance* GameInstance = GetGameInstance())
        {
            if (UDMVoiceChatSubsystem* Voice = GameInstance->GetSubsystem<UDMVoiceChatSubsystem>())
            {
                Voice->LeaveVoiceWorld();
            }
        }
    }

    // 맵을 떠날 때 초기화 재시도가 남지 않도록 정리한다.
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(LobbyGameStateRetryTimer);
    }

    if (IsValid(LobbyWidget.Get()))
    {
        LobbyWidget->RemoveFromParent();
    }

    LobbyWidget = nullptr;

    Super::EndPlay(EndPlayReason);
}

void ADMLobbyPlayerController::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    if (IsValid(LobbyWidget.Get()))
    {
        LobbyWidget->RefreshLobby();

        // 접속이 느려 초기 재시도(약 10초)를 모두 쓴 뒤에도 다시 연결을 시도할 수 있게 횟수를 초기화한다.
        // 이미 재시도 타이머가 돌고 있으면 ScheduleLobbyGameStateRetry가 중복 등록하지 않는다.
        LobbyGameStateRetryAttempts = 0;

        // PlayerState 수신 시 GameState 연결도 다시 검사한다.
        ScheduleLobbyGameStateRetry();
    }
}

void ADMLobbyPlayerController::ClientEnableNetworkVoice_Implementation(const bool bEnable)
{
    // 엔진이 PostLogin 시 보내는 ClientEnableNetworkVoice(!bRequiresPushToTalk)가
    // 음성 감지 모드의 전송을 꺼 버리지 않도록 Super(ToggleSpeaking)를 호출하지 않는다.
    // 실제 전송 여부는 UDMVoiceChatSubsystem::UpdateNetworkVoiceTransmission()이 결정한다.
}

#pragma endregion

#pragma region Voice Input

void ADMLobbyPlayerController::InputPushToTalkStarted()
{
    if (!IsLocalController())
    {
        return;
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UDMVoiceChatSubsystem* Voice = GameInstance->GetSubsystem<UDMVoiceChatSubsystem>())
        {
            Voice->SetPushToTalkPressed(true);
        }
    }
}

void ADMLobbyPlayerController::InputPushToTalkStopped()
{
    if (!IsLocalController())
    {
        return;
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UDMVoiceChatSubsystem* Voice = GameInstance->GetSubsystem<UDMVoiceChatSubsystem>())
        {
            Voice->SetPushToTalkPressed(false);
        }
    }
}

void ADMLobbyPlayerController::InputToggleMicrophoneMute()
{
    if (!IsLocalController())
    {
        return;
    }

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UDMVoiceChatSubsystem* Voice = GameInstance->GetSubsystem<UDMVoiceChatSubsystem>())
        {
            Voice->ToggleMicrophoneMuted();
        }
    }
}

#pragma endregion

#pragma region Lobby Initialization

void ADMLobbyPlayerController::ScheduleLobbyGameStateRetry()
{
    if (!IsLocalController() || !IsValid(LobbyWidget.Get()))
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    FTimerManager& TimerManager = World->GetTimerManager();

    // 연결에 성공했다면 더 이상 확인하지 않는다.
    if (LobbyWidget->IsGameStateBound())
    {
        TimerManager.ClearTimer(LobbyGameStateRetryTimer);
        LobbyGameStateRetryAttempts = 0;
        return;
    }

    // 최대 40회, 약 10초 동안 초기 연결을 재시도한다.
    if (LobbyGameStateRetryAttempts >= 40)
    {
        // 바인딩 실패 원인(BP GameMode의 GameState 클래스 오버라이드, 맵 GameMode Override 등)을
        // 바로 구분할 수 있도록 실제 GameState/GameMode 클래스를 함께 기록한다.
        const AGameStateBase* RawGameState = World->GetGameState();

        const AGameModeBase* AuthGameMode = World->GetAuthGameMode(); // 원격 클라이언트에서는 nullptr

        UE_LOG(
            LogDMLobbyPlayerController,
            Warning,
            TEXT("Lobby GameState binding timed out. Attempts=%d, GameStateClass=%s, GameModeClass=%s"),
            LobbyGameStateRetryAttempts,
            *GetNameSafe(IsValid(RawGameState) ? RawGameState->GetClass() : nullptr),
            *GetNameSafe(IsValid(AuthGameMode) ? AuthGameMode->GetClass() : nullptr)
        );
        return;
    }

    // 이미 예정된 재시도 Timer를 중복 등록하지 않는다.
    if (TimerManager.IsTimerActive(LobbyGameStateRetryTimer))
    {
        return;
    }

    TimerManager.SetTimer(
        LobbyGameStateRetryTimer, this, &ADMLobbyPlayerController::RetryLobbyGameStateBinding, 0.25f, false
    );
}

void ADMLobbyPlayerController::RetryLobbyGameStateBinding()
{
    if (!IsLocalController() || !IsValid(LobbyWidget.Get()))
    {
        return;
    }

    ++LobbyGameStateRetryAttempts;

    // 기존 로비 갱신 함수가 GameState를 다시 검색한다.
    LobbyWidget->RefreshLobby();

    if (LobbyWidget->IsGameStateBound())
    {
        UE_LOG(
            LogDMLobbyPlayerController,
            Log,
            TEXT("Lobby GameState binding completed. Attempts=%d"),
            LobbyGameStateRetryAttempts
        );

        LobbyGameStateRetryAttempts = 0;
        return;
    }

    // 아직 수신하지 못했다면 다음 한 번만 예약한다.
    ScheduleLobbyGameStateRetry();
}

#pragma endregion

void ADMLobbyPlayerController::RequestSetReady(const bool bNewReady)
{
    if (!IsLocalController())
    {
        return;
    }

    const ADMLobbyPlayerState* LobbyPlayerState = GetPlayerState<ADMLobbyPlayerState>();

    UE_LOG(
        LogDMLobbyPlayerController,
        Log,
        TEXT("Local ready request. PlayerName=%s, Requested=%s"),
        LobbyPlayerState != nullptr ? *LobbyPlayerState->GetPlayerName() : TEXT("Invalid"),
        bNewReady ? TEXT("true") : TEXT("false")
    );

    ServerSetReady(bNewReady);
}

void ADMLobbyPlayerController::RequestStartMatch()
{
    if (!IsLocalController())
    {
        return;
    }

    const ADMLobbyPlayerState* LobbyPlayerState = GetPlayerState<ADMLobbyPlayerState>();

    UE_LOG(
        LogDMLobbyPlayerController,
        Log,
        TEXT("Local start request. PlayerName=%s"),
        LobbyPlayerState != nullptr ? *LobbyPlayerState->GetPlayerName() : TEXT("Invalid")
    );

    ServerRequestStartMatch();
}

void ADMLobbyPlayerController::LeaveLobby()
{
    if (!IsLocalController())
    {
        return;
    }

    UWorld* World = GetWorld();

    UDMGameInstance* GameInstance = IsValid(World) ? Cast<UDMGameInstance>(World->GetGameInstance()) : nullptr;

    if (!IsValid(GameInstance))
    {
        UE_LOG(LogDMLobbyPlayerController, Error, TEXT("Leave lobby failed: GameInstance is invalid."));
        return;
    }

    // Steam 세션 해제를 완료한 뒤 로컬 메인 메뉴로 이동한다.
    if (!GameInstance->RequestLeaveOnlineRoom(World, MainMenuMap, false))
    {
        UE_LOG(LogDMLobbyPlayerController, Warning, TEXT("Leave lobby request was rejected."));
    }
}

void ADMLobbyPlayerController::DMReady()
{
    RequestSetReady(true);
}

void ADMLobbyPlayerController::DMUnready()
{
    RequestSetReady(false);
}

void ADMLobbyPlayerController::DMStartMatch()
{
    RequestStartMatch();
}

void ADMLobbyPlayerController::ServerSetReady_Implementation(const bool bNewReady)
{
    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return;
    }

    ADMLobbyGameMode* LobbyGameMode = Cast<ADMLobbyGameMode>(World->GetAuthGameMode());

    if (LobbyGameMode == nullptr)
    {
        return;
    }

    LobbyGameMode->HandleReadyRequest(this, bNewReady);
}

void ADMLobbyPlayerController::ServerRequestStartMatch_Implementation()
{
    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return;
    }

    ADMLobbyGameMode* LobbyGameMode = Cast<ADMLobbyGameMode>(World->GetAuthGameMode());

    if (LobbyGameMode == nullptr)
    {
        return;
    }

    LobbyGameMode->HandleStartMatchRequest(this);
}