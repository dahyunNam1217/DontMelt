
#pragma once

#include "CoreMinimal.h"
#include "Core/DMMatchTypes.h"
#include "GameFramework/OnlineReplStructs.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "TimerManager.h"

#include "DMVoiceChatSubsystem.generated.h"

class ADMMatchPlayerController;
class APlayerState;
class IOnlineVoice;
class USoundAttenuation;
class UVOIPTalker;
class UWorld;

/**
 * 원격 플레이어 한 명의 로컬 청취 상태.
 *
 * Talker 객체의 수명은 UDMVoiceChatSubsystem::RemoteTalkerObjects가 보장한다.
 */
struct FDMRemoteTalkerEntry
{
    TWeakObjectPtr<UVOIPTalker> Talker;

    // PlayerState가 먼저 파괴되어도 로컬 음소거를 해제할 수 있도록 ID를 보관한다.
    FUniqueNetIdRepl PlayerId;

    // 근접 규칙상 들리지 않아야 해서 로컬 음소거한 상태인지 나타낸다.
    bool bLocallyMuted = false;
};

// 로컬 음성 조작 상태가 변경되었음을 UI에 알린다.
DECLARE_MULTICAST_DELEGATE(FDMOnVoiceControlStateChanged);

UCLASS(Config = Game)
class DONTMELT_API UDMVoiceChatSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
#pragma region Lifecycle

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    virtual void Deinitialize() override;

#pragma endregion

#pragma region Voice Backend

    // Steam OSS 음성 인터페이스를 확인하고 로컬 Talker를 등록한다.
    bool InitializeLocalVoiceBackend();

    // Steam 음성 인터페이스와 Talker 등록 상태를 확인한다.
    bool IsBackendInitialized() const;

    // 실제 원격 음성 통신 성공을 의미하지 않는 준비 상태다.
    bool IsVoiceCommunicationReady() const;

    // PTT 모드에서 키를 누른 동안에만 마이크 송신을 허용한다.
    void SetPushToTalkPressed(bool bPressed);

    // 음성 감지 모드에서 M 입력마다 음소거를 전환한다.
    void ToggleMicrophoneMuted();

    // 음성 감지 모드를 활성화하거나 비활성화한다.
    UFUNCTION(BlueprintCallable, Category = "DM|Voice")
    void SetVoiceActivationEnabled(bool bEnabled);

    // 로컬 마이크 음소거를 변경한다.
    UFUNCTION(BlueprintCallable, Category = "DM|Voice")
    void SetMicrophoneMuted(bool bMuted);

    // 음성 감지 민감도에 사용할 임계값을 설정한다.
    UFUNCTION(BlueprintCallable, Category = "DM|Voice")
    void SetVoiceActivationThreshold(float Threshold);

    UFUNCTION(BlueprintPure, Category = "DM|Voice")
    bool IsNetworkVoiceTransmitting() const
    {
        return bNetworkVoiceTransmitting;
    }

    // 현재 음성 감지 모드인지 확인한다.
    UFUNCTION(BlueprintPure, Category = "DM|Voice")
    bool IsVoiceActivationEnabled() const
    {
        return bVoiceActivationEnabled;
    }

    // 로컬 마이크의 음소거 설정 상태를 확인한다.
    UFUNCTION(BlueprintPure, Category = "DM|Voice")
    bool IsMicrophoneMuted() const
    {
        return bMicrophoneMuted;
    }

    // 로컬 음성 제어 변경 알림을 반환한다.
    FDMOnVoiceControlStateChanged& OnVoiceControlStateChanged()
    {
        return VoiceControlStateChangedDelegate;
    }

#pragma endregion

#pragma region Voice Policy

    void EnterLobbyVoice();

    void BindMatchController(ADMMatchPlayerController* Controller);

    void UnbindMatchController(ADMMatchPlayerController* Controller);

    void LeaveVoiceWorld();

    EDMVoiceMode GetAppliedVoiceMode() const
    {
        return AppliedVoiceMode;
    }

#pragma endregion

private:
#pragma region Backend Internal

    // 기존 Steam OnlineSubsystem이 소유하는 인터페이스다.
    // Subsystem에서 직접 삭제하지 않는다.
    TSharedPtr<IOnlineVoice, ESPMode::ThreadSafe> OnlineVoice;

    bool bLocalTalkerRegistered = false;

    static constexpr uint8 LocalUserNum = 0;

    // 실제 음성 송신 허용 상태를 로컬에서만 관리한다.
    bool bNetworkVoiceTransmitting = false;

    // 현재 PTT 키 홀드 입력 상태다. 음성 정책이 해제되면 false로 복원한다.
    bool bPushToTalkLatched = false;

    // 기본값은 눌러서 말하기 모드다.
    bool bVoiceActivationEnabled = false;

    // 로컬 마이크 음소거 상태다.
    bool bMicrophoneMuted = false;

    // 현재 설정과 음성 정책을 기준으로 송신 허용 상태를 갱신한다.
    void UpdateNetworkVoiceTransmission();

    // 로컬 음성 조작 상태 변경 이벤트다.
    FDMOnVoiceControlStateChanged VoiceControlStateChangedDelegate;

    void ReleaseLocalVoiceBackend();

    void LogVoiceBackendState(const TCHAR* Context) const;

#pragma endregion

#pragma region Policy Internal

    void ApplyVoiceMode(EDMVoiceMode NewMode);

    void HandleMatchVoiceModeChanged(EDMVoiceMode NewMode);

    void ClearMatchControllerBinding();

    TWeakObjectPtr<ADMMatchPlayerController> BoundMatchController;

    FDelegateHandle MatchVoiceModeChangedHandle;

    EDMVoiceMode AppliedVoiceMode = EDMVoiceMode::Disabled;

#pragma endregion

#pragma region Remote Talker Policy

    /*
     * 청취 규칙을 로컬 청취자 기준으로 적용한다.
     *
     * - AliveProximity: 원격 플레이어 Pawn 위치 기준 거리 감쇠(근접 음성)
     *                   위치를 알 수 없는 원격 플레이어(Relevancy 밖, Pawn 없음)는 들리지 않는다.
     * - DeadGlobal:     모든 원격 플레이어를 거리 감쇠·방향성 없이 동일 음량으로 청취
     *
     * 송신 허용 여부는 기존 UpdateNetworkVoiceTransmission()이 담당한다.
     */

    // 근접 음성 최대 음량 반경(cm). DefaultGame.ini에서 조정한다.
    UPROPERTY(Config)
    float ProximityVoiceFullVolumeRadius = 300.0f;

    // 근접 음성 최대 청취 거리(cm). 이 거리 밖의 음성은 들리지 않는다.
    UPROPERTY(Config)
    float ProximityVoiceMaxDistance = 2500.0f;

    // 원격 Talker 청취 정책 갱신 주기(초).
    UPROPERTY(Config)
    float RemoteTalkerRefreshInterval = 0.25f;

    void StartRemoteTalkerRefresh();

    void StopRemoteTalkerRefresh();

    void RefreshRemoteTalkerPolicy();

    // 모든 원격 Talker를 해제하고 로컬 음소거를 원래대로 되돌린다.
    void ReleaseRemoteTalkers();

    USoundAttenuation* GetOrCreateProximityAttenuation();

    void SetRemoteTalkerLocallyMuted(FDMRemoteTalkerEntry& Entry, bool bMuted);

    // PlayerState별 로컬 청취 상태(조회용 약한 참조).
    TMap<TWeakObjectPtr<APlayerState>, FDMRemoteTalkerEntry> RemoteTalkers;

    // CreateTalkerForPlayer()로 만든 Talker는 다른 곳에서 강한 참조를 갖지 않으므로
    // GC되지 않도록 여기서 보관한다.
    UPROPERTY(Transient)
    TArray<TObjectPtr<UVOIPTalker>> RemoteTalkerObjects;

    UPROPERTY(Transient)
    TObjectPtr<USoundAttenuation> ProximityVoiceAttenuation;

    FTimerHandle RemoteTalkerRefreshTimer;

    TWeakObjectPtr<UWorld> RemoteTalkerRefreshWorld;

#pragma endregion
};
