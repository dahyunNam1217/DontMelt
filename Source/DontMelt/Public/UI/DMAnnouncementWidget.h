
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "Quests/DMQuestTypes.h"
#include "Events/DMRandomEventTypes.h"

#include "DMAnnouncementWidget.generated.h"

class UTextBlock;
class ADMMatchPlayerController;
class ADMMatchGameState;
class ADMMatchPlayerState;
class UDMQuestProgressComponent;

/**
 * 퀘스트·랜덤 이벤트·유물 도난·스킬 경고를
 * 하나의 화면 중앙 알림으로 표시한다.
 *
 * 알림은 표시 전용이며 Gameplay 상태를 변경하지 않는다.
 */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMAnnouncementWidget : public UUserWidget
{
    GENERATED_BODY()

public:
#pragma region Data Source

    void SetDataSources(
        ADMMatchPlayerController* InController, ADMMatchGameState* InGameState, ADMMatchPlayerState* InPlayerState
    );

    void SetPresentationActive(bool bActive);

#pragma endregion

protected:
    virtual void NativeDestruct() override;

#pragma region Configuration

    /** 알림 한 건을 표시하는 기본 시간 */
    UPROPERTY(EditDefaultsOnly, Category = "DM|UI|Announcement", meta = (ClampMin = "0.1"))
    float MessageDisplayDuration = 2.5f;

#pragma endregion

#pragma region Widget References

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_Announcement = nullptr;

#pragma endregion

private:
#pragma region Event Handlers

    void HandleQuestPublicStateChanged(ADMMatchGameState* Source, FDMQuestRuntimeState NewState);

    void HandleQuestProgressChanged(UDMQuestProgressComponent* Source, FDMQuestPlayerProgress NewProgress);

    void HandleRandomEventPublicStateChanged(ADMMatchGameState* Source, FDMRandomEventRuntimeState NewState);

    void HandleRelicStolen();

#pragma endregion

#pragma region Presentation

    void ShowMessage(const FText& Message);
    void HideMessage();

    void StartEventWatchIfNeeded();
    void HandleEventStartTimeReached();
    void StopEventWatch();

    void StopMessageTimer();

    TWeakObjectPtr<ADMMatchPlayerController> BoundController;

    TWeakObjectPtr<ADMMatchGameState> BoundGameState;

    TWeakObjectPtr<UDMQuestProgressComponent> BoundQuestProgress;

    FTimerHandle MessageTimerHandle;
    FTimerHandle EventStartTimerHandle;

    // 중복 알림을 방지하는 로컬 표시 이력이다.
    int32 LastQuestRunId = 0;
    bool bHadQuest = false;

    int32 LastProgressRunId = 0;

    EDMQuestParticipantState LastParticipantState = EDMQuestParticipantState::Inactive;

    double LastEventStartServerTime = 0.0;
    double LastEventEndServerTime = 0.0;
    bool bHadEvent = false;

    bool bPresentationActive = false;

#pragma endregion
};
