#pragma once

#include "CoreMinimal.h"
#include "GameFramework/PlayerController.h"
#include "TimerManager.h"

#include "DMLobbyPlayerController.generated.h"

class UDMLobbyWidget;
class UWorld;
class UDMInputConfigDataAsset;

/**
 * 로비 UI와 로비 요청의 소유 클라이언트 경계.
 *
 * 게임 데이터를 저장하지 않으며,
 * 로컬 입력 의도를 서버로 전달한다.
 */
UCLASS()
class DONTMELT_API ADMLobbyPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    UFUNCTION(BlueprintCallable, Category = "DM|Lobby|Requests")
    void RequestSetReady(bool bNewReady);

    UFUNCTION(BlueprintCallable, Category = "DM|Lobby|Requests")
    void RequestStartMatch();

    UFUNCTION(Exec)
    void DMReady();

    UFUNCTION(Exec)
    void DMUnready();

    UFUNCTION(Exec)
    void DMStartMatch();

    UFUNCTION(BlueprintCallable, Category = "DM|Lobby|Requests")
    void LeaveLobby();

protected:
#pragma region Lifecycle

    virtual void SetupInputComponent() override;

    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual void OnRep_PlayerState() override;

    /**
     * 엔진 기본 구현(ToggleSpeaking)은 GameSession::bRequiresPushToTalk 값으로
     * 네트워크 음성 전송을 강제로 켜고 끄므로 UDMVoiceChatSubsystem의 전송 상태와 어긋난다.
     * 음성 전송 판단은 VoiceChatSubsystem에만 맡긴다.
     */
    virtual void ClientEnableNetworkVoice_Implementation(bool bEnable) override;

#pragma endregion

private:
    UFUNCTION(Server, Reliable)
    void ServerSetReady(bool bNewReady);

    UFUNCTION(Server, Reliable)
    void ServerRequestStartMatch();

#pragma region UI

    UPROPERTY(EditDefaultsOnly, Category = "DM|Lobby|UI")
    TSubclassOf<UDMLobbyWidget> LobbyWidgetClass;

    UPROPERTY(Transient)
    TObjectPtr<UDMLobbyWidget> LobbyWidget = nullptr;

#pragma endregion

#pragma region Lobby Initialization

    // 초기 GameState 복제가 늦어지는 경우에만 재시도한다.
    void ScheduleLobbyGameStateRetry();

    // GameState 연결을 다시 시도한다.
    void RetryLobbyGameStateBinding();

    // 로비 UI 초기화 중에만 사용하는 로컬 타이머다.
    FTimerHandle LobbyGameStateRetryTimer;

    // 무한 재시도를 방지하기 위한 로컬 횟수다.
    int32 LobbyGameStateRetryAttempts = 0;

#pragma endregion

#pragma region Map Configuration

    UPROPERTY(EditDefaultsOnly, Category = "DM|Lobby|Maps")
    TSoftObjectPtr<UWorld> MainMenuMap;

#pragma endregion

#pragma region Voice Input

    UPROPERTY(EditDefaultsOnly, Category = "DM|Voice|Input")
    TObjectPtr<UDMInputConfigDataAsset> VoiceInputConfig = nullptr;

    // V 키 PTT 홀드 입력이다.
    void InputPushToTalkStarted();
    void InputPushToTalkStopped();

    // M 키 마이크 음소거 토글 입력이다.
    void InputToggleMicrophoneMute();

#pragma endregion
};
