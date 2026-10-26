
#include "Framework/Frontend/DMFrontendPlayerController.h"

#include "Core/DMMatchConstants.h"

#include "UI/DMMainMenuWidget.h"

#include "Core/DMGameInstance.h"
#include "Data/DMMatchRuleDataAsset.h"
#include "Online/DMOnlineSessionSubsystem.h"
#include "Voice/DMVoiceChatSubsystem.h"
#include "Blueprint/UserWidget.h"
#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"
#include "Kismet/KismetSystemLibrary.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMFrontend, Log, All);

#pragma region Lifecycle

void ADMFrontendPlayerController::BeginPlay()
{
    Super::BeginPlay();

    if (!IsLocalController())
    {
        return;
    }

    // 메인 메뉴에서는 음성을 송신하지 않는다.
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        if (UDMVoiceChatSubsystem* Voice = GameInstance->GetSubsystem<UDMVoiceChatSubsystem>())
        {
            Voice->LeaveVoiceWorld();
        }
    }

    if (!ensureMsgf(MainMenuWidgetClass, TEXT("MainMenuWidgetClass is not assigned.")))
    {
        return;
    }

    MainMenuWidget = CreateWidget<UDMMainMenuWidget>(this, MainMenuWidgetClass);

    if (!IsValid(MainMenuWidget.Get()))
    {
        UE_LOG(LogDMFrontend, Error, TEXT("Failed to create the main menu."));
        return;
    }

    MainMenuWidget->AddToViewport();

    FInputModeUIOnly InputMode;

    InputMode.SetWidgetToFocus(MainMenuWidget->TakeWidget());

    SetInputMode(InputMode);

    bShowMouseCursor = true;
    bEnableClickEvents = true;

    // 레벨 이동 콜백보다 늦게 생성된 메뉴에서도 음악 선택을 한 번 보장한다.
    if (UDMGameInstance* DMInstance = Cast<UDMGameInstance>(GetGameInstance()))
    {
        DMInstance->RefreshMusicForWorld(GetWorld());
    }

    // 메인 메뉴 UI와 입력이 준비된 시점에
    // GameInstance의 네트워크 복구 완료를 알린다.
    if (UDMGameInstance* DMGameInstance = Cast<UDMGameInstance>(GetGameInstance()))
    {
        DMGameInstance->NotifyFrontendReady(this);
    }

    // 이전 접속 실패나 퇴장 타임아웃으로 남은 Steam 세션이 있으면 정리한다.
    CleanupStaleLobbySession();

    UE_LOG(LogDMFrontend, Log, TEXT("Main menu created successfully."));
}

void ADMFrontendPlayerController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 이전 Controller로 완료 이벤트가 전달되지 않도록 한다.
    UnbindHostSessionCompletion();
    bHostSessionRequestPending = false;

    // 검색·참가·세션 정리 완료 이벤트를 해제한다.
    UnbindSteamJoinCompletion();
    SteamJoinOperation = ESteamJoinOperation::None;
    PendingJoinFailureMessage.Reset();

    if (IsValid(MainMenuWidget.Get()))
    {
        MainMenuWidget->RemoveFromParent();
    }

    MainMenuWidget = nullptr;

    Super::EndPlay(EndPlayReason);
}

#pragma endregion

#pragma region Menu Actions

bool ADMFrontendPlayerController::HostLobby()
{
    UWorld* World = GetWorld();

    if (!IsLocalController() || !IsValid(World))
    {
        return false;
    }

    // 이미 네트워크 접속 중인 월드에서는 새 호스트를 생성하지 않는다.
    if (World->GetNetMode() != NM_Standalone)
    {
        UE_LOG(LogDMFrontend, Warning, TEXT("Host rejected: frontend is not standalone."));

        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetHostCreationState(
                false, FText::FromString(TEXT("현재 상태에서는 방을 생성할 수 없습니다."))
            );
        }

        return false;
    }

    if (bHostSessionRequestPending)
    {
        UE_LOG(LogDMFrontend, Verbose, TEXT("Host rejected: create request is pending."));
        return false;
    }

    if (!ensureMsgf(!LobbyMap.IsNull(), TEXT("LobbyMap is not assigned.")))
    {
        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetHostCreationState(false, FText::FromString(TEXT("로비 맵이 설정되지 않았습니다.")));
        }

        return false;
    }

    if (!ensureMsgf(IsValid(LobbyMatchRuleData.Get()), TEXT("LobbyMatchRuleData is not assigned.")))
    {
        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetHostCreationState(
                false, FText::FromString(TEXT("로비 규칙 데이터가 설정되지 않았습니다."))
            );
        }

        return false;
    }

    const int32 MinPlayers = LobbyMatchRuleData->MinPlayerCount;

    const int32 MaxPlayers = LobbyMatchRuleData->MaxPlayerCount;

    if (!ensureMsgf(
            MinPlayers >= DMMatch::MinPlayers && MaxPlayers <= DMMatch::MaxPlayers && MinPlayers <= MaxPlayers,
            TEXT("Invalid lobby player limits. Min=%d, Max=%d"),
            MinPlayers,
            MaxPlayers
        ))
    {
        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetHostCreationState(false, FText::FromString(TEXT("로비 인원 설정이 올바르지 않습니다.")));
        }

        return false;
    }

    UGameInstance* GameInstance = GetGameInstance();

    UDMOnlineSessionSubsystem* Session =
        IsValid(GameInstance) ? GameInstance->GetSubsystem<UDMOnlineSessionSubsystem>() : nullptr;

    if (!IsValid(Session))
    {
        UE_LOG(LogDMFrontend, Error, TEXT("Online Session Subsystem is unavailable."));

        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetHostCreationState(
                false, FText::FromString(TEXT("온라인 세션 시스템을 사용할 수 없습니다."))
            );
        }

        return false;
    }

    if (Session->IsRecoveryBlocked())
    {
        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetHostCreationState(
                false, FText::FromString(TEXT("Steam 세션 복구가 완료되지 않았습니다. 게임과 Steam을 재시작하세요."))
            );
        }
        return false;
    }

    if (Session->IsBusy())
    {
        UE_LOG(LogDMFrontend, Verbose, TEXT("Host rejected: session operation is busy."));

        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetHostCreationState(false, FText::FromString(TEXT("다른 온라인 작업이 진행 중입니다.")));
        }

        return false;
    }

    // 이전 실패로 남은 세션이 있으면 먼저 정리하고 이번 요청은 중단한다.
    if (Session->HasLocalLobbySession())
    {
        CleanupStaleLobbySession();

        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetHostCreationState(
                false, FText::FromString(TEXT("이전 세션을 정리하는 중입니다. 잠시 후 다시 시도하세요."))
            );
        }

        return false;
    }

    // 비동기 요청을 시작하기 전에 결과 수신 준비를 완료한다.
    bHostSessionRequestPending = true;
    BoundHostSession = Session;

    HostSessionCreateHandle =
        Session->OnCreateCompleted.AddUObject(this, &ADMFrontendPlayerController::HandleHostSessionCreated);

    if (IsValid(MainMenuWidget.Get()))
    {
        MainMenuWidget->SetHostCreationState(true, FText::FromString(TEXT("Steam 로비를 생성하는 중입니다...")));
    }

    UE_LOG(LogDMFrontend, Log, TEXT("Requesting Steam lobby session. MaxPlayers=%d"), MaxPlayers);

    const bool bRequestStarted = Session->CreateLobbySession(MaxPlayers);

    if (!bRequestStarted && bHostSessionRequestPending)
    {
        // 즉시 실패하면서 완료 이벤트가 발생하지 않은 경우도 정리한다.
        bHostSessionRequestPending = false;
        UnbindHostSessionCompletion();

        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetHostCreationState(
                false, FText::FromString(TEXT("Steam 로비 생성 요청에 실패했습니다."))
            );
        }
    }

    // 실제 생성 성공은 HandleHostSessionCreated에서 판정한다.
    return bRequestStarted;
}

bool ADMFrontendPlayerController::FindSteamLobbies()
{
    UWorld* World = GetWorld();

    if (!IsLocalController() || !IsValid(World) || World->GetNetMode() != NM_Standalone)
    {
        return false;
    }

    if (SteamJoinOperation != ESteamJoinOperation::None || bHostSessionRequestPending)
    {
        return false;
    }

    UGameInstance* GameInstance = GetGameInstance();

    UDMOnlineSessionSubsystem* Session =
        IsValid(GameInstance) ? GameInstance->GetSubsystem<UDMOnlineSessionSubsystem>() : nullptr;

    if (!IsValid(Session))
    {
        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetSteamJoinBusy(false, FText::FromString(TEXT("Steam 세션 시스템을 찾을 수 없습니다.")));
        }
        return false;
    }

    if (Session->IsRecoveryBlocked())
    {
        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetSteamJoinBusy(
                false, FText::FromString(TEXT("Steam 세션 복구가 완료되지 않았습니다. 게임과 Steam을 재시작하세요."))
            );
        }
        return false;
    }

    if (Session->IsBusy())
    {
        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetSteamJoinBusy(false, FText::FromString(TEXT("다른 온라인 작업이 진행 중입니다.")));
        }
        return false;
    }

    SteamJoinOperation = ESteamJoinOperation::Find;
    BoundSteamJoinSession = Session;

    SteamJoinDelegateHandle =
        Session->OnFindCompleted.AddUObject(this, &ADMFrontendPlayerController::HandleSteamFindCompleted);

    if (IsValid(MainMenuWidget.Get()))
    {
        MainMenuWidget->SetSteamSessionOptions(TArray<FString>());

        MainMenuWidget->SetSteamJoinBusy(true, FText::FromString(TEXT("Steam 로비를 검색하고 있습니다...")));
    }

    UE_LOG(LogDMFrontend, Log, TEXT("Steam lobby search requested."));

    const bool bStarted = Session->FindLobbySessions();

    // 즉시 실패하면서 완료 이벤트가 호출되지 않은 경우를 처리한다.
    if (!bStarted && SteamJoinOperation == ESteamJoinOperation::Find)
    {
        UnbindSteamJoinCompletion();
        SteamJoinOperation = ESteamJoinOperation::None;

        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetSteamJoinBusy(false, FText::FromString(TEXT("Steam 로비 검색 요청에 실패했습니다.")));
        }
    }

    return bStarted;
}

bool ADMFrontendPlayerController::JoinSteamLobby(const int32 ResultIndex)
{
    UWorld* World = GetWorld();

    if (!IsLocalController() || !IsValid(World) || World->GetNetMode() != NM_Standalone)
    {
        return false;
    }

    if (SteamJoinOperation != ESteamJoinOperation::None || bHostSessionRequestPending)
    {
        return false;
    }

    UGameInstance* GameInstance = GetGameInstance();

    UDMOnlineSessionSubsystem* Session =
        IsValid(GameInstance) ? GameInstance->GetSubsystem<UDMOnlineSessionSubsystem>() : nullptr;

    if (!IsValid(Session) || Session->IsBusy())
    {
        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetSteamJoinBusy(false, FText::FromString(TEXT("온라인 세션을 사용할 수 없습니다.")));
        }
        return false;
    }

    if (Session->IsRecoveryBlocked())
    {
        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetSteamJoinBusy(
                false, FText::FromString(TEXT("Steam 세션 복구가 완료되지 않았습니다. 게임과 Steam을 재시작하세요."))
            );
        }
        return false;
    }

    // 이전 실패로 남은 세션이 있으면 먼저 정리하고 이번 요청은 중단한다.
    if (Session->HasLocalLobbySession())
    {
        CleanupStaleLobbySession();

        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetSteamJoinBusy(
                false,
                FText::FromString(TEXT("이전 세션을 정리하는 중입니다. 방 목록을 새로고침한 뒤 다시 시도하세요."))
            );
        }

        return false;
    }

    if (ResultIndex < 0 || ResultIndex >= Session->GetFoundSessionCount())
    {
        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetSteamJoinBusy(false, FText::FromString(TEXT("참가할 방을 선택해주세요.")));
        }
        return false;
    }

    if (Session->GetFoundSessionOpenSlots(ResultIndex) <= 0)
    {
        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetSteamJoinBusy(false, FText::FromString(TEXT("선택한 방의 정원이 가득 찼습니다.")));
        }
        return false;
    }

    SteamJoinOperation = ESteamJoinOperation::Join;
    BoundSteamJoinSession = Session;

    SteamJoinDelegateHandle =
        Session->OnJoinCompleted.AddUObject(this, &ADMFrontendPlayerController::HandleSteamJoinCompleted);

    if (IsValid(MainMenuWidget.Get()))
    {
        MainMenuWidget->SetSteamJoinBusy(true, FText::FromString(TEXT("Steam 로비 참가를 요청하는 중입니다...")));
    }

    UE_LOG(LogDMFrontend, Log, TEXT("Steam join requested. ResultIndex=%d"), ResultIndex);

    const bool bStarted = Session->JoinLobbySession(ResultIndex);

    if (!bStarted && SteamJoinOperation == ESteamJoinOperation::Join)
    {
        UnbindSteamJoinCompletion();

        StartSteamJoinFailureCleanup(TEXT("Steam 참가 요청을 시작하지 못했습니다."));
    }

    return bStarted;
}

bool ADMFrontendPlayerController::JoinLobbyByAddress(const FString& RawAddress)
{
    if (!IsLocalController())
    {
        return false;
    }

    const FString Address = RawAddress.TrimStartAndEnd();

    if (!IsValidDirectJoinAddress(Address))
    {
        UE_LOG(LogDMFrontend, Verbose, TEXT("Invalid direct join address."));
        return false;
    }

    UE_LOG(LogDMFrontend, Log, TEXT("Client connection requested."));

    ClientTravel(Address, TRAVEL_Absolute);

    // 실제 접속 성공 여부는 별도로 확인해야 한다.
    return true;
}

void ADMFrontendPlayerController::QuitGame()
{
    if (!IsLocalController())
    {
        return;
    }

    UKismetSystemLibrary::QuitGame(this, this, EQuitPreference::Quit, false);
}

void ADMFrontendPlayerController::RestoreMainMenuAfterNotice()
{
    if (!IsLocalController() || !IsValid(MainMenuWidget.Get()))
    {
        return;
    }

    if (!MainMenuWidget->IsInViewport())
    {
        MainMenuWidget->AddToViewport();
    }

    MainMenuWidget->ShowMainMenuPanelForRecovery();

    // BeginPlay와 같은 입력 설정을 다시 적용한다.
    FInputModeUIOnly InputMode;
    InputMode.SetWidgetToFocus(MainMenuWidget->TakeWidget());
    SetInputMode(InputMode);

    bShowMouseCursor = true;
    bEnableClickEvents = true;

    // 메인 메뉴에서 접속에 실패한 경우 JoinSession으로 생긴 세션이 남아 있으므로 정리한다.
    CleanupStaleLobbySession();
}

#pragma endregion

#pragma region Steam Hosting

void ADMFrontendPlayerController::HandleHostSessionCreated(const bool bWasSuccessful, const FString& Detail)
{
    if (!bHostSessionRequestPending)
    {
        return;
    }

    // 새 작업이 중복 요청되지 않도록 기존 구독을 먼저 해제한다.
    bHostSessionRequestPending = false;
    UnbindHostSessionCompletion();

    if (!bWasSuccessful)
    {
        UE_LOG(LogDMFrontend, Warning, TEXT("Steam lobby creation failed: %s"), *Detail);

        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetHostCreationState(false, FText::FromString(Detail));
        }

        return;
    }

    UWorld* World = GetWorld();

    if (!IsLocalController() || !IsValid(World) || World->GetNetMode() != NM_Standalone || LobbyMap.IsNull())
    {
        UE_LOG(LogDMFrontend, Error, TEXT("Steam session was created, but lobby travel prerequisites are invalid."));

        // 생성에 성공한 세션이 남지 않도록 정리를 요청한다.
        if (UGameInstance* GameInstance = GetGameInstance())
        {
            if (UDMOnlineSessionSubsystem* Session = GameInstance->GetSubsystem<UDMOnlineSessionSubsystem>())
            {
                if (!Session->IsBusy())
                {
                    Session->DestroyLobbySession();
                }
            }
        }

        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetHostCreationState(
                false, FText::FromString(TEXT("로비 이동 조건을 확인할 수 없습니다."))
            );
        }

        return;
    }

    UE_LOG(LogDMFrontend, Log, TEXT("Steam session created successfully. Opening lobby as Listen Server."));

    if (IsValid(MainMenuWidget.Get()))
    {
        MainMenuWidget->SetHostCreationState(
            true, FText::FromString(TEXT("Steam 로비 생성 완료. 로비로 이동합니다..."))
        );
    }

    // Steam 세션 생성이 완료된 이후에만 로비로 이동한다.
    UGameplayStatics::OpenLevelBySoftObjectPtr(this, LobbyMap, true, TEXT("listen"));
}

void ADMFrontendPlayerController::UnbindHostSessionCompletion()
{
    if (UDMOnlineSessionSubsystem* Session = BoundHostSession.Get())
    {
        if (HostSessionCreateHandle.IsValid())
        {
            Session->OnCreateCompleted.Remove(HostSessionCreateHandle);
        }
    }

    HostSessionCreateHandle.Reset();
    BoundHostSession.Reset();
}

#pragma endregion

#pragma region Steam Joining

void ADMFrontendPlayerController::HandleSteamFindCompleted(const bool bWasSuccessful, const FString& Detail)
{
    if (SteamJoinOperation != ESteamJoinOperation::Find)
    {
        return;
    }

    UDMOnlineSessionSubsystem* Session = BoundSteamJoinSession.Get();

    UnbindSteamJoinCompletion();
    SteamJoinOperation = ESteamJoinOperation::None;

    if (!bWasSuccessful || !IsValid(Session))
    {
        UE_LOG(LogDMFrontend, Warning, TEXT("Steam search failed: %s"), *Detail);

        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetSteamSessionOptions(TArray<FString>());

            MainMenuWidget->SetSteamJoinBusy(false, FText::FromString(Detail));
        }
        return;
    }

    TArray<FString> DisplayOptions;

    const int32 ResultCount = Session->GetFoundSessionCount();

    DisplayOptions.Reserve(ResultCount);

    for (int32 Index = 0; Index < ResultCount; ++Index)
    {
        FString HostName = Session->GetFoundSessionOwnerName(Index);

        if (HostName.IsEmpty())
        {
            HostName = TEXT("호스트 정보 없음");
        }

        const int32 OpenSlots = Session->GetFoundSessionOpenSlots(Index);

        // 표시 문자열은 원본 세션 데이터가 아닌 UI 표현이다.
        DisplayOptions.Add(FString::Printf(
            TEXT("%d. %s | 잔여 슬롯 %d%s"),
            Index + 1,
            *HostName,
            OpenSlots,
            OpenSlots > 0 ? TEXT("") : TEXT(" (정원 마감)")
        ));
    }

    if (IsValid(MainMenuWidget.Get()))
    {
        MainMenuWidget->SetSteamSessionOptions(DisplayOptions);

        const FText ResultMessage = ResultCount > 0
            ? FText::FromString(FString::Printf(TEXT("%d개의 로비를 찾았습니다."), ResultCount))
            : FText::FromString(TEXT("검색된 로비가 없습니다."));

        MainMenuWidget->SetSteamJoinBusy(false, ResultMessage);
    }

    UE_LOG(LogDMFrontend, Log, TEXT("Steam search completed. Results=%d"), ResultCount);
}

void ADMFrontendPlayerController::HandleSteamJoinCompleted(const bool bWasSuccessful, const FString& Detail)
{
    if (SteamJoinOperation != ESteamJoinOperation::Join)
    {
        return;
    }

    UDMOnlineSessionSubsystem* Session = BoundSteamJoinSession.Get();

    FString ConnectString;

    if (bWasSuccessful && IsValid(Session))
    {
        ConnectString = Session->GetJoinedConnectString();
    }

    UnbindSteamJoinCompletion();

    if (!bWasSuccessful || ConnectString.IsEmpty())
    {
        StartSteamJoinFailureCleanup(bWasSuccessful ? TEXT("Steam 연결 주소가 비어 있습니다.") : Detail);
        return;
    }

    // 세션 참가에 성공했으므로 로컬 Join 작업을 종료한다.
    SteamJoinOperation = ESteamJoinOperation::None;

    if (!IsLocalController() || !IsValid(GetWorld()))
    {
        StartSteamJoinFailureCleanup(TEXT("로비 이동에 필요한 컨트롤러가 유효하지 않습니다."));
        return;
    }

    UE_LOG(LogDMFrontend, Log, TEXT("Steam JoinSession completed. ClientTravel requested."));

    if (IsValid(MainMenuWidget.Get()))
    {
        MainMenuWidget->SetSteamJoinBusy(true, FText::FromString(TEXT("Steam 로비에 연결하는 중입니다...")));
    }

    // 접속 주소는 Online Subsystem이 해석한 문자열 그대로 사용한다.
    ClientTravel(ConnectString, TRAVEL_Absolute);
}

void ADMFrontendPlayerController::StartSteamJoinFailureCleanup(const FString& FailureMessage)
{
    PendingJoinFailureMessage = FailureMessage;

    UGameInstance* GameInstance = GetGameInstance();

    UDMOnlineSessionSubsystem* Session =
        IsValid(GameInstance) ? GameInstance->GetSubsystem<UDMOnlineSessionSubsystem>() : nullptr;

    if (!IsValid(Session) || Session->IsBusy())
    {
        SteamJoinOperation = ESteamJoinOperation::None;

        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetSteamJoinBusy(
                false, FText::FromString(TEXT("접속 실패 후 세션을 정리하지 못했습니다."))
            );
        }
        return;
    }

    SteamJoinOperation = ESteamJoinOperation::Cleanup;
    BoundSteamJoinSession = Session;

    SteamJoinDelegateHandle =
        Session->OnDestroyCompleted.AddUObject(this, &ADMFrontendPlayerController::HandleSteamJoinCleanupCompleted);

    if (IsValid(MainMenuWidget.Get()))
    {
        MainMenuWidget->SetSteamJoinBusy(true, FText::FromString(TEXT("참가 실패. 로컬 세션을 정리하고 있습니다...")));
    }

    const bool bStarted = Session->DestroyLobbySession();

    if (!bStarted && SteamJoinOperation == ESteamJoinOperation::Cleanup)
    {
        UnbindSteamJoinCompletion();
        SteamJoinOperation = ESteamJoinOperation::None;

        if (IsValid(MainMenuWidget.Get()))
        {
            MainMenuWidget->SetSteamJoinBusy(
                false, FText::FromString(TEXT("세션 정리에 실패했습니다. 게임 재시작이 필요할 수 있습니다."))
            );
        }
    }
}

void ADMFrontendPlayerController::HandleSteamJoinCleanupCompleted(const bool bWasSuccessful, const FString& Detail)
{
    if (SteamJoinOperation != ESteamJoinOperation::Cleanup)
    {
        return;
    }

    UnbindSteamJoinCompletion();
    SteamJoinOperation = ESteamJoinOperation::None;

    UE_LOG(
        LogDMFrontend, Log, TEXT("Steam join cleanup completed. Success=%d, Detail=%s"), bWasSuccessful ? 1 : 0, *Detail
    );

    if (IsValid(MainMenuWidget.Get()))
    {
        // Destroy 성공 시 기존 검색 결과도 무효화되므로 목록을 비운다.
        MainMenuWidget->SetSteamSessionOptions(TArray<FString>());

        const FString Message = bWasSuccessful
            ? PendingJoinFailureMessage + TEXT(" 방 목록을 새로고침한 뒤 다시 시도하세요.")
            : PendingJoinFailureMessage + TEXT(" 세션 정리에 실패했습니다. 게임을 재시작해야 할 수 있습니다.");

        MainMenuWidget->SetSteamJoinBusy(false, FText::FromString(Message));
    }

    PendingJoinFailureMessage.Reset();
}

void ADMFrontendPlayerController::UnbindSteamJoinCompletion()
{
    if (UDMOnlineSessionSubsystem* Session = BoundSteamJoinSession.Get())
    {
        if (SteamJoinDelegateHandle.IsValid())
        {
            switch (SteamJoinOperation)
            {
            case ESteamJoinOperation::Find:
                Session->OnFindCompleted.Remove(SteamJoinDelegateHandle);
                break;

            case ESteamJoinOperation::Join:
                Session->OnJoinCompleted.Remove(SteamJoinDelegateHandle);
                break;

            case ESteamJoinOperation::Cleanup:
                Session->OnDestroyCompleted.Remove(SteamJoinDelegateHandle);
                break;

            default:
                break;
            }
        }
    }

    SteamJoinDelegateHandle.Reset();
    BoundSteamJoinSession.Reset();
}

#pragma endregion

#pragma region Stale Session Cleanup

bool ADMFrontendPlayerController::CleanupStaleLobbySession()
{
    UWorld* World = GetWorld();

    // 메인 메뉴(Standalone)에는 유효한 방이 없으므로 남은 GameSession은 이전 실패의 잔여물이다.
    if (!IsLocalController() || !IsValid(World) || World->GetNetMode() != NM_Standalone)
    {
        return false;
    }

    // 방금 시작한 생성·검색·참가·정리 요청의 세션은 지우지 않는다.
    if (bHostSessionRequestPending || SteamJoinOperation != ESteamJoinOperation::None)
    {
        return false;
    }

    UGameInstance* GameInstance = GetGameInstance();

    UDMOnlineSessionSubsystem* Session =
        IsValid(GameInstance) ? GameInstance->GetSubsystem<UDMOnlineSessionSubsystem>() : nullptr;

    if (!IsValid(Session) || !Session->HasLocalLobbySession())
    {
        return false;
    }

    if (Session->IsRecoveryBlocked())
    {
        UE_LOG(LogDMFrontend, Error, TEXT("Stale Steam cleanup cannot be verified: recovery is blocked."));
        return false;
    }

    // 다른 작업(예: 퇴장 중인 Destroy)이 끝나지 않았으면 이번에는 기다린다.
    if (Session->IsBusy())
    {
        UE_LOG(LogDMFrontend, Log, TEXT("Stale Steam session cleanup deferred: session operation is busy."));

        return true;
    }

    UE_LOG(LogDMFrontend, Warning, TEXT("Stale Steam session found on main menu. Requesting destroy."));

    // 세션 해제가 끝나면 검색 결과 캐시도 비워지므로 화면의 방 목록도 함께 비운다.
    if (IsValid(MainMenuWidget.Get()))
    {
        MainMenuWidget->SetSteamSessionOptions(TArray<FString>());
    }

    // 완료 결과는 기다리지 않는다. 실패하면 다음 Host/Join 시도에서 다시 정리한다.
    Session->DestroyLobbySession();

    return true;
}

#pragma endregion

#pragma region Validation

bool ADMFrontendPlayerController::IsValidDirectJoinAddress(const FString& Address)
{
    // IPv4 또는 IPv4:Port만 허용한다.

    auto IsDigits = [](const FString& Value) -> bool
    {
        if (Value.IsEmpty())
        {
            return false;
        }

        for (const TCHAR Character : Value)
        {
            if (Character < TEXT('0') || Character > TEXT('9'))
            {
                return false;
            }
        }

        return true;
    };

    if (Address.IsEmpty())
    {
        return false;
    }

    FString Host = Address;
    FString Port;

    if (Address.Split(TEXT(":"), &Host, &Port))
    {
        if (Port.Len() > 5 || !IsDigits(Port))
        {
            return false;
        }

        const int32 PortNumber = FCString::Atoi(*Port);

        if (PortNumber < 1 || PortNumber > 65535)
        {
            return false;
        }
    }

    TArray<FString> Octets;

    Host.ParseIntoArray(Octets, TEXT("."), false);

    if (Octets.Num() != 4)
    {
        return false;
    }

    for (const FString& Octet : Octets)
    {
        if (Octet.Len() > 3 || !IsDigits(Octet))
        {
            return false;
        }

        const int32 Value = FCString::Atoi(*Octet);

        if (Value < 0 || Value > 255)
        {
            return false;
        }
    }

    return true;
}

#pragma endregion
