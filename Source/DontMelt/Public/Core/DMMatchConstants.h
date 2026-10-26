#pragma once

#include "CoreMinimal.h"

/**
 * 매치 인원과 진영(BaseSlot)에 관한 공통 규칙 상수.
 *
 * 로비·매치·UI가 같은 범위를 사용하도록 한 곳에서 정의한다.
 * UPROPERTY meta(ClampMin/ClampMax)는 문자열이라 이 상수를 쓸 수 없으므로 값을 함께 맞춰야 한다.
 */
namespace DMMatch
{
    /** 매치를 시작할 수 있는 최소·최대 인원. */
    inline constexpr int32 MinPlayers = 2;
    inline constexpr int32 MaxPlayers = 4;

    /** 진영 수. 유효한 BaseSlot은 0 ~ MaxBaseSlots - 1이다. */
    inline constexpr int32 MaxBaseSlots = 4;

    /** 전멸 승리를 적용하는 최소 참가자 수. 2인 경기에는 적용하지 않는다. */
    inline constexpr int32 EliminationVictoryMinParticipants = 3;

    FORCEINLINE bool IsValidBaseSlot(const int32 BaseSlot)
    {
        return FMath::IsWithinInclusive(BaseSlot, 0, MaxBaseSlots - 1);
    }

    FORCEINLINE bool IsValidPlayerCount(const int32 PlayerCount)
    {
        return FMath::IsWithinInclusive(PlayerCount, MinPlayers, MaxPlayers);
    }
}
