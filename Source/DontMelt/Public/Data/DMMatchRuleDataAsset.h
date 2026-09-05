#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DMMatchRuleDataAsset.generated.h"

/**
 * 매치 흐름에 사용하는 불변 규칙 데이터.
 *
 * 런타임 매치 상태를 저장하지 않으며,
 * 에디터에서 조정할 기본 규칙값만 보관한다.
 */
UCLASS(BlueprintType)
class DONTMELT_API UDMMatchRuleDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    /** 매치를 시작할 수 있는 최소 플레이어 수. */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Match|Players",
        meta = (ClampMin = "2", ClampMax = "4", UIMin = "2", UIMax = "4")
    )
    int32 MinPlayerCount = 2;

    /** 로비와 매치가 허용하는 최대 플레이어 수. */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Match|Players",
        meta = (ClampMin = "2", ClampMax = "4", UIMin = "2", UIMax = "4")
    )
    int32 MaxPlayerCount = 4;

    /**
     * Waiting에서 Playing으로 넘어가기 전 Countdown 시간.
     *
     * 3초는 개발 테스트용 초기값이며 최종 기획 확정값은 아니다.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Match|Time",
        meta = (ClampMin = "0.1")
    )
    float CountdownDuration = 3.0f;

    /** 정규 경기 시간. 15분 = 900초. */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Match|Time",
        meta = (ClampMin = "0.1")
    )
    float MatchDuration = 900.0f;

    /** 연장전 진입 전에 진영별 유물 수를 공개하는 시간. */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Match|Time",
        meta = (ClampMin = "0.1")
    )
    float OvertimeIntroDuration = 5.0f;

    /** 연장전 시간. 3분 = 180초. */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Match|Time",
        meta = (ClampMin = "0.1")
    )
    float OvertimeDuration = 180.0f;
};