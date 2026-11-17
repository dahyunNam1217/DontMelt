#include "Framework/Match/DMMatchResultRules.h"

#include "Misc/AutomationTest.h"

#if WITH_DEV_AUTOMATION_TESTS

namespace
{
    FDMMatchParticipantSnapshot
    MakeParticipant(const int32 BaseSlot, const EDMPlayerLifeState LifeState, const int32 RegisteredRelicCount = 0)
    {
        FDMMatchParticipantSnapshot Participant;
        Participant.bHasAssignedBaseSlot = true;
        Participant.BaseSlot = BaseSlot;
        Participant.LifeState = LifeState;
        Participant.RegisteredRelicCount = RegisteredRelicCount;
        return Participant;
    }

    FDMMatchResultEntry MakeResult(const int32 BaseSlot, const int32 RelicCount)
    {
        FDMMatchResultEntry Entry;
        Entry.BaseSlot = BaseSlot;
        Entry.RelicCount = RelicCount;
        return Entry;
    }
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDMRelicVictoryRuleTest,
    "DontMelt.Match.ResultRules.RelicVictory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FDMRelicVictoryRuleTest::RunTest(const FString& Parameters)
{
    int32 WinningBaseSlot = INDEX_NONE;
    int32 WinningCount = 0;

    // 목표에 도달한 첫 참가자가 승리한다.
    const FDMMatchParticipantSnapshotList Reached = {
        MakeParticipant(0, EDMPlayerLifeState::Alive, 2),
        MakeParticipant(1, EDMPlayerLifeState::Dead, 3),
        MakeParticipant(2, EDMPlayerLifeState::Alive, 3)
    };
    TestTrue(
        TEXT("Reached target wins"),
        DMMatchResultRules::FindRelicVictoryWinner(Reached, 3, WinningBaseSlot, WinningCount)
    );
    TestEqual(TEXT("First reached BaseSlot"), WinningBaseSlot, 1);
    TestEqual(TEXT("Winning relic count"), WinningCount, 3);

    // 연결이 끊긴 참가자는 목표에 도달해도 승리하지 않는다.
    const FDMMatchParticipantSnapshotList Disconnected = {MakeParticipant(0, EDMPlayerLifeState::Disconnected, 5)};
    TestFalse(
        TEXT("Disconnected cannot win"),
        DMMatchResultRules::FindRelicVictoryWinner(Disconnected, 3, WinningBaseSlot, WinningCount)
    );
    TestEqual(TEXT("No winner when disconnected"), WinningBaseSlot, INDEX_NONE);

    // 목표가 0 이하이면 판정하지 않는다.
    TestFalse(
        TEXT("Non-positive target"),
        DMMatchResultRules::FindRelicVictoryWinner(Reached, 0, WinningBaseSlot, WinningCount)
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDMEliminationVictoryRuleTest,
    "DontMelt.Match.ResultRules.EliminationVictory",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FDMEliminationVictoryRuleTest::RunTest(const FString& Parameters)
{
    int32 WinningBaseSlot = INDEX_NONE;
    int32 Connected = 0;
    int32 Alive = 0;

    // 3인 이상에서 한 명만 생존하면 승리한다.
    const FDMMatchParticipantSnapshotList OneAlive = {
        MakeParticipant(0, EDMPlayerLifeState::Spectating),
        MakeParticipant(1, EDMPlayerLifeState::Alive),
        MakeParticipant(2, EDMPlayerLifeState::Dead)
    };
    TestTrue(
        TEXT("One alive of three"),
        DMMatchResultRules::FindEliminationWinner(OneAlive, WinningBaseSlot, Connected, Alive)
    );
    TestEqual(TEXT("Survivor wins"), WinningBaseSlot, 1);
    TestEqual(TEXT("Connected count"), Connected, 3);
    TestEqual(TEXT("Alive count"), Alive, 1);

    // 2인 경기에서는 전멸 승리를 사용하지 않는다.
    const FDMMatchParticipantSnapshotList TwoPlayers = {
        MakeParticipant(0, EDMPlayerLifeState::Alive), MakeParticipant(1, EDMPlayerLifeState::Dead)
    };
    TestFalse(
        TEXT("Two players"), DMMatchResultRules::FindEliminationWinner(TwoPlayers, WinningBaseSlot, Connected, Alive)
    );
    TestEqual(TEXT("No winner in two players"), WinningBaseSlot, INDEX_NONE);

    // 연결이 끊긴 참가자는 인원 수에서 제외한다.
    const FDMMatchParticipantSnapshotList WithDisconnected = {
        MakeParticipant(0, EDMPlayerLifeState::Alive),
        MakeParticipant(1, EDMPlayerLifeState::Dead),
        MakeParticipant(2, EDMPlayerLifeState::Disconnected)
    };
    TestFalse(
        TEXT("Disconnected excluded"),
        DMMatchResultRules::FindEliminationWinner(WithDisconnected, WinningBaseSlot, Connected, Alive)
    );
    TestEqual(TEXT("Connected excludes disconnected"), Connected, 2);

    // 아무도 생존하지 않으면 승자가 없다.
    const FDMMatchParticipantSnapshotList NoneAlive = {
        MakeParticipant(0, EDMPlayerLifeState::Dead),
        MakeParticipant(1, EDMPlayerLifeState::Respawning),
        MakeParticipant(2, EDMPlayerLifeState::Spectating)
    };
    TestFalse(
        TEXT("Nobody alive"), DMMatchResultRules::FindEliminationWinner(NoneAlive, WinningBaseSlot, Connected, Alive)
    );

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDMOvertimeWinnerRuleTest,
    "DontMelt.Match.ResultRules.OvertimeWinner",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FDMOvertimeWinnerRuleTest::RunTest(const FString& Parameters)
{
    int32 WinningBaseSlot = INDEX_NONE;

    // 사망 중이어도 등록 유물 단독 최다 진영이 승리한다.
    const FDMMatchParticipantSnapshotList UniqueLeader = {
        MakeParticipant(0, EDMPlayerLifeState::Alive, 1),
        MakeParticipant(1, EDMPlayerLifeState::Spectating, 3),
        MakeParticipant(2, EDMPlayerLifeState::Alive, 2)
    };
    TestTrue(
        TEXT("Unique leader resolved"),
        DMMatchResultRules::ResolveOvertimeWinner(UniqueLeader, WinningBaseSlot)
            == EDMOvertimeWinnerResolveResult::Resolved
    );
    TestEqual(TEXT("Unique leader wins"), WinningBaseSlot, 1);

    // 최다가 동률이면 판정은 성공하지만 승자는 없다.
    const FDMMatchParticipantSnapshotList Tied = {
        MakeParticipant(0, EDMPlayerLifeState::Alive, 2), MakeParticipant(1, EDMPlayerLifeState::Alive, 2)
    };
    TestTrue(
        TEXT("Tie resolved"),
        DMMatchResultRules::ResolveOvertimeWinner(Tied, WinningBaseSlot) == EDMOvertimeWinnerResolveResult::Resolved
    );
    TestEqual(TEXT("Tie has no winner"), WinningBaseSlot, INDEX_NONE);

    // 연결이 끊긴 참가자가 있으면 판정 입력이 잘못된 것이다.
    const FDMMatchParticipantSnapshotList Disconnected = {
        MakeParticipant(0, EDMPlayerLifeState::Alive, 2), MakeParticipant(1, EDMPlayerLifeState::Disconnected, 1)
    };
    TestTrue(
        TEXT("Disconnected is invalid"),
        DMMatchResultRules::ResolveOvertimeWinner(Disconnected, WinningBaseSlot)
            == EDMOvertimeWinnerResolveResult::InvalidParticipant
    );
    TestEqual(TEXT("Invalid input has no winner"), WinningBaseSlot, INDEX_NONE);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDMResultRankingRuleTest,
    "DontMelt.Match.ResultRules.Ranking",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FDMResultRankingRuleTest::RunTest(const FString& Parameters)
{
    // 4,2,2,1 → 1,2,2,4 공동 순위. 같은 등록 수에서는 승자가 먼저다.
    TArray<FDMMatchResultEntry> Results = {MakeResult(0, 2), MakeResult(1, 1), MakeResult(2, 2), MakeResult(3, 4)};

    DMMatchResultRules::SortAndRankResults(Results, 2);

    TestEqual(TEXT("Order 1"), Results[0].BaseSlot, 3);
    TestEqual(TEXT("Order 2 is winner among tie"), Results[1].BaseSlot, 2);
    TestEqual(TEXT("Order 3"), Results[2].BaseSlot, 0);
    TestEqual(TEXT("Order 4"), Results[3].BaseSlot, 1);

    TestEqual(TEXT("Rank 1"), Results[0].Rank, 1);
    TestEqual(TEXT("Rank 2"), Results[1].Rank, 2);
    TestEqual(TEXT("Rank 2 tied"), Results[2].Rank, 2);
    TestEqual(TEXT("Rank 4"), Results[3].Rank, 4);

    // 무승부·전원 탈락은 승리 슬롯이 없어도 공동 순위가 계산돼야 한다.
    TArray<FDMMatchResultEntry> DrawResults = {
        MakeResult(2, 2), MakeResult(0, 2), MakeResult(3, 0), MakeResult(1, 1)
    };
    DMMatchResultRules::SortAndRankResults(DrawResults, INDEX_NONE);
    TestEqual(TEXT("Draw first slot"), DrawResults[0].BaseSlot, 0);
    TestEqual(TEXT("Draw second slot"), DrawResults[1].BaseSlot, 2);
    TestEqual(TEXT("Draw rank one"), DrawResults[0].Rank, 1);
    TestEqual(TEXT("Draw tied rank one"), DrawResults[1].Rank, 1);
    TestEqual(TEXT("Draw next rank three"), DrawResults[2].Rank, 3);
    TestEqual(TEXT("Draw last rank four"), DrawResults[3].Rank, 4);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FDMEliminationResultRankingTest,
    "DontMelt.Match.ResultRules.EliminationRanking",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter
)

bool FDMEliminationResultRankingTest::RunTest(const FString& Parameters)
{
    // 승자 등록 수가 적거나 같거나 많아도 단독 1위이며, 나머지는 공동 순위다.
    for (const int32 WinnerCount : {0, 2, 5})
    {
        TArray<FDMMatchResultEntry> Results = {
            MakeResult(0, 2), MakeResult(1, WinnerCount), MakeResult(2, 2), MakeResult(3, 1)
        };
        for (FDMMatchResultEntry& Entry : Results)
        {
            Entry.AcquiredRelicCount = 10 - Entry.BaseSlot;
            Entry.KillCount = Entry.BaseSlot;
            Entry.PlayerName = FString::FromInt(Entry.BaseSlot);
        }

        DMMatchResultRules::SortAndRankResults(Results, 1, true);
        TestEqual(TEXT("Pinned winner"), Results[0].BaseSlot, 1);
        TestEqual(TEXT("Winner alone ranks first"), Results[0].Rank, 1);
        TestEqual(TEXT("First remaining slot"), Results[1].BaseSlot, 0);
        TestEqual(TEXT("Second remaining slot"), Results[2].BaseSlot, 2);
        TestEqual(TEXT("Remaining rank two"), Results[1].Rank, 2);
        TestEqual(TEXT("Remaining tied rank two"), Results[2].Rank, 2);
        TestEqual(TEXT("Remaining rank four"), Results[3].Rank, 4);
        for (const FDMMatchResultEntry& Entry : Results)
        {
            TestEqual(TEXT("Acquisition stays with player"), Entry.AcquiredRelicCount, 10 - Entry.BaseSlot);
            TestEqual(TEXT("Kills stay with player"), Entry.KillCount, Entry.BaseSlot);
            TestEqual(TEXT("Name stays with player"), Entry.PlayerName, FString::FromInt(Entry.BaseSlot));
            TestEqual(TEXT("Registered count preserved"), Entry.RelicCount, Entry.BaseSlot == 1 ? WinnerCount : (Entry.BaseSlot == 3 ? 1 : 2));
        }

        // 재정렬해도 결과는 같아야 한다.
        DMMatchResultRules::SortAndRankResults(Results, 1, true);
        TestEqual(TEXT("Repeated ranking keeps winner"), Results[0].BaseSlot, 1);
        TestEqual(TEXT("Repeated ranking keeps tie"), Results[2].Rank, 2);
    }

    TArray<FDMMatchResultEntry> ZeroResults = {
        MakeResult(3, 0), MakeResult(2, 0), MakeResult(1, 0), MakeResult(0, 0)
    };
    DMMatchResultRules::SortAndRankResults(ZeroResults, 2, true);
    TestEqual(TEXT("Zero-count winner first"), ZeroResults[0].BaseSlot, 2);
    TestEqual(TEXT("Zero-count winner rank"), ZeroResults[0].Rank, 1);
    for (int32 Index = 1; Index < ZeroResults.Num(); ++Index)
    {
        TestEqual(TEXT("Zero-count others tied second"), ZeroResults[Index].Rank, 2);
    }

    // 일반 종료에서는 낮은 등록 수의 승자를 강제로 올리지 않는다.
    TArray<FDMMatchResultEntry> NormalResults = {MakeResult(0, 4), MakeResult(1, 1), MakeResult(2, 2)};
    NormalResults[1].AcquiredRelicCount = 99;
    NormalResults[1].KillCount = 99;
    DMMatchResultRules::SortAndRankResults(NormalResults, 1, false);
    TestEqual(TEXT("Normal leader preserved"), NormalResults[0].BaseSlot, 0);
    TestEqual(TEXT("Acquisition and kills do not change rank"), NormalResults[2].BaseSlot, 1);
    TestEqual(TEXT("Normal winner ranks by registration"), NormalResults[2].Rank, 3);
    return true;
}

#endif
