#pragma once

#include "CoreMinimal.h"
#include "Core/DMMatchConstants.h"
#include "Core/DMMatchTypes.h"

/**
 * 승패 판정에 쓰는 참가자 한 명의 값 복사본.
 *
 * GameMode가 PlayerState와 RelicManager에서 값을 모아 만들고,
 * DMMatchResultRules는 이 값만으로 판정한다.
 * 월드 없이 계산할 수 있어 Automation Test로 검증한다.
 */
struct FDMMatchParticipantSnapshot
{
    /** 매치 PlayerState이며 BaseSlot이 배정된 참가자인가. */
    bool bHasAssignedBaseSlot = false;

    int32 BaseSlot = INDEX_NONE;

    EDMPlayerLifeState LifeState = EDMPlayerLifeState::Alive;

    /** 판정 순간 자기 진영에 등록된 유물 수. */
    int32 RegisteredRelicCount = 0;
};

using FDMMatchParticipantSnapshotList = TArray<FDMMatchParticipantSnapshot, TInlineAllocator<DMMatch::MaxPlayers>>;

/** 연장전 승자 판정 결과. Resolved가 아니면 판정 입력이 잘못된 것이다. */
enum class EDMOvertimeWinnerResolveResult : uint8
{
    Resolved,
    InvalidParticipant,
    InvalidBaseSlot,
    DuplicateBaseSlot
};

namespace DMMatchResultRules
{
    /**
     * 등록 유물 수가 목표 이상인 첫 참가자를 찾는다.
     *
     * 최대 4명이고 승리 판정 이벤트 때만 실행되므로 순서대로 순회한다.
     * BaseSlot 미배정과 Disconnected 참가자는 제외한다.
     */
    bool FindRelicVictoryWinner(
        TConstArrayView<FDMMatchParticipantSnapshot> Participants,
        int32 RequiredRelicCount,
        int32& OutWinningBaseSlot,
        int32& OutWinningRegisteredRelicCount
    );

    /**
     * 전멸 승리를 판정한다.
     *
     * 연결된 참가자가 3명 이상이고 Alive가 정확히 한 명일 때만 성공한다.
     */
    bool FindEliminationWinner(
        TConstArrayView<FDMMatchParticipantSnapshot> Participants,
        int32& OutWinningBaseSlot,
        int32& OutConnectedParticipantCount,
        int32& OutAlivePlayerCount
    );

    /**
     * 연장전 종료 시 등록 유물 단독 최다 진영을 찾는다.
     *
     * 생존 여부는 보지 않는다. 판정 후 동률이면 OutWinningBaseSlot은 INDEX_NONE이다.
     */
    EDMOvertimeWinnerResolveResult
    ResolveOvertimeWinner(TConstArrayView<FDMMatchParticipantSnapshot> Participants, int32& OutWinningBaseSlot);

    /**
     * 결과표를 등록 유물 수 기준으로 정렬하고 공동 순위를 매긴다.
     *
     * 같은 등록 수에서는 승자를 먼저, 그다음 BaseSlot 오름차순으로 표시한다.
     */
    void SortAndRankResults(TArray<FDMMatchResultEntry>& Results, int32 WinningBaseSlot);
}
