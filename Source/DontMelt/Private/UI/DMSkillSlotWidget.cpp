
#include "UI/DMSkillSlotWidget.h"

#include "Skills/DMSkillTypes.h"

#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Components/Image.h"
#include "Engine/Texture2D.h"

#pragma region Presentation

void UDMSkillSlotWidget::ApplyDefinition(const FDMSkillDefinition* Definition, const FText& InputKey)
{
    if (IsValid(TXT_SkillKey))
    {
        TXT_SkillKey->SetText(InputKey);
    }

    // 스킬 아이콘은 Data Asset의 정의를 사용한다.
    if (IsValid(IMG_SkillIcon))
    {
        UTexture2D* IconTexture = Definition ? Definition->Icon.Get() : nullptr;

        if (IsValid(IconTexture))
        {
            IMG_SkillIcon->SetBrushFromTexture(IconTexture);

            IMG_SkillIcon->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
        }
        else
        {
            IMG_SkillIcon->SetVisibility(ESlateVisibility::Collapsed);
        }
    }
}

void UDMSkillSlotWidget::UpdateCooldown(
    const float RemainingSeconds, const float CooldownDuration, const bool bDataReady
)
{
    if (!IsValid(TXT_Cooldown) || !IsValid(PB_Cooldown))
    {
        return;
    }

    if (!bDataReady)
    {
        TXT_Cooldown->SetText(FText::FromString(TEXT("--")));

        PB_Cooldown->SetPercent(0.0f);
        return;
    }

    const float Remaining = FMath::Max(0.0f, RemainingSeconds);

    const bool bCoolingDown = Remaining > 0.01f;

    if (bCoolingDown)
    {
        // 0.1초 단위로 올림 표시한다.
        const float DisplayTime = FMath::CeilToInt(Remaining * 10.0f) * 0.1f;

        TXT_Cooldown->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), DisplayTime)));
    }
    else
    {
        TXT_Cooldown->SetText(NSLOCTEXT("DMSkillSlot", "CooldownReady", "완료"));
    }

    const float Percent =
        bCoolingDown ? FMath::Clamp(1.0f - Remaining / FMath::Max(CooldownDuration, 0.01f), 0.0f, 1.0f) : 1.0f;

    PB_Cooldown->SetPercent(Percent);
}

#pragma endregion
