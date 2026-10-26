#include "UI/DMConnectionLostWidget.h"

#include "Components/TextBlock.h"

void UDMConnectionLostWidget::SetMessage(const FText& InMessage)
{
    CurrentMessage = InMessage;

    if (IsValid(TXT_Message.Get()))
    {
        TXT_Message->SetText(CurrentMessage);
    }
}

void UDMConnectionLostWidget::NativeConstruct()
{
    Super::NativeConstruct();

    SetIsFocusable(true);
    SetVisibility(ESlateVisibility::Visible);

    if (IsValid(TXT_Message.Get()))
    {
        TXT_Message->SetText(CurrentMessage);
    }
}

FReply UDMConnectionLostWidget::NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent)
{
    return FReply::Handled();
}

FReply UDMConnectionLostWidget::NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    return FReply::Handled();
}

FReply
UDMConnectionLostWidget::NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    return FReply::Handled();
}

FReply UDMConnectionLostWidget::NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent)
{
    return FReply::Handled();
}