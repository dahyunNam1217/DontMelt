#include "Data/DMSkillSetDataAsset.h"

#include "Core/DMGameplayTags.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

const FDMSkillDefinition* UDMSkillSetDataAsset::FindSkillDefinition(const FGameplayTag& SkillTag) const
{
    if (!SkillTag.IsValid())
    {
        return nullptr;
    }

    return SkillDefinitions.FindByPredicate(
        [&SkillTag](const FDMSkillDefinition& Definition)
        {
            return Definition.SkillTag == SkillTag;
        }
    );
}

bool UDMSkillSetDataAsset::ValidateDefinitionSet(FString& OutFailureReason) const
{
    OutFailureReason.Reset();

    constexpr int32 ExpectedSkillCount = 3;

    if (SkillDefinitions.Num() != ExpectedSkillCount)
    {
        OutFailureReason = FString::Printf(
            TEXT("Skill count mismatch. Expected=%d, Actual=%d"), ExpectedSkillCount, SkillDefinitions.Num()
        );

        return false;
    }

    TSet<FGameplayTag> RequiredSkillTags;

    RequiredSkillTags.Add(DMGameplayTags::Skill_Invisibility);

    RequiredSkillTags.Add(DMGameplayTags::Skill_FootstepAmplify);

    RequiredSkillTags.Add(DMGameplayTags::Skill_VisionDistort);

    TSet<FGameplayTag> FoundSkillTags;

    for (const FDMSkillDefinition& Definition : SkillDefinitions)
    {
        if (!Definition.SkillTag.IsValid())
        {
            OutFailureReason = TEXT("Invalid SkillTag exists.");

            return false;
        }

        if (!RequiredSkillTags.Contains(Definition.SkillTag))
        {
            OutFailureReason = FString::Printf(TEXT("Unexpected SkillTag=%s"), *Definition.SkillTag.ToString());

            return false;
        }

        if (FoundSkillTags.Contains(Definition.SkillTag))
        {
            OutFailureReason = FString::Printf(TEXT("Duplicate SkillTag=%s"), *Definition.SkillTag.ToString());

            return false;
        }

        if (Definition.Cooldown <= 0.0f)
        {
            OutFailureReason = FString::Printf(
                TEXT("Invalid Skill Cooldown. Skill=%s, Cooldown=%.3f"),
                *Definition.SkillTag.ToString(),
                Definition.Cooldown
            );

            return false;
        }

        if (Definition.Duration <= 0.0f)
        {
            OutFailureReason = FString::Printf(
                TEXT("Invalid Skill Duration. Skill=%s, Duration=%.3f"),
                *Definition.SkillTag.ToString(),
                Definition.Duration
            );

            return false;
        }

        if (Definition.SkillTag == DMGameplayTags::Skill_Invisibility && Definition.bRequiresTargetSelection)
        {
            OutFailureReason = TEXT("Invisibility must not require target selection.");

            return false;
        }

        if ((Definition.SkillTag == DMGameplayTags::Skill_FootstepAmplify
             || Definition.SkillTag == DMGameplayTags::Skill_VisionDistort)
            && !Definition.bRequiresTargetSelection)
        {
            OutFailureReason = FString::Printf(
                TEXT("Targeted Skill must require target selection. Skill=%s"), *Definition.SkillTag.ToString()
            );

            return false;
        }

        FoundSkillTags.Add(Definition.SkillTag);
    }

    for (const FGameplayTag& RequiredTag : RequiredSkillTags)
    {
        if (!FoundSkillTags.Contains(RequiredTag))
        {
            OutFailureReason = FString::Printf(TEXT("Required Skill is missing. Skill=%s"), *RequiredTag.ToString());

            return false;
        }
    }

    return true;
}

#if WITH_EDITOR
EDataValidationResult UDMSkillSetDataAsset::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    FString FailureReason;

    if (!ValidateDefinitionSet(FailureReason))
    {
        Context.AddError(FText::FromString(FailureReason));
        Result = EDataValidationResult::Invalid;
    }

    return Result;
}
#endif
