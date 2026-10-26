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
    /**
     * 규칙값이 서로 모순되지 않는지 검사한다.
     *
     * 에디터 Data Validation과 런타임 InitGame이 같은 함수를 사용한다.
     */
    bool ValidateRules(FString& OutFailureReason) const;

#if WITH_EDITOR
    virtual EDataValidationResult IsDataValid(class FDataValidationContext& Context) const override;
#endif

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
     * 기본 NPC 배율.
     *
     * 기본 Match에서는:
     * ResolvedNPCCount =
     * StartingPlayerCount * DefaultNPCMultiplier
     *
     * 현재 지원 최대 규모는
     * 4 Players * 10 = 40 NPC다.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Match|NPC",
        meta = (ClampMin = "1", ClampMax = "15", UIMin = "1", UIMax = "15")
    )
    int32 DefaultNPCMultiplier = 15;

    /**
     * NPC가 오인 공격을 목격하고
     * Scream / Flee 반응에 참여할 최대 거리.
     *
     * 직접 공격받은 NPC는 거리와 무관하게 반응한다.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Match|NPC Reaction",
        meta = (ClampMin = "100.0", UIMin = "100.0")
    )
    float NPCWrongTargetReactionRadius = 1200.0f;

    /**
     * 직접 공격받은 NPC 외에
     * 주변에서 추가로 반응할 최대 NPC 수.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Match|NPC Reaction",
        meta = (ClampMin = "0", ClampMax = "10", UIMin = "0", UIMax = "10")
    )
    int32 MaxNearbyNPCReactionResponders = 6;

    /**
     * 오인 공격 반응 시 NPC가
     * 공격자로부터 도주하려는 기본 거리.
     *
     * Unreal Unit(cm).
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Match|NPC Reaction",
        meta = (ClampMin = "100.0", UIMin = "100.0")
    )
    float NPCFleeDistance = 900.0f;

    /** 플레이어 실제 사망 시 비웃음 반응 후보의 수평 검색 반경(cm). */
    UPROPERTY(
        EditAnywhere, BlueprintReadOnly, Category = "DM|Match|NPC Death Reaction",
        meta = (ClampMin = "0.0", UIMin = "0.0")
    )
    float NPCDeathMockReactionRadius = 800.0f;

    /** 한 번의 플레이어 사망에 반응할 최대 NPC 수. 0이면 비활성화. */
    UPROPERTY(
        EditAnywhere, BlueprintReadOnly, Category = "DM|Match|NPC Death Reaction",
        meta = (ClampMin = "0", ClampMax = "10", UIMin = "0", UIMax = "10")
    )
    int32 MaxNearbyNPCDeathMockResponders = 3;

    /** NPC가 멈춰서 사망 지점을 바라보는 시간(초). */
    UPROPERTY(
        EditAnywhere, BlueprintReadOnly, Category = "DM|Match|NPC Death Reaction",
        meta = (ClampMin = "0.1", UIMin = "0.1")
    )
    float NPCDeathMockDuration = 2.5f;

    /**
     * Waiting에서 Playing으로 넘어가기 전 Countdown 시간.
     *
     * 3초는 개발 테스트용 초기값이며 최종 기획 확정값은 아니다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Match|Time", meta = (ClampMin = "0.1"))
    float CountdownDuration = 3.0f;

    /** 정규 경기 시간. 15분 = 900초. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Match|Time", meta = (ClampMin = "0.1"))
    float MatchDuration = 900.0f;

    /** 연장전 진입 전에 진영별 유물 수를 공개하는 시간. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Match|Time", meta = (ClampMin = "0.1"))
    float OvertimeIntroDuration = 5.0f;

    /** 연장전 시간. 3분 = 180초. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Match|Time", meta = (ClampMin = "0.1"))
    float OvertimeDuration = 180.0f;

    /**
     * 사망 후 다시 PlayerCharacter가
     * Spawn될 때까지의 총 시간.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Match|Time", meta = (ClampMin = "0.1"))
    float RespawnDuration = 30.0f;

    /**
     * 경기당 글로벌 Quest 발생 횟수 범위.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Match|Quest",
        meta = (ClampMin = "2", ClampMax = "3", UIMin = "2", UIMax = "3")
    )
    int32 QuestMinCount = 2;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Match|Quest",
        meta = (ClampMin = "2", ClampMax = "3", UIMin = "2", UIMax = "3")
    )
    int32 QuestMaxCount = 3;

    /**
     * Playing 시작 이후 Quest 발생 시도 시각.
     *
     * QuestMinCount / QuestMaxCount로
     * 이번 경기의 실제 시도 개수를 2~3개로 결정한 뒤
     * 배열 앞쪽 N개를 사용한다.
     *
     * 현재 120 / 360 / 600초는
     * 초기 플레이테스트 값이며
     * 최종 밸런스 값은 아니다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Match|Quest")
    TArray<float> QuestAttemptTimes = {120.0f, 360.0f, 600.0f};

    /**
     * 경기당 Random Event 발생 시도 횟수.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Match|Random Event",
        meta = (ClampMin = "2", ClampMax = "3", UIMin = "2", UIMax = "3")
    )
    int32 RandomEventMinCount = 2;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Match|Random Event",
        meta = (ClampMin = "2", ClampMax = "3", UIMin = "2", UIMax = "3")
    )
    int32 RandomEventMaxCount = 3;

    /**
     * Match Playing 시작 이후 첫 Event 발생 시도 시점.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Match|Random Event", meta = (ClampMin = "0.0"))
    float FirstRandomEventTime = 240.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Match|Random Event", meta = (ClampMin = "0.0"))
    float SecondRandomEventTime = 480.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Match|Random Event", meta = (ClampMin = "0.0"))
    float ThirdRandomEventTime = 720.0f;

    /**
     * Random Event 시도 시점 사이에서 허용하는 최소 간격.
     *
     * 현재 고정 Schedule은 실제로 240초 간격이지만,
     * 기획상 최소 허용값은 120초다.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Match|Random Event",
        meta = (ClampMin = "120.0", UIMin = "120.0")
    )
    float RandomEventMinimumInterval = 120.0f;

    /**
     * 3~4인 Match에서 전멸 승리를 사용할지 여부.
     *
     * 2인 Match에서는 true여도 적용하지 않는다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Match|Victory")
    bool bEnableEliminationVictory = false;
};