#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "DMControlsHelpWidget.generated.h"

class UPanelWidget;
class UTextBlock;

/** 기본 조작 안내와 F1 홀드 상세 안내의 로컬 표시만 담당한다. */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMControlsHelpWidget : public UUserWidget
{
    GENERATED_BODY()

public:
    void SetPresentationState(bool bAllowed, bool bShowDetailed);

protected:
    virtual void NativeConstruct() override;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UPanelWidget> BasicControlsPanel = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UPanelWidget> DetailedControlsPanel = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_BasicControls = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UTextBlock> TXT_DetailedControls = nullptr;
};
