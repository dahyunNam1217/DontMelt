
#include "UI/DMSkillTargetPanelWidget.h"

#include "Framework/Match/DMMatchPlayerState.h"
#include "Skills/DMSkillComponent.h"
#include "Skills/DMSkillTypes.h"
#include "Data/DMSkillSetDataAsset.h"

#include "Components/TextBlock.h"

#pragma region Presentation

void UDMSkillTargetPanelWidget::ShowTargetingSkill(
    const FGameplayTag& InSkillTag, const ADMMatchPlayerState* InPlayerState
)
{
    if (!InSkillTag.IsValid() || !IsValid(TXT_SelectedSkillName))
    {
        HideTargetingSkill();
        return;
    }

    // 기본 안내 문구를 설정한다.
    FText SkillDisplayName = NSLOCTEXT("DMSkillTargetPanel", "UnknownSkill", "스킬 대상 선택");

    const UDMSkillComponent* SkillComponent = IsValid(InPlayerState) ? InPlayerState->GetSkillComponent() : nullptr;

    const UDMSkillSetDataAsset* SkillSet = IsValid(SkillComponent) ? SkillComponent->GetSkillSetData() : nullptr;

    // 기존 SkillSet Data Asset에서 표시명을 가져온다.
    if (IsValid(SkillSet))
    {
        const FDMSkillDefinition* Definition = SkillSet->FindSkillDefinition(InSkillTag);

        if (Definition != nullptr && !Definition->DisplayName.IsEmpty())
        {
            SkillDisplayName = Definition->DisplayName;
        }
    }

    TXT_SelectedSkillName->SetText(SkillDisplayName);

    // 패널은 입력을 직접 처리하지 않는다.
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UDMSkillTargetPanelWidget::HideTargetingSkill()
{
    SetVisibility(ESlateVisibility::Collapsed);
}

#pragma endregion
