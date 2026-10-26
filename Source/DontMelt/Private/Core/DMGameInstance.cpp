#include "Core/DMGameInstance.h"

#include "Core/DMMatchConstants.h"
#include "Core/DMGameplayTags.h"

#include "Core/DMLocalSettingsSubsystem.h"

#include "UI/DMConnectionLostWidget.h"

#include "Framework/Match/DMMatchGameState.h"
#include "Framework/Match/DMMatchPlayerState.h"
#include "Framework/Frontend/DMFrontendPlayerController.h"
#include "Framework/Lobby/DMLobbyGameState.h"
#include "Framework/Match/DMMatchGameMode.h"
#include "GameMapsSettings.h"
#include "Misc/PackageName.h"
#include "Online/DMOnlineSessionSubsystem.h"

#include "GameFramework/PlayerController.h"

#include "Kismet/GameplayStatics.h"
#include "Sound/SoundClass.h"
#include "Sound/SoundBase.h"
#include "Components/AudioComponent.h"
#include "Sound/SoundMix.h"

#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "Engine/GameViewportClient.h"
#include "Engine/PendingNetGame.h"
#include "HAL/PlatformTime.h"
#include "UObject/Class.h"
#include "Widgets/SWidget.h"
#include "UObject/UObjectGlobals.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMGameInstance, Log, All);

#pragma region Last Match Result Storage

void UDMGameInstance::RefreshLocalMatchIdentity(UWorld* SourceWorld)
{
    if (!IsValid(SourceWorld) || SourceWorld->GetGameInstance() != this || GetWorld() != SourceWorld)
    {
        return;
    }

    // 새 월드에서는 현재 경기의 식별 정보만 초기화한다.
    // 보관 중인 지난 경기 결과와 진영 정보는 유지한다.
    if (LocalMatchIdentityWorld.Get() != SourceWorld)
    {
        LocalMatchIdentityWorld = SourceWorld;
        LocalMatchBaseSlot = INDEX_NONE;
    }

    for (FConstPlayerControllerIterator It = SourceWorld->GetPlayerControllerIterator(); It; ++It)
    {
        APlayerController* Controller = It->Get();

        if (!IsValid(Controller) || !Controller->IsLocalController())
        {
            continue;
        }

        const ADMMatchPlayerState* MatchPlayerState = Controller->GetPlayerState<ADMMatchPlayerState>();

        if (!IsValid(MatchPlayerState))
        {
            continue;
        }

        const int32 BaseSlot = MatchPlayerState->GetBaseSlot();

        if (!DMMatch::IsValidBaseSlot(BaseSlot))
        {
            continue;
        }

        LocalMatchBaseSlot = BaseSlot;
        break;
    }

    // 결과보다 자신의 진영 정보가 늦게 도착한 경우에만 보완한다.
    // 다른 경기의 진영 정보로 지난 경기 기록을 변경하지 않는다.
    if (HasLastMatchResult() && LastMatchResultWorld.Get() == SourceWorld && LastMatchLocalBaseSlot == INDEX_NONE
        && DMMatch::IsValidBaseSlot(LocalMatchBaseSlot))
    {
        LastMatchLocalBaseSlot = LocalMatchBaseSlot;
    }
}

bool UDMGameInstance::CacheFinalMatchResult(const ADMMatchGameState* SourceGameState)
{
    if (!IsValid(SourceGameState))
    {
        return false;
    }

    UWorld* SourceWorld = SourceGameState->GetWorld();

    if (!IsValid(SourceWorld) || SourceWorld->GetGameInstance() != this || GetWorld() != SourceWorld
        || SourceWorld->GetGameState<ADMMatchGameState>() != SourceGameState)
    {
        return false;
    }

    const FDMMatchResult NewResult = SourceGameState->GetMatchResult();

    // 새 경기의 초기 상태로 지난 결과를 지우지 않는다.
    if (NewResult.EndReason == EDMMatchEndReason::None)
    {
        return false;
    }

    RefreshLocalMatchIdentity(SourceWorld);

    // 같은 경기의 결과는 한 번만 복사한다.
    // 진영 정보의 늦은 도착은 위 함수에서 별도로 보완한다.
    if (HasLastMatchResult() && LastMatchResult.ResultId == NewResult.ResultId)
    {
        return true;
    }

    LastMatchResult = NewResult;
    LastMatchResultWorld = SourceWorld;

    LastMatchLocalBaseSlot = LocalMatchIdentityWorld.Get() == SourceWorld ? LocalMatchBaseSlot : INDEX_NONE;

    UE_LOG(
        LogDMGameInstance,
        Log,
        TEXT("Stored last match result. Reason=%s, Rows=%d, LocalBaseSlot=%d"),
        *UEnum::GetValueAsString(LastMatchResult.EndReason),
        LastMatchResult.PlayerResults.Num(),
        LastMatchLocalBaseSlot
    );

    return true;
}

#pragma endregion

#pragma region Main Menu Navigation

void UDMGameInstance::SetOpenMainMenuPanelOnNextEntry(const bool bOpenMainMenuPanel)
{
    bOpenMainMenuPanelOnNextEntry = bOpenMainMenuPanel;
}

bool UDMGameInstance::ConsumeOpenMainMenuPanelOnNextEntry()
{
    const bool bShouldOpenMainMenuPanel = bOpenMainMenuPanelOnNextEntry;

    bOpenMainMenuPanelOnNextEntry = false;

    return bShouldOpenMainMenuPanel;
}

void UDMGameInstance::MarkIntentionalDisconnect(UWorld* SourceWorld)
{
    if (!IsValid(SourceWorld) || SourceWorld != GetWorld() || SourceWorld->GetGameInstance() != this)
    {
        return;
    }

    IntentionalDisconnectWorld = SourceWorld;
}

bool UDMGameInstance::CanBeginOnlineRoomExit()
{
    if (bOnlineRoomExitInProgress || bHandlingNetworkFailure)
    {
        return false;
    }

    UDMOnlineSessionSubsystem* Session = GetSubsystem<UDMOnlineSessionSubsystem>();

    return !IsValid(Session) || !Session->IsBusy();
}

bool UDMGameInstance::RequestLeaveOnlineRoom(
    UWorld* SourceWorld, const TSoftObjectPtr<UWorld>& MainMenuMap, const bool bOpenMainMenuPanel
)
{
    if (!CanBeginOnlineRoomExit() || !IsValid(SourceWorld) || SourceWorld != GetWorld() || MainMenuMap.IsNull())
    {
        return false;
    }

    const FString MapPackage = MainMenuMap.ToSoftObjectPath().GetLongPackageName();

    if (!ensureMsgf(
            FPackageName::IsValidLongPackageName(MapPackage) && FPackageName::DoesPackageExist(MapPackage),
            TEXT("Leave online room failed: main menu map is invalid.")
        ))
    {
        return false;
    }

    return BeginOnlineRoomExit(SourceWorld, MapPackage, false, bOpenMainMenuPanel);
}

#pragma endregion

#pragma region Online Room Exit

bool UDMGameInstance::BeginOnlineRoomExit(
    UWorld* SourceWorld, const FString& DestinationPackage, const bool bRecovery, const bool bOpenMainMenuPanel
)
{
    if (!IsValid(SourceWorld) || SourceWorld != GetWorld() || bOnlineRoomExitInProgress)
    {
        return false;
    }

    UDMOnlineSessionSubsystem* Session = GetSubsystem<UDMOnlineSessionSubsystem>();

    // 정상 퇴장은 진행 중인 세션 작업이 끝난 뒤 요청한다.
    if (!bRecovery && IsValid(Session) && Session->IsBusy())
    {
        return false;
    }

    bOnlineRoomExitInProgress = true;
    bOnlineRoomExitRecovery = bRecovery;
    bOnlineRoomExitFromClient = SourceWorld->GetNetMode() == NM_Client;
    bOnlineRoomExitOpenMainMenu = bOpenMainMenuPanel;

    OnlineRoomExitDestination = DestinationPackage;
    OnlineRoomExitSourceWorld = SourceWorld;

    // 자발적 퇴장으로 발생한 연결 해제를 오류 팝업으로 취급하지 않는다.
    if (!bRecovery)
    {
        MarkIntentionalDisconnect(SourceWorld);
    }

    if (!IsValid(Session) || Session->IsBusy())
    {
        // 복구 상황에서는 Steam 응답 불가로 메뉴 복귀까지 막지 않는다.
        FinishOnlineRoomExit(false, TEXT("Steam 세션 해제 요청을 시작할 수 없습니다."));
        return true;
    }

    OnlineRoomExitSession = Session;

    // 즉시 완료 이벤트가 올 수 있으므로 구독부터 등록한다.
    OnlineRoomDestroyHandle =
        Session->OnDestroyCompleted.AddUObject(this, &UDMGameInstance::HandleOnlineRoomDestroyCompleted);

    // 네트워크 오류 발생 시 무한정 기다리지 않는다.
    SourceWorld->GetTimerManager().SetTimer(
        OnlineRoomExitTimeoutHandle, this, &UDMGameInstance::HandleOnlineRoomExitTimeout, 8.0f, false
    );

    UE_LOG(LogDMGameInstance, Log, TEXT("Leaving online room: requesting Steam session destroy."));

    const bool bRequestStarted = Session->DestroyLobbySession();

    // 즉시 실패 시 완료 이벤트가 이미 호출됐을 수 있다.
    if (!bRequestStarted && bOnlineRoomExitInProgress)
    {
        FinishOnlineRoomExit(false, TEXT("Steam 세션 해제 요청이 실패했습니다."));
    }

    return true;
}

void UDMGameInstance::HandleOnlineRoomDestroyCompleted(const bool bWasSuccessful, const FString& Detail)
{
    if (!bOnlineRoomExitInProgress)
    {
        return;
    }

    FinishOnlineRoomExit(bWasSuccessful, Detail);
}

void UDMGameInstance::HandleOnlineRoomExitTimeout()
{
    if (!bOnlineRoomExitInProgress)
    {
        return;
    }

    FinishOnlineRoomExit(false, TEXT("Steam 세션 해제 완료 응답이 시간 내에 도착하지 않았습니다."));
}

void UDMGameInstance::UnbindOnlineRoomDestroy()
{
    if (UDMOnlineSessionSubsystem* Session = OnlineRoomExitSession.Get())
    {
        if (OnlineRoomDestroyHandle.IsValid())
        {
            Session->OnDestroyCompleted.Remove(OnlineRoomDestroyHandle);
        }
    }

    OnlineRoomDestroyHandle.Reset();
    OnlineRoomExitSession.Reset();
}

void UDMGameInstance::FinishOnlineRoomExit(const bool bSessionDestroyed, const FString& Detail)
{
    if (!bOnlineRoomExitInProgress)
    {
        return;
    }

    UWorld* SourceWorld = OnlineRoomExitSourceWorld.Get();

    const bool bRecovery = bOnlineRoomExitRecovery;
    const bool bFromClient = bOnlineRoomExitFromClient;
    const bool bOpenMenu = bOnlineRoomExitOpenMainMenu;
    const FString Destination = OnlineRoomExitDestination;

    // 다른 전환 요청보다 먼저 등록된 완료 이벤트와 타이머를 제거한다.
    UnbindOnlineRoomDestroy();

    if (IsValid(SourceWorld))
    {
        SourceWorld->GetTimerManager().ClearTimer(OnlineRoomExitTimeoutHandle);
    }

    OnlineRoomExitTimeoutHandle.Invalidate();

    bOnlineRoomExitInProgress = false;
    bOnlineRoomExitRecovery = false;
    bOnlineRoomExitFromClient = false;
    bOnlineRoomExitOpenMainMenu = false;

    OnlineRoomExitDestination.Reset();
    OnlineRoomExitSourceWorld.Reset();

    if (!bSessionDestroyed)
    {
        UE_LOG(LogDMGameInstance, Error, TEXT("Steam session cleanup was not confirmed: %s"), *Detail);
    }
    else
    {
        UE_LOG(LogDMGameInstance, Log, TEXT("Steam session destroyed. Requesting travel to main menu."));
    }

    // 다른 월드로 이미 이동했다면 중복 이동하지 않는다.
    if (!IsValid(SourceWorld) || SourceWorld != GetWorld())
    {
        return;
    }

    SetOpenMainMenuPanelOnNextEntry(bOpenMenu);

    if (bFromClient)
    {
        if (bRecovery)
        {
            // 기존 GameInstance의 연결 해제·기본 맵 복귀 경로를 유지한다.
            ReturnToMainMenu();
            return;
        }

        APlayerController* LocalController = GetFirstLocalPlayerController();

        if (IsValid(LocalController))
        {
            LocalController->ClientTravel(Destination, TRAVEL_Absolute);
        }
        else
        {
            ReturnToMainMenu();
        }

        return;
    }

    // Listen Server는 기존 서버를 종료하고 로컬 메뉴를 연다.
    UGameplayStatics::OpenLevel(this, FName(*Destination), true, FString());
}

#pragma endregion

#pragma region Lifecycle

void UDMGameInstance::Init()
{
    Super::Init();

    if (GEngine == nullptr)
    {
        UE_LOG(LogDMGameInstance, Error, TEXT("Network failure handler registration failed: GEngine is unavailable."));

        return;
    }

    if (NetworkFailureDelegateHandle.IsValid())
    {
        UE_LOG(
            LogDMGameInstance,
            Warning,
            TEXT("Network failure handler registration ignored: the Delegate is already registered.")
        );

        return;
    }

    NetworkFailureDelegateHandle = GEngine->OnNetworkFailure().AddUObject(this, &UDMGameInstance::HandleNetworkFailure);

    if (!NetworkFailureDelegateHandle.IsValid())
    {
        UE_LOG(
            LogDMGameInstance,
            Error,
            TEXT("Network failure handler registration failed: an invalid Delegate Handle was returned.")
        );

        return;
    }

    UE_LOG(LogDMGameInstance, Log, TEXT("Network failure handler registered. GameInstance=%s"), *GetNameSafe(this));

    PostLoadMapDelegateHandle =
        FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(this, &UDMGameInstance::HandlePostLoadMap);

    if (!PostLoadMapDelegateHandle.IsValid())
    {
        UE_LOG(LogDMGameInstance, Error, TEXT("PostLoadMapWithWorld delegate registration failed."));

        if (IsValid(GEngine) && NetworkFailureDelegateHandle.IsValid())
        {
            GEngine->OnNetworkFailure().Remove(NetworkFailureDelegateHandle);

            NetworkFailureDelegateHandle.Reset();
        }

        return;
    }

    UE_LOG(LogDMGameInstance, Log, TEXT("PostLoadMap handler registered. GameInstance=%s"), *GetNameSafe(this));

    if (!TravelFailureDelegateHandle.IsValid())
    {
        TravelFailureDelegateHandle =
            GEngine->OnTravelFailure().AddUObject(this, &UDMGameInstance::HandleTravelFailure);
    }

    PendingNetDriverTrackerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UDMGameInstance::TickTrackPendingNetDriver), 0.0f
    );
}

void UDMGameInstance::Shutdown()
{
    // 게임 실행 전역 음악은 GameInstance 종료 시 한 번 정리한다.
    StopAndReleaseMusic();

    // 게임 종료 시 미완료 온라인 퇴장 콜백을 해제한다.
    UnbindOnlineRoomDestroy();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(OnlineRoomExitTimeoutHandle);
    }

    OnlineRoomExitTimeoutHandle.Invalidate();

    bOnlineRoomExitInProgress = false;
    OnlineRoomExitSourceWorld.Reset();
    OnlineRoomExitDestination.Reset();

    if (PendingNetDriverTrackerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(PendingNetDriverTrackerHandle);
        PendingNetDriverTrackerHandle.Reset();
    }

    if (NetworkFailureTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(NetworkFailureTickerHandle);

        NetworkFailureTickerHandle.Reset();
    }

    bNetworkFailureNoticeActive = false;

    ClearNetworkFailurePresentation();

    NetworkFailureNoticeText = FText::GetEmpty();
    NetworkFailureNoticeEndTime = 0.0;

    NetworkFailureSourceWorld.Reset();
    IntentionalDisconnectWorld.Reset();

    if (GEngine != nullptr && NetworkFailureDelegateHandle.IsValid())
    {
        GEngine->OnNetworkFailure().Remove(NetworkFailureDelegateHandle);

        NetworkFailureDelegateHandle.Reset();

        UE_LOG(
            LogDMGameInstance, Log, TEXT("Network failure handler unregistered. GameInstance=%s"), *GetNameSafe(this)
        );
    }

    if (GEngine != nullptr && TravelFailureDelegateHandle.IsValid())
    {
        GEngine->OnTravelFailure().Remove(TravelFailureDelegateHandle);

        TravelFailureDelegateHandle.Reset();
    }

    bRecoveryTravelRequested = false;

    if (PostLoadMapDelegateHandle.IsValid())
    {
        FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapDelegateHandle);

        PostLoadMapDelegateHandle.Reset();

        UE_LOG(LogDMGameInstance, Log, TEXT("PostLoadMap handler unregistered. GameInstance=%s"), *GetNameSafe(this));
    }

    bHandlingNetworkFailure = false;

    Super::Shutdown();
}

#pragma endregion

#pragma region Network Failure

void UDMGameInstance::HandleNetworkFailure(
    UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString
)
{
    if (IsValid(World))
    {
        // 이미 접속한 경기·로비의 연결 종료
        if (World->GetGameInstance() != this || GetWorld() != World || World->GetNetMode() != NM_Client)
        {
            return;
        }

        // 비컨·데모 등 다른 NetDriver의 오류는 방 연결 종료로 보지 않는다.
        if (NetDriver != nullptr && NetDriver != World->GetNetDriver())
        {
            return;
        }

        // 서버가 지시한 레벨 이동(ClientTravel) 중에 기존 연결이 닫히는 것은 정상 흐름이다.
        // 새 연결이 실패하면 World가 없는 PendingNetGame 경로에서 따로 처리된다.
        if (const FWorldContext* Context = GetWorldContext())
        {
            if (!Context->TravelURL.IsEmpty() || Context->PendingNetGame != nullptr)
            {
                UE_LOG(
                    LogDMGameInstance,
                    Log,
                    TEXT("Network failure ignored during client travel. Type=%s, Message=%s"),
                    GetNetworkFailureName(FailureType),
                    *ErrorString
                );

                return;
            }
        }

        HandleConnectedNetworkFailure(World, NetDriver, FailureType, ErrorString);
        return;
    }

    // World가 없으면 PendingNetGame 단계다.
    // 다른 PIE 인스턴스의 실패를 처리하지 않도록 소유 GameInstance를 확인한다.
    if (IsOwnPendingNetDriver(NetDriver))
    {
        HandlePendingConnectionFailure(FailureType, ErrorString);
    }
}

void UDMGameInstance::HandleConnectedNetworkFailure(
    UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString
)
{
    if (IntentionalDisconnectWorld.Get() == World || bHandlingNetworkFailure)
    {
        return;
    }

    const bool bWasInMultiplayerRoom =
        IsValid(World->GetGameState<ADMMatchGameState>()) || IsValid(World->GetGameState<ADMLobbyGameState>());

    const FText Message = bWasInMultiplayerRoom
        ? NSLOCTEXT("DMNetwork", "HostLostReturnToMenu", "호스트의 연결이 끊겼습니다.\n잠시 후 메인 메뉴로 돌아갑니다.")
        : NSLOCTEXT(
              "DMNetwork", "ConnectionFailedReturnToMenu", "서버에 연결할 수 없습니다.\n잠시 후 메인 메뉴로 돌아갑니다."
          );

    UE_LOG(
        LogDMGameInstance,
        Warning,
        TEXT("Network failure. Type=%s, Message=%s, Driver=%s"),
        GetNetworkFailureName(FailureType),
        *ErrorString,
        *GetNameSafe(NetDriver)
    );

    StartLocalFailureRecovery(World, Message);
}

bool UDMGameInstance::TickTrackPendingNetDriver(float DeltaTime)
{
    const FWorldContext* Context = GetWorldContext();

    if (Context != nullptr && Context->PendingNetGame != nullptr && Context->PendingNetGame->NetDriver != nullptr)
    {
        TrackedPendingNetDriverKey = Context->PendingNetGame->NetDriver;
    }
    else
    {
        TrackedPendingNetDriverKey = nullptr;
    }

    return true;
}

bool UDMGameInstance::IsOwnPendingNetDriver(const UNetDriver* NetDriver) const
{
    if (NetDriver == nullptr)
    {
        return false;
    }

    // 1) 아직 PendingNetGame이 남아 있으면 소유 인스턴스로 확인한다.
    if (GEngine != nullptr)
    {
        if (const FWorldContext* PendingContext = GEngine->GetWorldContextFromPendingNetGameNetDriver(NetDriver))
        {
            return PendingContext->OwningGameInstance == this;
        }
    }

    // 2) 엔진이 먼저 정리했다면, 이 인스턴스가 기억한 드라이버인지 비교한다.
    return TrackedPendingNetDriverKey == NetDriver;
}

void UDMGameInstance::HandlePendingConnectionFailure(ENetworkFailure::Type FailureType, const FString& ErrorString)
{
    TrackedPendingNetDriverKey = nullptr;

    // 중복 실패 알림이 들어와도 한 번만 처리한다.
    if (bHandlingNetworkFailure)
    {
        return;
    }

    UWorld* CurrentWorld = GetWorld();

    if (!IsValid(CurrentWorld))
    {
        return;
    }

    // 메인 메뉴에서의 접속 실패인지, 방 이동 후 재접속 실패인지 구분한다.
    const bool bWasTravelingFromRoom = IsValid(CurrentWorld->GetGameState<ADMMatchGameState>())
        || IsValid(CurrentWorld->GetGameState<ADMLobbyGameState>());

    const FText Message = bWasTravelingFromRoom
        ? NSLOCTEXT(
              "DMNetwork",
              "ReconnectFailedReturnToMenu",
              "화면 이동 중 서버와의 연결이 끊겼습니다.\n잠시 후 메인 메뉴로 돌아갑니다."
          )
        : NSLOCTEXT(
              "DMNetwork", "ConnectionFailedReturnToMenu", "서버에 연결할 수 없습니다.\n잠시 후 메인 메뉴로 돌아갑니다."
          );

    UE_LOG(
        LogDMGameInstance,
        Warning,
        TEXT("Network failure while preparing connection. Type=%s, Message=%s"),
        GetNetworkFailureName(FailureType),
        *ErrorString
    );

    StartLocalFailureRecovery(CurrentWorld, Message);
}

void UDMGameInstance::StartLocalFailureRecovery(UWorld* SourceWorld, const FText& Message)
{
    if (!IsValid(SourceWorld) || SourceWorld->GetGameInstance() != this || SourceWorld != GetWorld()
        || bHandlingNetworkFailure)
    {
        return;
    }

    bHandlingNetworkFailure = true;
    bRecoveryTravelRequested = false;

    NetworkFailureSourceWorld = SourceWorld;
    NetworkFailureNoticeText = Message;

    // 실패한 자발적 퇴장도 이제 오류 복구 대상으로 처리한다.
    IntentionalDisconnectWorld.Reset();

    if (ADMMatchGameMode* MatchGameMode = SourceWorld->GetAuthGameMode<ADMMatchGameMode>())
    {
        MatchGameMode->StopResultTravelForRecovery();
    }

    SetOpenMainMenuPanelOnNextEntry(true);

    if (!ensureMsgf(
            ConnectionLostWidgetClass && !ConnectionLostWidgetClass->HasAnyClassFlags(CLASS_Abstract),
            TEXT("Failure notice widget class is missing. Requesting default map recovery immediately.")
        ))
    {
        RequestRecoveryTravel();
        return;
    }

    bNetworkFailureNoticeActive = true;

    NetworkFailureNoticeEndTime = FPlatformTime::Seconds() + 5.0;

    RefreshNetworkFailurePresentation();

    NetworkFailureTickerHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UDMGameInstance::TickNetworkFailureNotice), 0.1f
    );

    if (!NetworkFailureTickerHandle.IsValid())
    {
        RequestRecoveryTravel();
    }
}

void UDMGameInstance::HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString)
{
    if (!IsValid(World) || World->GetGameInstance() != this || GetWorld() != World)
    {
        return;
    }

    const FString Diagnostic = FString::Printf(TEXT("종류=%d, 내용=%s"), static_cast<int32>(FailureType), *ErrorString);

    StartTravelFailureRecovery(World, Diagnostic);
}

void UDMGameInstance::StartTravelFailureRecovery(UWorld* SourceWorld, const FString& Diagnostic)
{
    if (!IsValid(SourceWorld) || SourceWorld->GetGameInstance() != this || SourceWorld != GetWorld())
    {
        return;
    }

    UE_LOG(LogDMGameInstance, Error, TEXT("Travel failed. %s"), *Diagnostic);

    // 기본 맵으로의 복구까지 실패했다면 다시 이동하지 않는다.
    if (bRecoveryTravelRequested)
    {
        ShowRecoveryFailedNotice();
        return;
    }

    // 이미 안내 중이면 중복 오류로 대기 시간을 다시 시작하지 않는다.
    if (bHandlingNetworkFailure)
    {
        return;
    }

    StartLocalFailureRecovery(
        SourceWorld,
        NSLOCTEXT("DMNetwork", "TravelFailedReturnToMenu", "화면 이동에 실패했습니다.\n잠시 후 메인 메뉴로 돌아갑니다.")
    );
}

void UDMGameInstance::RequestRecoveryTravel()
{
    if (bRecoveryTravelRequested)
    {
        return;
    }

    // 이동 요청 전에 기록하여 동기적으로 발생하는 실패도 구분한다.
    bRecoveryTravelRequested = true;

    UWorld* World = GetWorld();

    const FString DefaultMapPackage = FPackageName::ObjectPathToPackageName(UGameMapsSettings::GetGameDefaultMap());

    if (!IsValid(World) || !FPackageName::IsValidLongPackageName(DefaultMapPackage)
        || !FPackageName::DoesPackageExist(DefaultMapPackage))
    {
        UE_LOG(LogDMGameInstance, Error, TEXT("Default map recovery unavailable. Path=%s"), *DefaultMapPackage);

        ShowRecoveryFailedNotice();
        return;
    }

    // 기존 오류 안내가 종료되면 Steam 세션 정리부터 요청한다.
    bNetworkFailureNoticeActive = false;
    ClearNetworkFailurePresentation();

    if (!BeginOnlineRoomExit(World, DefaultMapPackage, true, true))
    {
        ShowRecoveryFailedNotice();
    }
}

void UDMGameInstance::ShowRecoveryFailedNotice()
{
    if (NetworkFailureTickerHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(NetworkFailureTickerHandle);

        NetworkFailureTickerHandle.Reset();
    }

    bHandlingNetworkFailure = true;
    bRecoveryTravelRequested = true;
    bNetworkFailureNoticeActive = true;

    NetworkFailureNoticeText = NSLOCTEXT(
        "DMNetwork", "RecoveryFailed", "메인 메뉴로 돌아갈 수 없습니다.\n게임을 종료한 뒤 다시 실행해 주세요."
    );

    NetworkFailureNoticeEndTime = 0.0;

    if (IsValid(ConnectionLostWidget.Get()))
    {
        ConnectionLostWidget->SetMessage(NetworkFailureNoticeText);
    }

    RefreshNetworkFailurePresentation(true);

    UE_LOG(LogDMGameInstance, Error, TEXT("Default map recovery failed. Stopping further automatic travel."));
}

void UDMGameInstance::FinishLocalFailureRecovery()
{
    bNetworkFailureNoticeActive = false;

    ClearNetworkFailurePresentation();

    bHandlingNetworkFailure = false;
    bRecoveryTravelRequested = false;

    NetworkFailureNoticeText = FText::GetEmpty();
    NetworkFailureNoticeEndTime = 0.0;

    NetworkFailureSourceWorld.Reset();
    IntentionalDisconnectWorld.Reset();

    // 이미 Frontend에 있다면 NativeConstruct가 다시 호출되지 않는다.
    // 플래그를 직접 소비하고, 현재 메뉴에 화면과 포커스 복원을 요청한다.
    ConsumeOpenMainMenuPanelOnNextEntry();

    if (ADMFrontendPlayerController* FrontendController =
            Cast<ADMFrontendPlayerController>(GetFirstLocalPlayerController()))
    {
        FrontendController->RestoreMainMenuAfterNotice();
    }
}

void UDMGameInstance::RefreshNetworkFailurePresentation(const bool bReattach)
{
    if (!bNetworkFailureNoticeActive || !ConnectionLostWidgetClass
        || ConnectionLostWidgetClass->HasAnyClassFlags(CLASS_Abstract))
    {
        return;
    }

    UGameViewportClient* Viewport = GetGameViewportClient();

    if (!IsValid(Viewport))
    {
        return;
    }

    if (!IsValid(ConnectionLostWidget.Get()))
    {
        ConnectionLostWidget = CreateWidget<UDMConnectionLostWidget>(this, ConnectionLostWidgetClass);

        if (!IsValid(ConnectionLostWidget.Get()))
        {
            return;
        }

        ConnectionLostWidget->SetMessage(NetworkFailureNoticeText);

        ConnectionLostSlateWidget = ConnectionLostWidget->TakeWidget();
    }

    if (!ConnectionLostSlateWidget.IsValid())
    {
        return;
    }

    const bool bViewportChanged = ConnectionLostViewport.Get() != Viewport;

    if (bViewportChanged)
    {
        if (UGameViewportClient* PreviousViewport = ConnectionLostViewport.Get())
        {
            PreviousViewport->RemoveViewportWidgetContent(ConnectionLostSlateWidget.ToSharedRef());

            PreviousViewport->SetIgnoreInput(bViewportPreviouslyIgnoredInput);
        }

        bViewportPreviouslyIgnoredInput = Viewport->IgnoreInput();

        ConnectionLostViewport = Viewport;

        Viewport->AddViewportWidgetContent(ConnectionLostSlateWidget.ToSharedRef(), 10000);
    }
    else if (bReattach)
    {
        // 레벨 이동 중 화면 계층이 정리된 경우 다시 부착한다.
        Viewport->RemoveViewportWidgetContent(ConnectionLostSlateWidget.ToSharedRef());

        Viewport->AddViewportWidgetContent(ConnectionLostSlateWidget.ToSharedRef(), 10000);
    }

    Viewport->SetIgnoreInput(true);

    APlayerController* Controller = GetFirstLocalPlayerController();

    if (IsValid(Controller) && Controller->IsLocalController()
        && (bViewportChanged || bReattach || ConnectionLostInputController.Get() != Controller))
    {
        FInputModeUIOnly InputMode;

        InputMode.SetWidgetToFocus(ConnectionLostSlateWidget);

        Controller->SetInputMode(InputMode);
        Controller->bShowMouseCursor = true;

        ConnectionLostInputController = Controller;
    }
}

void UDMGameInstance::ClearNetworkFailurePresentation()
{
    if (UGameViewportClient* Viewport = ConnectionLostViewport.Get())
    {
        if (ConnectionLostSlateWidget.IsValid())
        {
            Viewport->RemoveViewportWidgetContent(ConnectionLostSlateWidget.ToSharedRef());
        }

        Viewport->SetIgnoreInput(bViewportPreviouslyIgnoredInput);
    }

    ConnectionLostInputController.Reset();
    ConnectionLostViewport.Reset();

    ConnectionLostSlateWidget.Reset();
    ConnectionLostWidget = nullptr;

    bViewportPreviouslyIgnoredInput = false;
}

bool UDMGameInstance::TickNetworkFailureNotice(float DeltaTime)
{
    if (!bNetworkFailureNoticeActive)
    {
        NetworkFailureTickerHandle.Reset();
        return false;
    }

    RefreshNetworkFailurePresentation();

    if (FPlatformTime::Seconds() < NetworkFailureNoticeEndTime)
    {
        return true;
    }

    // 이 콜백은 false 반환으로 종료한다.
    NetworkFailureTickerHandle.Reset();

    UWorld* CurrentWorld = GetWorld();

    APlayerController* Controller = GetFirstLocalPlayerController();

    const bool bAlreadyAtMainMenu = IsValid(CurrentWorld) && CurrentWorld->GetNetMode() == NM_Standalone
        && IsValid(Cast<ADMFrontendPlayerController>(Controller));

    if (bAlreadyAtMainMenu)
    {
        FinishLocalFailureRecovery();
        return false;
    }

    RequestRecoveryTravel();

    return false;
}

void UDMGameInstance::HandlePostLoadMap(UWorld* LoadedWorld)
{
    if (!IsValid(LoadedWorld) || LoadedWorld->GetGameInstance() != this)
    {
        return;
    }

    ApplyLocalAudioSettings(LoadedWorld);

    // 새 레벨에서도 로컬 음악을 재생한다. 매치의 실제 Phase는 GameState 변경 통지에서 보정한다.
    RefreshMusicForWorld(LoadedWorld);

    if (IntentionalDisconnectWorld.Get() != LoadedWorld)
    {
        IntentionalDisconnectWorld.Reset();
    }

    if (!bHandlingNetworkFailure)
    {
        return;
    }

    if (bRecoveryTravelRequested)
    {
        // PostLoadMap 시점에는 새로운 PlayerController가
        // 아직 준비되지 않았을 수 있다.
        // 따라서 맵 로드만으로 복구 실패를 확정하지 않는다.
        if (LoadedWorld->GetNetMode() == NM_Standalone)
        {
            if (ADMFrontendPlayerController* FrontendController =
                    Cast<ADMFrontendPlayerController>(GetFirstLocalPlayerController()))
            {
                NotifyFrontendReady(FrontendController);
            }
        }

        // 최종 준비 완료는 FrontendPlayerController에서 알린다.
        return;
    }

    if (bNetworkFailureNoticeActive)
    {
        // 엔진이 먼저 이동했다면 남은 안내 시간을 새 화면에서 유지한다.
        RefreshNetworkFailurePresentation(true);
    }
}

void UDMGameInstance::NotifyFrontendReady(const ADMFrontendPlayerController* FrontendController)
{
    if (!bHandlingNetworkFailure || !bRecoveryTravelRequested || !IsValid(FrontendController))
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World) || FrontendController->GetWorld() != World || World->GetGameInstance() != this
        || World->GetNetMode() != NM_Standalone)
    {
        return;
    }

    UE_LOG(LogDMGameInstance, Log, TEXT("Frontend is ready. Network recovery completed."));

    FinishLocalFailureRecovery();
}

#pragma endregion

#pragma region Local Music

void UDMGameInstance::ClearScheduledMusicTransition()
{
    // 이전 월드에 남아 있는 예약을 취소한다. 로비/매치 이동에서도 중복 실행을 막는다.
    if (UWorld* ScheduledWorld = MusicScheduledWorld.Get())
    {
        ScheduledWorld->GetTimerManager().ClearTimer(MusicTransitionTimerHandle);
    }

    MusicTransitionTimerHandle.Invalidate();
    MusicScheduledWorld.Reset();
}

void UDMGameInstance::HandleScheduledMusicTransition()
{
    UWorld* ScheduledWorld = MusicScheduledWorld.Get();

    // 이미 다른 레벨로 이동한 경우 이전 레벨의 전환 요청은 무시한다.
    if (!IsValid(ScheduledWorld) || ScheduledWorld->GetGameInstance() != this || GetWorld() != ScheduledWorld)
    {
        ClearScheduledMusicTransition();
        return;
    }

    MusicTransitionTimerHandle.Invalidate();
    MusicScheduledWorld.Reset();
    RefreshMusicForWorld(ScheduledWorld);
}

void UDMGameInstance::RefreshMusicForWorld(UWorld* TargetWorld)
{
    if (!IsValid(TargetWorld) || TargetWorld->GetGameInstance() != this
        || TargetWorld->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    // 복제 상태가 갱신되거나 레벨이 바뀌면 예약된 로컬 전환 시각을 다시 계산한다.
    ClearScheduledMusicTransition();

    const ADMMatchGameState* MatchState = TargetWorld->GetGameState<ADMMatchGameState>();
    const EDMMatchPhase Phase = IsValid(MatchState) ? MatchState->GetCurrentPhase() : EDMMatchPhase::Waiting;
    const bool bMatchPhase = Phase == EDMMatchPhase::Playing || Phase == EDMMatchPhase::OvertimeIntro
        || Phase == EDMMatchPhase::Overtime;

    // 메인/일반 경기 음악이 기본 우선순위다.
    USoundBase* SelectedSound = bMatchPhase ? MatchBackgroundSound.Get() : MainThemeSound.Get();
    TObjectPtr<UAudioComponent>* SelectedComponent =
        bMatchPhase ? &MatchBackgroundAudioComponent : &MainThemeAudioComponent;

    if (bMatchPhase && IsValid(MatchState))
    {
        const double ServerNow = MatchState->GetServerWorldTimeSeconds();
        double NextMusicTransitionServerTime = 0.0;

        // 한 번만 실행할 가장 가까운 시각을 선택한다. Tick으로 남은 시간을 검사하지 않는다.
        const auto ConsiderTransition = [&NextMusicTransitionServerTime, ServerNow](const double BoundaryTime)
        {
            if (BoundaryTime > ServerNow && (NextMusicTransitionServerTime <= 0.0
                || BoundaryTime < NextMusicTransitionServerTime))
            {
                NextMusicTransitionServerTime = BoundaryTime;
            }
        };

        // 연장전은 이미 정규 경기 제한시간 이후이므로 긴급 트랙을 유지한다.
        bool bUrgentMusic = Phase == EDMMatchPhase::OvertimeIntro || Phase == EDMMatchPhase::Overtime;

        if (Phase == EDMMatchPhase::Playing)
        {
            const FDMMatchPhaseState CurrentPhaseState = MatchState->GetPhaseState();

            if (CurrentPhaseState.PhaseEndServerTime > 0.0)
            {
                const double UrgentStartServerTime = CurrentPhaseState.PhaseEndServerTime
                    - static_cast<double>(FMath::Max(0.0f, UrgentMusicLeadSeconds));

                bUrgentMusic = ServerNow >= UrgentStartServerTime;
                ConsiderTransition(UrgentStartServerTime);
            }
        }

        // 긴급 음악이 미설정이면 기본 경기 음악으로 되돌아간다.
        if (bUrgentMusic && IsValid(UrgentMatchSound.Get()))
        {
            SelectedSound = UrgentMatchSound.Get();
            SelectedComponent = &UrgentMatchAudioComponent;
        }

        if (Phase == EDMMatchPhase::Playing)
        {
            const FDMRandomEventRuntimeState EventState = MatchState->GetRandomEventPublicState();

            if (EventState.IsScheduledOrActive())
            {
                // 예고 구간은 기존 BGM을 유지하고 실제 이벤트 시작/종료 시점에만 변경한다.
                ConsiderTransition(EventState.StartServerTime);
                ConsiderTransition(EventState.EndServerTime);

                const bool bEventActive = ServerNow >= EventState.StartServerTime
                    && ServerNow < EventState.EndServerTime;

                if (bEventActive)
                {
                    // 새 이벤트 타입이나 네트워크 상태를 만들지 않고 기존 GameplayTag로 음악을 선택한다.
                    if (EventState.EventTag == DMGameplayTags::RandomEvent_Blackout
                        && IsValid(BlackoutEventSound.Get()))
                    {
                        SelectedSound = BlackoutEventSound.Get();
                        SelectedComponent = &BlackoutEventAudioComponent;
                    }
                    else if (EventState.EventTag == DMGameplayTags::RandomEvent_Panic
                        && IsValid(PanicEventSound.Get()))
                    {
                        SelectedSound = PanicEventSound.Get();
                        SelectedComponent = &PanicEventAudioComponent;
                    }
                    else if (EventState.EventTag == DMGameplayTags::RandomEvent_InputReverse
                        && IsValid(InputReverseEventSound.Get()))
                    {
                        SelectedSound = InputReverseEventSound.Get();
                        SelectedComponent = &InputReverseEventAudioComponent;
                    }
                }
            }
        }

        if (NextMusicTransitionServerTime > 0.0)
        {
            // 서버-클라이언트 시간 보정 오차로 동일 경계에서 연속 호출되지 않도록 여유를 둔다.
            const float Delay = static_cast<float>(FMath::Max(0.1, NextMusicTransitionServerTime - ServerNow + 0.05));
            MusicScheduledWorld = TargetWorld;
            TargetWorld->GetTimerManager().SetTimer(
                MusicTransitionTimerHandle,
                this,
                &UDMGameInstance::HandleScheduledMusicTransition,
                Delay,
                false
            );
        }
    }

    // 패키징에서 GameInstance BP의 음악 에셋 미연결을 침묵 상태로 넘기지 않는다.
    if (!IsValid(SelectedSound))
    {
        UE_LOG(LogDMGameInstance, Warning,
            TEXT("Music sound is unassigned. Phase=%s Match=%s Main=%s Normal=%s Urgent=%s"),
            *UEnum::GetValueAsString(Phase), bMatchPhase ? TEXT("true") : TEXT("false"),
            *GetNameSafe(MainThemeSound.Get()), *GetNameSafe(MatchBackgroundSound.Get()),
            *GetNameSafe(UrgentMatchSound.Get()));
    }

    // 선택된 음악이 없으면 이전 트랙만 종료한다.
    if (!IsValid(SelectedSound))
    {
        if (IsValid(ActiveMusicAudioComponent.Get()))
        {
            ActiveMusicAudioComponent->FadeOut(MusicFadeDuration, 0.0f);
            ActiveMusicAudioComponent = nullptr;
        }
        return;
    }

    // 트랙별로 처음 요청할 때만 지속형 2D AudioComponent를 생성한다.
    if (!IsValid(SelectedComponent->Get()))
    {
        *SelectedComponent = UGameplayStatics::CreateSound2D(
            TargetWorld,
            SelectedSound,
            1.0f,
            1.0f,
            0.0f,
            nullptr,
            true,
            false
        );
    }

    if (!IsValid(SelectedComponent->Get()))
    {
        UE_LOG(LogDMGameInstance, Warning, TEXT("Music audio component creation failed. Sound=%s"),
            *GetNameSafe(SelectedSound));
        return;
    }

    // 현재 트랙이 재생 중이면 상태 복제나 이벤트 알림에서 재시작하지 않는다.
    if (ActiveMusicAudioComponent == *SelectedComponent && (*SelectedComponent)->IsPlaying())
    {
        return;
    }

    if (IsValid(ActiveMusicAudioComponent.Get()) && ActiveMusicAudioComponent != *SelectedComponent)
    {
        ActiveMusicAudioComponent->FadeOut(MusicFadeDuration, 0.0f);
    }

    ActiveMusicAudioComponent = *SelectedComponent;
    ActiveMusicAudioComponent->FadeIn(MusicFadeDuration, 1.0f);

    UE_LOG(LogDMGameInstance, Log, TEXT("Music selected. Phase=%s, Sound=%s"),
        *UEnum::GetValueAsString(Phase), *GetNameSafe(SelectedSound));
}

void UDMGameInstance::StopAndReleaseMusic()
{
    ClearScheduledMusicTransition();

    // 생성된 트랙만 정리한다. 아직 재생하지 않은 트랙에 AudioComponent를 할당하지 않는다.
    const auto ReleaseMusicComponent = [](TObjectPtr<UAudioComponent>& Component)
    {
        if (IsValid(Component.Get()))
        {
            Component->Stop();
            Component->DestroyComponent();
        }
        Component = nullptr;
    };

    ReleaseMusicComponent(MainThemeAudioComponent);
    ReleaseMusicComponent(MatchBackgroundAudioComponent);
    ReleaseMusicComponent(UrgentMatchAudioComponent);
    ReleaseMusicComponent(BlackoutEventAudioComponent);
    ReleaseMusicComponent(PanicEventAudioComponent);
    ReleaseMusicComponent(InputReverseEventAudioComponent);

    ActiveMusicAudioComponent = nullptr;
}

#pragma endregion

#pragma region Local Audio Settings

void UDMGameInstance::ApplyLocalAudioSettings(UWorld* TargetWorld)
{
    if (!IsValid(TargetWorld) || TargetWorld->GetGameInstance() != this)
    {
        return;
    }

    UDMLocalSettingsSubsystem* LocalSettings = GetSubsystem<UDMLocalSettingsSubsystem>();

    if (!IsValid(LocalSettings))
    {
        return;
    }

    if (!IsValid(SettingsSoundMix.Get()) || !IsValid(SettingsMasterClass.Get()) || !IsValid(SettingsSFXClass.Get())
        || !IsValid(SettingsMusicClass.Get()))
    {
        UE_LOG(LogDMGameInstance, Warning, TEXT("Local audio SoundMix/SoundClass is not configured."));
        return;
    }

    // 새 월드 진입 시 SoundMix를 활성화한다.
    if (AudioAppliedWorld.Get() != TargetWorld)
    {
        UGameplayStatics::PushSoundMixModifier(TargetWorld, SettingsSoundMix.Get());

        AudioAppliedWorld = TargetWorld;
    }

    UGameplayStatics::SetSoundMixClassOverride(
        TargetWorld,
        SettingsSoundMix.Get(),
        SettingsMasterClass.Get(),
        LocalSettings->GetMasterVolume(),
        1.0f,
        0.0f,
        true
    );

    UGameplayStatics::SetSoundMixClassOverride(
        TargetWorld, SettingsSoundMix.Get(), SettingsSFXClass.Get(), LocalSettings->GetSFXVolume(), 1.0f, 0.0f, false
    );

    UGameplayStatics::SetSoundMixClassOverride(
        TargetWorld,
        SettingsSoundMix.Get(),
        SettingsMusicClass.Get(),
        LocalSettings->GetMusicVolume(),
        1.0f,
        0.0f,
        false
    );

    UE_LOG(
        LogDMGameInstance,
        Log,
        TEXT("Local audio settings applied. World=%s, Master=%.2f, SFX=%.2f, Music=%.2f"),
        *GetNameSafe(TargetWorld),
        LocalSettings->GetMasterVolume(),
        LocalSettings->GetSFXVolume(),
        LocalSettings->GetMusicVolume()
    );
}

#pragma endregion

#pragma region Internal Helpers

const TCHAR* UDMGameInstance::GetNetworkFailureName(ENetworkFailure::Type FailureType)
{
    switch (FailureType)
    {
    case ENetworkFailure::NetDriverAlreadyExists:
        return TEXT("NetDriverAlreadyExists");

    case ENetworkFailure::NetDriverCreateFailure:
        return TEXT("NetDriverCreateFailure");

    case ENetworkFailure::NetDriverListenFailure:
        return TEXT("NetDriverListenFailure");

    case ENetworkFailure::ConnectionLost:
        return TEXT("ConnectionLost");

    case ENetworkFailure::ConnectionTimeout:
        return TEXT("ConnectionTimeout");

    case ENetworkFailure::FailureReceived:
        return TEXT("FailureReceived");

    case ENetworkFailure::OutdatedClient:
        return TEXT("OutdatedClient");

    case ENetworkFailure::OutdatedServer:
        return TEXT("OutdatedServer");

    case ENetworkFailure::PendingConnectionFailure:
        return TEXT("PendingConnectionFailure");

    case ENetworkFailure::NetGuidMismatch:
        return TEXT("NetGuidMismatch");

    case ENetworkFailure::NetChecksumMismatch:
        return TEXT("NetChecksumMismatch");

    default:
        return TEXT("Unknown");
    }
}

#pragma endregion
