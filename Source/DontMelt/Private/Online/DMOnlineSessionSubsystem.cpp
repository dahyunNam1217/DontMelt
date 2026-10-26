
#include "Online/DMOnlineSessionSubsystem.h"

#include "Core/DMMatchConstants.h"

#include "Engine/GameInstance.h"
#include "TimerManager.h"

#include "OnlineSubsystem.h"
#include "OnlineSessionSettings.h"
#include "Online/OnlineSessionNames.h"
#include "Interfaces/OnlineSessionInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMOnlineSession, Log, All);

namespace
{
    // 기존 엔진 GameSession과 동일한 로컬 세션 이름을 사용한다.
    const FName DMSessionName(TEXT("GameSession"));

    // Steam 공유 테스트 App ID의 다른 게임과 구별한다.
    const FName DMProjectKey(TEXT("DM_PROJECT"));
    const FString DMProjectValue(TEXT("DontMelt"));

    // 네트워크 호환성이 변경되면 배포 빌드에서 함께 올린다.
    // 16-3 Pawn Handoff RPC 계약이 변경되어 구형 패키지와 함께 접속할 수 없다.
    constexpr int32 DMNetworkBuildId = 2;

    const FName DMNetworkBuildKey(TEXT("DM_NETWORK_BUILD"));

    // Steam 검색에 노출하는 로비 진행 상태다.
    const FName DMLobbyStatusKey(TEXT("DM_LOBBY_STATUS"));

    const FString DMLobbyOpenValue(TEXT("Lobby"));
    const FString DMLobbyStartingValue(TEXT("Starting"));

    // 세션 작업 하나를 기다리는 최대 시간(초)이다.
    constexpr float DMSessionOperationTimeoutSeconds = 30.0f;

    // 원래 작업이 만료된 뒤 늦은 완료 콜백을 기다리는 추가 제한시간이다.
    constexpr float DMSessionRecoveryTimeoutSeconds = 20.0f;

    // Steam 사용자 식별 타입과 값이 모두 일치하도록 키를 구성한다.
    FString MakeMatchPlayerKey(const FUniqueNetIdRepl& PlayerId)
    {
        if (!PlayerId.IsValid())
        {
            return FString();
        }

        return PlayerId.GetType().ToString() + TEXT(":") + PlayerId.ToString();
    }

    // 검색된 세션이 현재 게임에서 참가 가능한 로비인지 검사한다.
    bool IsCompatibleOpenLobby(const FOnlineSessionSearchResult& Result)
    {
        if (!Result.IsValid())
        {
            return false;
        }

        const FOnlineSessionSettings& Settings = Result.Session.SessionSettings;

        FString ProjectValue;

        // 다른 게임의 Steam 로비는 제외한다.
        if (!Settings.Get(DMProjectKey, ProjectValue) || ProjectValue != DMProjectValue)
        {
            return false;
        }

        FString LobbyStatus;

        // 경기가 시작 중이거나 광고 상태가 불명확하면 제외한다.
        if (!Settings.Get(DMLobbyStatusKey, LobbyStatus) || LobbyStatus != DMLobbyOpenValue)
        {
            UE_LOG(
                LogDMOnlineSession, Verbose, TEXT("DontMelt lobby rejected: invalid status. Status=%s"), *LobbyStatus
            );

            return false;
        }

        int32 NetworkBuildId = INDEX_NONE;

        // 게임 자체의 네트워크 버전만 비교한다.
        if (!Settings.Get(DMNetworkBuildKey, NetworkBuildId) || NetworkBuildId != DMNetworkBuildId)
        {
            UE_LOG(
                LogDMOnlineSession,
                Verbose,
                TEXT("DontMelt lobby rejected: network version. Found=%d, Expected=%d"),
                NetworkBuildId,
                DMNetworkBuildId
            );

            return false;
        }

        // 이미 인원이 가득 찬 로비는 검색 목록에서 제외한다.
        if (Result.Session.NumOpenPublicConnections <= 0)
        {
            UE_LOG(LogDMOnlineSession, Log, TEXT("DontMelt lobby rejected: no open public slots."));

            return false;
        }

        // 엔진 BuildUniqueId의 호환성은 Steam OnlineSubsystem에 맡긴다.
        // bUsesPresence와 bUseLobbiesIfAvailable은 검색 필터로 재검사하지 않는다.
        return true;
    }

    // Steam 세션 인터페이스만 반환한다.
    IOnlineSessionPtr GetSteamSessionInterface()
    {
        IOnlineSubsystem* Subsystem = IOnlineSubsystem::Get(FName(TEXT("STEAM")));

        if (Subsystem == nullptr)
        {
            return IOnlineSessionPtr();
        }

        return Subsystem->GetSessionInterface();
    }
}

#pragma region Lifecycle

void UDMOnlineSessionSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    UE_LOG(LogDMOnlineSession, Log, TEXT("Online Session Subsystem initialized."));
}

void UDMOnlineSessionSubsystem::Deinitialize()
{
    ClearOperationTimeout();

    ClearActiveDelegate();
    ClearCancelFindDelegate();

    CurrentOperation = EOperation::None;
    bWaitingForTimedOutCompletion = false;
    bRecoveryDestroyActive = false;
    RecoveryOrigin = EOperation::None;
    RecoveryFailureDetail.Reset();
    bRecoveryBlocked = false;
    PendingUpdateSettings.Reset();

    ClearApprovedMatchRoster();
    ResetCachedSessionData();

    UE_LOG(LogDMOnlineSession, Log, TEXT("Online Session Subsystem deinitialized."));

    Super::Deinitialize();
}

#pragma endregion

#pragma region Session Requests

bool UDMOnlineSessionSubsystem::CreateLobbySession(const int32 MaxPlayers)
{
    if (RejectBlockedOperation(EOperation::Create))
    {
        return false;
    }

    if (IsBusy())
    {
        UE_LOG(LogDMOnlineSession, Verbose, TEXT("Create rejected: another operation is active."));
        return false;
    }

    if (!ensureMsgf(DMMatch::IsValidPlayerCount(MaxPlayers), TEXT("Invalid player count: %d"), MaxPlayers))
    {
        return false;
    }

    IOnlineSessionPtr Sessions = GetSteamSessionInterface();

    if (!Sessions.IsValid())
    {
        UE_LOG(LogDMOnlineSession, Error, TEXT("Steam session interface is unavailable."));
        OnCreateCompleted.Broadcast(false, TEXT("Steam 세션 인터페이스를 찾을 수 없습니다."));
        return false;
    }

    if (Sessions->GetNamedSession(DMSessionName) != nullptr)
    {
        UE_LOG(LogDMOnlineSession, Warning, TEXT("A local session already exists."));
        OnCreateCompleted.Broadcast(false, TEXT("기존 세션을 먼저 종료해야 합니다."));
        return false;
    }

    FOnlineSessionSettings Settings;

    Settings.bIsLANMatch = false;
    Settings.bIsDedicated = false;
    Settings.bShouldAdvertise = true;

    // Listen Server와 Steam Lobby를 사용한다.
    Settings.bUsesPresence = true;
    Settings.bUseLobbiesIfAvailable = true;

    // 로비에서는 참가를 허용한다. 경기 시작 시 변경한다.
    Settings.bAllowJoinInProgress = true;
    Settings.bAllowJoinViaPresence = true;

    Settings.NumPublicConnections = MaxPlayers;
    Settings.NumPrivateConnections = 0;

    // 게임 자체의 네트워크 버전은 별도 Steam 세션 키로 광고한다.
    // 엔진 BuildUniqueId는 OnlineSubsystem이 관리하도록 둔다.
    Settings.Set(DMNetworkBuildKey, DMNetworkBuildId, EOnlineDataAdvertisementType::ViaOnlineService);

    // 공유 App ID를 사용하는 다른 프로젝트를 구별한다.
    Settings.Set(DMProjectKey, DMProjectValue, EOnlineDataAdvertisementType::ViaOnlineService);

    // 새로 생성하는 세션은 로비 상태로 광고한다.
    Settings.Set(DMLobbyStatusKey, DMLobbyOpenValue, EOnlineDataAdvertisementType::ViaOnlineService);

    BeginOperation(EOperation::Create);

    // 완료 콜백이 오지 않아도 작업 상태가 영구히 남지 않도록 한다.
    StartOperationTimeout();

    ActiveDelegateHandle = Sessions->AddOnCreateSessionCompleteDelegate_Handle(
        FOnCreateSessionCompleteDelegate::CreateUObject(this, &UDMOnlineSessionSubsystem::HandleCreateComplete)
    );

    // 반환값은 요청 시작 여부이며 실제 성공은 콜백에서 확인한다.
    const bool bStarted = Sessions->CreateSession(0, DMSessionName, Settings);

    if (!bStarted && CurrentOperation == EOperation::Create)
    {
        BeginRecoveryDestroy(TEXT("Steam 세션 생성 요청을 시작하지 못했습니다."));
    }

    return bStarted;
}

bool UDMOnlineSessionSubsystem::FindLobbySessions()
{
    if (RejectBlockedOperation(EOperation::Find))
    {
        return false;
    }

    if (IsBusy())
    {
        UE_LOG(LogDMOnlineSession, Verbose, TEXT("Find rejected: another operation is active."));
        return false;
    }

    IOnlineSessionPtr Sessions = GetSteamSessionInterface();

    if (!Sessions.IsValid())
    {
        OnFindCompleted.Broadcast(false, TEXT("Steam 세션 인터페이스를 찾을 수 없습니다."));
        return false;
    }

    // 이전 검색 결과를 무효화한다.
    SessionSearch.Reset();
    VisibleResultIndices.Reset();

    SessionSearch = MakeShared<FOnlineSessionSearch>();
    SessionSearch->bIsLanQuery = false;
    SessionSearch->MaxSearchResults = 100;

    SessionSearch->QuerySettings.Set(SEARCH_LOBBIES, true, EOnlineComparisonOp::Equals);

    // 공유 App ID(480)에서는 다른 게임 로비가 결과 상한을 채울 수 있으므로
    // Steam 서버 쪽에서 프로젝트와 로비 상태로 먼저 거른다.
    // 결과는 HandleFindComplete의 IsCompatibleOpenLobby에서 다시 검사한다.
    SessionSearch->QuerySettings.Set(DMProjectKey, DMProjectValue, EOnlineComparisonOp::Equals);
    SessionSearch->QuerySettings.Set(DMLobbyStatusKey, DMLobbyOpenValue, EOnlineComparisonOp::Equals);

    BeginOperation(EOperation::Find);

    StartOperationTimeout();

    ActiveDelegateHandle = Sessions->AddOnFindSessionsCompleteDelegate_Handle(
        FOnFindSessionsCompleteDelegate::CreateUObject(this, &UDMOnlineSessionSubsystem::HandleFindComplete)
    );

    const bool bStarted = Sessions->FindSessions(0, SessionSearch.ToSharedRef());

    if (!bStarted && CurrentOperation == EOperation::Find)
    {
        CompleteOperation(false, TEXT("Steam 세션 검색을 시작하지 못했습니다."));
    }

    return bStarted;
}

bool UDMOnlineSessionSubsystem::JoinLobbySession(const int32 ResultIndex)
{
    if (RejectBlockedOperation(EOperation::Join))
    {
        return false;
    }

    if (IsBusy())
    {
        UE_LOG(LogDMOnlineSession, Verbose, TEXT("Join rejected: another operation is active."));
        return false;
    }

    if (!VisibleResultIndices.IsValidIndex(ResultIndex) || !SessionSearch.IsValid())
    {
        UE_LOG(LogDMOnlineSession, Warning, TEXT("Invalid session result index: %d"), ResultIndex);
        return false;
    }

    IOnlineSessionPtr Sessions = GetSteamSessionInterface();

    if (!Sessions.IsValid())
    {
        OnJoinCompleted.Broadcast(false, TEXT("Steam 세션 인터페이스를 찾을 수 없습니다."));
        return false;
    }

    if (Sessions->GetNamedSession(DMSessionName) != nullptr)
    {
        OnJoinCompleted.Broadcast(false, TEXT("기존 세션을 먼저 종료해야 합니다."));
        return false;
    }

    const int32 SourceIndex = VisibleResultIndices[ResultIndex];

    if (!SessionSearch->SearchResults.IsValidIndex(SourceIndex))
    {
        return false;
    }

    // Steam 검색 결과를 복사하여 참가에 필요한 세션 설정을 보정한다.
    FOnlineSessionSearchResult SearchResult = SessionSearch->SearchResults[SourceIndex];

    // 검색된 로비의 프로젝트·상태·버전·정원을 검사한다.
    if (!IsCompatibleOpenLobby(SearchResult))
    {
        UE_LOG(LogDMOnlineSession, Verbose, TEXT("Join rejected: incompatible or unavailable lobby."));

        OnJoinCompleted.Broadcast(false, TEXT("참가할 수 없는 로비이거나 게임 빌드가 다릅니다."));

        return false;
    }

    // Steam 로비 참가 시 Presence와 Lobby 플래그가 일치해야 한다.
    // 검색 결과에서 누락된 Lobby 플래그를 보정한다.
    SearchResult.Session.SessionSettings.bUsesPresence = true;
    SearchResult.Session.SessionSettings.bUseLobbiesIfAvailable = true;

    UE_LOG(
        LogDMOnlineSession,
        Log,
        TEXT("Steam join settings normalized. Presence=%d, UseLobbies=%d"),
        SearchResult.Session.SessionSettings.bUsesPresence ? 1 : 0,
        SearchResult.Session.SessionSettings.bUseLobbiesIfAvailable ? 1 : 0
    );

    // 참가 완료 콜백에서 검색 원본을 참조할 수 있도록 인덱스를 보관한다.
    PendingJoinSourceIndex = SourceIndex;

    JoinedConnectString.Reset();
    BeginOperation(EOperation::Join);

    StartOperationTimeout();

    ActiveDelegateHandle = Sessions->AddOnJoinSessionCompleteDelegate_Handle(
        FOnJoinSessionCompleteDelegate::CreateUObject(this, &UDMOnlineSessionSubsystem::HandleJoinComplete)
    );

    const bool bStarted = Sessions->JoinSession(0, DMSessionName, SearchResult);

    if (!bStarted && CurrentOperation == EOperation::Join)
    {
        PendingJoinSourceIndex = INDEX_NONE;

        BeginRecoveryDestroy(TEXT("Steam 세션 참가 요청을 시작하지 못했습니다."));
    }

    return bStarted;
}

bool UDMOnlineSessionSubsystem::DestroyLobbySession()
{
    if (RejectBlockedOperation(EOperation::Destroy))
    {
        return false;
    }

    if (IsBusy())
    {
        UE_LOG(LogDMOnlineSession, Verbose, TEXT("Destroy rejected: another operation is active."));
        return false;
    }

    IOnlineSessionPtr Sessions = GetSteamSessionInterface();

    if (!Sessions.IsValid())
    {
        OnDestroyCompleted.Broadcast(false, TEXT("Steam 세션 인터페이스를 찾을 수 없습니다."));
        return false;
    }

    if (Sessions->GetNamedSession(DMSessionName) == nullptr)
    {
        // 로컬 세션이 없어도 이전 매치 승인 기록은 제거한다.
        ClearApprovedMatchRoster();
        ResetCachedSessionData();

        OnDestroyCompleted.Broadcast(true, TEXT("해제할 세션이 없습니다."));

        return true;
    }

    BeginOperation(EOperation::Destroy);

    StartOperationTimeout();

    ActiveDelegateHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
        FOnDestroySessionCompleteDelegate::CreateUObject(this, &UDMOnlineSessionSubsystem::HandleDestroyComplete)
    );

    const bool bStarted = Sessions->DestroySession(DMSessionName);

    if (!bStarted && CurrentOperation == EOperation::Destroy)
    {
        BeginRecoveryDestroy(TEXT("세션 해제 요청을 시작하지 못했습니다."));
    }

    return bStarted;
}

bool UDMOnlineSessionSubsystem::UpdateLobbyMatchStarting(const bool bStarting)
{
    if (RejectBlockedOperation(EOperation::Update))
    {
        return false;
    }

    if (IsBusy())
    {
        UE_LOG(LogDMOnlineSession, Verbose, TEXT("Session update rejected: operation is busy."));

        return false;
    }

    IOnlineSessionPtr Sessions = GetSteamSessionInterface();

    if (!Sessions.IsValid())
    {
        UE_LOG(LogDMOnlineSession, Error, TEXT("Session update failed: Steam interface unavailable."));

        return false;
    }

    const FOnlineSessionSettings* CurrentSettings = Sessions->GetSessionSettings(DMSessionName);

    if (CurrentSettings == nullptr)
    {
        UE_LOG(LogDMOnlineSession, Error, TEXT("Session update failed: GameSession is missing."));

        return false;
    }

    // 기존 세션 설정을 보존하면서 광고 상태만 변경한다.
    PendingUpdateSettings = MakeShared<FOnlineSessionSettings>(*CurrentSettings);

    PendingUpdateSettings->Set(
        DMLobbyStatusKey,
        bStarting ? DMLobbyStartingValue : DMLobbyOpenValue,
        EOnlineDataAdvertisementType::ViaOnlineService
    );

    // 승인된 기존 참가자의 서버 이동은 별도 명단으로 검증한다.
    // 새 Steam Join은 Starting과 매치 진행 중 허용하지 않는다.
    PendingUpdateSettings->bAllowJoinInProgress = !bStarting;
    PendingUpdateSettings->bAllowJoinViaPresence = !bStarting;

    // 비동기 작업 상태와 완료 콜백을 먼저 등록한다.
    BeginOperation(EOperation::Update);

    StartOperationTimeout();

    ActiveDelegateHandle = Sessions->AddOnUpdateSessionCompleteDelegate_Handle(
        FOnUpdateSessionCompleteDelegate::CreateUObject(this, &UDMOnlineSessionSubsystem::HandleUpdateComplete)
    );

    const bool bStarted = Sessions->UpdateSession(DMSessionName, *PendingUpdateSettings, true);

    // 요청 시작에 실패한 경우 완료 이벤트를 동일하게 전달한다.
    if (!bStarted && CurrentOperation == EOperation::Update)
    {
        CompleteOperation(false, TEXT("Steam 세션 설정 갱신 요청 실패."));
    }

    return bStarted;
}

#pragma endregion

#pragma region Session Queries

bool UDMOnlineSessionSubsystem::IsBusy() const
{
    return CurrentOperation != EOperation::None;
}

bool UDMOnlineSessionSubsystem::IsRecoveryBlocked() const
{
    return bRecoveryBlocked;
}

int32 UDMOnlineSessionSubsystem::GetFoundSessionCount() const
{
    return VisibleResultIndices.Num();
}

FString UDMOnlineSessionSubsystem::GetFoundSessionOwnerName(const int32 ResultIndex) const
{
    if (!SessionSearch.IsValid() || !VisibleResultIndices.IsValidIndex(ResultIndex))
    {
        return FString();
    }

    const int32 SourceIndex = VisibleResultIndices[ResultIndex];

    if (!SessionSearch->SearchResults.IsValidIndex(SourceIndex))
    {
        return FString();
    }

    return SessionSearch->SearchResults[SourceIndex].Session.OwningUserName;
}

int32 UDMOnlineSessionSubsystem::GetFoundSessionOpenSlots(const int32 ResultIndex) const
{
    if (!SessionSearch.IsValid() || !VisibleResultIndices.IsValidIndex(ResultIndex))
    {
        return 0;
    }

    const int32 SourceIndex = VisibleResultIndices[ResultIndex];

    if (!SessionSearch->SearchResults.IsValidIndex(SourceIndex))
    {
        return 0;
    }

    return SessionSearch->SearchResults[SourceIndex].Session.NumOpenPublicConnections;
}

const FString& UDMOnlineSessionSubsystem::GetJoinedConnectString() const
{
    return JoinedConnectString;
}

bool UDMOnlineSessionSubsystem::HasLocalLobbySession() const
{
    IOnlineSessionPtr Sessions = GetSteamSessionInterface();

    // Steam 인터페이스가 없으면 남은 세션도 없다고 본다.
    return Sessions.IsValid() && Sessions->GetNamedSession(DMSessionName) != nullptr;
}

#pragma endregion

#pragma region Match Admission

bool UDMOnlineSessionSubsystem::SetApprovedMatchRoster(const TArray<FUniqueNetIdRepl>& PlayerIds)
{
    // 경기가 중단된 뒤 한 명만 로비로 복귀하는 상황도 허용한다.
    if (PlayerIds.Num() < 1 || PlayerIds.Num() > DMMatch::MaxPlayers)
    {
        return false;
    }

    TSet<FString> NewApprovedIds;

    for (const FUniqueNetIdRepl& PlayerId : PlayerIds)
    {
        const FString PlayerKey = MakeMatchPlayerKey(PlayerId);

        // 유효하지 않거나 중복된 Steam ID가 있으면 시작을 거부한다.
        if (PlayerKey.IsEmpty() || NewApprovedIds.Contains(PlayerKey))
        {
            UE_LOG(LogDMOnlineSession, Error, TEXT("Match roster contains an invalid or duplicate ID."));

            return false;
        }

        NewApprovedIds.Add(PlayerKey);
    }

    // 모든 검사가 성공한 경우에만 이전 명단을 교체한다.
    ApprovedMatchPlayerIds = MoveTemp(NewApprovedIds);
    ArrivedMatchPlayerIds.Reset();

    UE_LOG(LogDMOnlineSession, Log, TEXT("Approved match roster captured. Players=%d"), ApprovedMatchPlayerIds.Num());

    return true;
}

int32 UDMOnlineSessionSubsystem::GetApprovedMatchPlayerCount() const
{
    return ApprovedMatchPlayerIds.Num();
}

bool UDMOnlineSessionSubsystem::HasApprovedMatchRoster() const
{
    return ApprovedMatchPlayerIds.Num() >= 1 && ApprovedMatchPlayerIds.Num() <= DMMatch::MaxPlayers;
}
bool UDMOnlineSessionSubsystem::IsApprovedMatchPlayer(const FUniqueNetIdRepl& PlayerId) const
{
    const FString PlayerKey = MakeMatchPlayerKey(PlayerId);

    return !PlayerKey.IsEmpty() && ApprovedMatchPlayerIds.Contains(PlayerKey);
}

bool UDMOnlineSessionSubsystem::HasMatchPlayerArrived(const FUniqueNetIdRepl& PlayerId) const
{
    const FString PlayerKey = MakeMatchPlayerKey(PlayerId);

    return !PlayerKey.IsEmpty() && ArrivedMatchPlayerIds.Contains(PlayerKey);
}

bool UDMOnlineSessionSubsystem::MarkMatchPlayerArrived(const FUniqueNetIdRepl& PlayerId)
{
    const FString PlayerKey = MakeMatchPlayerKey(PlayerId);

    if (PlayerKey.IsEmpty() || !ApprovedMatchPlayerIds.Contains(PlayerKey) || ArrivedMatchPlayerIds.Contains(PlayerKey))
    {
        return false;
    }

    ArrivedMatchPlayerIds.Add(PlayerKey);
    return true;
}

void UDMOnlineSessionSubsystem::ResetMatchArrivalHistory()
{
    ArrivedMatchPlayerIds.Reset();
}

void UDMOnlineSessionSubsystem::ClearApprovedMatchRoster()
{
    ApprovedMatchPlayerIds.Reset();
    ArrivedMatchPlayerIds.Reset();
}

#pragma endregion

#pragma region Completion Callbacks

void UDMOnlineSessionSubsystem::HandleCreateComplete(const FName SessionName, const bool bWasSuccessful)
{
    if (CurrentOperation != EOperation::Create || SessionName != DMSessionName)
    {
        return;
    }

    if (bWaitingForTimedOutCompletion)
    {
        // 만료된 요청이 뒤늦게 성공해도 로비로 이동시키지 않고 잔여 세션을 정리한다.
        BeginRecoveryDestroy(TEXT("Steam 세션 생성 응답이 제한시간 이후 도착했습니다."));
        return;
    }

    if (!bWasSuccessful)
    {
        BeginRecoveryDestroy(TEXT("Steam 세션 생성 실패."));
        return;
    }

    IOnlineSessionPtr Sessions = GetSteamSessionInterface();
    if (!Sessions.IsValid() || Sessions->GetNamedSession(DMSessionName) == nullptr)
    {
        CompleteOperation(false, TEXT("Steam 생성 성공 응답을 받았지만 로컬 GameSession을 확인하지 못했습니다."));
        return;
    }

    CompleteOperation(true, TEXT("Steam 세션 생성 완료."));
}

void UDMOnlineSessionSubsystem::HandleCancelFindComplete(const bool bWasSuccessful)
{
    if (CurrentOperation != EOperation::Find || !bWaitingForTimedOutCompletion)
    {
        return;
    }

    if (bWasSuccessful)
    {
        UE_LOG(LogDMOnlineSession, Log, TEXT("Timed-out Steam search cancellation confirmed."));
        CompleteOperation(false, TEXT("시간이 초과된 Steam 검색을 취소했습니다. 다시 검색할 수 있습니다."));
    }
    else
    {
        UE_LOG(LogDMOnlineSession, Warning, TEXT("Steam search cancellation failed; awaiting original callback."));
    }
}

void UDMOnlineSessionSubsystem::HandleFindComplete(const bool bWasSuccessful)
{
    if (CurrentOperation != EOperation::Find)
    {
        return;
    }

    if (bWaitingForTimedOutCompletion)
    {
        CompleteOperation(false, TEXT("Steam 세션 검색의 지연된 완료를 회수했습니다. 검색을 다시 실행하세요."));
        return;
    }

    VisibleResultIndices.Reset();

    int32 RawResultCount = 0;
    int32 ProjectMatchCount = 0;

    if (bWasSuccessful && SessionSearch.IsValid())
    {
        const TArray<FOnlineSessionSearchResult>& Results = SessionSearch->SearchResults;

        RawResultCount = Results.Num();

        for (int32 Index = 0; Index < Results.Num(); ++Index)
        {
            const FOnlineSessionSearchResult& Result = Results[Index];

            if (!Result.IsValid())
            {
                continue;
            }

            FString ProjectValue;

            // 먼저 Don't Melt 프로젝트로 광고된 로비인지 확인한다.
            if (!Result.Session.SessionSettings.Get(DMProjectKey, ProjectValue) || ProjectValue != DMProjectValue)
            {
                continue;
            }

            ++ProjectMatchCount;

            FString LobbyStatus;
            int32 NetworkBuildId = INDEX_NONE;

            Result.Session.SessionSettings.Get(DMLobbyStatusKey, LobbyStatus);

            Result.Session.SessionSettings.Get(DMNetworkBuildKey, NetworkBuildId);

            // Don't Melt 로비에 대해서만 상세 진단 로그를 남긴다.
            UE_LOG(
                LogDMOnlineSession,
                Log,
                TEXT(
                    "DontMelt candidate: Owner=%s, Status=%s, NetworkBuild=%d, EngineBuild=0x%08X, OpenSlots=%d, Presence=%d, UseLobbies=%d"
                ),
                *Result.Session.OwningUserName,
                *LobbyStatus,
                NetworkBuildId,
                static_cast<uint32>(Result.Session.SessionSettings.BuildUniqueId),
                Result.Session.NumOpenPublicConnections,
                Result.Session.SessionSettings.bUsesPresence ? 1 : 0,
                Result.Session.SessionSettings.bUseLobbiesIfAvailable ? 1 : 0
            );

            if (!IsCompatibleOpenLobby(Result))
            {
                continue;
            }

            VisibleResultIndices.Add(Index);
        }
    }

    UE_LOG(
        LogDMOnlineSession,
        Log,
        TEXT("Steam lobby search summary: Success=%d, Raw=%d, ProjectMatched=%d, Compatible=%d"),
        bWasSuccessful ? 1 : 0,
        RawResultCount,
        ProjectMatchCount,
        VisibleResultIndices.Num()
    );

    CompleteOperation(
        bWasSuccessful,
        bWasSuccessful ? FString::Printf(TEXT("Don’t Melt 세션 %d개 검색 완료."), VisibleResultIndices.Num())
                       : TEXT("Steam 세션 검색 실패.")
    );
}

void UDMOnlineSessionSubsystem::HandleJoinComplete(
    const FName SessionName, const EOnJoinSessionCompleteResult::Type Result
)
{
    if (CurrentOperation != EOperation::Join || SessionName != DMSessionName)
    {
        return;
    }

    if (bWaitingForTimedOutCompletion)
    {
        BeginRecoveryDestroy(TEXT("Steam 참가 응답이 제한시간 이후 도착했습니다."));
        return;
    }

    if (Result != EOnJoinSessionCompleteResult::Success)
    {
        BeginRecoveryDestroy(FString::Printf(TEXT("세션 참가 실패. Result=%d"), static_cast<int32>(Result)));
        return;
    }

    IOnlineSessionPtr Sessions = GetSteamSessionInterface();

    if (!Sessions.IsValid())
    {
        BeginRecoveryDestroy(TEXT("Steam 세션 인터페이스를 찾을 수 없습니다."));
        return;
    }

    // JoinSession이 성공하더라도 실제 연결 정보가 유효한지 검사한다.
    const FNamedOnlineSession* NamedSession = Sessions->GetNamedSession(DMSessionName);

    const bool bNamedSessionExists = NamedSession != nullptr;

    const bool bNamedSessionInfoValid =
        bNamedSessionExists && NamedSession->SessionInfo.IsValid() && NamedSession->SessionInfo->IsValid();

    UE_LOG(
        LogDMOnlineSession,
        Log,
        TEXT("Steam Join diagnosis: NamedSession=%d, NamedInfoValid=%d"),
        bNamedSessionExists ? 1 : 0,
        bNamedSessionInfoValid ? 1 : 0
    );

    FString ResolvedAddress;

    // 엔진에서 정의한 게임 연결 포트 식별자를 사용한다.
    const bool bNamedResolved = bNamedSessionInfoValid
        && Sessions->GetResolvedConnectString(DMSessionName, ResolvedAddress, NAME_GamePort)
        && !ResolvedAddress.IsEmpty();

    if (bNamedResolved)
    {
        UE_LOG(LogDMOnlineSession, Log, TEXT("Steam connect address resolved from NamedSession."));
    }
    else
    {
        // Named Session의 주소 확인이 실패한 경우 검색 원본을 검사한다.
        const bool bSearchResultExists =
            SessionSearch.IsValid() && SessionSearch->SearchResults.IsValidIndex(PendingJoinSourceIndex);

        const FOnlineSessionSearchResult* OriginalResult =
            bSearchResultExists ? &SessionSearch->SearchResults[PendingJoinSourceIndex] : nullptr;

        const bool bSearchSessionInfoValid = OriginalResult != nullptr && OriginalResult->Session.SessionInfo.IsValid()
            && OriginalResult->Session.SessionInfo->IsValid();

        UE_LOG(
            LogDMOnlineSession,
            Log,
            TEXT("Steam Join fallback diagnosis: SearchResult=%d, SearchInfoValid=%d"),
            bSearchResultExists ? 1 : 0,
            bSearchSessionInfoValid ? 1 : 0
        );

        // 검색 원본에서 주소 해석을 시도한다.
        if (bSearchSessionInfoValid)
        {
            ResolvedAddress.Reset();

            // 검색 원본에서도 동일한 게임 포트 식별자를 사용한다.
            const bool bSearchResolved =
                Sessions->GetResolvedConnectString(*OriginalResult, NAME_GamePort, ResolvedAddress);

            if (bSearchResolved && !ResolvedAddress.IsEmpty())
            {
                UE_LOG(LogDMOnlineSession, Log, TEXT("Steam connect address resolved from SearchResult fallback."));
            }
            else
            {
                ResolvedAddress.Reset();
            }
        }
    }

    PendingJoinSourceIndex = INDEX_NONE;

    if (ResolvedAddress.IsEmpty())
    {
        BeginRecoveryDestroy(TEXT("Steam 세션 연결 주소를 확인하지 못했습니다."));
        return;
    }

    JoinedConnectString = MoveTemp(ResolvedAddress);

    CompleteOperation(true, TEXT("세션 참가 및 연결 주소 확인 완료."));
}

void UDMOnlineSessionSubsystem::HandleDestroyComplete(const FName SessionName, const bool bWasSuccessful)
{
    if (CurrentOperation != EOperation::Destroy || SessionName != DMSessionName)
    {
        return;
    }

    IOnlineSessionPtr Sessions = GetSteamSessionInterface();
    const bool bStillExists = !Sessions.IsValid() || Sessions->GetNamedSession(DMSessionName) != nullptr;

    if (bRecoveryDestroyActive)
    {
        if (!bWasSuccessful || bStillExists)
        {
            FailClosedRecovery(TEXT("Steam 잔여 세션 Destroy 결과를 확인하지 못했습니다."));
            return;
        }

        // 최초 요청이 Destroy였으면 최종 해제 성공을, 그 외 요청은 원래 실패를 전달한다.
        const bool bOriginalDestroy = RecoveryOrigin == EOperation::Destroy;
        CompleteOperation(
            bOriginalDestroy,
            bOriginalDestroy ? TEXT("Steam 세션 해제 복구가 완료되었습니다.")
                             : RecoveryFailureDetail + TEXT(" 잔여 세션을 정상 해제했습니다.")
        );
        return;
    }

    if (bWaitingForTimedOutCompletion)
    {
        if (bWasSuccessful && !bStillExists)
        {
            CompleteOperation(true, TEXT("시간이 초과됐던 Steam 세션 해제가 완료되었습니다."));
        }
        else
        {
            BeginRecoveryDestroy(TEXT("Steam 세션 해제의 지연된 결과를 수신했지만 세션이 남았습니다."));
        }
        return;
    }

    if (!bWasSuccessful || bStillExists)
    {
        BeginRecoveryDestroy(TEXT("Steam 세션 해제 완료 여부를 확인하지 못했습니다."));
        return;
    }

    CompleteOperation(true, TEXT("Steam 세션 해제 완료."));
}

void UDMOnlineSessionSubsystem::HandleUpdateComplete(const FName SessionName, const bool bWasSuccessful)
{
    if (CurrentOperation != EOperation::Update || SessionName != DMSessionName)
    {
        return;
    }

    if (bWaitingForTimedOutCompletion)
    {
        // 로비 GameMode가 실패 완료를 받은 뒤 기존 광고 상태로 복구하도록 맡긴다.
        CompleteOperation(false, TEXT("Steam 세션 광고 갱신 응답이 지연됐습니다. 광고 상태 복구가 필요합니다."));
        return;
    }

    CompleteOperation(
        bWasSuccessful,
        bWasSuccessful ? TEXT("Steam 세션 광고 상태 갱신 완료.") : TEXT("Steam 세션 광고 상태 갱신 실패.")
    );
}

#pragma endregion

#pragma region Internal Helpers

void UDMOnlineSessionSubsystem::ClearActiveDelegate()
{
    if (!ActiveDelegateHandle.IsValid())
    {
        return;
    }

    IOnlineSessionPtr Sessions = GetSteamSessionInterface();

    if (Sessions.IsValid())
    {
        switch (CurrentOperation)
        {
        case EOperation::Create:
            Sessions->ClearOnCreateSessionCompleteDelegate_Handle(ActiveDelegateHandle);
            break;

        case EOperation::Find:
            Sessions->ClearOnFindSessionsCompleteDelegate_Handle(ActiveDelegateHandle);
            break;

        case EOperation::Join:
            Sessions->ClearOnJoinSessionCompleteDelegate_Handle(ActiveDelegateHandle);
            break;

        case EOperation::Destroy:
            Sessions->ClearOnDestroySessionCompleteDelegate_Handle(ActiveDelegateHandle);
            break;

        case EOperation::Update:
            Sessions->ClearOnUpdateSessionCompleteDelegate_Handle(ActiveDelegateHandle);
            break;

        default:
            break;
        }
    }

    ActiveDelegateHandle.Reset();
}

void UDMOnlineSessionSubsystem::ClearCancelFindDelegate()
{
    if (!CancelFindDelegateHandle.IsValid())
    {
        return;
    }

    if (IOnlineSessionPtr Sessions = GetSteamSessionInterface(); Sessions.IsValid())
    {
        Sessions->ClearOnCancelFindSessionsCompleteDelegate_Handle(CancelFindDelegateHandle);
    }

    CancelFindDelegateHandle.Reset();
}

void UDMOnlineSessionSubsystem::CompleteOperation(const bool bWasSuccessful, const FString& Detail)
{
    const EOperation FinishedOperation = bRecoveryDestroyActive ? RecoveryOrigin : CurrentOperation;
    const uint64 FinishedSequence = ActiveOperationSequence;

    // 이벤트 호출 전에 현재 작업과 Delegate, 응답 대기 타이머를 정리한다.
    ClearOperationTimeout();
    ClearActiveDelegate();
    ClearCancelFindDelegate();
    CurrentOperation = EOperation::None;
    ActiveOperationSequence = 0;
    bWaitingForTimedOutCompletion = false;
    bRecoveryDestroyActive = false;
    RecoveryOrigin = EOperation::None;
    RecoveryFailureDetail.Reset();

    if (FinishedOperation == EOperation::Join)
    {
        PendingJoinSourceIndex = INDEX_NONE;
        if (!bWasSuccessful)
        {
            // 이전 방 목록을 이용한 중복 참가를 방지한다.
            ResetCachedSessionData();
        }
    }

    if (FinishedOperation == EOperation::Update)
    {
        // 비동기 업데이트에 사용한 임시 설정을 정리한다.
        PendingUpdateSettings.Reset();
    }

    if (FinishedOperation == EOperation::Find && !bWasSuccessful)
    {
        SessionSearch.Reset();
        VisibleResultIndices.Reset();
    }

    if (FinishedOperation == EOperation::Create && bWasSuccessful)
    {
        // 새 Steam 방을 만들면 이전 매치의 접속 허용 명단을 지운다.
        ClearApprovedMatchRoster();
    }

    if (FinishedOperation == EOperation::Destroy && bWasSuccessful)
    {
        ClearApprovedMatchRoster();
        ResetCachedSessionData();
    }

    UE_LOG(
        LogDMOnlineSession,
        Log,
        TEXT("Session operation completed. Request=%llu, Operation=%d, Success=%d, Detail=%s"),
        static_cast<unsigned long long>(FinishedSequence),
        static_cast<int32>(FinishedOperation),
        bWasSuccessful ? 1 : 0,
        *Detail
    );

    switch (FinishedOperation)
    {
    case EOperation::Create:
        OnCreateCompleted.Broadcast(bWasSuccessful, Detail);
        break;

    case EOperation::Find:
        OnFindCompleted.Broadcast(bWasSuccessful, Detail);
        break;

    case EOperation::Join:
        OnJoinCompleted.Broadcast(bWasSuccessful, Detail);
        break;

    case EOperation::Destroy:
        OnDestroyCompleted.Broadcast(bWasSuccessful, Detail);
        break;

    case EOperation::Update:
        OnUpdateCompleted.Broadcast(bWasSuccessful, Detail);
        break;

    default:
        break;
    }
}

void UDMOnlineSessionSubsystem::ResetCachedSessionData()
{
    SessionSearch.Reset();
    VisibleResultIndices.Reset();
    JoinedConnectString.Reset();
    // 이전 참가 요청의 검색 결과 인덱스를 초기화한다.
    PendingJoinSourceIndex = INDEX_NONE;
}

void UDMOnlineSessionSubsystem::StartOperationTimeout()
{
    UGameInstance* GameInstance = GetGameInstance();

    if (!IsValid(GameInstance))
    {
        return;
    }

    // GameInstance 타이머는 맵 이동 중에도 유지된다.
    GameInstance->GetTimerManager().SetTimer(
        OperationTimeoutHandle,
        this,
        &UDMOnlineSessionSubsystem::HandleOperationTimeout,
        DMSessionOperationTimeoutSeconds,
        false
    );
}

void UDMOnlineSessionSubsystem::ClearOperationTimeout()
{
    if (UGameInstance* GameInstance = GetGameInstance())
    {
        GameInstance->GetTimerManager().ClearTimer(OperationTimeoutHandle);
    }

    OperationTimeoutHandle.Invalidate();
}

void UDMOnlineSessionSubsystem::HandleOperationTimeout()
{
    if (!IsBusy() || bWaitingForTimedOutCompletion || bRecoveryDestroyActive)
    {
        return;
    }

    UE_LOG(
        LogDMOnlineSession, Error,
        TEXT("Steam operation timed out; retaining callback for late completion. Request=%llu, Operation=%d"),
        static_cast<unsigned long long>(ActiveOperationSequence), static_cast<int32>(CurrentOperation)
    );

    bWaitingForTimedOutCompletion = true;

    // Find는 취소를 요청하되, 실제 취소 완료 전 새 검색을 시작하지 않는다.
    if (CurrentOperation == EOperation::Find)
    {
        if (IOnlineSessionPtr Sessions = GetSteamSessionInterface(); Sessions.IsValid())
        {
            CancelFindDelegateHandle = Sessions->AddOnCancelFindSessionsCompleteDelegate_Handle(
                FOnCancelFindSessionsCompleteDelegate::CreateUObject(
                    this, &UDMOnlineSessionSubsystem::HandleCancelFindComplete
                )
            );
            const bool bCancelStarted = Sessions->CancelFindSessions();
            if (!bCancelStarted)
            {
                ClearCancelFindDelegate();
            }
        }
    }

    // CancelFindSessions가 즉시 완료 콜백을 호출했으면 복구 타이머를 생성하지 않는다.
    if (!IsBusy())
    {
        return;
    }

    ClearOperationTimeout();

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        GameInstance->GetTimerManager().SetTimer(
            OperationTimeoutHandle, this, &UDMOnlineSessionSubsystem::HandleRecoveryTimeout,
            DMSessionRecoveryTimeoutSeconds, false
        );
    }
    else
    {
        FailClosedRecovery(TEXT("Steam 요청 복구용 GameInstance가 유효하지 않습니다."));
    }
}

void UDMOnlineSessionSubsystem::BeginOperation(const EOperation Operation)
{
    CurrentOperation = Operation;
    ActiveOperationSequence = ++OperationSequence;

    UE_LOG(
        LogDMOnlineSession, Log, TEXT("Steam session operation started. Request=%llu, Operation=%d"),
        static_cast<unsigned long long>(ActiveOperationSequence), static_cast<int32>(Operation)
    );
}

bool UDMOnlineSessionSubsystem::RejectBlockedOperation(const EOperation RequestedOperation)
{
    if (!bRecoveryBlocked)
    {
        return false;
    }

    const FString Detail = TEXT("이전 Steam 작업의 종료를 확인할 수 없습니다. 게임과 Steam을 재시작한 뒤 다시 시도하세요.");
    UE_LOG(LogDMOnlineSession, Error, TEXT("Session request blocked after unresolved callback. Operation=%d"),
        static_cast<int32>(RequestedOperation));

    switch (RequestedOperation)
    {
    case EOperation::Create: OnCreateCompleted.Broadcast(false, Detail); break;
    case EOperation::Find: OnFindCompleted.Broadcast(false, Detail); break;
    case EOperation::Join: OnJoinCompleted.Broadcast(false, Detail); break;
    case EOperation::Destroy: OnDestroyCompleted.Broadcast(false, Detail); break;
    case EOperation::Update: OnUpdateCompleted.Broadcast(false, Detail); break;
    default: break;
    }
    return true;
}

void UDMOnlineSessionSubsystem::BeginRecoveryDestroy(const FString& FailureDetail)
{
    if (bRecoveryDestroyActive || CurrentOperation == EOperation::None)
    {
        return;
    }

    // 원래 작업의 콜백이 끝난 경우에만 Destroy를 요청한다.
    // 콜백이 끝나지 않은 요청은 HandleOperationTimeout에서 계속 기다린다.
    const EOperation OriginalOperation = CurrentOperation;
    IOnlineSessionPtr Sessions = GetSteamSessionInterface();

    if (!Sessions.IsValid())
    {
        FailClosedRecovery(FailureDetail + TEXT(" Steam 세션 인터페이스를 사용할 수 없습니다."));
        return;
    }

    if (Sessions->GetNamedSession(DMSessionName) == nullptr)
    {
        // 원래 Destroy 요청의 완료가 이미 확인된 경우에는 성공으로 반환한다.
        CompleteOperation(OriginalOperation == EOperation::Destroy, FailureDetail);
        return;
    }

    RecoveryOrigin = OriginalOperation;
    RecoveryFailureDetail = FailureDetail;
    bRecoveryDestroyActive = true;
    bWaitingForTimedOutCompletion = false;

    ClearOperationTimeout();
    ClearActiveDelegate();
    BeginOperation(EOperation::Destroy);

    // 실패한 Create/Join은 원래 요청의 사용자 완료 이벤트를 마지막에 한 번만 보낸다.
    ActiveDelegateHandle = Sessions->AddOnDestroySessionCompleteDelegate_Handle(
        FOnDestroySessionCompleteDelegate::CreateUObject(this, &UDMOnlineSessionSubsystem::HandleDestroyComplete)
    );

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        GameInstance->GetTimerManager().SetTimer(
            OperationTimeoutHandle, this, &UDMOnlineSessionSubsystem::HandleRecoveryTimeout,
            DMSessionRecoveryTimeoutSeconds, false
        );
    }
    else
    {
        FailClosedRecovery(TEXT("잔여 Steam 세션 해제를 위한 GameInstance가 유효하지 않습니다."));
        return;
    }

    UE_LOG(LogDMOnlineSession, Warning, TEXT("Cleaning up stale GameSession. Origin=%d"),
        static_cast<int32>(RecoveryOrigin));

    if (!Sessions->DestroySession(DMSessionName) && bRecoveryDestroyActive)
    {
        FailClosedRecovery(TEXT("잔여 Steam 세션 해제 요청을 시작하지 못했습니다."));
    }
}

void UDMOnlineSessionSubsystem::HandleRecoveryTimeout()
{
    if (!IsBusy())
    {
        return;
    }

    // Steam 비동기 요청의 실제 취소를 증명할 수 없으므로 성공으로 가장하지 않는다.
    FailClosedRecovery(TEXT("Steam 완료 응답 또는 세션 정리 응답을 끝내 확인하지 못했습니다."));
}

void UDMOnlineSessionSubsystem::FailClosedRecovery(const FString& Detail)
{
    UE_LOG(LogDMOnlineSession, Error, TEXT("Steam recovery blocked: %s"), *Detail);

    bRecoveryBlocked = true;
    CompleteOperation(false, Detail + TEXT(" 게임과 Steam을 재시작한 뒤 다시 시도하세요."));
}

#pragma endregion
