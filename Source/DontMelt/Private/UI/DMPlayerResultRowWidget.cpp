#include "UI/DMPlayerResultRowWidget.h"

#include "Components/TextBlock.h"

void UDMPlayerResultRowWidget::SetResultEntry(const FDMMatchResultEntry& Entry)
{
    if (!IsValid(TXT_PlayerName) || !IsValid(TXT_Rank) || !IsValid(TXT_RelicCount) || !IsValid(TXT_KillCount))
    {
        return;
    }

    TXT_PlayerName->SetText(FText::FromString(Entry.PlayerName));

    TXT_Rank->SetText(Entry.Rank == INDEX_NONE ? FText::GetEmpty() : FText::AsNumber(Entry.Rank));

    // 경기 종료 시 상자에 등록된 유물 수를 표시한다.
    TXT_RelicCount->SetText(FText::AsNumber(Entry.RelicCount));
    TXT_KillCount->SetText(FText::AsNumber(Entry.KillCount));
}