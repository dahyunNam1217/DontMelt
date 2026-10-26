#include "Data/DMRelicRuleDataAsset.h"

UDMRelicRuleDataAsset::UDMRelicRuleDataAsset()
{
    // 2 Players
    TotalRelicCountByPlayerCount.Add(2, 3);
    RequiredRelicCountByPlayerCount.Add(2, 3);

    // 3 Players
    TotalRelicCountByPlayerCount.Add(3, 4);
    RequiredRelicCountByPlayerCount.Add(3, 3);

    // 4 Players
    TotalRelicCountByPlayerCount.Add(4, 5);
    RequiredRelicCountByPlayerCount.Add(4, 4);
}

bool UDMRelicRuleDataAsset::TryGetRelicCountRule(
    const int32 StartingPlayerCount, int32& OutTotalRelicCount, int32& OutRequiredRelicCount
) const
{
    OutTotalRelicCount = INDEX_NONE;
    OutRequiredRelicCount = INDEX_NONE;

    const int32* TotalRelicCount = TotalRelicCountByPlayerCount.Find(StartingPlayerCount);

    const int32* RequiredRelicCount = RequiredRelicCountByPlayerCount.Find(StartingPlayerCount);

    if (TotalRelicCount == nullptr || RequiredRelicCount == nullptr)
    {
        return false;
    }

    if (*TotalRelicCount <= 0 || *RequiredRelicCount <= 0 || *RequiredRelicCount > *TotalRelicCount)
    {
        return false;
    }

    OutTotalRelicCount = *TotalRelicCount;
    OutRequiredRelicCount = *RequiredRelicCount;

    return true;
}