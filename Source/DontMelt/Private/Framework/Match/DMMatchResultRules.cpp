#include "Framework/Match/DMMatchResultRules.h"

namespace DMMatchResultRules
{
    bool FindRelicVictoryWinner(
        const TConstArrayView<FDMMatchParticipantSnapshot> Participants,
        const int32 RequiredRelicCount,
        int32& OutWinningBaseSlot,
        int32& OutWinningRegisteredRelicCount
    )
    {
        OutWinningBaseSlot = INDEX_NONE;

        OutWinningRegisteredRelicCount = 0;

        if (RequiredRelicCount <= 0)
        {
            return false;
        }

        for (const FDMMatchParticipantSnapshot& Participant : Participants)
        {
            if (!Participant.bHasAssignedBaseSlot)
            {
                continue;
            }

            if (Participant.LifeState == EDMPlayerLifeState::Disconnected)
            {
                continue;
            }

            if (Participant.RegisteredRelicCount < RequiredRelicCount)
            {
                continue;
            }

            OutWinningBaseSlot = Participant.BaseSlot;

            OutWinningRegisteredRelicCount = Participant.RegisteredRelicCount;

            return true;
        }

        return false;
    }

    bool FindEliminationWinner(
        const TConstArrayView<FDMMatchParticipantSnapshot> Participants,
        int32& OutWinningBaseSlot,
        int32& OutConnectedParticipantCount,
        int32& OutAlivePlayerCount
    )
    {
        OutWinningBaseSlot = INDEX_NONE;

        OutConnectedParticipantCount = 0;

        OutAlivePlayerCount = 0;

        // LifeState가 Elimination 판정의 SSOT다.
        for (const FDMMatchParticipantSnapshot& Participant : Participants)
        {
            if (!Participant.bHasAssignedBaseSlot)
            {
                continue;
            }

            // Disconnect Player는 생존자/사망자 양쪽 계산에서 제외한다.
            if (Participant.LifeState == EDMPlayerLifeState::Disconnected)
            {
                continue;
            }

            ++OutConnectedParticipantCount;

            // SpawnProtected는 별도 Status이므로 Alive로 계산된다.
            if (Participant.LifeState != EDMPlayerLifeState::Alive)
            {
                continue;
            }

            ++OutAlivePlayerCount;

            OutWinningBaseSlot = Participant.BaseSlot;
        }

        // 2인 경기에서는 전멸 승리를 사용하지 않는다.
        if (OutConnectedParticipantCount < DMMatch::EliminationVictoryMinParticipants)
        {
            OutWinningBaseSlot = INDEX_NONE;

            return false;
        }

        // 정확히 한 명만 Alive일 때만 승리다. 0명 Alive도 승자가 없으므로 실패다.
        if (OutAlivePlayerCount != 1)
        {
            OutWinningBaseSlot = INDEX_NONE;

            return false;
        }

        return DMMatch::IsValidBaseSlot(OutWinningBaseSlot);
    }

    EDMOvertimeWinnerResolveResult
    ResolveOvertimeWinner(const TConstArrayView<FDMMatchParticipantSnapshot> Participants, int32& OutWinningBaseSlot)
    {
        OutWinningBaseSlot = INDEX_NONE;

        bool SeenBaseSlots[DMMatch::MaxBaseSlots] = {};

        int32 HighestRelicCount = INDEX_NONE;
        int32 HighestPlayerCount = 0;
        int32 LeadingBaseSlot = INDEX_NONE;

        for (const FDMMatchParticipantSnapshot& Participant : Participants)
        {
            if (!Participant.bHasAssignedBaseSlot || Participant.LifeState == EDMPlayerLifeState::Disconnected)
            {
                return EDMOvertimeWinnerResolveResult::InvalidParticipant;
            }

            const int32 BaseSlot = Participant.BaseSlot;

            if (!ensureMsgf(
                    DMMatch::IsValidBaseSlot(BaseSlot), TEXT("Overtime result resolution failed: invalid BaseSlot.")
                ))
            {
                return EDMOvertimeWinnerResolveResult::InvalidBaseSlot;
            }

            if (!ensureMsgf(
                    !SeenBaseSlots[BaseSlot],
                    TEXT("Overtime result resolution failed: duplicate BaseSlot. BaseSlot=%d"),
                    BaseSlot
                ))
            {
                return EDMOvertimeWinnerResolveResult::DuplicateBaseSlot;
            }

            SeenBaseSlots[BaseSlot] = true;

            // 사망·리스폰 대기 중이어도 자신의 진영에 등록된 유물을 비교한다.
            const int32 RelicCount = Participant.RegisteredRelicCount;

            if (RelicCount > HighestRelicCount)
            {
                HighestRelicCount = RelicCount;
                HighestPlayerCount = 1;
                LeadingBaseSlot = BaseSlot;
            }
            else if (RelicCount == HighestRelicCount)
            {
                ++HighestPlayerCount;
            }
        }

        // 최다 참가자가 정확히 한 명일 때만 승리 진영을 확정한다.
        if (HighestPlayerCount == 1)
        {
            OutWinningBaseSlot = LeadingBaseSlot;
        }

        return EDMOvertimeWinnerResolveResult::Resolved;
    }

    void SortAndRankResults(TArray<FDMMatchResultEntry>& Results, const int32 WinningBaseSlot)
    {
        // 승리 판정에는 영향을 주지 않으며, 승자는 등록 수가 같을 때의 표시 순서에만 사용한다.
        Results.Sort(
            [WinningBaseSlot](const FDMMatchResultEntry& A, const FDMMatchResultEntry& B)
            {
                if (A.RelicCount != B.RelicCount)
                {
                    return A.RelicCount > B.RelicCount;
                }

                const bool bAIsWinner = A.BaseSlot == WinningBaseSlot;
                const bool bBIsWinner = B.BaseSlot == WinningBaseSlot;

                if (bAIsWinner != bBIsWinner)
                {
                    return bAIsWinner;
                }

                return A.BaseSlot < B.BaseSlot;
            }
        );

        // 같은 등록 유물 수는 공동 순위를 부여한다(예: 4,2,2,1 → 1,2,2,4).
        for (int32 Index = 0; Index < Results.Num(); ++Index)
        {
            FDMMatchResultEntry& Entry = Results[Index];

            const bool bIsTied = Index > 0 && Entry.RelicCount == Results[Index - 1].RelicCount;

            Entry.Rank = bIsTied ? Results[Index - 1].Rank : Index + 1;
        }
    }
}
