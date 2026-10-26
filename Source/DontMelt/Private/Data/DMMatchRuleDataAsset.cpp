#include "Data/DMMatchRuleDataAsset.h"

#include "Core/DMMatchConstants.h"

#if WITH_EDITOR
#include "Misc/DataValidation.h"
#endif

bool UDMMatchRuleDataAsset::ValidateRules(FString& OutFailureReason) const
{
    OutFailureReason.Reset();

    if (MinPlayerCount < DMMatch::MinPlayers || MaxPlayerCount > DMMatch::MaxPlayers || MinPlayerCount > MaxPlayerCount)
    {
        OutFailureReason =
            FString::Printf(TEXT("Match player rules are invalid. Min=%d, Max=%d"), MinPlayerCount, MaxPlayerCount);
        return false;
    }

    if (!FMath::IsWithinInclusive(DefaultNPCMultiplier, 1, 15))
    {
        OutFailureReason =
            FString::Printf(TEXT("Default NPC multiplier is invalid. Multiplier=%d"), DefaultNPCMultiplier);
        return false;
    }

    if (NPCWrongTargetReactionRadius <= 0.0f || MaxNearbyNPCReactionResponders < 0
        || MaxNearbyNPCReactionResponders > 10 || NPCFleeDistance <= 0.0f)
    {
        OutFailureReason = FString::Printf(
            TEXT("NPC reaction rules are invalid. Radius=%.2f, MaxNearby=%d, FleeDistance=%.2f"),
            NPCWrongTargetReactionRadius,
            MaxNearbyNPCReactionResponders,
            NPCFleeDistance
        );
        return false;
    }

    if (!FMath::IsFinite(NPCDeathMockReactionRadius) || NPCDeathMockReactionRadius < 0.0f
        || MaxNearbyNPCDeathMockResponders < 0 || MaxNearbyNPCDeathMockResponders > 10
        || !FMath::IsFinite(NPCDeathMockDuration) || NPCDeathMockDuration < 0.1f)
    {
        OutFailureReason = FString::Printf(
            TEXT("NPC death mock rules are invalid. Radius=%.2f, MaxResponders=%d, Duration=%.2f"),
            NPCDeathMockReactionRadius,
            MaxNearbyNPCDeathMockResponders,
            NPCDeathMockDuration
        );
        return false;
    }

    return true;
}

#if WITH_EDITOR
EDataValidationResult UDMMatchRuleDataAsset::IsDataValid(FDataValidationContext& Context) const
{
    EDataValidationResult Result = Super::IsDataValid(Context);

    FString FailureReason;

    if (!ValidateRules(FailureReason))
    {
        Context.AddError(FText::FromString(FailureReason));
        Result = EDataValidationResult::Invalid;
    }

    return Result;
}
#endif
