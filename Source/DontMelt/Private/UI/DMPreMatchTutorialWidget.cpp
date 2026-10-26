#include "UI/DMPreMatchTutorialWidget.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"
#include "Input/Reply.h"

UDMPreMatchTutorialWidget::UDMPreMatchTutorialWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Blueprint Class Defaults에서 페이지를 원하는 만큼 교체할 수 있다.
    TutorialPageTitles = {
        FText::FromString(TEXT("게임 목표")),
        FText::FromString(TEXT("유물 등록")),
        FText::FromString(TEXT("전투와 위장")),
        FText::FromString(TEXT("스킬과 이벤트"))
    };
    TutorialPageDescriptions = {
        FText::FromString(TEXT("NPC 사이에서 유물을 모아 자신의 상자에 등록하세요.")),
        FText::FromString(TEXT("F로 상자를 열고 스페이스바를 길게 눌러 등록합니다. Q는 유물 내려놓기입니다.")),
        FText::FromString(TEXT("NPC와 플레이어의 외형은 같습니다. 오인공격에는 불이익이 있습니다.")),
        FText::FromString(TEXT("스킬과 랜덤 이벤트를 활용하세요. 튜토리얼 완료 후 전원이 준비되면 시작합니다."))
    };
}

bool UDMPreMatchTutorialWidget::HasRequiredBindings() const
{
    return IsValid(BTN_Previous) && IsValid(BTN_Next) && IsValid(BTN_Skip)
        && IsValid(TXT_PageTitle) && IsValid(TXT_PageDescription) && IsValid(TXT_PageIndex);
}

void UDMPreMatchTutorialWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();
    if (IsValid(BTN_Previous)) BTN_Previous->OnClicked.AddDynamic(this, &ThisClass::HandlePreviousClicked);
    if (IsValid(BTN_Next)) BTN_Next->OnClicked.AddDynamic(this, &ThisClass::HandleNextClicked);
    if (IsValid(BTN_Skip)) BTN_Skip->OnClicked.AddDynamic(this, &ThisClass::HandleSkipClicked);
    RefreshPage();
}

void UDMPreMatchTutorialWidget::NativeDestruct()
{
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(SpaceSkipTimer);
    TutorialCompletedDelegate.Clear();
    Super::NativeDestruct();
}

void UDMPreMatchTutorialWidget::RefreshPage()
{
    const int32 Count = FMath::Min(TutorialPageTitles.Num(), TutorialPageDescriptions.Num());
    if (Count <= 0) return;
    PageIndex = FMath::Clamp(PageIndex, 0, Count - 1);
    if (IsValid(TXT_PageTitle)) TXT_PageTitle->SetText(TutorialPageTitles[PageIndex]);
    if (IsValid(TXT_PageDescription)) TXT_PageDescription->SetText(TutorialPageDescriptions[PageIndex]);
    if (IsValid(TXT_PageIndex)) TXT_PageIndex->SetText(FText::FromString(
        FString::Printf(TEXT("%d / %d"), PageIndex + 1, Count)));
    if (IsValid(TXT_NextButton)) TXT_NextButton->SetText(FText::FromString(
        PageIndex == Count - 1 ? TEXT("게임 시작") : TEXT("다음 페이지")));
    if (IsValid(BTN_Previous)) BTN_Previous->SetIsEnabled(!bCompleted && PageIndex > 0);
    if (IsValid(BTN_Next)) BTN_Next->SetIsEnabled(!bCompleted);
    if (IsValid(BTN_Skip)) BTN_Skip->SetIsEnabled(!bCompleted);
}

void UDMPreMatchTutorialWidget::HandlePreviousClicked()
{
    if (!bCompleted && PageIndex > 0) { --PageIndex; RefreshPage(); }
}

void UDMPreMatchTutorialWidget::HandleNextClicked()
{
    if (bCompleted) return;
    const int32 Count = FMath::Min(TutorialPageTitles.Num(), TutorialPageDescriptions.Num());
    if (PageIndex + 1 >= Count) CompleteTutorial();
    else { ++PageIndex; RefreshPage(); }
}

void UDMPreMatchTutorialWidget::HandleSkipClicked()
{
    CompleteTutorial();
}

void UDMPreMatchTutorialWidget::CompleteTutorial()
{
    if (bCompleted) return;
    bCompleted = true;
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(SpaceSkipTimer);
    RefreshPage();
    TutorialCompletedDelegate.Broadcast();
}

void UDMPreMatchTutorialWidget::MarkWaitingForOtherPlayers()
{
    const FText WaitingMessage = FText::FromString(TEXT("다른 플레이어의 튜토리얼 완료를 기다리는 중..."));
    if (IsValid(TXT_Waiting))
    {
        TXT_Waiting->SetVisibility(ESlateVisibility::Visible);
        TXT_Waiting->SetText(WaitingMessage);
    }
    else if (IsValid(TXT_PageDescription))
    {
        // 선택적 대기 안내 위젯이 없어도 완료 후 대기 상태를 확인할 수 있게 한다.
        TXT_PageDescription->SetText(WaitingMessage);
    }
}

FReply UDMPreMatchTutorialWidget::NativeOnMouseButtonDown(
    const FGeometry& Geometry, const FPointerEvent& MouseEvent)
{
    if (!bCompleted)
    {
        if (MouseEvent.GetEffectingButton() == EKeys::LeftMouseButton)
        { HandleNextClicked(); return FReply::Handled(); }
        if (MouseEvent.GetEffectingButton() == EKeys::RightMouseButton)
        { HandlePreviousClicked(); return FReply::Handled(); }
    }
    return Super::NativeOnMouseButtonDown(Geometry, MouseEvent);
}

FReply UDMPreMatchTutorialWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
    if (!bCompleted && KeyEvent.GetKey() == EKeys::SpaceBar)
    {
        if (UWorld* World = GetWorld(); World && !World->GetTimerManager().IsTimerActive(SpaceSkipTimer))
        {
            World->GetTimerManager().SetTimer(SpaceSkipTimer, this,
                &ThisClass::HandleSpaceSkipHoldComplete, FMath::Max(0.2f, SkipHoldSeconds), false);
        }
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(Geometry, KeyEvent);
}

FReply UDMPreMatchTutorialWidget::NativeOnKeyUp(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
    if (KeyEvent.GetKey() == EKeys::SpaceBar)
    {
        if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(SpaceSkipTimer);
        return FReply::Handled();
    }
    return Super::NativeOnKeyUp(Geometry, KeyEvent);
}

void UDMPreMatchTutorialWidget::HandleSpaceSkipHoldComplete()
{
    CompleteTutorial();
}
