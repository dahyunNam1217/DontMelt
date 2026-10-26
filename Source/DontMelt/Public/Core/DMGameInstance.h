#pragma once

#include "CoreMinimal.h"
#include "Core/DMMatchTypes.h"
#include "Engine/GameInstance.h"
#include "Net/Core/Connection/NetEnums.h"
#include "Containers/Ticker.h"
#include "Engine/EngineBaseTypes.h"
#include "TimerManager.h"

#include "DMGameInstance.generated.h"

class ADMMatchGameState;
class APlayerController;
class UDMConnectionLostWidget;
class UGameViewportClient;
class SWidget;
class ADMMatchPlayerState;
class ADMMatchPlayerController;
class UNetDriver;
class UWorld;
class USoundMix;
class USoundClass;
class USoundBase;
class UAudioComponent;
class ADMFrontendPlayerController;
class UDMOnlineSessionSubsystem;

/**
 * 게임 실행 동안 유지되는 전역 실행 흐름 관리자.
 *
 * 현재는 Client Network Failure를 감지하고
 * 기본 맵으로 복귀시키는 책임을 가진다.
 */
UCLASS()
class DONTMELT_API UDMGameInstance : public UGameInstance
{
    GENERATED_BODY()

    // 경기 결과와 로컬 참가자 정보의 전달 경로만 저장 함수를 호출한다.
    friend class ADMMatchGameState;
    friend class ADMMatchPlayerState;
    friend class ADMMatchPlayerController;

public:
#pragma region Lifecycle

    virtual void Init() override;
    virtual void Shutdown() override;

#pragma endregion

#pragma region Local Audio Settings

    void ApplyLocalAudioSettings(UWorld* TargetWorld);

    /** 로컬 월드의 매치 진행 단계에 따라 기본 음악을 전환한다. */
    void RefreshMusicForWorld(UWorld* TargetWorld);

#pragma endregion

#pragma region Last Match Result Queries

    // 저장된 최종 경기 결과가 있는지 확인한다.
    UFUNCTION(BlueprintPure, Category = "DM|Match|History")
    bool HasLastMatchResult() const
    {
        return LastMatchResult.EndReason != EDMMatchEndReason::None;
    }

    // 조회 시 복사본을 반환하므로 호출자가 보관 원본을 수정할 수 없다.
    UFUNCTION(BlueprintPure, Category = "DM|Match|History")
    FDMMatchResult GetLastMatchResult() const
    {
        return LastMatchResult;
    }

    // 지난 경기에서 자신의 진영이다. 확인되지 않았다면 INDEX_NONE이다.
    UFUNCTION(BlueprintPure, Category = "DM|Match|History")
    int32 GetLastMatchLocalBaseSlot() const
    {
        return LastMatchLocalBaseSlot;
    }

#pragma endregion

#pragma region Main Menu Navigation

    // 다음 메인 메뉴 생성 시 표시할 화면을 지정한다.
    void SetOpenMainMenuPanelOnNextEntry(bool bOpenMainMenuPanel);

    // 진입 요청을 한 번 읽고 초기화한다.
    bool ConsumeOpenMainMenuPanelOnNextEntry();

    // 메인 메뉴 컨트롤러와 UI가 준비된 뒤 복구 완료를 확정한다.
    void NotifyFrontendReady(const ADMFrontendPlayerController* FrontendController);

    // 사용자가 직접 방을 떠나는 경우 연결 종료 안내를 생략한다.
    void MarkIntentionalDisconnect(UWorld* SourceWorld);

    // GameMode에서 확인한 이동 실패도 같은 복구 경로로 처리한다.
    void StartTravelFailureRecovery(UWorld* SourceWorld, const FString& Diagnostic);

    // 종료 요청 중이거나 다른 Steam 작업 중이면 새 종료 요청을 막는다.
    bool CanBeginOnlineRoomExit();

    // 정상적인 로비·매치 퇴장 시 Steam 세션부터 해제한다.
    bool
    RequestLeaveOnlineRoom(UWorld* SourceWorld, const TSoftObjectPtr<UWorld>& MainMenuMap, bool bOpenMainMenuPanel);

#pragma endregion

private:
#pragma region Online Room Exit

    // 메뉴 복귀 대상과 세션 해제 순서를 관리한다.
    bool BeginOnlineRoomExit(
        UWorld* SourceWorld, const FString& DestinationPackage, bool bRecovery, bool bOpenMainMenuPanel
    );

    // Steam 세션 해제 완료 이벤트를 받는다.
    void HandleOnlineRoomDestroyCompleted(bool bWasSuccessful, const FString& Detail);

    // 플랫폼이 완료 이벤트를 반환하지 않는 경우에만 호출된다.
    void HandleOnlineRoomExitTimeout();

    // 모든 종료 경로를 한 번만 마무리한다.
    void FinishOnlineRoomExit(bool bSessionDestroyed, const FString& Detail);

    // 이 GameInstance가 등록한 Destroy 완료 이벤트를 해제한다.
    void UnbindOnlineRoomDestroy();

    bool bOnlineRoomExitInProgress = false;
    bool bOnlineRoomExitRecovery = false;
    bool bOnlineRoomExitFromClient = false;
    bool bOnlineRoomExitOpenMainMenu = false;

    FString OnlineRoomExitDestination;

    TWeakObjectPtr<UWorld> OnlineRoomExitSourceWorld;

    TWeakObjectPtr<UDMOnlineSessionSubsystem> OnlineRoomExitSession;

    FDelegateHandle OnlineRoomDestroyHandle;
    FTimerHandle OnlineRoomExitTimeoutHandle;

#pragma endregion

#pragma region Network Failure

    void HandleNetworkFailure(
        UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString
    );

    void HandlePostLoadMap(UWorld* LoadedWorld);

    // 오류 안내 중에만 일정 간격으로 호출된다.
    bool TickNetworkFailureNotice(float DeltaTime);

    // 현재 Viewport에 안내를 표시하고 입력을 차단한다.
    void RefreshNetworkFailurePresentation(bool bReattach = false);

    // 안내와 임시 입력 차단을 정리한다.
    void ClearNetworkFailurePresentation();

    void HandleTravelFailure(UWorld* World, ETravelFailure::Type FailureType, const FString& ErrorString);

    // 네트워크 종료와 이동 실패가 공유하는 안내 시작 함수다.
    void StartLocalFailureRecovery(UWorld* SourceWorld, const FText& Message);

    // 기본 맵으로의 대체 이동을 한 번 요청한다.
    void RequestRecoveryTravel();

    // 기본 맵 복구도 실패하면 자동 이동을 중단한다.
    void ShowRecoveryFailedNotice();

    // 메인 메뉴 도착과 안내 종료가 확인되면 상태를 정리한다.
    void FinishLocalFailureRecovery();

    void HandleConnectedNetworkFailure(
        UWorld* World, UNetDriver* NetDriver, ENetworkFailure::Type FailureType, const FString& ErrorString
    );

    // 자신의 PendingNetGame 접속 실패다.
    void HandlePendingConnectionFailure(ENetworkFailure::Type FailureType, const FString& ErrorString);

    bool IsOwnPendingNetDriver(const UNetDriver* NetDriver) const;

#pragma endregion

#pragma region Internal Helpers

    static const TCHAR* GetNetworkFailureName(ENetworkFailure::Type FailureType);

#pragma endregion

#pragma region Delegate Handles

    FDelegateHandle NetworkFailureDelegateHandle;
    FDelegateHandle PostLoadMapDelegateHandle;
    FDelegateHandle TravelFailureDelegateHandle;

#pragma endregion

#pragma region Runtime State

    // 연결 종료와 이동 실패의 공통 복구 진행 상태다.
    bool bHandlingNetworkFailure = false;

    // 기본 맵 복구의 반복 요청을 막는다.
    bool bRecoveryTravelRequested = false;

    // 레벨 이동 후 사용할 로컬 화면 이동 정보다. 복제하지 않는다.
    bool bOpenMainMenuPanelOnNextEntry = false;

    // 접속 준비 중인 NetDriver의 주소다. 비교에만 사용하고 역참조하지 않는다.
    const void* TrackedPendingNetDriverKey = nullptr;

    FTSTicker::FDelegateHandle PendingNetDriverTrackerHandle;

    bool TickTrackPendingNetDriver(float DeltaTime);

#pragma endregion

#pragma region Network Failure Presentation

    UPROPERTY(EditDefaultsOnly, Category = "DM|Network|UI")
    TSubclassOf<UDMConnectionLostWidget> ConnectionLostWidgetClass;

    UPROPERTY(Transient)
    TObjectPtr<UDMConnectionLostWidget> ConnectionLostWidget = nullptr;

    // 월드의 게임 UI 계층과 분리하여 Viewport에 부착한다.
    TSharedPtr<SWidget> ConnectionLostSlateWidget;

    TWeakObjectPtr<UGameViewportClient> ConnectionLostViewport;

    TWeakObjectPtr<APlayerController> ConnectionLostInputController;

    // 이전 월드의 제거를 막지 않는 약한 참조다.
    TWeakObjectPtr<UWorld> NetworkFailureSourceWorld;

    TWeakObjectPtr<UWorld> IntentionalDisconnectWorld;

    FTSTicker::FDelegateHandle NetworkFailureTickerHandle;

    FText NetworkFailureNoticeText;

    double NetworkFailureNoticeEndTime = 0.0;

    bool bNetworkFailureNoticeActive = false;

    // 안내를 띄우기 전 Viewport의 입력 차단 상태다.
    bool bViewportPreviouslyIgnoredInput = false;

#pragma endregion

#pragma region Last Match Result Storage

    // 보관에 성공했거나 같은 결과를 이미 보관했다면 true다.
    bool CacheFinalMatchResult(const ADMMatchGameState* SourceGameState);

    // 현재 월드의 로컬 참가자 진영을 확인한다.
    void RefreshLocalMatchIdentity(UWorld* SourceWorld);

    // 이전 월드의 Actor를 참조하지 않는 결과 값 복사본이다.
    UPROPERTY(Transient)
    FDMMatchResult LastMatchResult;

    UPROPERTY(Transient)
    int32 LastMatchLocalBaseSlot = INDEX_NONE;

    // 저장된 결과가 어느 월드에서 왔는지 구분한다.
    // 약한 참조이므로 이전 월드의 제거를 막지 않는다.
    TWeakObjectPtr<UWorld> LastMatchResultWorld;

    // 현재 경기에서 확인한 로컬 참가자 정보다.
    // 지난 경기의 참가자 정보와 분리하여 관리한다.
    TWeakObjectPtr<UWorld> LocalMatchIdentityWorld;

    int32 LocalMatchBaseSlot = INDEX_NONE;

#pragma endregion

#pragma region Local Music

    /** 비게임플레이 화면에서 반복 재생할 메인 음악. */
    UPROPERTY(EditDefaultsOnly, Category = "DM|Settings|Music")
    TObjectPtr<USoundBase> MainThemeSound = nullptr;

    /** 정규 경기와 연장전에서 반복 재생할 음악. */
    UPROPERTY(EditDefaultsOnly, Category = "DM|Settings|Music")
    TObjectPtr<USoundBase> MatchBackgroundSound = nullptr;

    /** 정규 경기 종료까지 남은 시간이 이 값 이하일 때 긴급 음악으로 전환한다. */
    UPROPERTY(EditDefaultsOnly, Category = "DM|Settings|Music", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float UrgentMusicLeadSeconds = 300.0f;

    /** 정규 경기 종료 임박 및 연장전에 사용할 음악. */
    UPROPERTY(EditDefaultsOnly, Category = "DM|Settings|Music")
    TObjectPtr<USoundBase> UrgentMatchSound = nullptr;

    /** 정전 이벤트 진행 중 사용할 음악. */
    UPROPERTY(EditDefaultsOnly, Category = "DM|Settings|Music")
    TObjectPtr<USoundBase> BlackoutEventSound = nullptr;

    /** NPC 패닉 이벤트 진행 중 사용할 음악. */
    UPROPERTY(EditDefaultsOnly, Category = "DM|Settings|Music")
    TObjectPtr<USoundBase> PanicEventSound = nullptr;

    /** 입력 반전 이벤트 진행 중 사용할 음악. */
    UPROPERTY(EditDefaultsOnly, Category = "DM|Settings|Music")
    TObjectPtr<USoundBase> InputReverseEventSound = nullptr;

    /** 음악 전환 시 사용되는 페이드 시간(초). */
    UPROPERTY(EditDefaultsOnly, Category = "DM|Settings|Music", meta = (ClampMin = "0.0", ClampMax = "5.0"))
    float MusicFadeDuration = 0.75f;

    /** 맵 이동을 넘어 유지되는 메인 음악 재생 컴포넌트. */
    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> MainThemeAudioComponent = nullptr;

    /** 맵 이동을 넘어 유지되는 경기 음악 재생 컴포넌트. */
    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> MatchBackgroundAudioComponent = nullptr;

    /** 긴급 경기 음악용 로컬 재생 컴포넌트. */
    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> UrgentMatchAudioComponent = nullptr;

    /** 랜덤 이벤트별 로컬 재생 컴포넌트. */
    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> BlackoutEventAudioComponent = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> PanicEventAudioComponent = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> InputReverseEventAudioComponent = nullptr;

    /** 현재 선택된 로컬 음악 컴포넌트. 게임플레이 상태가 아니다. */
    UPROPERTY(Transient)
    TObjectPtr<UAudioComponent> ActiveMusicAudioComponent = nullptr;

    /** 미래의 음악 전환 시각에만 실행되는 로컬 단발 Timer. */
    FTimerHandle MusicTransitionTimerHandle;
    TWeakObjectPtr<UWorld> MusicScheduledWorld;

    void HandleScheduledMusicTransition();
    void ClearScheduledMusicTransition();

    /** 게임 종료 시 로컬 재생 컴포넌트와 예약된 전환을 정리한다. */
    void StopAndReleaseMusic();

#pragma endregion

#pragma region Local Audio Configuration

    UPROPERTY(EditDefaultsOnly, Category = "DM|Settings|Audio")
    TObjectPtr<USoundMix> SettingsSoundMix = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "DM|Settings|Audio")
    TObjectPtr<USoundClass> SettingsMasterClass = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "DM|Settings|Audio")
    TObjectPtr<USoundClass> SettingsSFXClass = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "DM|Settings|Audio")
    TObjectPtr<USoundClass> SettingsMusicClass = nullptr;

    TWeakObjectPtr<UWorld> AudioAppliedWorld;

#pragma endregion
};
