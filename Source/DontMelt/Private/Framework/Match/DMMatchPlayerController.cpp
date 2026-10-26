#include "Framework/Match/DMMatchPlayerController.h"

#include "Core/DMGameplayUtils.h"
#include "Core/DMMatchConstants.h"

#include "Framework/Match/DMMatchGameMode.h"
#include "Framework/Match/DMMatchGameState.h"
#include "Framework/Match/DMMatchPlayerState.h"

#include "Characters/DMPlayerCharacter.h"
#include "Characters/Components/DMCharacterAudioComponent.h"
#include "Characters/Components/DMJellyPhysicsComponent.h"

#include "Input/DMInputConfigDataAsset.h"

#include "Skills/DMSkillComponent.h"

#include "Relic/DMRelicChest.h"
#include "Relic/DMRelicManager.h"
#include "Relic/DMRelicTypes.h"

#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

#include "UI/DMGameRootWidget.h"
#include "UI/DMPreMatchTutorialWidget.h"
#include "UI/DMSpectatorCountdownWidget.h"
#include "UI/DMIngameMenuWidget.h"

#include "Spectator/DMSpectatorPresentationComponent.h"
#include "Core/DMGameplayTags.h"
#include "Core/DMGameInstance.h"
#include "Voice/DMVoiceChatSubsystem.h"

#include "Camera/CameraComponent.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"

#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialInstanceDynamic.h"

#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystemInterface.h"
#include "EnhancedInputSubsystems.h"
#include "InputMappingContext.h"
#include "InputActionValue.h"
#include "InputAction.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/SoftObjectPath.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "EngineUtils.h"
#include "TimerManager.h"
#include "GameFramework/GameStateBase.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMMatchPlayerController, Log, All);

namespace
{
    constexpr int32 GameRootWidgetZOrder = 10;
}

#pragma region Lifecycle

ADMMatchPlayerController::ADMMatchPlayerController()
{
    // 관전 연출은 로컬 전용 컴포넌트가 맡는다. 복제하지 않는다.
    SpectatorPresentation = CreateDefaultSubobject<UDMSpectatorPresentationComponent>(TEXT("SpectatorPresentation"));
}

void ADMMatchPlayerController::SetupInputComponent()
{
    Super::SetupInputComponent();

    UEnhancedInputComponent* EnhancedInput = Cast<UEnhancedInputComponent>(InputComponent);

    if (!ensureMsgf(IsValid(EnhancedInput), TEXT("EnhancedInputComponent is unavailable.")))
    {
        return;
    }

    // 로컬 메뉴 입력은 Controller에서 직접 바인딩한다.
    if (IsValid(MenuAction.Get()))
    {
        EnhancedInput->BindAction(MenuAction.Get(), ETriggerEvent::Started, this, &ThisClass::InputMenuStarted);
    }
    else
    {
        UE_LOG(LogDMMatchPlayerController, Warning, TEXT("Menu input binding is not configured."));
    }

    // 음성 입력은 Pawn이 아닌 로컬 Controller가 처리한다.
    if (IsValid(VoiceInputConfig.Get()))
    {
        // V 키: 키를 누르는 동안만 송신
        if (IsValid(VoiceInputConfig->PushToTalkAction.Get()))
        {
            EnhancedInput->BindAction(
                VoiceInputConfig->PushToTalkAction.Get(),
                ETriggerEvent::Started,
                this,
                &ThisClass::InputPushToTalkStarted
            );
            EnhancedInput->BindAction(
                VoiceInputConfig->PushToTalkAction.Get(), ETriggerEvent::Completed,
                this, &ThisClass::InputPushToTalkStopped
            );
            EnhancedInput->BindAction(
                VoiceInputConfig->PushToTalkAction.Get(), ETriggerEvent::Canceled,
                this, &ThisClass::InputPushToTalkStopped
            );
        }

        // M 키: 음성 감지 모드의 음소거 토글
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
    else
    {
        UE_LOG(LogDMMatchPlayerController, Warning, TEXT("Voice input configuration is unavailable."));
    }
}

void ADMMatchPlayerController::BeginPlay()
{
    Super::BeginPlay();

    // UI는 Owning Local Controller에서만 생성한다.
    if (!IsLocalController())
    {
        return;
    }

    // 로비·게임플레이·관전과 독립적으로 음성 Context를 등록한다.
    if (IsValid(VoiceInputConfig.Get()) && IsValid(VoiceInputConfig->VoiceMappingContext.Get()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetEnhancedInputSubsystem())
        {
            FModifyContextOptions ModifyOptions;
            ModifyOptions.bIgnoreAllPressedKeysUntilRelease = true;

            InputSubsystem->RemoveMappingContext(VoiceInputConfig->VoiceMappingContext.Get(), ModifyOptions);

            InputSubsystem->AddMappingContext(VoiceInputConfig->VoiceMappingContext.Get(), 10, ModifyOptions);
        }
    }

    // 이미 존재하는 LocalVoiceMode와 변경 Delegate를 연결한다.
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UDMVoiceChatSubsystem* Voice = GameInstance->GetSubsystem<UDMVoiceChatSubsystem>())
        {
            Voice->BindMatchController(this);
        }
    }

    // 위젯 생성 성공 여부와 관계없이 로컬 참가자 정보를 확인한다.
    {
        UWorld* World = GetWorld();

        UDMGameInstance* GameInstance = IsValid(World) ? Cast<UDMGameInstance>(World->GetGameInstance()) : nullptr;

        if (IsValid(GameInstance))
        {
            GameInstance->RefreshLocalMatchIdentity(World);
        }
    }

    // PostLoadMap보다 Controller BeginPlay가 늦는 경우에도 음악을 초기화한다.
    if (UDMGameInstance* DMInstance = Cast<UDMGameInstance>(GetGameInstance()))
    {
        DMInstance->RefreshMusicForWorld(GetWorld());
    }

    // LifeState가 Pawn보다 늦게 복제돼도 인계를 다시 검사한다.
    BindRespawnLifeStateChanges();

    // 슬롯과 매치 단계가 늦게 도착해도 로컬 마커가 만들어지도록 연결한다.
    BindOwnChestMarkerSources();
    RefreshOwnChestMarker();

    // HUD 생성이 실패해도 메뉴 입력 등록과 마우스 복원은 독립적으로 수행한다.
    if (!IsValid(GameRootWidget))
    {
        if (!IsValid(GameRootWidgetClass))
        {
            UE_LOG(LogDMMatchPlayerController, Error, TEXT("GameRootWidgetClass is not assigned."));
        }
        else
        {
            GameRootWidget = CreateWidget<UDMGameRootWidget>(this, GameRootWidgetClass);
            if (IsValid(GameRootWidget) && GameRootWidget->HasRequiredBindings())
            {
                GameRootWidget->AddToViewport(GameRootWidgetZOrder);
                UE_LOG(LogDMMatchPlayerController, Log, TEXT("Game Root Widget created successfully."));
                RefreshUIDataSources();
            }
            else if (IsValid(GameRootWidget))
            {
                UE_LOG(LogDMMatchPlayerController, Error,
                    TEXT("GameRootWidget has missing required bindings. Gameplay input initialization will continue."));
                GameRootWidget = nullptr;
            }
            else
            {
                UE_LOG(LogDMMatchPlayerController, Error, TEXT("Failed to create GameRootWidget."));
            }
        }
    }

    // 메뉴 전용 Mapping Context는 Controller 수명 동안 유지한다.
    if (IsValid(MenuMappingContext.Get()) && IsValid(MenuAction.Get()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetEnhancedInputSubsystem())
        {
            FModifyContextOptions ModifyOptions;
            ModifyOptions.bIgnoreAllPressedKeysUntilRelease = true;

            InputSubsystem->AddMappingContext(MenuMappingContext.Get(), 100, ModifyOptions);
        }
    }

    // 매치 진입 시 일반 캐릭터가 이미 준비됐다면 입력을 복구한다.
    // 아직 Pawn이 없으면 PawnClientRestart에서 다시 호출된다.
    RestoreGameplayMouseCapture();
}

void ADMMatchPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    ClosePreMatchTutorial();
    ClearPendingRespawnHandoff();
    UnbindRespawnLifeStateChanges();

    // 레벨 전환 시 이전 월드의 마커와 이벤트 구독을 남기지 않는다.
    ClearOwnChestMarker();
    UnbindOwnChestMarkerSources();

    // 로컬 메뉴 입력 Context만 제거한다.
    if (IsLocalController() && IsValid(MenuMappingContext.Get()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetEnhancedInputSubsystem())
        {
            InputSubsystem->RemoveMappingContext(MenuMappingContext.Get());
        }
    }

    bIngameMenuOpen = false;

    if (IsLocalController())
    {
        if (IsValid(VoiceInputConfig.Get()) && IsValid(VoiceInputConfig->VoiceMappingContext.Get()))
        {
            if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetEnhancedInputSubsystem())
            {
                InputSubsystem->RemoveMappingContext(VoiceInputConfig->VoiceMappingContext.Get());
            }
        }
    }

    SetLocalVoiceMode(EDMVoiceMode::Disabled);

    // 매치 Controller가 파괴되기 전에 Subsystem 구독을 정리한다.
    if (IsLocalController())
    {
        if (UGameInstance* GameInstance = GetGameInstance())
        {
            if (UDMVoiceChatSubsystem* Voice = GameInstance->GetSubsystem<UDMVoiceChatSubsystem>())
            {
                Voice->UnbindMatchController(this);
            }
        }
    }

    if (IsValid(SpectatorPresentation))
    {
        SpectatorPresentation->ShutdownPresentation();
    }

    CancelSkillTargetingLocal();

    ClearLocalFootstepAmplifyPresentation();

    ClearLocalVisionDistortPresentation();

    if (IsValid(GameRootWidget))
    {
        GameRootWidget->ShutdownDataSources();

        GameRootWidget->RemoveFromParent();

        GameRootWidget = nullptr;
    }

    SkillTargetingStateChangedDelegate.Clear();

    // 관전 정보 제한·관전 Phase를 구독했던 UI 연결을 정리한다.
    if (IsValid(SpectatorPresentation))
    {
        SpectatorPresentation->ClearPresentationDelegates();
    }

    LocalVoiceModeChangedDelegate.Clear();

    FootstepAmplifyWarningDelegate.Clear();

    VisionDistortPresentationChangedDelegate.Clear();

    Super::EndPlay(EndPlayReason);
}

void ADMMatchPlayerController::ClientEnableNetworkVoice_Implementation(const bool bEnable)
{
    // 엔진이 PostLogin 시 보내는 ClientEnableNetworkVoice(!bRequiresPushToTalk)가
    // 음성 감지 모드의 전송을 꺼 버리지 않도록 Super(ToggleSpeaking)를 호출하지 않는다.
    // 실제 전송 여부는 UDMVoiceChatSubsystem::UpdateNetworkVoiceTransmission()이 결정한다.
}

#pragma endregion

#pragma region UI Data Binding

void ADMMatchPlayerController::OnRep_PlayerState()
{
    Super::OnRep_PlayerState();

    if (IsLocalController())
    {
        UWorld* World = GetWorld();

        UDMGameInstance* GameInstance = IsValid(World) ? Cast<UDMGameInstance>(World->GetGameInstance()) : nullptr;

        if (IsValid(GameInstance))
        {
            GameInstance->RefreshLocalMatchIdentity(World);
        }
    }

    BindRespawnLifeStateChanges();
    TryCompletePendingRespawnHandoff();
    BindOwnChestMarkerSources();
    RefreshOwnChestMarker();
    RefreshUIDataSources();
}

void ADMMatchPlayerController::ClientShowPreMatchTutorial_Implementation()
{
    if (!IsLocalController())
    {
        return;
    }

    if (bPreMatchTutorialActive)
    {
        // 같은 요청이 재전달되더라도 Widget을 중복 생성하지 않는다.
        ServerReportPreMatchTutorialDisplay(IsValid(PreMatchTutorialWidget)
            && PreMatchTutorialWidget->HasRequiredBindings());
        return;
    }

    const ADMMatchGameState* State = GetWorld() ? GetWorld()->GetGameState<ADMMatchGameState>() : nullptr;
    if (IsValid(State) && State->GetCurrentPhase() != EDMMatchPhase::Waiting)
    {
        // 종료된 매치의 오래된 RPC는 무시한다.
        return;
    }

    if (!IsValid(PreMatchTutorialWidgetClass.Get()))
    {
        UE_LOG(LogDMMatchPlayerController, Error,
            TEXT("Tutorial display failed: PreMatchTutorialWidgetClass is not assigned."));
        ServerReportPreMatchTutorialDisplay(false);
        return;
    }

    PreMatchTutorialWidget = CreateWidget<UDMPreMatchTutorialWidget>(this, PreMatchTutorialWidgetClass);
    if (!IsValid(PreMatchTutorialWidget) || !PreMatchTutorialWidget->HasRequiredBindings())
    {
        UE_LOG(LogDMMatchPlayerController, Error,
            TEXT("Tutorial display failed: widget creation or required bindings are invalid."));
        if (IsValid(PreMatchTutorialWidget))
        {
            PreMatchTutorialWidget->RemoveFromParent();
        }
        PreMatchTutorialWidget = nullptr;
        ServerReportPreMatchTutorialDisplay(false);
        return;
    }

    bPreMatchTutorialActive = true;
    PreMatchTutorialWidget->OnTutorialCompleted().AddUObject(this, &ThisClass::HandlePreMatchTutorialFinished);
    PreMatchTutorialWidget->AddToViewport(200);

    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(PreMatchTutorialWidget->TakeWidget());
    SetInputMode(InputMode);
    bShowMouseCursor = true;
    bEnableClickEvents = true;
    PreMatchTutorialWidget->SetKeyboardFocus();

    if (!PreMatchTutorialWidget->IsInViewport())
    {
        UE_LOG(LogDMMatchPlayerController, Error, TEXT("Tutorial display failed: Widget was not added to viewport."));
        ClosePreMatchTutorial();
        ServerReportPreMatchTutorialDisplay(false);
        return;
    }

    // 실제 Widget 표시 이후에만 서버에 성공했다고 알린다.
    ServerReportPreMatchTutorialDisplay(true);
}

void ADMMatchPlayerController::ServerReportPreMatchTutorialDisplay_Implementation(const bool bSucceeded)
{
    if (ADMMatchGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ADMMatchGameMode>() : nullptr)
    {
        Mode->HandlePreMatchTutorialDisplayReported(this, bSucceeded);
    }
}

void ADMMatchPlayerController::ServerFinishPreMatchTutorial_Implementation()
{
    if (ADMMatchGameMode* Mode = GetWorld() ? GetWorld()->GetAuthGameMode<ADMMatchGameMode>() : nullptr)
    {
        Mode->HandlePreMatchTutorialCompleted(this);
    }
}

void ADMMatchPlayerController::HandlePreMatchTutorialFinished()
{
    if (!bPreMatchTutorialActive || !IsValid(PreMatchTutorialWidget))
    {
        return;
    }

    PreMatchTutorialWidget->MarkWaitingForOtherPlayers();
    ServerFinishPreMatchTutorial();
}

void ADMMatchPlayerController::ClosePreMatchTutorial()
{
    bPreMatchTutorialActive = false;
    if (IsValid(PreMatchTutorialWidget))
    {
        PreMatchTutorialWidget->OnTutorialCompleted().RemoveAll(this);
        PreMatchTutorialWidget->RemoveFromParent();
        PreMatchTutorialWidget = nullptr;
    }
}

bool ADMMatchPlayerController::IsPreMatchTutorialVisible() const
{
    return bPreMatchTutorialActive;
}

void ADMMatchPlayerController::OnRep_Pawn()
{
    Super::OnRep_Pawn();
    RefreshUIDataSources();
    TryCompletePendingRespawnHandoff();
}

void ADMMatchPlayerController::RefreshUIDataSources()
{
    if (!IsLocalController())
    {
        return;
    }

    // UI 위젯이 없어도 마커는 자체 Presentation으로 갱신한다.
    BindOwnChestMarkerSources();
    RefreshOwnChestMarker();

    if (!IsValid(GameRootWidget))
    {
        return;
    }

    UWorld* World = GetWorld();

    ADMMatchGameState* MatchGameState = nullptr;

    if (IsValid(World))
    {
        MatchGameState = World->GetGameState<ADMMatchGameState>();
    }

    ADMMatchPlayerState* MatchPlayerState = Cast<ADMMatchPlayerState>(PlayerState);

    ADMPlayerCharacter* CurrentPawn = Cast<ADMPlayerCharacter>(GetPawn());

    GameRootWidget->RebindDataSources(this, MatchGameState, MatchPlayerState, CurrentPawn);
}

#pragma endregion

#pragma region Own Chest Marker Presentation

void ADMMatchPlayerController::BindOwnChestMarkerSources()
{
    if (!IsLocalController())
    {
        return;
    }

    ADMMatchPlayerState* CurrentPlayerState = GetPlayerState<ADMMatchPlayerState>();

    if (ObservedOwnChestPlayerState.Get() != CurrentPlayerState)
    {
        if (ADMMatchPlayerState* PreviousPlayerState = ObservedOwnChestPlayerState.Get())
        {
            PreviousPlayerState->OnBaseSlotChanged().RemoveAll(this);
        }

        ObservedOwnChestPlayerState = CurrentPlayerState;

        if (IsValid(CurrentPlayerState))
        {
            CurrentPlayerState->OnBaseSlotChanged().AddUObject(
                this, &ThisClass::HandleOwnChestBaseSlotChanged
            );
        }
    }

    UWorld* World = GetWorld();
    ADMMatchGameState* CurrentGameState = IsValid(World) ? World->GetGameState<ADMMatchGameState>() : nullptr;

    if (ObservedOwnChestGameState.Get() != CurrentGameState)
    {
        if (ADMMatchGameState* PreviousGameState = ObservedOwnChestGameState.Get())
        {
            PreviousGameState->OnPhaseStateChanged().RemoveAll(this);
        }

        ObservedOwnChestGameState = CurrentGameState;

        if (IsValid(CurrentGameState))
        {
            CurrentGameState->OnPhaseStateChanged().AddUObject(
                this, &ThisClass::HandleOwnChestPhaseChanged
            );
        }
    }
}

void ADMMatchPlayerController::UnbindOwnChestMarkerSources()
{
    if (ADMMatchPlayerState* PreviousPlayerState = ObservedOwnChestPlayerState.Get())
    {
        PreviousPlayerState->OnBaseSlotChanged().RemoveAll(this);
    }

    if (ADMMatchGameState* PreviousGameState = ObservedOwnChestGameState.Get())
    {
        PreviousGameState->OnPhaseStateChanged().RemoveAll(this);
    }

    ObservedOwnChestPlayerState.Reset();
    ObservedOwnChestGameState.Reset();
}

void ADMMatchPlayerController::ClearOwnChestMarker()
{
    if (IsValid(ActiveOwnChestMarker))
    {
        ActiveOwnChestMarker->DeactivateImmediate();
        ActiveOwnChestMarker->DestroyComponent();
    }

    ActiveOwnChestMarker = nullptr;
    MarkedOwnChest.Reset();
}

void ADMMatchPlayerController::RefreshOwnChestMarker()
{
    if (!IsLocalController() || !IsValid(OwnChestMarkerEffect.Get()))
    {
        ClearOwnChestMarker();
        return;
    }

    UWorld* World = GetWorld();
    const ADMMatchPlayerState* LocalPlayerState = GetPlayerState<ADMMatchPlayerState>();
    const ADMMatchGameState* MatchGameState = IsValid(World) ? World->GetGameState<ADMMatchGameState>() : nullptr;

    if (!IsValid(World) || !IsValid(LocalPlayerState) || !LocalPlayerState->HasAssignedBaseSlot()
        || !IsValid(MatchGameState))
    {
        ClearOwnChestMarker();
        return;
    }

    // 경기 시작 전 Waiting 및 결과 상태에는 마커를 남기지 않는다.
    const EDMMatchPhase CurrentPhase = MatchGameState->GetCurrentPhase();
    const bool bDisplayDuringMatch = CurrentPhase == EDMMatchPhase::Countdown
        || CurrentPhase == EDMMatchPhase::Playing
        || CurrentPhase == EDMMatchPhase::OvertimeIntro
        || CurrentPhase == EDMMatchPhase::Overtime;

    if (!bDisplayDuringMatch)
    {
        ClearOwnChestMarker();
        return;
    }

    // 상자는 레벨에 배치된 정적 Actor이므로 상태 변화 시에만 검색한다.
    ADMRelicChest* OwnChest = nullptr;
    const int32 LocalBaseSlot = LocalPlayerState->GetBaseSlot();

    for (TActorIterator<ADMRelicChest> It(World); It; ++It)
    {
        ADMRelicChest* Chest = *It;

        if (!IsValid(Chest) || Chest->GetBaseSlot() != LocalBaseSlot || !Chest->IsBaseActive())
        {
            continue;
        }

        OwnChest = Chest;
        break;
    }

    if (!IsValid(OwnChest))
    {
        ClearOwnChestMarker();
        return;
    }

    // 같은 상자 위에 이미 마커가 살아 있다면 다시 생성하지 않는다.
    if (MarkedOwnChest.Get() == OwnChest && IsValid(ActiveOwnChestMarker))
    {
        return;
    }

    ClearOwnChestMarker();

    FVector BoundsOrigin = FVector::ZeroVector;
    FVector BoundsExtent = FVector::ZeroVector;

    // 상호작용 범위(Box)보다 실제 상자 메시 크기를 높이 계산에 우선 사용한다.
    UStaticMeshComponent* ChestMesh = OwnChest->FindComponentByClass<UStaticMeshComponent>();

    if (IsValid(ChestMesh) && ChestMesh->IsRegistered() && IsValid(ChestMesh->GetStaticMesh()))
    {
        BoundsOrigin = ChestMesh->Bounds.Origin;
        BoundsExtent = ChestMesh->Bounds.BoxExtent;
    }
    else
    {
        OwnChest->GetActorBounds(false, BoundsOrigin, BoundsExtent);
    }

    const FVector MarkerPosition = BoundsOrigin + FVector(0.0f, 0.0f, BoundsExtent.Z + OwnChestMarkerHeightOffset);

    // 소유 클라이언트의 월드에만 NiagaraComponent를 만든다. 서버 Spawn/RPC는 없다.
    UNiagaraComponent* CreatedMarker = UNiagaraFunctionLibrary::SpawnSystemAtLocation(
        World,
        OwnChestMarkerEffect.Get(),
        MarkerPosition,
        FRotator::ZeroRotator,
        FVector::OneVector,
        false, // 플레이어가 직접 정리하므로 자동 파괴를 끈다.
        true,  // 생성과 동시에 활성화한다.
        ENCPoolMethod::None,
        false  // 멀리 있는 내 상자도 마커를 생성할 수 있게 한다.
    );

    if (!IsValid(CreatedMarker))
    {
        UE_LOG(LogDMMatchPlayerController, Warning, TEXT("OwnChestMarker spawn failed. BaseSlot=%d"), LocalBaseSlot);
        return;
    }

    ActiveOwnChestMarker = CreatedMarker;
    MarkedOwnChest = OwnChest;

    UE_LOG(LogDMMatchPlayerController, Log, TEXT("OwnChestMarker created. BaseSlot=%d, Chest=%s"),
        LocalBaseSlot, *GetNameSafe(OwnChest));
}

void ADMMatchPlayerController::HandleOwnChestBaseSlotChanged(
    ADMMatchPlayerState* ChangedPlayerState, const int32 NewBaseSlot)
{
    (void)NewBaseSlot;

    if (IsLocalController() && ChangedPlayerState == GetPlayerState<ADMMatchPlayerState>())
    {
        RefreshOwnChestMarker();
    }
}

void ADMMatchPlayerController::HandleOwnChestPhaseChanged(
    ADMMatchGameState* ChangedGameState, const FDMMatchPhaseState NewPhaseState)
{
    if (IsLocalController() && ChangedGameState == ObservedOwnChestGameState.Get())
    {
        // 서버에서 전원 완료 후 Countdown에 진입하면 개별 튜토리얼 화면을 닫는다.
        if (bPreMatchTutorialActive && NewPhaseState.CurrentPhase != EDMMatchPhase::Waiting)
        {
            ClosePreMatchTutorial();
            RestoreGameplayMouseCapture();
        }
        RefreshOwnChestMarker();
    }
}

#pragma endregion

#pragma region Voice Input

void ADMMatchPlayerController::InputPushToTalkStarted()
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

void ADMMatchPlayerController::InputPushToTalkStopped()
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

void ADMMatchPlayerController::InputToggleMicrophoneMute()
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

#pragma region Gameplay Input

void ADMMatchPlayerController::RestoreGameplayMouseCapture()
{
    // 소유 클라이언트의 일반 캐릭터 조작 상태에서만 복구한다.
    if (!IsLocalController() || !IsValid(GetPawn<ADMPlayerCharacter>()) || bIngameMenuOpen || bResultInputModeActive
        || bMatchResultPresentationStarted || bLeavingMatchToTitle || bPreMatchTutorialActive
        || IsSpectatorInputModeActive()
        || IsSpectatorInformationRestricted() || IsChestInteractionOpen() || IsSkillTargetingActive())
    {
        return;
    }

    // 경기 종료 후 결과 UI의 마우스 입력을 가로채지 않는다.
    const UWorld* World = GetWorld();

    const ADMMatchGameState* MatchGameState = IsValid(World) ? World->GetGameState<ADMMatchGameState>() : nullptr;

    if (IsValid(MatchGameState))
    {
        const EDMMatchPhase Phase = MatchGameState->GetCurrentPhase();

        if (Phase == EDMMatchPhase::Finished || Phase == EDMMatchPhase::Aborted)
        {
            return;
        }
    }

    // 게임플레이에서는 마우스를 뷰포트가 캡처하도록 한다.
    FInputModeGameOnly InputMode;

    InputMode.SetConsumeCaptureMouseDown(false);

    SetInputMode(InputMode);

    bShowMouseCursor = false;
    bEnableClickEvents = false;
    bEnableMouseOverEvents = false;

    UE_LOG(
        LogDMMatchPlayerController, Log, TEXT("Gameplay mouse input mode restored. Pawn=%s"), *GetNameSafe(GetPawn())
    );
}

#pragma endregion

#pragma region Ingame Menu

void ADMMatchPlayerController::InputMenuStarted()
{
    if (!IsLocalController())
    {
        return;
    }

    if (bIngameMenuOpen)
    {
        CloseIngameMenu();
        return;
    }

    OpenIngameMenu();
}

void ADMMatchPlayerController::OpenIngameMenu()
{
    if (!IsLocalController() || bIngameMenuOpen || !IsValid(GameRootWidget.Get()))
    {
        return;
    }

    UWorld* World = GetWorld();

    const ADMMatchGameState* MatchGameState = IsValid(World) ? World->GetGameState<ADMMatchGameState>() : nullptr;

    if (!IsValid(MatchGameState))
    {
        return;
    }

    const EDMMatchPhase Phase = MatchGameState->GetCurrentPhase();

    // 경기 종료 후에는 메뉴를 열지 않는다.
    if (Phase == EDMMatchPhase::Finished || Phase == EDMMatchPhase::Aborted)
    {
        return;
    }

    // 기존 모달의 입력 상태를 보호한다.
    if (IsChestInteractionOpen() || IsSkillTargetingActive())
    {
        return;
    }

    UDMIngameMenuWidget* MenuWidget = GameRootWidget->GetIngameMenuWidget();

    if (!IsValid(MenuWidget))
    {
        return;
    }

    bIngameMenuOpen = true;

    MenuWidget->OpenMenu();

    GameRootWidget->RefreshIngameMenuPresentation();

    // 로컬 조작 입력을 UI로 전환한다.
    // 서버 매치와 다른 플레이어의 입력은 멈추지 않는다.
    FInputModeUIOnly InputMode;

    InputMode.SetWidgetToFocus(MenuWidget->TakeWidget());

    SetInputMode(InputMode);

    bShowMouseCursor = true;
    bEnableClickEvents = true;
}

void ADMMatchPlayerController::CloseIngameMenu(const bool bRefreshPresentation)
{
    if (!IsLocalController() || !bIngameMenuOpen)
    {
        return;
    }

    bIngameMenuOpen = false;

    // 일반 게임플레이로 돌아갈 수 있는 상태인지 확인한 뒤 복구한다.
    RestoreGameplayMouseCapture();

    if (bRefreshPresentation && IsValid(GameRootWidget.Get()))
    {
        GameRootWidget->RefreshIngameMenuPresentation();
    }
}

void ADMMatchPlayerController::LeaveMatchToTitle(const bool bOpenMainMenuPanel)
{
    if (!IsLocalController() || bLeavingMatchToTitle)
    {
        return;
    }

    UWorld* World = GetWorld();

    UDMGameInstance* GameInstance = IsValid(World) ? Cast<UDMGameInstance>(World->GetGameInstance()) : nullptr;

    if (!IsValid(World) || !IsValid(GameInstance))
    {
        return;
    }

    // 설정 오류도 이동 실패와 같은 복구 경로로 보낸다.
    // 호스트: 결과 이동 중단 → 기본 맵 OpenLevel / Client: ReturnToMainMenu
    auto RecoverFromInvalidMainMenuMap = [&](const FString& Diagnostic)
    {
        UE_LOG(LogDMMatchPlayerController, Error, TEXT("Return to main menu failed: %s"), *Diagnostic);

        GameInstance->StartTravelFailureRecovery(World, Diagnostic);
    };

    if (MainMenuMap.IsNull())
    {
        RecoverFromInvalidMainMenuMap(TEXT("MainMenuMap이 설정되지 않았습니다."));
        return;
    }

    const FString MapPath = MainMenuMap.ToSoftObjectPath().GetLongPackageName();

    // 경로는 있지만 패키지가 없는 경우도 같은 문제이므로 함께 검사한다.
    if (MapPath.IsEmpty() || !FPackageName::IsValidLongPackageName(MapPath) || !FPackageName::DoesPackageExist(MapPath))
    {
        RecoverFromInvalidMainMenuMap(FString::Printf(TEXT("메인 메뉴 맵을 찾을 수 없습니다. 경로=%s"), *MapPath));
        return;
    }

    const ENetMode NetMode = GetNetMode();

    const bool bLocalServer = NetMode == NM_ListenServer || NetMode == NM_Standalone;

    if (!bLocalServer && NetMode != NM_Client)
    {
        return;
    }

    // Steam 작업 중이라면 호스트 이동 잠금을 걸기 전에 거부한다.
    if (!GameInstance->CanBeginOnlineRoomExit())
    {
        UE_LOG(
            LogDMMatchPlayerController, Warning, TEXT("Leave match deferred: another online operation is in progress.")
        );
        return;
    }

    if (bLocalServer)
    {
        ADMMatchGameMode* MatchGameMode = World->GetAuthGameMode<ADMMatchGameMode>();

        if (!IsValid(MatchGameMode) || !MatchGameMode->TryCommitHostLeaveToMainMenu(this))
        {
            return;
        }
    }

    bLeavingMatchToTitle = true;

    // Steam 세션이 정리된 다음 메뉴에 진입한다.
    const bool bAccepted = GameInstance->RequestLeaveOnlineRoom(World, MainMenuMap, bOpenMainMenuPanel);

    if (!bAccepted)
    {
        bLeavingMatchToTitle = false;

        // 호스트가 이미 매치 이동을 잠갔다면 오류 복구로 전환한다.
        if (bLocalServer)
        {
            GameInstance->StartTravelFailureRecovery(World, TEXT("Steam 세션 정리 요청을 시작하지 못했습니다."));
        }

        UE_LOG(LogDMMatchPlayerController, Error, TEXT("Leave match request was rejected."));
    }
}

#pragma endregion

#pragma region Voice Integration Boundary

void ADMMatchPlayerController::SetLocalVoiceMode(const EDMVoiceMode NewVoiceMode)
{
    if (!IsLocalController())
    {
        return;
    }

    if (LocalVoiceMode == NewVoiceMode)
    {
        return;
    }

    const EDMVoiceMode PreviousVoiceMode = LocalVoiceMode;

    LocalVoiceMode = NewVoiceMode;

    LocalVoiceModeChangedDelegate.Broadcast(LocalVoiceMode);

    UE_LOG(
        LogDMMatchPlayerController,
        Log,
        TEXT("Local Voice Mode changed. %s -> %s"),
        *UEnum::GetValueAsString(PreviousVoiceMode),
        *UEnum::GetValueAsString(LocalVoiceMode)
    );
}

void ADMMatchPlayerController::RefreshLocalVoiceModeForGameplayPawn(ADMPlayerCharacter* PlayerCharacter)
{
    if (!IsLocalController() || !IsValid(PlayerCharacter))
    {
        return;
    }

    /*
     * 현재 실제 Local Possessed Pawn이어야 한다.
     */
    if (GetPawn() != PlayerCharacter)
    {
        return;
    }

    /*
     * Respawn 실패 등으로 기존 Pawn이 다시
     * PawnClientRestart되더라도,
     * Spectator Information Restriction이 유지되는 동안에는
     * Voice Policy를 AliveProximity로 복구하지 않는다.
     */
    if (IsSpectatorInformationRestricted())
    {
        return;
    }

    SetLocalVoiceMode(EDMVoiceMode::AliveProximity);
}

#pragma endregion

#pragma region Spectator

void ADMMatchPlayerController::InitializeSpectatorInput(ADMPlayerCharacter* PlayerCharacter)
{
    if (IsValid(SpectatorPresentation))
    {
        SpectatorPresentation->InitializeSpectatorInput(PlayerCharacter);
    }
}

bool ADMMatchPlayerController::IsSpectatorInformationRestricted() const
{
    return IsValid(SpectatorPresentation) && SpectatorPresentation->IsSpectatorInformationRestricted();
}

FDMOnSpectatorInformationRestrictionChanged& ADMMatchPlayerController::OnSpectatorInformationRestrictionChanged()
{
    // 생성자에서 항상 만드는 기본 서브오브젝트다.
    return SpectatorPresentation->OnSpectatorInformationRestrictionChanged();
}

EDMSpectatorPhase ADMMatchPlayerController::GetLocalSpectatorPhase() const
{
    return IsValid(SpectatorPresentation) ? SpectatorPresentation->GetLocalSpectatorPhase() : EDMSpectatorPhase::Corpse;
}

double ADMMatchPlayerController::GetRemainingSpectatorTimeSeconds() const
{
    return IsValid(SpectatorPresentation) ? SpectatorPresentation->GetRemainingSpectatorTimeSeconds() : 0.0;
}

FDMOnLocalSpectatorPhaseChanged& ADMMatchPlayerController::OnLocalSpectatorPhaseChanged()
{
    // 생성자에서 항상 만드는 기본 서브오브젝트다.
    return SpectatorPresentation->OnLocalSpectatorPhaseChanged();
}

bool ADMMatchPlayerController::IsSpectatorInputModeActive() const
{
    return IsValid(SpectatorPresentation) && SpectatorPresentation->IsSpectatorInputModeActive();
}

FDMSpectatorPresentationSettings ADMMatchPlayerController::GetSpectatorPresentationSettings() const
{
    FDMSpectatorPresentationSettings Settings;

    Settings.DeathCamDistance = DeathCamDistance;
    Settings.DeathCamHeight = DeathCamHeight;
    Settings.DeathCamBlendTime = DeathCamBlendTime;
    Settings.KillerViewBlendTime = KillerViewBlendTime;
    Settings.FreeCameraMaxSpeed = FreeCameraMaxSpeed;
    Settings.SpectatorFadeToBlackDuration = SpectatorFadeToBlackDuration;
    Settings.SpectatorCountdownWidgetClass = SpectatorCountdownWidgetClass;

    return Settings;
}

bool ADMMatchPlayerController::ShouldBlockSpectatorPresentation() const
{
    return IsValid(SpectatorPresentation) && SpectatorPresentation->ShouldBlockSpectatorPresentation();
}

void ADMMatchPlayerController::PrepareLocalPresentationForMatchResult()
{
    ClosePreMatchTutorial();
    if (!IsLocalController())
    {
        return;
    }

    // 결과 화면은 관전/부활 인계보다 우선한다. 지연된 응답도 무효화한다.
    bMatchResultPresentationStarted = true;
    ClearPendingRespawnHandoff();

    if (IsValid(SpectatorPresentation))
    {
        SpectatorPresentation->StopForMatchResult();
    }

    if (!bResultInputModeActive)
    {
        FInputModeUIOnly InputMode;
        SetInputMode(InputMode);

        bShowMouseCursor = true;
        bEnableClickEvents = true;

        bResultInputModeActive = true;
    }
}

void ADMMatchPlayerController::PrepareLocalGameplayForSpectating()
{
    if (!IsLocalController())
    {
        return;
    }

    /*
     * ==================================================
     * Skill Targeting
     * ==================================================
     *
     * Pending Skill 선택과 해당 Local Delegate를 정리한다.
     */
    CancelSkillTargetingLocal();

    /*
     * ==================================================
     * Chest Interaction Snapshot
     * ==================================================
     *
     * RegisteredRelicCount 등의 Inspect Snapshot이
     * Spectator 중 남지 않도록 Local Context를 비운다.
     */
    CloseChestInteractionLocal();

    /*
     * ==================================================
     * Pawn-bound Skill Presentation
     * ==================================================
     *
     * 죽기 전에 VisionDistort를 맞은 상태라면
     * Spectator Camera까지 PostProcess가 이어지지 않게 한다.
     *
     * 이미 Server의 Skill Death Cleanup RPC가
     * 실행했더라도 이 함수는 idempotent하다.
     */
    ClearLocalVisionDistortPresentation();

    /*
     * FootstepAmplify는 여기서 제거하지 않는다.
     *
     * 현재 설계상 PlayerState lifetime effect이며
     * Death만으로 종료하지 않는다.
     */

    ADMPlayerCharacter* DeadPlayerCharacter = GetPawn<ADMPlayerCharacter>();

    if (IsValid(DeadPlayerCharacter))
    {
        DeadPlayerCharacter->PrepareLocalInputForSpectating();
    }

    UE_LOG(LogDMMatchPlayerController, Log, TEXT("Local Gameplay presentation prepared for spectating."));
}

void ADMMatchPlayerController::ClientBeginSpectatorSequence_Implementation(FDMSpectatorContext SpectatorContext)
{
    if (!IsLocalController() || ShouldBlockSpectatorPresentation() || !IsValid(SpectatorPresentation))
    {
        return;
    }

    // 이전 사망에서 도착한 응답/타이머를 먼저 제거한다.
    ClearPendingRespawnHandoff();
    LocalPreviousDeadPawn = GetPawn<ADMPlayerCharacter>();
    SpectatorPresentation->ReceiveSpectatorContext(SpectatorContext);

    SetLocalVoiceMode(EDMVoiceMode::DeadGlobal);

    PrepareLocalGameplayForSpectating();

    UE_LOG(
        LogDMMatchPlayerController,
        Log,
        TEXT("Spectator Context received. DeathLocation=%s, KillerPawn=%s, StartServerTime=%.3f"),
        *SpectatorContext.DeathLocation.ToCompactString(),
        *GetNameSafe(SpectatorContext.KillerPawn),
        SpectatorContext.SpectatorStartServerTime
    );

    SpectatorPresentation->BeginCorpseSpectatorPhase();

    // RPC 자체가 도착하지 않아도 서버 부활 상태 재확인을 유한 시간 안에 시작한다.
    const float Remaining = static_cast<float>(SpectatorPresentation->GetRemainingSpectatorTimeSeconds());
    ArmRespawnHandoffWatchdog(FMath::Max(45.0f, Remaining + 12.0f));
}

void ADMMatchPlayerController::ClientFinalizeSpectatorRespawnHandoff_Implementation(ADMPlayerCharacter* ExpectedNewPawn)
{
    if (!IsLocalController() || ShouldBlockSpectatorPresentation())
    {
        return;
    }

    // 서버가 방금 실제 Possess를 완료한 Pawn을 함께 보낸다.
    // 임의의 살아 있는 이전 Pawn으로 인계하지 않기 위해 동일 포인터를 검사한다.
    if (!IsValid(ExpectedNewPawn) || ExpectedNewPawn == LocalPreviousDeadPawn.Get())
    {
        UE_LOG(
            LogDMMatchPlayerController, Warning,
            TEXT("Respawn handoff approval contains no valid new Pawn. Expected=%s, Previous=%s"),
            *GetNameSafe(ExpectedNewPawn), *GetNameSafe(LocalPreviousDeadPawn.Get())
        );
        ServerRequestRespawnHandoffResync();
        return;
    }

    const bool bNewApproval = !bRespawnHandoffApproved || ServerApprovedRespawnPawn.Get() != ExpectedNewPawn;
    bRespawnHandoffApproved = true;
    ServerApprovedRespawnPawn = ExpectedNewPawn;

    if (bNewApproval)
    {
        RespawnWatchdogExpirations = 0;
        RespawnResyncRequestCount = 0;
        ArmRespawnHandoffWatchdog(12.0f);

        if (UWorld* World = GetWorld())
        {
            World->GetTimerManager().SetTimer(
                RespawnHandoffResyncHandle, this, &ThisClass::HandleRespawnHandoffResyncTimer, 3.0f, true
            );
        }
    }

    TryCompletePendingRespawnHandoff();
}

void ADMMatchPlayerController::ServerRequestRespawnHandoffResync_Implementation()
{
    if (!HasAuthority())
    {
        return;
    }

    UWorld* World = GetWorld();
    ADMMatchGameState* MatchGameState = IsValid(World) ? World->GetGameState<ADMMatchGameState>() : nullptr;
    ADMMatchPlayerState* MatchPlayerState = GetPlayerState<ADMMatchPlayerState>();
    ADMPlayerCharacter* CurrentPawn = GetPawn<ADMPlayerCharacter>();

    if (!IsValid(World) || !IsValid(MatchGameState) || !IsValid(MatchPlayerState)
        || MatchGameState->GetMatchEndReason() != EDMMatchEndReason::None
        || (MatchGameState->GetCurrentPhase() != EDMMatchPhase::Playing
            && MatchGameState->GetCurrentPhase() != EDMMatchPhase::Overtime)
        || MatchPlayerState->GetLifeState() != EDMPlayerLifeState::Alive
        || !IsValid(CurrentPawn) || CurrentPawn->GetController() != this
        || CurrentPawn->GetPlayerState<ADMMatchPlayerState>() != MatchPlayerState)
    {
        UE_LOG(LogDMMatchPlayerController, Verbose, TEXT("Respawn resync deferred: server is not in Alive/Possessed state."));
        return;
    }

    // 클라이언트 오류/중복 요청이 세션 트래픽을 늘리지 않도록 서버에서도 제한한다.
    const double Now = World->GetTimeSeconds();
    if (Now - LastServerRespawnResyncTime < 2.0)
    {
        return;
    }
    LastServerRespawnResyncTime = Now;

    UE_LOG(LogDMMatchPlayerController, Warning, TEXT("Resending authoritative respawn Pawn. Pawn=%s"), *GetNameSafe(CurrentPawn));
    // 엔진의 기존 재소유 검사를 재사용한다. 이미 확인된 Pawn이면 불필요한 Restart를 건너뛴다.
    ClientRetryClientRestart(CurrentPawn);
    ClientFinalizeSpectatorRespawnHandoff(CurrentPawn);
}

void ADMMatchPlayerController::NotifyLocalGameplayPawnReady(ADMPlayerCharacter* ReadyPawn)
{
    if (!IsLocalController() || !IsValid(ReadyPawn) || GetPawn() != ReadyPawn)
    {
        return;
    }

    // 실제 PawnClientRestart의 입력 MappingContext 적용 후에만 기록한다.
    LocalInputReadyPawn = ReadyPawn;
    TryCompletePendingRespawnHandoff();
}

void ADMMatchPlayerController::BindRespawnLifeStateChanges()
{
    if (!IsLocalController())
    {
        return;
    }

    ADMMatchPlayerState* Current = GetPlayerState<ADMMatchPlayerState>();
    if (ObservedRespawnPlayerState.Get() == Current)
    {
        return;
    }

    UnbindRespawnLifeStateChanges();
    if (IsValid(Current))
    {
        Current->OnLifeStateChanged().AddUObject(this, &ThisClass::HandleRespawnLifeStateChanged);
        ObservedRespawnPlayerState = Current;
    }
}

void ADMMatchPlayerController::UnbindRespawnLifeStateChanges()
{
    if (ADMMatchPlayerState* Previous = ObservedRespawnPlayerState.Get())
    {
        Previous->OnLifeStateChanged().RemoveAll(this);
    }
    ObservedRespawnPlayerState.Reset();
}

void ADMMatchPlayerController::HandleRespawnLifeStateChanged(
    ADMMatchPlayerState* ChangedPlayerState, const EDMPlayerLifeState NewLifeState)
{
    if (ChangedPlayerState != GetPlayerState<ADMMatchPlayerState>())
    {
        return;
    }

    // Alive가 늦게 복제된 경우에도 Pawn 준비 완료 검사를 다시 수행한다.
    if (NewLifeState == EDMPlayerLifeState::Alive)
    {
        TryCompletePendingRespawnHandoff();
    }
    else if (NewLifeState == EDMPlayerLifeState::Disconnected)
    {
        ClearPendingRespawnHandoff();
    }
}

void ADMMatchPlayerController::ArmRespawnHandoffWatchdog(const float DelaySeconds)
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            RespawnHandoffWatchdogHandle, this, &ThisClass::HandleRespawnHandoffWatchdog,
            FMath::Max(1.0f, DelaySeconds), false
        );
    }
}

void ADMMatchPlayerController::HandleRespawnHandoffResyncTimer()
{
    if (!IsLocalController() || ShouldBlockSpectatorPresentation())
    {
        ClearPendingRespawnHandoff();
        return;
    }

    TryCompletePendingRespawnHandoff();
    if (!bRespawnHandoffApproved)
    {
        return;
    }

    // 반복 요청은 두 번으로 제한하고, 완료 대기는 Watchdog이 담당한다.
    if (RespawnResyncRequestCount < 2)
    {
        ++RespawnResyncRequestCount;
        ServerRequestRespawnHandoffResync();
    }
    else if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RespawnHandoffResyncHandle);
    }
}

void ADMMatchPlayerController::HandleRespawnHandoffWatchdog()
{
    if (!IsLocalController() || ShouldBlockSpectatorPresentation())
    {
        ClearPendingRespawnHandoff();
        return;
    }

    TryCompletePendingRespawnHandoff();
    if (!IsValid(SpectatorPresentation) || !SpectatorPresentation->HasActiveSpectatorPresentation())
    {
        ClearPendingRespawnHandoff();
        return;
    }

    // 일반 RPC 부재에도 안전하게 서버에게 현재 Possess 결과를 한 번 더 조회한다.
    if (RespawnWatchdogExpirations++ == 0)
    {
        UE_LOG(LogDMMatchPlayerController, Warning, TEXT("Respawn handoff timeout. Requesting authoritative resync."));
        ServerRequestRespawnHandoffResync();
        ArmRespawnHandoffWatchdog(12.0f);
        return;
    }

    // 실제 서버 Pawn이 준비되지 않은 상태에서는 검은 Fade를 임의로 열지 않는다.
    UE_LOG(
        LogDMMatchPlayerController, Error,
        TEXT("Respawn handoff unrecoverable: Pawn=%s, ApprovedPawn=%s, ReadyPawn=%s, LifeState=%s"),
        *GetNameSafe(GetPawn()), *GetNameSafe(ServerApprovedRespawnPawn.Get()),
        *GetNameSafe(LocalInputReadyPawn.Get()),
        *UEnum::GetValueAsString(IsValid(GetPlayerState<ADMMatchPlayerState>())
            ? GetPlayerState<ADMMatchPlayerState>()->GetLifeState() : EDMPlayerLifeState::Disconnected)
    );

    ClearPendingRespawnHandoff();
    if (UDMGameInstance* GameInstance = Cast<UDMGameInstance>(GetGameInstance()))
    {
        GameInstance->StartTravelFailureRecovery(GetWorld(), TEXT("부활 후 클라이언트 Pawn/카메라 동기화를 복구하지 못했습니다."));
    }
    else
    {
        LeaveMatchToTitle(true);
    }
}

void ADMMatchPlayerController::ClearPendingRespawnHandoff()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RespawnHandoffWatchdogHandle);
        World->GetTimerManager().ClearTimer(RespawnHandoffResyncHandle);
    }
    bRespawnHandoffApproved = false;
    ServerApprovedRespawnPawn.Reset();
    LocalPreviousDeadPawn.Reset();
    LocalInputReadyPawn.Reset();
    RespawnWatchdogExpirations = 0;
    RespawnResyncRequestCount = 0;
}

void ADMMatchPlayerController::TryCompletePendingRespawnHandoff()
{
    if (!bRespawnHandoffApproved || !IsLocalController() || ShouldBlockSpectatorPresentation())
    {
        return;
    }

    ADMPlayerCharacter* NewCharacter = GetPawn<ADMPlayerCharacter>();
    ADMMatchPlayerState* MatchPlayerState = GetPlayerState<ADMMatchPlayerState>();

    // LifeState 원본과 서버가 승인한 바로 그 Pawn이 모두 준비돼야 한다.
    if (!IsValid(NewCharacter) || NewCharacter != ServerApprovedRespawnPawn.Get()
        || NewCharacter == LocalPreviousDeadPawn.Get() || LocalInputReadyPawn.Get() != NewCharacter
        || !IsValid(MatchPlayerState) || MatchPlayerState->GetLifeState() != EDMPlayerLifeState::Alive
        || NewCharacter->GetController() != this
        || NewCharacter->GetPlayerState<ADMMatchPlayerState>() != MatchPlayerState)
    {
        return;
    }

    const UDMJellyPhysicsComponent* Physics = NewCharacter->GetJellyPhysicsComponent();
    if (IsValid(Physics) && Physics->IsDeathPresentationActive())
    {
        return;
    }

    CompleteSpectatorRespawnHandoff(NewCharacter);

    if (IsValid(SpectatorPresentation) && !SpectatorPresentation->HasActiveSpectatorPresentation())
    {
        UE_LOG(LogDMMatchPlayerController, Log, TEXT("Respawn handoff confirmed: Pawn=%s, LifeState=Alive."), *GetNameSafe(NewCharacter));
        ClearPendingRespawnHandoff();
        RefreshLocalVoiceModeForGameplayPawn(NewCharacter);
        RefreshUIDataSources();
    }
}

void ADMMatchPlayerController::CompleteSpectatorRespawnHandoff(ADMPlayerCharacter* NewPlayerCharacter)
{
    if (!IsLocalController() || ShouldBlockSpectatorPresentation() || !IsValid(NewPlayerCharacter)
        || !IsValid(SpectatorPresentation) || GetPawn() != NewPlayerCharacter)
    {
        return;
    }

    const bool bHasSpectatorPresentation =
        SpectatorPresentation->HasActiveSpectatorPresentation() || LocalVoiceMode == EDMVoiceMode::DeadGlobal;

    if (!bHasSpectatorPresentation)
    {
        return;
    }

    // 카메라 대상 먼저 교체 → 관전 로컬 Actor/입력 정리 → 마지막으로 Fade 해제.
    SpectatorPresentation->FinishSequenceForRespawn(NewPlayerCharacter);
    RestoreGameplayMouseCapture();

    UE_LOG(
        LogDMMatchPlayerController, Log,
        TEXT("Spectator Respawn Handoff completed. NewPawn=%s, ViewTarget=%s"),
        *GetNameSafe(NewPlayerCharacter), *GetNameSafe(GetViewTarget())
    );
}

UEnhancedInputLocalPlayerSubsystem* ADMMatchPlayerController::GetEnhancedInputSubsystem() const
{
    return DMGameplay::GetLocalInputSubsystem(this);
}

#pragma endregion

#pragma region Relic HUD

void ADMMatchPlayerController::ClientNotifyRelicStolen_Implementation()
{
    if (!IsLocalController())
    {
        return;
    }

    RelicStolenNotificationDelegate.Broadcast();
}

#pragma endregion

#pragma region Debug Commands

void ADMMatchPlayerController::DMTestFinishMatch(const int32 WinningBaseSlot)
{
#if !UE_BUILD_SHIPPING
    if (!HasAuthority())
    {
        UE_LOG(
            LogDMMatchPlayerController,
            Warning,
            TEXT("DMTestFinishMatch rejected: only the Listen Server host may execute this command.")
        );

        return;
    }

    UWorld* World = GetWorld();

    ADMMatchGameMode* MatchGameMode = IsValid(World) ? World->GetAuthGameMode<ADMMatchGameMode>() : nullptr;

    if (!IsValid(MatchGameMode))
    {
        UE_LOG(LogDMMatchPlayerController, Error, TEXT("DMTestFinishMatch failed: MatchGameMode is unavailable."));

        return;
    }

    const ADMMatchGameState* MatchGameState = World->GetGameState<ADMMatchGameState>();

    bool bWinnerExists = false;

    if (IsValid(MatchGameState) && DMMatch::IsValidBaseSlot(WinningBaseSlot))
    {
        for (const ADMMatchPlayerState* MatchPlayerState : DMGameplay::GetMatchPlayerStates(MatchGameState))
        {
            if (MatchPlayerState->GetBaseSlot() == WinningBaseSlot)
            {
                bWinnerExists = true;
                break;
            }
        }
    }

    if (!bWinnerExists)
    {
        UE_LOG(
            LogDMMatchPlayerController,
            Warning,
            TEXT("DMTestFinishMatch rejected: specify an existing winning BaseSlot (0-3). Input=%d"),
            WinningBaseSlot
        );
        return;
    }

    MatchGameMode->TryFinishMatch(EDMMatchEndReason::RegularTimeVictory, WinningBaseSlot);

#else
    // Shipping 빌드에서는 테스트 명령을 실행하지 않는다.
    (void)WinningBaseSlot;
#endif
}

void ADMMatchPlayerController::DMTestAbortMatch()
{
#if !UE_BUILD_SHIPPING
    if (!HasAuthority())
    {
        UE_LOG(
            LogDMMatchPlayerController,
            Warning,
            TEXT("DMTestAbortMatch rejected: only the Listen Server host may execute this command.")
        );

        return;
    }

    UWorld* World = GetWorld();

    ADMMatchGameMode* MatchGameMode = IsValid(World) ? World->GetAuthGameMode<ADMMatchGameMode>() : nullptr;

    if (!IsValid(MatchGameMode))
    {
        UE_LOG(LogDMMatchPlayerController, Error, TEXT("DMTestAbortMatch failed: MatchGameMode is unavailable."));

        return;
    }

    MatchGameMode->TryAbortMatch(EDMMatchEndReason::HostLeft);
#else
    // Shipping 빌드에서는 테스트 명령을 실행하지 않는다.
#endif
}

void ADMMatchPlayerController::DMTestStartOvertime()
{
#if !UE_BUILD_SHIPPING
    if (!HasAuthority())
    {
        UE_LOG(
            LogDMMatchPlayerController,
            Warning,
            TEXT("DMTestStartOvertime rejected: only the Listen Server host may execute this command.")
        );

        return;
    }

    UWorld* World = GetWorld();

    ADMMatchGameMode* MatchGameMode = IsValid(World) ? World->GetAuthGameMode<ADMMatchGameMode>() : nullptr;

    if (!IsValid(MatchGameMode))
    {
        UE_LOG(LogDMMatchPlayerController, Error, TEXT("DMTestStartOvertime failed: MatchGameMode is unavailable."));

        return;
    }

    MatchGameMode->TryStartOvertimeIntro();
#else
    // Shipping 빌드에서는 테스트 명령을 실행하지 않는다.
#endif
}

void ADMMatchPlayerController::DMTestFinishOvertime()
{
#if !UE_BUILD_SHIPPING
    if (!HasAuthority())
    {
        UE_LOG(
            LogDMMatchPlayerController,
            Warning,
            TEXT("DMTestFinishOvertime rejected: only the Listen Server host may execute this command.")
        );

        return;
    }

    UWorld* World = GetWorld();

    ADMMatchGameMode* MatchGameMode = IsValid(World) ? World->GetAuthGameMode<ADMMatchGameMode>() : nullptr;

    if (!IsValid(MatchGameMode))
    {
        UE_LOG(LogDMMatchPlayerController, Error, TEXT("DMTestFinishOvertime failed: MatchGameMode is unavailable."));

        return;
    }

#if !UE_BUILD_SHIPPING
    MatchGameMode->DebugResolveOvertimeEnd();
#endif
#else
    // Shipping 빌드에서는 테스트 명령을 실행하지 않는다.
#endif
}

void ADMMatchPlayerController::DMTestNPCPanic()
{
#if !UE_BUILD_SHIPPING
    if (!HasAuthority())
    {
        UE_LOG(
            LogDMMatchPlayerController,
            Warning,
            TEXT("DMTestNPCPanic rejected: only the Listen Server host may execute this command.")
        );

        return;
    }

    UWorld* World = GetWorld();

    ADMMatchGameMode* MatchGameMode = IsValid(World) ? World->GetAuthGameMode<ADMMatchGameMode>() : nullptr;

    if (!IsValid(MatchGameMode))
    {
        UE_LOG(LogDMMatchPlayerController, Error, TEXT("DMTestNPCPanic failed: MatchGameMode is unavailable."));

        return;
    }

    MatchGameMode->RequestNPCPanic();

    UE_LOG(LogDMMatchPlayerController, Log, TEXT("DMTestNPCPanic requested."));
#else
    // Shipping 빌드에서는 테스트 명령을 실행하지 않는다.
#endif
}

#pragma endregion

#pragma region Match Result Commands

void ADMMatchPlayerController::ServerSetRematchReady_Implementation(const bool bNewReady)
{
    UWorld* World = GetWorld();

    ADMMatchGameMode* MatchGameMode = IsValid(World) ? World->GetAuthGameMode<ADMMatchGameMode>() : nullptr;

    if (IsValid(MatchGameMode))
    {
        MatchGameMode->HandleRematchReadyRequest(this, bNewReady);
    }
}

void ADMMatchPlayerController::ServerRequestReturnToLobby_Implementation()
{
    UWorld* World = GetWorld();

    ADMMatchGameMode* MatchGameMode = IsValid(World) ? World->GetAuthGameMode<ADMMatchGameMode>() : nullptr;

    if (!IsValid(MatchGameMode))
    {
        return;
    }

    MatchGameMode->HandleReturnToLobbyRequest(this);
}

void ADMMatchPlayerController::ServerRequestRestartMatch_Implementation()
{
    UWorld* World = GetWorld();

    ADMMatchGameMode* MatchGameMode = IsValid(World) ? World->GetAuthGameMode<ADMMatchGameMode>() : nullptr;

    if (!IsValid(MatchGameMode))
    {
        return;
    }

    MatchGameMode->HandleRestartMatchRequest(this);
}

void ADMMatchPlayerController::NotifyMatchResultCached(const FGuid& ResultId)
{
    if (!IsLocalController() || HasAuthority() // 호스트는 확인 대상이 아니다.
        || !ResultId.IsValid() || LastAcknowledgedResultId == ResultId)
    {
        return;
    }

    LastAcknowledgedResultId = ResultId;
    ServerAcknowledgeMatchResult(ResultId);
}

void ADMMatchPlayerController::ServerAcknowledgeMatchResult_Implementation(FGuid ResultId)
{
    UWorld* World = GetWorld();

    if (ADMMatchGameMode* MatchGameMode = IsValid(World) ? World->GetAuthGameMode<ADMMatchGameMode>() : nullptr)
    {
        MatchGameMode->HandleMatchResultAcknowledged(this, ResultId);
    }
}

#pragma endregion

#pragma region Relic Commands

void ADMMatchPlayerController::RequestDropCarriedRelic()
{
    if (!IsLocalController())
    {
        return;
    }

    ServerRequestDropCarriedRelic();
}

void ADMMatchPlayerController::ServerRequestDropCarriedRelic_Implementation()
{
    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    ADMMatchPlayerState* MatchPlayerState = GetPlayerState<ADMMatchPlayerState>();

    if (!IsValid(MatchPlayerState))
    {
        return;
    }

    /*
     * Client가 Q를 조작하더라도
     * 서버 LifeState를 다시 확인한다.
     */
    if (MatchPlayerState->GetLifeState() != EDMPlayerLifeState::Alive)
    {
        UE_LOG(LogDMMatchPlayerController, Log, TEXT("Manual Relic Drop rejected: player is not Alive."));

        return;
    }

    ADMMatchGameState* MatchGameState = World->GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        return;
    }

    const EDMMatchPhase CurrentPhase = MatchGameState->GetCurrentPhase();

    if (CurrentPhase != EDMMatchPhase::Playing && CurrentPhase != EDMMatchPhase::Overtime)
    {
        UE_LOG(
            LogDMMatchPlayerController,
            Log,
            TEXT("Manual Relic Drop rejected: invalid MatchPhase=%s"),
            *UEnum::GetValueAsString(CurrentPhase)
        );

        return;
    }

    ADMPlayerCharacter* PlayerCharacter = Cast<ADMPlayerCharacter>(GetPawn());

    if (!IsValid(PlayerCharacter))
    {
        return;
    }

    ADMMatchGameMode* MatchGameMode = World->GetAuthGameMode<ADMMatchGameMode>();

    if (!IsValid(MatchGameMode))
    {
        return;
    }

    ADMRelicManager* RelicManager = MatchGameMode->GetRelicManager();

    if (!IsValid(RelicManager))
    {
        return;
    }

    FString FailureReason;

    if (!RelicManager->TryDropCarriedRelic(
            MatchPlayerState, PlayerCharacter, EDMRelicDropReason::Manual, FailureReason
        ))
    {
        UE_LOG(LogDMMatchPlayerController, Log, TEXT("Manual Relic Drop rejected. Reason=%s"), *FailureReason);

        return;
    }

    UE_LOG(
        LogDMMatchPlayerController,
        Log,
        TEXT("Manual Relic Drop accepted. PlayerState=%s"),
        *GetNameSafe(MatchPlayerState)
    );
}

#pragma endregion

#pragma region Skill Commands

UDMSkillComponent* ADMMatchPlayerController::GetOwnedSkillComponent() const
{
    const ADMMatchPlayerState* MatchPlayerState = GetPlayerState<ADMMatchPlayerState>();

    if (!IsValid(MatchPlayerState))
    {
        return nullptr;
    }

    return MatchPlayerState->GetSkillComponent();
}

void ADMMatchPlayerController::InputSkillInvisibilityStarted()
{
    if (!IsLocalController())
    {
        return;
    }

    ServerRequestUseSelfSkill(DMGameplayTags::Skill_Invisibility);
}

void ADMMatchPlayerController::InputSkillFootstepAmplifyStarted()
{
    if (!IsLocalController())
    {
        return;
    }

    ServerRequestBeginSkillTargeting(DMGameplayTags::Skill_FootstepAmplify);
}

void ADMMatchPlayerController::InputSkillVisionDistortStarted()
{
    if (!IsLocalController())
    {
        return;
    }

    ServerRequestBeginSkillTargeting(DMGameplayTags::Skill_VisionDistort);
}

void ADMMatchPlayerController::InputTargetRandomStarted()
{
    CommitPendingTargetSkill(EDMSkillTargetMode::RandomEligible);
}

void ADMMatchPlayerController::InputTargetLeaderStarted()
{
    CommitPendingTargetSkill(EDMSkillTargetMode::LeadingEligible);
}

void ADMMatchPlayerController::InputCancelTargetingStarted()
{
    CancelSkillTargetingLocal();
}

void ADMMatchPlayerController::CancelSkillTargetingLocal()
{
    if (!IsLocalController())
    {
        return;
    }

    const FGameplayTag PreviousSkillTag = PendingTargetSkillTag;

    const bool bWasTargeting = PreviousSkillTag.IsValid();

    if (ADMPlayerCharacter* PlayerCharacter = Cast<ADMPlayerCharacter>(GetPawn()))
    {
        PlayerCharacter->ExitSkillTargetingInputMode();
    }

    PendingTargetSkillTag = FGameplayTag();

    if (bWasTargeting)
    {
        /*
         * Gameplay Mapping Context 복원이 끝난 뒤
         * Local UI에 Targeting 종료를 알린다.
         */
        SkillTargetingStateChangedDelegate.Broadcast(false, PreviousSkillTag);

        UE_LOG(LogDMMatchPlayerController, Log, TEXT("Skill targeting cancelled locally."));
    }
}

void ADMMatchPlayerController::CommitPendingTargetSkill(const EDMSkillTargetMode TargetMode)
{
    if (!IsLocalController() || !PendingTargetSkillTag.IsValid())
    {
        return;
    }

    const FGameplayTag SkillTag = PendingTargetSkillTag;

    /*
     * 입력 모드는 즉시 원래 Gameplay로 되돌린다.
     *
     * Server에서는 이후 현재 상태를 다시 검증한다.
     */
    CancelSkillTargetingLocal();

    ServerRequestTargetedSkill(SkillTag, TargetMode);
}

void ADMMatchPlayerController::ServerRequestUseSelfSkill_Implementation(const FGameplayTag SkillTag)
{
    UDMSkillComponent* SkillComponent = GetOwnedSkillComponent();

    if (!IsValid(SkillComponent))
    {
        return;
    }

    /*
     * 현재 설계에서 Self Skill은
     * Invisibility 하나뿐이다.
     *
     * Client가 다른 Tag를 Self Skill RPC로 전달해도
     * Server가 거부한다.
     */
    if (SkillTag != DMGameplayTags::Skill_Invisibility)
    {
        UE_LOG(
            LogDMMatchPlayerController,
            Warning,
            TEXT("Self Skill request rejected. Unsupported Skill=%s"),
            *SkillTag.ToString()
        );

        return;
    }

    /*
     * 실제 Gameplay 검증/실행은
     * SkillComponent가 담당한다.
     */
    if (!SkillComponent->TryExecuteInvisibility())
    {
        UE_LOG(LogDMMatchPlayerController, Log, TEXT("Self Skill execution rejected. Skill=%s"), *SkillTag.ToString());

        return;
    }

    UE_LOG(LogDMMatchPlayerController, Log, TEXT("Self Skill executed successfully. Skill=%s"), *SkillTag.ToString());
}

void ADMMatchPlayerController::ServerRequestBeginSkillTargeting_Implementation(const FGameplayTag SkillTag)
{
    UDMSkillComponent* SkillComponent = GetOwnedSkillComponent();

    if (!IsValid(SkillComponent))
    {
        return;
    }

    UDMSkillSetDataAsset* SkillSetData = SkillComponent->GetSkillSetData();

    if (!IsValid(SkillSetData))
    {
        return;
    }

    const FDMSkillDefinition* Definition = SkillSetData->FindSkillDefinition(SkillTag);

    /*
     * Self Skill은 Targeting Mode 진입 불가.
     */
    if (Definition == nullptr || !Definition->bRequiresTargetSelection)
    {
        return;
    }

    const FDMActionAvailabilityResult Availability = SkillComponent->GetSkillAvailability(SkillTag);

    if (!Availability.bAllowed)
    {
        UE_LOG(
            LogDMMatchPlayerController,
            Log,
            TEXT("Skill targeting request rejected. Skill=%s, Reason=%s"),
            *SkillTag.ToString(),
            *UEnum::GetValueAsString(Availability.BlockReason)
        );

        return;
    }

    /*
     * Client가 임의로 Targeting Mode에 들어가는 것이 아니라
     * Server validation 통과 후 승인한다.
     */
    ClientBeginSkillTargeting(SkillTag);

    UE_LOG(LogDMMatchPlayerController, Log, TEXT("Skill targeting request accepted. Skill=%s"), *SkillTag.ToString());
}

void ADMMatchPlayerController::ClientBeginSkillTargeting_Implementation(const FGameplayTag SkillTag)
{
    if (!IsLocalController() || !SkillTag.IsValid() || IsSpectatorInformationRestricted())
    {
        return;
    }

    ADMPlayerCharacter* PlayerCharacter = Cast<ADMPlayerCharacter>(GetPawn());

    if (!IsValid(PlayerCharacter))
    {
        return;
    }

    /*
     * 기존 Pending Context가 남아 있다면
     * 우선 정리한다.
     */
    if (PendingTargetSkillTag.IsValid())
    {
        CancelSkillTargetingLocal();
    }

    if (!PlayerCharacter->EnterSkillTargetingInputMode())
    {
        UE_LOG(
            LogDMMatchPlayerController,
            Error,
            TEXT("Failed to enter Skill Targeting input mode. Skill=%s"),
            *SkillTag.ToString()
        );

        return;
    }

    PendingTargetSkillTag = SkillTag;

    /*
     * 실제 Input Context 전환 성공 후에만
     * UI에 Targeting 시작을 알린다.
     */
    SkillTargetingStateChangedDelegate.Broadcast(true, SkillTag);

    UE_LOG(LogDMMatchPlayerController, Log, TEXT("Skill Targeting input mode entered. Skill=%s"), *SkillTag.ToString());
}

void ADMMatchPlayerController::ServerRequestTargetedSkill_Implementation(
    const FGameplayTag SkillTag, const EDMSkillTargetMode TargetMode
)
{
    UDMSkillComponent* SkillComponent = GetOwnedSkillComponent();

    if (!IsValid(SkillComponent))
    {
        return;
    }

    UDMSkillSetDataAsset* SkillSetData = SkillComponent->GetSkillSetData();

    if (!IsValid(SkillSetData))
    {
        return;
    }

    const FDMSkillDefinition* Definition = SkillSetData->FindSkillDefinition(SkillTag);

    /*
     * Self Skill을 Targeted RPC로 우회하지 못하게 한다.
     */
    if (Definition == nullptr || !Definition->bRequiresTargetSelection)
    {
        return;
    }

    /*
     * Targeting Mode에 들어온 이후
     * 상태가 바뀌었을 수 있으므로 다시 검사한다.
     */
    const FDMActionAvailabilityResult Availability = SkillComponent->GetSkillAvailability(SkillTag);

    if (!Availability.bAllowed)
    {
        UE_LOG(
            LogDMMatchPlayerController,
            Log,
            TEXT("Targeted Skill request rejected. Skill=%s, Reason=%s"),
            *SkillTag.ToString(),
            *UEnum::GetValueAsString(Availability.BlockReason)
        );

        return;
    }

    ADMMatchPlayerState* TargetPlayerState = SkillComponent->ResolveSkillTarget(SkillTag, TargetMode);

    if (!IsValid(TargetPlayerState))
    {
        UE_LOG(
            LogDMMatchPlayerController,
            Log,
            TEXT("Targeted Skill request rejected: no valid target. Skill=%s"),
            *SkillTag.ToString()
        );

        return;
    }

    if (SkillTag == DMGameplayTags::Skill_FootstepAmplify)
    {
        if (!SkillComponent->TryExecuteFootstepAmplify(TargetPlayerState))
        {
            UE_LOG(LogDMMatchPlayerController, Log, TEXT("Footstep Amplify execution rejected."));

            return;
        }

        UE_LOG(
            LogDMMatchPlayerController,
            Log,
            TEXT("Footstep Amplify executed successfully. Mode=%s"),
            *UEnum::GetValueAsString(TargetMode)
        );

        return;
    }

    if (SkillTag == DMGameplayTags::Skill_VisionDistort)
    {
        if (!SkillComponent->TryExecuteVisionDistort(TargetPlayerState))
        {
            UE_LOG(LogDMMatchPlayerController, Log, TEXT("Vision Distort execution rejected."));

            return;
        }

        UE_LOG(
            LogDMMatchPlayerController,
            Log,
            TEXT("Vision Distort executed successfully. Mode=%s"),
            *UEnum::GetValueAsString(TargetMode)
        );

        return;
    }

    /*
     * 알 수 없는 Target Skill에 대한 방어적 fallback.
     *
     * 따라서 여기서 cooldown을 소비하지 않는다.
     */
    UE_LOG(
        LogDMMatchPlayerController,
        Log,
        TEXT("Targeted Skill validated. Skill=%s, Mode=%s, Target=%s. Execution is deferred."),
        *SkillTag.ToString(),
        *UEnum::GetValueAsString(TargetMode),
        *GetNameSafe(TargetPlayerState)
    );
}

void ADMMatchPlayerController::ClientSetFootstepAmplifyPresentation_Implementation(
    ADMMatchPlayerState* TargetPlayerState, const bool bActive, const double EffectEndServerTime
)
{
    if (!IsLocalController())
    {
        return;
    }

    /*
     * 종료 요청이면 현재 Local Presentation만 정리한다.
     */
    if (!bActive)
    {
        ClearLocalFootstepAmplifyPresentation();

        return;
    }

    if (!IsValid(TargetPlayerState))
    {
        return;
    }

    /*
     * 이전 Presentation이 남아 있다면 먼저 정리.
     */
    ClearLocalFootstepAmplifyPresentation();

    ADMPlayerCharacter* TargetCharacter = Cast<ADMPlayerCharacter>(TargetPlayerState->GetPawn());

    if (!IsValid(TargetCharacter))
    {
        return;
    }

    UDMCharacterAudioComponent* AudioComponent = TargetCharacter->GetCharacterAudioComponent();

    if (!IsValid(AudioComponent))
    {
        return;
    }

    AudioComponent->SetFootstepAmplifyListenerPresentationActive(true);

    LocalFootstepAmplifyTarget = TargetPlayerState;

    /*
     * ServerTime 기준 남은 시간을 계산하여
     * RPC 지연 때문에 Local 효과가 8초보다 길어지는 것을
     * 최소화한다.
     */
    double CurrentServerTime = 0.0;

    if (const UWorld* World = GetWorld())
    {
        if (const AGameStateBase* GameState = World->GetGameState<AGameStateBase>())
        {
            CurrentServerTime = GameState->GetServerWorldTimeSeconds();
        }
    }

    const float RemainingDuration = static_cast<float>(FMath::Max(0.0, EffectEndServerTime - CurrentServerTime));

    if (RemainingDuration <= 0.0f)
    {
        ClearLocalFootstepAmplifyPresentation();

        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().SetTimer(
            LocalFootstepAmplifyPresentationTimerHandle,
            this,
            &ThisClass::ClearLocalFootstepAmplifyPresentation,
            RemainingDuration,
            false
        );
    }

    UE_LOG(
        LogDMMatchPlayerController,
        Log,
        TEXT("Footstep Amplify local presentation started. Target=%s, Remaining=%.3f"),
        *GetNameSafe(TargetPlayerState),
        RemainingDuration
    );
}

void ADMMatchPlayerController::ClearLocalFootstepAmplifyPresentation()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(LocalFootstepAmplifyPresentationTimerHandle);
    }

    ADMMatchPlayerState* TargetPlayerState = LocalFootstepAmplifyTarget.Get();

    if (IsValid(TargetPlayerState))
    {
        if (ADMPlayerCharacter* TargetCharacter = Cast<ADMPlayerCharacter>(TargetPlayerState->GetPawn()))
        {
            if (UDMCharacterAudioComponent* AudioComponent = TargetCharacter->GetCharacterAudioComponent())
            {
                AudioComponent->SetFootstepAmplifyListenerPresentationActive(false);
            }
        }
    }

    LocalFootstepAmplifyTarget.Reset();
}

void ADMMatchPlayerController::ClientNotifyFootstepAmplifyWarning_Implementation(const double EffectEndServerTime)
{
    if (!IsLocalController())
    {
        return;
    }

    // 피해자에게 공격자 정보 없이 종료 시각만 전달한다.
    FootstepAmplifyWarningDelegate.Broadcast(EffectEndServerTime);

    UE_LOG(
        LogDMMatchPlayerController,
        Log,
        TEXT("Footstep Amplify warning received. EndServerTime=%.3f"),
        EffectEndServerTime
    );
}

void ADMMatchPlayerController::ClientBeginVisionDistortPresentation_Implementation(
    const double EffectStartServerTime, const float EntryDuration, const float MainDuration, const float ExitDuration
)
{
    if (!IsLocalController() || IsSpectatorInformationRestricted())
    {
        return;
    }

    ClearLocalVisionDistortPresentation();

    if (EntryDuration <= 0.0f || MainDuration <= 0.0f || ExitDuration <= 0.0f)
    {
        return;
    }

    if (!ensureMsgf(
            IsValid(VisionDistortPostProcessMaterial.Get()),
            TEXT("Vision Distort presentation failed: Post Process Material is not assigned.")
        ))
    {
        return;
    }

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return;
    }

    ADMPlayerCharacter* PlayerCharacter = Cast<ADMPlayerCharacter>(GetPawn());

    if (!IsValid(PlayerCharacter))
    {
        return;
    }

    UCameraComponent* CameraComponent = PlayerCharacter->FindComponentByClass<UCameraComponent>();

    if (!IsValid(CameraComponent))
    {
        return;
    }

    const AGameStateBase* GameState = World->GetGameState<AGameStateBase>();

    if (!IsValid(GameState))
    {
        return;
    }

    const double CurrentServerTime = GameState->GetServerWorldTimeSeconds();

    const float TotalDuration = EntryDuration + MainDuration + ExitDuration;

    const float ElapsedServerTime = static_cast<float>(FMath::Max(0.0, CurrentServerTime - EffectStartServerTime));

    const float RemainingDuration = FMath::Max(0.0f, TotalDuration - ElapsedServerTime);

    /*
     * 이미 효과가 끝난 시점에 늦게 RPC가 도착했다면
     * 뒤늦게 Presentation을 시작하지 않는다.
     */
    if (RemainingDuration <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    UMaterialInstanceDynamic* NewMID = UMaterialInstanceDynamic::Create(VisionDistortPostProcessMaterial.Get(), this);

    if (!IsValid(NewMID))
    {
        return;
    }

    LocalVisionDistortMID = NewMID;

    LocalVisionDistortCamera = CameraComponent;

    /*
     * Material의 Time 노드와 Client World Time을
     * 조합해서 Server effect 진행도를 보정한다.
     */
    LocalVisionDistortMID->SetScalarParameterValue(TEXT("DM_LocalStartTime"), World->GetTimeSeconds());

    LocalVisionDistortMID->SetScalarParameterValue(TEXT("DM_ElapsedOffset"), ElapsedServerTime);

    LocalVisionDistortMID->SetScalarParameterValue(TEXT("DM_EntryDuration"), EntryDuration);

    LocalVisionDistortMID->SetScalarParameterValue(TEXT("DM_MainDuration"), MainDuration);

    LocalVisionDistortMID->SetScalarParameterValue(TEXT("DM_ExitDuration"), ExitDuration);

    CameraComponent->PostProcessSettings.AddBlendable(LocalVisionDistortMID.Get(), 1.0f);

    bIsLocalVisionDistortPresentationActive = true;

    LocalVisionDistortEndServerTime = EffectStartServerTime + static_cast<double>(TotalDuration);

    VisionDistortPresentationChangedDelegate.Broadcast(true);

    World->GetTimerManager().SetTimer(
        LocalVisionDistortPresentationTimerHandle,
        this,
        &ThisClass::ClearLocalVisionDistortPresentation,
        RemainingDuration,
        false
    );

    UE_LOG(
        LogDMMatchPlayerController,
        Log,
        TEXT("Vision Distort presentation started. Entry=%.3f, Main=%.3f, Exit=%.3f, Elapsed=%.3f, Remaining=%.3f"),
        EntryDuration,
        MainDuration,
        ExitDuration,
        ElapsedServerTime,
        RemainingDuration
    );
}

void ADMMatchPlayerController::ClearLocalVisionDistortPresentation()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(LocalVisionDistortPresentationTimerHandle);
    }

    const bool bWasActive = bIsLocalVisionDistortPresentationActive;

    UCameraComponent* CameraComponent = LocalVisionDistortCamera.Get();

    if (IsValid(CameraComponent) && IsValid(LocalVisionDistortMID.Get()))
    {
        CameraComponent->PostProcessSettings.RemoveBlendable(LocalVisionDistortMID.Get());
    }

    LocalVisionDistortCamera.Reset();

    LocalVisionDistortMID = nullptr;

    bIsLocalVisionDistortPresentationActive = false;

    LocalVisionDistortEndServerTime = 0.0;

    if (bWasActive)
    {
        VisionDistortPresentationChangedDelegate.Broadcast(false);

        UE_LOG(LogDMMatchPlayerController, Log, TEXT("Vision Distort presentation ended."));
    }
}

void ADMMatchPlayerController::ClientClearVisionDistortPresentation_Implementation()
{
    ClearLocalVisionDistortPresentation();
}

void ADMMatchPlayerController::ClientCleanupSkillPresentationForLifeStateChange_Implementation()
{
    if (!IsLocalController())
    {
        return;
    }

    /*
     * Target 선택 중 죽었을 경우
     * IMC_SkillTargeting을 즉시 종료한다.
     */
    CancelSkillTargetingLocal();

    /*
     * VisionDistort를 맞은 상태로 죽더라도
     * Death/Spectator 화면까지 PostProcess가
     * 남지 않도록 한다.
     */
    ClearLocalVisionDistortPresentation();

    /*
     * 중요:
     *
     * 이 Player가 Caster인 FootstepAmplify는
     * PlayerState lifetime effect이므로
     * Death만으로 종료하지 않는다.
     *
     * Cooldown도 그대로 유지한다.
     */
}

void ADMMatchPlayerController::ClientCleanupAllSkillPresentationForMatchEnd_Implementation()
{
    if (!IsLocalController())
    {
        return;
    }

    CancelSkillTargetingLocal();

    ClearLocalFootstepAmplifyPresentation();

    ClearLocalVisionDistortPresentation();
}

#pragma endregion

#pragma region Chest Interaction

void ADMMatchPlayerController::ClientOpenChestInteraction_Implementation(
    ADMRelicChest* Chest, int32 BaseSlot, int32 RegisteredRelicCount
)
{
    if (!IsLocalController() || IsSpectatorInformationRestricted())
    {
        return;
    }

    if (!IsValid(Chest))
    {
        UE_LOG(LogDMMatchPlayerController, Warning, TEXT("ClientOpenChestInteraction rejected: Chest is invalid."));

        return;
    }

    OpenedChest = Chest;

    InspectedBaseSlot = BaseSlot;

    InspectedRegisteredRelicCount = RegisteredRelicCount;

    ADMPlayerCharacter* PlayerCharacter = Cast<ADMPlayerCharacter>(GetPawn());

    if (IsValid(PlayerCharacter))
    {
        PlayerCharacter->EnterChestInteractionInputMode();
    }

    ChestInteractionChangedDelegate.Broadcast(true, InspectedBaseSlot, InspectedRegisteredRelicCount);

    UE_LOG(
        LogDMMatchPlayerController,
        Log,
        TEXT("Chest interaction opened. Chest=%s, BaseSlot=%d, RegisteredRelics=%d"),
        *GetNameSafe(Chest),
        BaseSlot,
        RegisteredRelicCount
    );
}

void ADMMatchPlayerController::CloseChestInteractionLocal()
{
    const bool bHadChestInteractionState =
        OpenedChest.IsValid() || InspectedBaseSlot != INDEX_NONE || InspectedRegisteredRelicCount != 0;

    if (!bHadChestInteractionState)
    {
        return;
    }

    UE_LOG(
        LogDMMatchPlayerController,
        Log,
        TEXT("Chest interaction closed. Chest=%s, BaseSlot=%d"),
        *GetNameSafe(OpenedChest.Get()),
        InspectedBaseSlot
    );

    OpenedChest.Reset();

    InspectedBaseSlot = INDEX_NONE;

    InspectedRegisteredRelicCount = 0;

    ChestInteractionChangedDelegate.Broadcast(false, INDEX_NONE, 0);
}

#pragma endregion