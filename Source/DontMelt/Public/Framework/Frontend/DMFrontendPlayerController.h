
#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"

#include "DMFrontendPlayerController.generated.h"

class UDMMainMenuWidget;
class UDMMatchRuleDataAsset;
class UDMOnlineSessionSubsystem;
class UWorld;

/**
 * Title / Main Menu 전용 PlayerController.
 *
 * 로컬 메뉴 생성, Listen Server 로비 생성,
 * IP 직접 접속 및 게임 종료를 담당한다.
 *
 * 로비 내부의 Ready / Match Start 기능은
 * 기존 ADMLobbyPlayerController가 담당한다.
 */
UCLASS()
class DONTMELT_API ADMFrontendPlayerController : public APlayerController
{
    GENERATED_BODY()

protected:
#pragma region Lifecycle

    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#pragma endregion

public:
#pragma region Menu Actions

    bool HostLobby();

    bool JoinLobbyByAddress(const FString& RawAddress);

    // Steam에서 Don't Melt 로비 목록을 검색한다.
    bool FindSteamLobbies();

    // 표시된 검색 결과 인덱스로 Steam 로비에 참가한다.
    bool JoinSteamLobby(int32 ResultIndex);

    void QuitGame();

    // 오류 안내가 끝난 뒤 메인 메뉴 화면과 입력·포커스를 복구한다.
    void RestoreMainMenuAfterNotice();

#pragma endregion

private:
#pragma region Validation

    static bool IsValidDirectJoinAddress(const FString& Address);

#pragma endregion

#pragma region Steam Hosting

    // 이번 Controller가 요청한 세션 생성의 완료를 처리한다.
    void HandleHostSessionCreated(bool bWasSuccessful, const FString& Detail);

    // 다른 맵으로 이동하거나 종료될 때 완료 이벤트를 해제한다.
    void UnbindHostSessionCompletion();

    FDelegateHandle HostSessionCreateHandle;

    TWeakObjectPtr<UDMOnlineSessionSubsystem> BoundHostSession;

    // 세션 전체 상태가 아닌 이번 호스트 요청의 로컬 대기 상태다.
    bool bHostSessionRequestPending = false;

#pragma endregion

#pragma region Steam Joining

    // Frontend에서 시작한 검색·참가·실패 정리 요청만 구분한다.
    enum class ESteamJoinOperation : uint8
    {
        None,
        Find,
        Join,
        Cleanup
    };

    ESteamJoinOperation SteamJoinOperation = ESteamJoinOperation::None;

    TWeakObjectPtr<UDMOnlineSessionSubsystem> BoundSteamJoinSession;

    FDelegateHandle SteamJoinDelegateHandle;

    // 참가 실패 시 정리 완료 후 표시할 메시지다.
    FString PendingJoinFailureMessage;

    void HandleSteamFindCompleted(bool bWasSuccessful, const FString& Detail);

    void HandleSteamJoinCompleted(bool bWasSuccessful, const FString& Detail);

    void HandleSteamJoinCleanupCompleted(bool bWasSuccessful, const FString& Detail);

    // 참가 실패 후 남아 있을 수 있는 로컬 세션을 정리한다.
    void StartSteamJoinFailureCleanup(const FString& FailureMessage);

    // 이번 Controller가 등록한 완료 Delegate만 해제한다.
    void UnbindSteamJoinCompletion();

#pragma endregion

#pragma region Stale Session Cleanup

    // 메인 메뉴에 남은 이전 접속 실패의 GameSession을 정리한다.
    // 정리를 요청했거나 아직 정리 중이면 true를 반환한다.
    bool CleanupStaleLobbySession();

#pragma endregion

#pragma region UI Configuration

    UPROPERTY(EditDefaultsOnly, Category = "DM|Frontend|UI")
    TSubclassOf<UDMMainMenuWidget> MainMenuWidgetClass;

    UPROPERTY(Transient)
    TObjectPtr<UDMMainMenuWidget> MainMenuWidget = nullptr;

#pragma endregion

#pragma region Map Configuration

    UPROPERTY(EditDefaultsOnly, Category = "DM|Frontend|Maps")
    TSoftObjectPtr<UWorld> LobbyMap;

    // Lobby GameMode와 동일한 규칙 에셋을 참조한다.
    UPROPERTY(EditDefaultsOnly, Category = "DM|Frontend|Rules")
    TObjectPtr<UDMMatchRuleDataAsset> LobbyMatchRuleData = nullptr;

#pragma endregion
};
