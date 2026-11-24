#include "UI/DMPreMatchTutorialWidget.h"
#include "Core/DMGameInstance.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Engine/World.h"
#include "InputCoreTypes.h"
#include "Input/Events.h"
#include "Input/Reply.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameViewportClient.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "Framework/Application/SlateUser.h"
#include "Layout/WidgetPath.h"
#include "Widgets/SViewport.h"
#include "Widgets/SWindow.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMTutorialFocus, Log, All);

UDMPreMatchTutorialWidget::UDMPreMatchTutorialWidget(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    // Blueprint Class Defaults에서 페이지를 원하는 만큼 교체할 수 있다.
    TutorialPageTitles = {
        NSLOCTEXT("DMTutorial", "GoalTitle", "게임 목표"),
        NSLOCTEXT("DMTutorial", "RelicTitle", "유물 등록"),
        NSLOCTEXT("DMTutorial", "CombatTitle", "전투와 위장"),
        NSLOCTEXT("DMTutorial", "SkillTitle", "스킬과 이벤트")
    };
    TutorialPageDescriptions = {
        NSLOCTEXT("DMTutorial", "GoalDescription", "NPC 사이에서 유물을 모아 자신의 상자에 등록하세요."),
        NSLOCTEXT("DMTutorial", "RelicDescription", "F로 상자를 열고 스페이스바를 길게 눌러 등록합니다. Q는 유물 내려놓기입니다."),
        NSLOCTEXT("DMTutorial", "CombatDescription", "NPC와 플레이어의 외형은 같습니다. 오인공격에는 불이익이 있습니다."),
        NSLOCTEXT("DMTutorial", "SkillDescription", "스킬과 랜덤 이벤트를 활용하세요. 튜토리얼 완료 후 전원이 준비되면 시작합니다.")
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

    if (UDMGameInstance* GameInstance = Cast<UDMGameInstance>(GetGameInstance()))
    {
        GameInstance->BindCommonUIClickSound(this);
    }
    if (IsValid(BTN_Previous)) BTN_Previous->OnClicked.AddDynamic(this, &ThisClass::HandlePreviousClicked);
    if (IsValid(BTN_Next)) BTN_Next->OnClicked.AddDynamic(this, &ThisClass::HandleNextClicked);
    if (IsValid(BTN_Skip)) BTN_Skip->OnClicked.AddDynamic(this, &ThisClass::HandleSkipClicked);
    RefreshPage();
}

void UDMPreMatchTutorialWidget::NativeDestruct()
{
    StopTutorialFocusTracking();
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
    if (IsValid(TXT_PageIndex)) TXT_PageIndex->SetText(FText::Format(
        NSLOCTEXT("DMTutorial", "PageIndexFormat", "{0} / {1}"),
        FText::AsNumber(PageIndex + 1), FText::AsNumber(Count)));
    if (IsValid(TXT_NextButton)) TXT_NextButton->SetText(
        PageIndex == Count - 1
            ? NSLOCTEXT("DMTutorial", "StartGame", "게임 시작")
            : NSLOCTEXT("DMTutorial", "NextPage", "다음 페이지"));
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
    CancelSpaceHold(TEXT("Completed"));
    UE_LOG(LogDMTutorialFocus, Log, TEXT("Tutorial completed once. Owner=%s"), *GetNameSafe(GetOwningPlayer()));
    RefreshPage();
    TutorialCompletedDelegate.Broadcast();
}

void UDMPreMatchTutorialWidget::MarkWaitingForOtherPlayers()
{
    CancelSpaceHold(TEXT("Waiting"));
    const FText WaitingMessage = NSLOCTEXT(
        "DMTutorial", "WaitingForOthers", "다른 플레이어의 튜토리얼 완료를 기다리는 중..."
    );
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

FReply UDMPreMatchTutorialWidget::NativeOnPreviewKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
    // 자식 버튼이 Space를 클릭 입력으로 소비하기 전에 튜토리얼 홀드를 처리한다.
    if (KeyEvent.GetKey() == EKeys::SpaceBar)
    {
        return NativeOnKeyDown(Geometry, KeyEvent);
    }
    return Super::NativeOnPreviewKeyDown(Geometry, KeyEvent);
}

FReply UDMPreMatchTutorialWidget::NativeOnKeyDown(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
    if (KeyEvent.GetKey() == EKeys::SpaceBar && IsFocusTrackingValid())
    {
        // 포커스 상실 후 놓침 이벤트를 못 받았어도 자동 반복으로 홀드를 재시작하지 않는다.
        if (!bCompleted && !KeyEvent.IsRepeat() && !bSpaceHeld && IsOwningWindowActive() && HasTutorialUserFocus())
        {
            bSpaceHeld = true;
            GetWorld()->GetTimerManager().SetTimer(SpaceSkipTimer, this,
                &ThisClass::HandleSpaceSkipHoldComplete, FMath::Max(0.2f, SkipHoldSeconds), false);
            UE_LOG(LogDMTutorialFocus, Log, TEXT("Space hold started. Owner=%s"), *GetNameSafe(GetOwningPlayer()));
        }
        return FReply::Handled();
    }
    return Super::NativeOnKeyDown(Geometry, KeyEvent);
}

FReply UDMPreMatchTutorialWidget::NativeOnKeyUp(const FGeometry& Geometry, const FKeyEvent& KeyEvent)
{
    if (KeyEvent.GetKey() == EKeys::SpaceBar)
    {
        UE_LOG(LogDMTutorialFocus, Log, TEXT("Space key released. Owner=%s"), *GetNameSafe(GetOwningPlayer()));
        CancelSpaceHold(TEXT("KeyUp"));
        return FReply::Handled();
    }
    return Super::NativeOnKeyUp(Geometry, KeyEvent);
}

void UDMPreMatchTutorialWidget::CancelSpaceHold(const TCHAR* Reason)
{
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(SpaceSkipTimer);
    if (bSpaceHeld)
    {
        UE_LOG(LogDMTutorialFocus, Log, TEXT("Space hold canceled. Owner=%s Reason=%s"), *GetNameSafe(GetOwningPlayer()), Reason);
    }
    bSpaceHeld = false;
}

void UDMPreMatchTutorialWidget::HandleSpaceSkipHoldComplete()
{
    if (bSpaceHeld && IsFocusTrackingValid() && IsOwningWindowActive() && HasTutorialUserFocus())
    {
        CompleteTutorial();
    }
    else
    {
        CancelSpaceHold(TEXT("ExpiredWithoutFocus"));
    }
}

bool UDMPreMatchTutorialWidget::IsFocusTrackingValid() const
{
    const APlayerController* Owner = GetOwningPlayer();
    return bFocusTrackingActive && IsInViewport() && GetCachedWidget().IsValid()
        && IsValid(GetWorld()) && IsValid(Owner) && Owner->IsLocalController() && FSlateApplication::IsInitialized();
}

bool UDMPreMatchTutorialWidget::IsOwningWindowActive() const
{
    if (!IsFocusTrackingValid()) return false;
    const TSharedPtr<SWindow> Window = FSlateApplication::Get().FindWidgetWindow(GetCachedWidget().ToSharedRef());
    return Window.IsValid() && FSlateApplication::Get().IsActive()
        && Window == FSlateApplication::Get().GetActiveTopLevelWindow();
}

bool UDMPreMatchTutorialWidget::HasTutorialUserFocus() const
{
    return HasUserFocus(GetOwningPlayer()) || HasUserFocusedDescendants(GetOwningPlayer());
}

void UDMPreMatchTutorialWidget::StartTutorialFocusTracking()
{
    if (bFocusTrackingActive || !FSlateApplication::IsInitialized()) return;
    bFocusTrackingActive = true;
    if (!IsFocusTrackingValid())
    {
        bFocusTrackingActive = false;
        return;
    }
    FocusChangingHandle = FSlateApplication::Get().OnFocusChanging().AddUObject(this, &ThisClass::HandleSlateFocusChanging);
    ApplicationActivationHandle = FSlateApplication::Get().OnApplicationActivationStateChanged().AddUObject(
        this, &ThisClass::HandleApplicationActivationChanged);
    if (IsOwningWindowActive()) SetUserFocus(GetOwningPlayer());
}

void UDMPreMatchTutorialWidget::StopTutorialFocusTracking()
{
    bFocusTrackingActive = false;
    CancelSpaceHold(TEXT("Closed"));
    if (UWorld* World = GetWorld()) World->GetTimerManager().ClearTimer(FocusRestoreTimer);
    PendingViewportFocus.Reset();
    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().OnFocusChanging().Remove(FocusChangingHandle);
        FSlateApplication::Get().OnApplicationActivationStateChanged().Remove(ApplicationActivationHandle);
    }
    FocusChangingHandle.Reset();
    ApplicationActivationHandle.Reset();
}

void UDMPreMatchTutorialWidget::HandleSlateFocusChanging(const FFocusEvent& Event, const FWeakWidgetPath& OldPath,
    const TSharedPtr<SWidget>& OldWidget, const FWidgetPath& NewPath, const TSharedPtr<SWidget>& NewWidget)
{
    if (!IsFocusTrackingValid()) return;
    const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
    const TSharedPtr<const FSlateUser> User = LocalPlayer ? LocalPlayer->GetSlateUser() : nullptr;
    if (!User.IsValid() || Event.GetUser() != User->GetUserIndex()) return;

    if (OldPath.ContainsWidget(GetCachedWidget().Get()) && OldWidget != NewWidget)
    {
        CancelSpaceHold(TEXT("FocusChanged"));
    }
    // 다른 UI·다른 PIE 창으로 이동한 포커스를 빼앗지 않는다.
    // 소유 뷰포트 자체가 대상인 경우만 이벤트 완료 후 한 번 복구한다.
    QueueViewportFocusRestore(NewWidget);
}

void UDMPreMatchTutorialWidget::QueueViewportFocusRestore(const TSharedPtr<SWidget>& Target)
{
    const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
    const UGameViewportClient* Viewport = LocalPlayer ? LocalPlayer->ViewportClient : nullptr;
    const TSharedPtr<SViewport> ViewportWidget = Viewport ? Viewport->GetGameViewportWidget() : nullptr;
    if (!IsFocusTrackingValid() || !Target.IsValid() || Target != ViewportWidget) return;
    PendingViewportFocus = Target;
    if (!FocusRestoreTimer.IsValid())
    {
        // Slate는 OnFocusChanging broadcast 이후 새 경로를 저장한다. 콜백 안의 재귀 변경을 피한다.
        FocusRestoreTimer = GetWorld()->GetTimerManager().SetTimerForNextTick(this, &ThisClass::RestoreTutorialFocus);
    }
}

void UDMPreMatchTutorialWidget::RestoreTutorialFocus()
{
    FocusRestoreTimer.Invalidate();
    const TSharedPtr<SWidget> Target = PendingViewportFocus.Pin();
    PendingViewportFocus.Reset();
    if (!IsOwningWindowActive() || !Target.IsValid()) return;
    const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
    const TSharedPtr<const FSlateUser> User = LocalPlayer ? LocalPlayer->GetSlateUser() : nullptr;
    if (!User.IsValid() || User->GetFocusedWidget() != Target) return;
    SetUserFocus(GetOwningPlayer());
    UE_LOG(LogDMTutorialFocus, Log, TEXT("Tutorial focus restored. Owner=%s User=%d"), *GetNameSafe(GetOwningPlayer()), User->GetUserIndex());
}

void UDMPreMatchTutorialWidget::HandleApplicationActivationChanged(bool bActive)
{
    if (!IsFocusTrackingValid()) return;
    if (!bActive)
    {
        CancelSpaceHold(TEXT("ApplicationDeactivated"));
        GetWorld()->GetTimerManager().ClearTimer(FocusRestoreTimer);
        PendingViewportFocus.Reset();
    }
    else
    {
        const ULocalPlayer* LocalPlayer = GetOwningLocalPlayer();
        const TSharedPtr<const FSlateUser> User = LocalPlayer ? LocalPlayer->GetSlateUser() : nullptr;
        if (User.IsValid()) QueueViewportFocusRestore(User->GetFocusedWidget());
    }
}
