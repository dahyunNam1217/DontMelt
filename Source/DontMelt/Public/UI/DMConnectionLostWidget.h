#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "Input/Reply.h"

#include "DMConnectionLostWidget.generated.h"

class UTextBlock;

UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMConnectionLostWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    // 표시할 안내 문구만 전달받는다.
    void SetMessage(const FText& InMessage);

protected:
    virtual void NativeConstruct() override;

    // 안내 중 키보드 입력이 다른 화면으로 전달되지 않도록 한다.
    virtual FReply NativeOnPreviewKeyDown(const FGeometry& InGeometry, const FKeyEvent& InKeyEvent) override;

    virtual FReply NativeOnMouseButtonDown(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    virtual FReply
    NativeOnMouseButtonDoubleClick(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

    virtual FReply NativeOnMouseWheel(const FGeometry& InGeometry, const FPointerEvent& InMouseEvent) override;

private:
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_Message = nullptr;

    FText CurrentMessage;
};