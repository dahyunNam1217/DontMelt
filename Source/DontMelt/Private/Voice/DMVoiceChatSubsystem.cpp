
#include "Voice/DMVoiceChatSubsystem.h"

#include "Framework/Match/DMMatchPlayerController.h"

#include "Core/DMLocalSettingsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerState.h"
#include "Sound/SoundAttenuation.h"
#include "TimerManager.h"

#include "OnlineSubsystem.h"
#include "Interfaces/VoiceInterface.h"
#include "Net/VoiceConfig.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMVoiceChat, Log, All);

#pragma region Lifecycle

void UDMVoiceChatSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 로컬 설정이 먼저 초기화되도록 보장한다.
    Collection.InitializeDependency<UDMLocalSettingsSubsystem>();

    const UGameInstance* GameInstance = GetGameInstance();

    const UDMLocalSettingsSubsystem* LocalSettings =
        IsValid(GameInstance) ? GameInstance->GetSubsystem<UDMLocalSettingsSubsystem>() : nullptr;

    if (IsValid(LocalSettings))
    {
        SetVoiceActivationEnabled(LocalSettings->GetVoiceActivationEnabled());
    }

    UE_LOG(
        LogDMVoiceChat, Log, TEXT("Steam Voice Subsystem initialized. Activation=%d"), bVoiceActivationEnabled ? 1 : 0
    );
}

void UDMVoiceChatSubsystem::Deinitialize()
{
    LeaveVoiceWorld();
    ReleaseLocalVoiceBackend();

    UE_LOG(LogDMVoiceChat, Log, TEXT("Steam Voice Subsystem deinitialized."));

    Super::Deinitialize();
}

#pragma endregion

#pragma region Voice Backend

bool UDMVoiceChatSubsystem::InitializeLocalVoiceBackend()
{
    if (IsBackendInitialized())
    {
        return true;
    }

    // Steam OnlineSubsystem을 명시적으로 요청한다.
    IOnlineSubsystem* SteamSubsystem = IOnlineSubsystem::Get(FName(TEXT("STEAM")));

    if (SteamSubsystem == nullptr)
    {
        UE_LOG(LogDMVoiceChat, Warning, TEXT("Steam OnlineSubsystem is unavailable."));

        return false;
    }

    OnlineVoice = SteamSubsystem->GetVoiceInterface();

    if (!OnlineVoice.IsValid())
    {
        UE_LOG(LogDMVoiceChat, Warning, TEXT("Steam Voice interface is unavailable."));

        return false;
    }

    // 음성 송신은 Talker 등록 이전에도 기본 차단한다.
    OnlineVoice->StopNetworkedVoice(LocalUserNum);

    // 이 프로젝트는 로컬 플레이어 1명만 사용한다.
    bLocalTalkerRegistered = OnlineVoice->RegisterLocalTalker(LocalUserNum);

    if (!bLocalTalkerRegistered)
    {
        UE_LOG(LogDMVoiceChat, Warning, TEXT("Steam local voice talker registration failed."));

        OnlineVoice.Reset();
        return false;
    }

    // PTT 입력 전에는 송신을 허용하지 않는다.
    OnlineVoice->StopNetworkedVoice(LocalUserNum);

    UE_LOG(LogDMVoiceChat, Log, TEXT("Steam local voice talker registered."));

    LogVoiceBackendState(TEXT("BackendInitialized"));

    // 백엔드가 준비된 뒤 현재 음성 정책을 다시 적용한다.
    UpdateNetworkVoiceTransmission();

    // 백엔드 준비 상태 변경을 로컬 UI에 알린다.
    VoiceControlStateChangedDelegate.Broadcast();

    return true;
}

bool UDMVoiceChatSubsystem::IsBackendInitialized() const
{
    return OnlineVoice.IsValid() && bLocalTalkerRegistered;
}

bool UDMVoiceChatSubsystem::IsVoiceCommunicationReady() const
{
    // 로컬 Steam 음성 인터페이스와 Talker 등록 완료 여부만 반환한다.
    // 원격 음성 수신의 품질·연결 성공을 보장하는 값은 아니다.
    return IsBackendInitialized();
}

#pragma region Voice Transmission

void UDMVoiceChatSubsystem::SetPushToTalkPressed(const bool bPressed)
{
    // 입력 해제는 정책 변경 중에도 허용해 이전 홀드 상태가 남지 않게 한다.
    const bool bShouldHold = bPressed && !bVoiceActivationEnabled && AppliedVoiceMode != EDMVoiceMode::Disabled;
    if (bPushToTalkLatched == bShouldHold)
    {
        return;
    }

    bPushToTalkLatched = bShouldHold;
    UpdateNetworkVoiceTransmission();
    VoiceControlStateChangedDelegate.Broadcast();
    UE_LOG(LogDMVoiceChat, Verbose, TEXT("PTT key %s"), bPushToTalkLatched ? TEXT("Pressed") : TEXT("Released"));
}

void UDMVoiceChatSubsystem::ToggleMicrophoneMuted()
{
    // M 키는 음성 감지 모드에서만 동작한다.
    if (!bVoiceActivationEnabled || AppliedVoiceMode == EDMVoiceMode::Disabled)
    {
        return;
    }

    SetMicrophoneMuted(!bMicrophoneMuted);
}

void UDMVoiceChatSubsystem::SetVoiceActivationEnabled(const bool bEnabled)
{
    if (bVoiceActivationEnabled == bEnabled)
    {
        return;
    }

    bVoiceActivationEnabled = bEnabled;

    // 모드 변경 시 PTT 송신과 음소거 상태를 초기화한다.
    bPushToTalkLatched = false;
    bMicrophoneMuted = false;

    UpdateNetworkVoiceTransmission();

    VoiceControlStateChangedDelegate.Broadcast();

    UE_LOG(
        LogDMVoiceChat,
        Log,
        TEXT("Voice mode changed: %s"),
        bVoiceActivationEnabled ? TEXT("VoiceActivation") : TEXT("PushToTalk")
    );
}

void UDMVoiceChatSubsystem::SetMicrophoneMuted(const bool bMuted)
{
    // PTT 모드에서는 별도 M 음소거를 사용하지 않는다.
    if (!bVoiceActivationEnabled || bMicrophoneMuted == bMuted)
    {
        return;
    }

    bMicrophoneMuted = bMuted;

    UpdateNetworkVoiceTransmission();

    VoiceControlStateChangedDelegate.Broadcast();

    UE_LOG(LogDMVoiceChat, Log, TEXT("Microphone muted: %d"), bMicrophoneMuted ? 1 : 0);
}

void UDMVoiceChatSubsystem::SetVoiceActivationThreshold(const float Threshold)
{
    const float ClampedThreshold = FMath::Clamp(Threshold, 0.0f, 1.0f);

    // 엔진 VOIP의 마이크 감지 임계값을 변경한다.
    UVOIPStatics::SetMicThreshold(ClampedThreshold);

    UE_LOG(LogDMVoiceChat, Log, TEXT("Microphone threshold updated: %.3f"), ClampedThreshold);
}

void UDMVoiceChatSubsystem::UpdateNetworkVoiceTransmission()
{
    const bool bPolicyAllowsTransmission = AppliedVoiceMode != EDMVoiceMode::Disabled;

    // 음성 감지 모드는 기본 허용, PTT 모드는 키를 누르는 동안만 허용한다.
    const bool bInputAllowsTransmission = bVoiceActivationEnabled ? !bMicrophoneMuted : bPushToTalkLatched;

    const bool bShouldTransmit = IsBackendInitialized() && bPolicyAllowsTransmission && bInputAllowsTransmission;

    // 실제 상태가 바뀔 때만 엔진 API를 호출한다.
    if (bNetworkVoiceTransmitting == bShouldTransmit)
    {
        return;
    }

    if (!OnlineVoice.IsValid())
    {
        bNetworkVoiceTransmitting = false;
        return;
    }

    if (bShouldTransmit)
    {
        OnlineVoice->StartNetworkedVoice(LocalUserNum);
    }
    else
    {
        OnlineVoice->StopNetworkedVoice(LocalUserNum);
    }

    bNetworkVoiceTransmitting = bShouldTransmit;

    UE_LOG(
        LogDMVoiceChat,
        Log,
        TEXT("Network voice transmission: %s, Mode=%s, Activation=%d, PTT=%d, Muted=%d"),
        bNetworkVoiceTransmitting ? TEXT("Enabled") : TEXT("Disabled"),
        *UEnum::GetValueAsString(AppliedVoiceMode),
        bVoiceActivationEnabled ? 1 : 0,
        bPushToTalkLatched ? 1 : 0,
        bMicrophoneMuted ? 1 : 0
    );
}

#pragma endregion

void UDMVoiceChatSubsystem::ReleaseLocalVoiceBackend()
{
    if (OnlineVoice.IsValid())
    {
        // 월드 이동·종료 시에도 송신을 먼저 차단한다.
        OnlineVoice->StopNetworkedVoice(LocalUserNum);

        if (bLocalTalkerRegistered)
        {
            OnlineVoice->UnregisterLocalTalker(LocalUserNum);
        }

        OnlineVoice.Reset();
    }

    bLocalTalkerRegistered = false;

    // 백엔드가 해제되면 로컬 송신 상태도 초기화한다.
    bNetworkVoiceTransmitting = false;
    bPushToTalkLatched = false;

    VoiceControlStateChangedDelegate.Broadcast();
}

void UDMVoiceChatSubsystem::LogVoiceBackendState(const TCHAR* Context) const
{
    const bool bVoiceAvailable = OnlineVoice.IsValid();

    const bool bHeadsetPresent = bVoiceAvailable && OnlineVoice->IsHeadsetPresent(LocalUserNum);

    UE_LOG(
        LogDMVoiceChat,
        Log,
        TEXT("Steam Voice [%s]: Interface=%d, LocalTalker=%d, Headset=%d"),
        Context != nullptr ? Context : TEXT("Unknown"),
        bVoiceAvailable ? 1 : 0,
        bLocalTalkerRegistered ? 1 : 0,
        bHeadsetPresent ? 1 : 0
    );
}

#pragma endregion

#pragma region Voice Policy

void UDMVoiceChatSubsystem::EnterLobbyVoice()
{
    ClearMatchControllerBinding();

    InitializeLocalVoiceBackend();

    ApplyVoiceMode(EDMVoiceMode::Lobby);

    LogVoiceBackendState(TEXT("EnterLobby"));
}

void UDMVoiceChatSubsystem::BindMatchController(ADMMatchPlayerController* Controller)
{
    if (!IsValid(Controller) || !Controller->IsLocalController())
    {
        return;
    }

    if (BoundMatchController.Get() == Controller)
    {
        return;
    }

    ClearMatchControllerBinding();

    InitializeLocalVoiceBackend();

    BoundMatchController = Controller;

    MatchVoiceModeChangedHandle =
        Controller->OnLocalVoiceModeChanged().AddUObject(this, &UDMVoiceChatSubsystem::HandleMatchVoiceModeChanged);

    // Controller가 이미 설정한 초기 정책도 반영한다.
    ApplyVoiceMode(Controller->GetLocalVoiceMode());

    // 원격 플레이어 청취 정책(근접/전역)을 주기적으로 갱신한다.
    StartRemoteTalkerRefresh();

    LogVoiceBackendState(TEXT("EnterMatch"));
}

void UDMVoiceChatSubsystem::UnbindMatchController(ADMMatchPlayerController* Controller)
{
    if (BoundMatchController.Get() != Controller)
    {
        return;
    }

    ClearMatchControllerBinding();

    ApplyVoiceMode(EDMVoiceMode::Disabled);
}

void UDMVoiceChatSubsystem::LeaveVoiceWorld()
{
    ClearMatchControllerBinding();

    bPushToTalkLatched = false;

    ApplyVoiceMode(EDMVoiceMode::Disabled);

    LogVoiceBackendState(TEXT("LeaveVoiceWorld"));
}

void UDMVoiceChatSubsystem::ApplyVoiceMode(const EDMVoiceMode NewMode)
{
    const EDMVoiceMode PreviousMode = AppliedVoiceMode;
    const bool bWasPTTLatched = bPushToTalkLatched;

    AppliedVoiceMode = NewMode;

    // 로비나 매치에서 완전히 벗어나면 PTT 송신을 해제한다.
    if (NewMode == EDMVoiceMode::Disabled)
    {
        bPushToTalkLatched = false;
    }

    UpdateNetworkVoiceTransmission();

    // 생존↔사망 전환 시 청취 규칙을 즉시 갱신한다(Match에 연결된 경우만).
    if (PreviousMode != AppliedVoiceMode && BoundMatchController.IsValid())
    {
        RefreshRemoteTalkerPolicy();
    }

    if (PreviousMode != AppliedVoiceMode || bWasPTTLatched != bPushToTalkLatched)
    {
        VoiceControlStateChangedDelegate.Broadcast();
    }

    if (PreviousMode != AppliedVoiceMode)
    {
        UE_LOG(
            LogDMVoiceChat,
            Log,
            TEXT("Voice policy changed: %s -> %s"),
            *UEnum::GetValueAsString(PreviousMode),
            *UEnum::GetValueAsString(AppliedVoiceMode)
        );
    }
}

void UDMVoiceChatSubsystem::HandleMatchVoiceModeChanged(const EDMVoiceMode NewMode)
{
    ApplyVoiceMode(NewMode);
}

void UDMVoiceChatSubsystem::ClearMatchControllerBinding()
{
    // Match 월드를 벗어나면 원격 Talker와 로컬 음소거를 정리한다.
    StopRemoteTalkerRefresh();
    ReleaseRemoteTalkers();

    ADMMatchPlayerController* Controller = BoundMatchController.Get();

    if (IsValid(Controller) && MatchVoiceModeChangedHandle.IsValid())
    {
        Controller->OnLocalVoiceModeChanged().Remove(MatchVoiceModeChangedHandle);
    }

    MatchVoiceModeChangedHandle.Reset();
    BoundMatchController.Reset();
}

#pragma endregion

#pragma region Remote Talker Policy

void UDMVoiceChatSubsystem::StartRemoteTalkerRefresh()
{
    ADMMatchPlayerController* Controller = BoundMatchController.Get();

    UWorld* World = IsValid(Controller) ? Controller->GetWorld() : nullptr;

    if (!IsValid(World))
    {
        return;
    }

    StopRemoteTalkerRefresh();

    RemoteTalkerRefreshWorld = World;

    World->GetTimerManager().SetTimer(
        RemoteTalkerRefreshTimer,
        this,
        &UDMVoiceChatSubsystem::RefreshRemoteTalkerPolicy,
        FMath::Max(0.05f, RemoteTalkerRefreshInterval),
        true
    );

    RefreshRemoteTalkerPolicy();
}

void UDMVoiceChatSubsystem::StopRemoteTalkerRefresh()
{
    if (UWorld* World = RemoteTalkerRefreshWorld.Get())
    {
        World->GetTimerManager().ClearTimer(RemoteTalkerRefreshTimer);
    }

    RemoteTalkerRefreshTimer.Invalidate();
    RemoteTalkerRefreshWorld.Reset();
}

void UDMVoiceChatSubsystem::RefreshRemoteTalkerPolicy()
{
    ADMMatchPlayerController* Controller = BoundMatchController.Get();

    const UWorld* World = IsValid(Controller) ? Controller->GetWorld() : nullptr;

    const AGameStateBase* GameState = IsValid(World) ? World->GetGameState() : nullptr;

    if (!IsValid(GameState))
    {
        return;
    }

    // Disabled(로딩·종료 중)에서는 기존 청취 상태를 바꾸지 않는다.
    if (AppliedVoiceMode == EDMVoiceMode::Disabled)
    {
        return;
    }

    const APlayerState* LocalPlayerState = Controller->GetPlayerState<APlayerState>();

    // 로컬 PlayerState 복제 전에는 자신을 원격으로 오인할 수 있으므로 다음 갱신을 기다린다.
    if (!IsValid(LocalPlayerState))
    {
        return;
    }

    // 로컬 청취자가 생존 중일 때만 근접 규칙을 적용한다.
    // 사망(DeadGlobal)이면 모든 원격 음성을 감쇠·방향성 없이 듣는다.
    const bool bUseProximity = AppliedVoiceMode == EDMVoiceMode::AliveProximity;

    // 1) 사라진 PlayerState 항목을 정리한다.
    for (auto It = RemoteTalkers.CreateIterator(); It; ++It)
    {
        if (It->Key.IsValid())
        {
            continue;
        }

        SetRemoteTalkerLocallyMuted(It->Value, false);

        if (UVOIPTalker* StaleTalker = It->Value.Talker.Get())
        {
            RemoteTalkerObjects.Remove(StaleTalker);
        }

        It.RemoveCurrent();
    }

    // 2) 현재 원격 플레이어마다 Talker를 확보하고 청취 규칙을 적용한다.
    for (APlayerState* RemotePlayerState : GameState->PlayerArray)
    {
        if (!IsValid(RemotePlayerState) || RemotePlayerState == LocalPlayerState
            || !RemotePlayerState->GetUniqueId().IsValid())
        {
            continue;
        }

        FDMRemoteTalkerEntry& Entry = RemoteTalkers.FindOrAdd(TWeakObjectPtr<APlayerState>(RemotePlayerState));

        UVOIPTalker* Talker = Entry.Talker.Get();

        if (!IsValid(Talker))
        {
            Talker = UVOIPTalker::CreateTalkerForPlayer(RemotePlayerState);

            if (!IsValid(Talker))
            {
                continue;
            }

            Entry.Talker = Talker;
            Entry.PlayerId = RemotePlayerState->GetUniqueId();
            RemoteTalkerObjects.Add(Talker);
        }

        if (bUseProximity)
        {
            const APawn* RemotePawn = RemotePlayerState->GetPawn();

            USceneComponent* AttachComponent = IsValid(RemotePawn) ? RemotePawn->GetRootComponent() : nullptr;

            // 원격 플레이어 Pawn 위치를 기준으로 거리 감쇠를 적용한다.
            // (설정은 다음 발화 시작 시점부터 오디오 컴포넌트에 반영된다.)
            Talker->Settings.ComponentToAttachTo = AttachComponent;
            Talker->Settings.AttenuationSettings = GetOrCreateProximityAttenuation();

            // 위치를 알 수 없는 원격 플레이어(Relevancy 밖 등)는
            // 근접 규칙상 들리지 않아야 하므로 로컬 음소거한다.
            SetRemoteTalkerLocallyMuted(Entry, AttachComponent == nullptr);
        }
        else
        {
            // DeadGlobal·Lobby: 감쇠·방향성 없이 동일 음량으로 듣는다.
            Talker->Settings.ComponentToAttachTo = nullptr;
            Talker->Settings.AttenuationSettings = nullptr;

            SetRemoteTalkerLocallyMuted(Entry, false);
        }
    }
}

void UDMVoiceChatSubsystem::ReleaseRemoteTalkers()
{
    for (auto& Pair : RemoteTalkers)
    {
        // 이 Subsystem이 건 로컬 음소거는 다음 월드에 남지 않도록 해제한다.
        SetRemoteTalkerLocallyMuted(Pair.Value, false);
    }

    RemoteTalkers.Reset();

    // 강한 참조를 해제하면 Talker는 GC 시 Voice Engine 등록에서 제거된다.
    RemoteTalkerObjects.Reset();
}

USoundAttenuation* UDMVoiceChatSubsystem::GetOrCreateProximityAttenuation()
{
    if (IsValid(ProximityVoiceAttenuation))
    {
        return ProximityVoiceAttenuation;
    }

    ProximityVoiceAttenuation = NewObject<USoundAttenuation>(this);

    const float FullVolumeRadius = FMath::Max(0.0f, ProximityVoiceFullVolumeRadius);

    const float MaxDistance = FMath::Max(FullVolumeRadius + 1.0f, ProximityVoiceMaxDistance);

    // 구형 감쇠: 반경 안은 최대 음량, 최대 거리 밖은 무음(선형 감쇠).
    FSoundAttenuationSettings& Settings = ProximityVoiceAttenuation->Attenuation;

    Settings.bAttenuate = true;
    Settings.bSpatialize = true;
    Settings.AttenuationShape = EAttenuationShape::Sphere;
    Settings.AttenuationShapeExtents = FVector(FullVolumeRadius, 0.0f, 0.0f);
    Settings.FalloffDistance = MaxDistance - FullVolumeRadius;
    Settings.DistanceAlgorithm = EAttenuationDistanceModel::Linear;

    return ProximityVoiceAttenuation;
}

void UDMVoiceChatSubsystem::SetRemoteTalkerLocallyMuted(FDMRemoteTalkerEntry& Entry, const bool bMuted)
{
    if (Entry.bLocallyMuted == bMuted)
    {
        return;
    }

    if (!OnlineVoice.IsValid() || !Entry.PlayerId.IsValid())
    {
        // 백엔드가 없으면 실제 음소거 상태도 없으므로 기록만 맞춘다.
        Entry.bLocallyMuted = false;
        return;
    }

    const FUniqueNetId& PlayerNetId = *Entry.PlayerId.GetUniqueNetId();

    // 시스템 전체가 아닌 이 게임 세션의 로컬 청취에만 적용한다.
    const bool bApplied = bMuted ? OnlineVoice->MuteRemoteTalker(LocalUserNum, PlayerNetId, false)
                                 : OnlineVoice->UnmuteRemoteTalker(LocalUserNum, PlayerNetId, false);

    if (bApplied)
    {
        Entry.bLocallyMuted = bMuted;
    }
}

#pragma endregion
