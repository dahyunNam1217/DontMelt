
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "DMSkillSlotWidget.generated.h"

class UTextBlock;
class UProgressBar;
class UImage;
struct FDMSkillDefinition;

/**
 * 개별 스킬 슬롯의 표시만 담당한다.
 * 스킬 데이터 원본과 타이머는 소유하지 않는다.
 */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMSkillSlotWidget : public UUserWidget
{
    GENERATED_BODY()

public:
#pragma region Presentation

    void ApplyDefinition(const FDMSkillDefinition* Definition, const FText& InputKey);

    void UpdateCooldown(float RemainingSeconds, float CooldownDuration, bool bDataReady);

#pragma endregion

protected:
#pragma region Widget References

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_SkillKey = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_Cooldown = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UProgressBar> PB_Cooldown = nullptr;

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UImage> IMG_SkillIcon = nullptr;

#pragma endregion
};
