#include "UI/DMControlsHelpWidget.h"

#include "Components/PanelWidget.h"
#include "Components/TextBlock.h"

void UDMControlsHelpWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (IsValid(TXT_BasicControls))
    {
        TXT_BasicControls->SetText(NSLOCTEXT("DMControlsHelp", "BasicControls", "WASD 이동 · Shift 달리기 · Space 점프 · F 상호작용 · F1 도움말"));
    }
    if (IsValid(TXT_DetailedControls))
    {
        TXT_DetailedControls->SetText(NSLOCTEXT(
            "DMControlsHelp", "DetailedControls",
            "이동 WASD | 시점 마우스 | 점프 Space | 달리기 Shift\n"
            "상호작용 F | 유물 내려놓기 Q | 공격 마우스 왼쪽 | 방어 E\n"
            "자세 R | 자세 종료 R | 감정표현 T\n"
            "상호작용 확인 Space | 닫기 F\n"
            "스킬 1 투명화 | 2 발소리 증폭 | 3 시야 교란 | 4 크기 증폭 | 5 이동 둔화\n"
            "대상 선택 마우스 왼쪽/오른쪽 | 취소 Esc | 도움말 F1"
        ));
    }

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
