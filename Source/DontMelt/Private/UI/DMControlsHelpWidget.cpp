#include "UI/DMControlsHelpWidget.h"

#include "Components/PanelWidget.h"

void UDMControlsHelpWidget::NativeConstruct()
{
    Super::NativeConstruct();

    SetPresentationState(false, false);
}

void UDMControlsHelpWidget::SetPresentationState(const bool bAllowed, const bool bShowDetailed)
{
    const bool bDetailedVisible = bAllowed && bShowDetailed;
    const bool bBasicVisible = bAllowed && !bShowDetailed;

    if (IsValid(BasicControlsPanel))
    {
        BasicControlsPanel->SetVisibility(bBasicVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
    }
    if (IsValid(DetailedControlsPanel))
    {
        DetailedControlsPanel->SetVisibility(
            bDetailedVisible ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed
        );
    }
    SetVisibility(bAllowed ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);
}
