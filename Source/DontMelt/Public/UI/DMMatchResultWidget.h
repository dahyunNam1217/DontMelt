#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Core/DMMatchTypes.h"
#include "TimerManager.h"

#include "DMMatchResultWidget.generated.h"

class UButton;
class UTextBlock;
class UVerticalBox;
class UWidget;
class UDMPlayerResultRowWidget;
class ADMMatchGameState;
class ADMMatchPlayerState;

/**
 * 서버가 확정한 경기 결과와 재경기 준비 상태를 표시한다.
 *
 * 통계와 등수는 GameState의 결과 스냅샷을 그대로 사용한다.
 * 버튼 요청은 소유 PlayerController의 Server RPC로 전달한다.
 */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMMatchResultWidget : public UUserWidget
{
    GENERATED_BODY()

public:
#pragma region Data Source

    void SetDataSources(ADMMatchGameState* InGameState, ADMMatchPlayerState* InPlayerState);

    void SetPresentationActive(bool bActive);

#pragma endregion

protected:
#pragma region Lifecycle

    virtual void NativeConstruct() override;

    virtual void NativeDestruct() override;

#pragma endregion

#pragma region Widget References

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ResultTitle = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_ResultDetail = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UWidget> ResultScoreboardPanel = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UVerticalBox> PlayerResultRows = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_AutoReturnCountdown = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_ReturnToLobby = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_QuitGame = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_RematchReady = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_RestartMatch = nullptr;

    // 준비 버튼 내부의 준비·준비 취소 문구다.
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_RematchReady = nullptr;

    // 참가자별 준비 상태를 표시한다.
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_RematchReadyStatus = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "DM|UI|Result")
    TSubclassOf<UDMPlayerResultRowWidget> PlayerResultRowClass;

#pragma endregion

private:
#pragma region Data Binding

    void HandlePhaseStateChanged(ADMMatchGameState* Source, FDMMatchPhaseState NewState);

    void HandleMatchResultChanged(ADMMatchGameState* Source, FDMMatchResult NewResult);

    void HandleBaseSlotChanged(ADMMatchPlayerState* Source, int32 NewBaseSlot);

    void HandleRematchReadyChanged(ADMMatchPlayerState* Source, bool bNewReady);

    // 참가자 목록이 바뀐 경우에만 준비 상태 구독을 다시 구성한다.
    bool RefreshReadySources();

    void ClearReadySources();

#pragma endregion

#pragma region Button Actions

    UFUNCTION()
    void OnReturnToLobbyClicked();

    UFUNCTION()
    void OnQuitGameClicked();

    UFUNCTION()
    void OnRematchReadyClicked();

    UFUNCTION()
    void OnRestartMatchClicked();

#pragma endregion

#pragma region Presentation

    void RefreshPresentation();

    void RebuildPlayerRows(const FDMMatchResult& Result);

    void RefreshActionButtons();

#pragma endregion

#pragma region Presentation Timer

    void StartRefreshTimer();

    void StopRefreshTimer();

    void HandleRefreshTimer();

    FTimerHandle RefreshTimerHandle;

    // 같은 초의 문구를 반복해서 설정하지 않는다.
    int32 LastRemainingSeconds = INDEX_NONE;

#pragma endregion

#pragma region Local References

    TWeakObjectPtr<ADMMatchGameState> BoundGameState;

    TWeakObjectPtr<ADMMatchPlayerState> BoundPlayerState;

    TArray<TWeakObjectPtr<ADMMatchPlayerState>> ReadyPlayerStates;

    bool bPresentationActive = false;

    bool bScoreboardDirty = true;

    // 같은 결과 화면에서 로컬 PlayerState 참조가 사라지는 경우에 사용한다.
    int32 CachedLocalBaseSlot = INDEX_NONE;

#pragma endregion
};