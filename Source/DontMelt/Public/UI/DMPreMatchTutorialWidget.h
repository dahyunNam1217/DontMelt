#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "DMPreMatchTutorialWidget.generated.h"

class UButton;
class UTextBlock;

DECLARE_MULTICAST_DELEGATE(FDMOnPreMatchTutorialCompleted);

/** 각 플레이어 화면에만 표시되는 경기 시작 전 튜토리얼. */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMPreMatchTutorialWidget : public UUserWidget
{
    GENERATED_BODY()
public:
    UDMPreMatchTutorialWidget(const FObjectInitializer& ObjectInitializer);

    FDMOnPreMatchTutorialCompleted& OnTutorialCompleted() { return TutorialCompletedDelegate; }
    void MarkWaitingForOtherPlayers();
    /** 필수 위젯 연결이 실제로 유효한지 확인한다. */
    bool HasRequiredBindings() const;

protected:
    virtual void NativeOnInitialized() override;
    virtual void NativeDestruct() override;
    virtual FReply NativeOnMouseButtonDown(const FGeometry& Geometry, const FPointerEvent& MouseEvent) override;
    virtual FReply NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;
    virtual FReply NativeOnKeyUp(const FGeometry& Geometry, const FKeyEvent& KeyEvent) override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Previous = nullptr;
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Next = nullptr;
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UButton> BTN_Skip = nullptr;
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_PageTitle = nullptr;
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_PageDescription = nullptr;
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_PageIndex = nullptr;
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_NextButton = nullptr;
    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_Waiting = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "DM|Tutorial")
    TArray<FText> TutorialPageTitles;
    UPROPERTY(EditDefaultsOnly, Category = "DM|Tutorial")
    TArray<FText> TutorialPageDescriptions;
    UPROPERTY(EditDefaultsOnly, Category = "DM|Tutorial", meta = (ClampMin = "0.2", UIMin = "0.2"))
    float SkipHoldSeconds = 1.0f;

private:
    UFUNCTION() void HandleNextClicked();
    UFUNCTION() void HandlePreviousClicked();
    UFUNCTION() void HandleSkipClicked();
    void RefreshPage();
    void CompleteTutorial();
    void HandleSpaceSkipHoldComplete();

    int32 PageIndex = 0;
    bool bCompleted = false;
    FTimerHandle SpaceSkipTimer;
    FDMOnPreMatchTutorialCompleted TutorialCompletedDelegate;
};
