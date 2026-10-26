
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "GameplayTagContainer.h"

#include "DMSkillTargetPanelWidget.generated.h"

class UTextBlock;
class ADMMatchPlayerState;

/**
 * 대상 지정 스킬의 선택 안내를 표시한다.
 *
 * 실제 대상 선정과 스킬 사용 승인에는 관여하지 않는다.
 * Controller의 대상 선택 상태와 SkillSet의 표시 정보만 사용한다.
 */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMSkillTargetPanelWidget : public UUserWidget
{
    GENERATED_BODY()

public:
#pragma region Presentation

    /** 현재 선택 중인 스킬명을 표시하고 패널을 연다. */
    void ShowTargetingSkill(const FGameplayTag& InSkillTag, const ADMMatchPlayerState* InPlayerState);

    /** 대상 선택 패널을 숨긴다. */
    void HideTargetingSkill();

#pragma endregion

protected:
#pragma region Widget References

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_SelectedSkillName = nullptr;

#pragma endregion
};
