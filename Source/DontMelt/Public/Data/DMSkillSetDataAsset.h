#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "Skills/DMSkillTypes.h"
#include "DMSkillSetDataAsset.generated.h"

/**
 * Don't Melt에서 사용하는 전체 Skill 정의 집합.
 *
 * 현재 Skill은 3종으로 고정되어 있으므로
 * Skill마다 개별 Data Asset을 생성하지 않는다.
 *
 * Runtime Skill 상태는 저장하지 않는다.
 */
UCLASS(BlueprintType)
class DONTMELT_API UDMSkillSetDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    const TArray<FDMSkillDefinition>& GetSkillDefinitions() const
    {
        return SkillDefinitions;
    }

    /**
     * SkillTag와 일치하는 정의를 찾는다.
     *
     * 존재하지 않으면 nullptr.
     */
    const FDMSkillDefinition* FindSkillDefinition(const FGameplayTag& SkillTag) const;

    /**
     * 현재 게임이 요구하는 3개의 공용 Skill Definition이
     * 정상적으로 구성되어 있는지 검사한다.
     */
    bool ValidateDefinitionSet(FString& OutFailureReason) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

private:
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Skill", meta = (AllowPrivateAccess = "true"))
    TArray<FDMSkillDefinition> SkillDefinitions;
};