
#include "UI/DMInteractionPromptWidget.h"

#include "Relic/DMRelic.h"
#include "Relic/DMRelicChest.h"

#include "Components/TextBlock.h"

void UDMInteractionPromptWidget::SetCandidate(AActor* InCandidate)
{
    if (!IsValid(TXT_InteractionPrompt))
    {
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    FText PromptText = FText::GetEmpty();

    if (IsValid(Cast<ADMRelicChest>(InCandidate)))
    {
        PromptText = NSLOCTEXT("DMInteractionPrompt", "OpenChest", "F · 상자 열기");
    }
    else if (IsValid(Cast<ADMRelic>(InCandidate)))
    {
        PromptText = NSLOCTEXT("DMInteractionPrompt", "PickupRelic", "F 길게 · 유물 줍기");
    }

    if (PromptText.IsEmpty())
    {
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    TXT_InteractionPrompt->SetText(PromptText);

    // 안내 UI 자체는 키보드·마우스 입력을 가로채지 않는다.
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}
