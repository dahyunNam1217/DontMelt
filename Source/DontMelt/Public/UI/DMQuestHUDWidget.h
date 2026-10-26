
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "Quests/DMQuestTypes.h"

#include "DMQuestHUDWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UDataTable;

class ADMMatchGameState;
class ADMMatchPlayerState;
class UDMQuestProgressComponent;

/**
 * 현재 퀘스트와 본인의 진행 상태를 표시하는 HUD.
 *
 * 퀘스트 발생 및 완료 판정은 수행하지 않는다.
 * GameState와 QuestProgressComponent의 기존 데이터를 읽는다.
 */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMQuestHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
#pragma region Data Source

    /** 현재 매치와 자신의 QuestProgress 원본을 연결한다. */
    void SetDataSources(ADMMatchGameState* InGameState, ADMMatchPlayerState* InPlayerState);

    /** 부모 Root의 표시 가능 상태를 전달받는다. */
    void SetPresentationActive(bool bActive);

#pragma endregion

protected:
#pragma region Lifecycle

    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

#pragma endregion

#pragma region Configuration

    /**
     * 서버 GameMode와 동일한 DT_QuestDefinitions 에셋을 연결한다.
     * 클라이언트에서는 표시 정보만 조회한다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|UI|Quest")
    TObjectPtr<UDataTable> QuestDefinitionTable = nullptr;

#pragma endregion

#pragma region Widget References

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_QuestName = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_QuestDescription = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_QuestTime = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_QuestProgress = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_QuestStatus = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> PB_QuestProgress = nullptr;

#pragma endregion

private:
#pragma region Data Binding

    void HandleQuestPublicStateChanged(ADMMatchGameState* Source, FDMQuestRuntimeState NewState);

    void HandleQuestProgressChanged(UDMQuestProgressComponent* Source, FDMQuestPlayerProgress NewProgress);

    void RefreshPresentation();

    const FDMQuestDefinitionRow* FindQuestDefinition(const FGameplayTag& QuestTag);

    /** 현재 퀘스트의 DataTable 행 검색 결과만 보관한다. */
    FGameplayTag CachedDefinitionQuestTag;

    FName CachedDefinitionRowName = NAME_None;

#pragma endregion

#pragma region Presentation Timer

    void StopRefreshTimer();

    TWeakObjectPtr<ADMMatchGameState> BoundGameState;

    TWeakObjectPtr<UDMQuestProgressComponent> BoundQuestProgressComponent;

    FTimerHandle RefreshTimerHandle;

    // 퀘스트의 성공 여부가 아니라 UI 표시 활성화 여부만 저장한다.
    bool bPresentationActive = false;

#pragma endregion
};
