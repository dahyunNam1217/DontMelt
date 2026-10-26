#pragma once

#include "CoreMinimal.h"
#include "Characters/DMCharacterTypes.h"
#include "Engine/DataTable.h"
#include "GameplayTagContainer.h"
#include "DMQuestTypes.generated.h"

/**
 * Quest 진행값을 해석하는 방식.
 */
UENUM(BlueprintType)
enum class EDMQuestProgressMode : uint8
{
    Count UMETA(DisplayName = "Count"),
    AccumulatedDuration UMETA(DisplayName = "Accumulated Duration"),
    ContinuousDuration UMETA(DisplayName = "Continuous Duration")
};

/**
 * 한 플레이어가 현재 Quest Run에서 가지는 결과 상태.
 *
 * bool 여러 개로 Completed / Failed / Excluded를 중복 저장하지 않는다.
 */
UENUM(BlueprintType)
enum class EDMQuestParticipantState : uint8
{
    Inactive UMETA(DisplayName = "Inactive"),
    Active UMETA(DisplayName = "Active"),
    CompletedInTime UMETA(DisplayName = "Completed In Time"),
    FailedPendingCompletion UMETA(DisplayName = "Failed Pending Completion"),
    CompletedAfterFailure UMETA(DisplayName = "Completed After Failure"),
    Excluded UMETA(DisplayName = "Excluded")
};

/**
 * One server-authoritative gameplay action
 * converted into a QuestEvent.
 *
 * This is transient event data.
 * It is not replicated or stored as runtime state.
 */
struct DONTMELT_API FDMQuestActionEvent
{
    FGameplayTag EventTag;

    EDMPoseType PoseType = EDMPoseType::Default;

    FName EmoteId = NAME_None;
};

/**
 * DT_QuestDefinitions의 한 행.
 *
 * Runtime 상태를 저장하지 않고 Quest 정의 데이터만 가진다.
 */
USTRUCT(BlueprintType)
struct DONTMELT_API FDMQuestDefinitionRow : public FTableRowBase
{
    GENERATED_BODY()

    /** Quest 콘텐츠 ID. */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Quest")
    FGameplayTag QuestTag;

    /** 퀘스트 HUD에 표시할 이름 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Quest|UI")
    FText DisplayName;

    /** 퀘스트 HUD에 표시할 목표 설명 */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Quest|UI")
    FText Description;

    /**
     * 진행을 시작시키거나 Count를 증가시키는 Gameplay Event Tag.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Quest|Progress")
    FGameplayTag StartEventTag;

    /**
     * Duration 추적을 종료시키는 Gameplay Event Tag.
     *
     * Count Quest에서는 비워둔다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Quest|Progress")
    FGameplayTag EndEventTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Quest|Progress")
    EDMQuestProgressMode ProgressMode = EDMQuestProgressMode::Count;

    /**
     * ProgressMode에 따라 Count 또는 Seconds로 해석한다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Quest|Progress", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float TargetValue = 0.0f;

    /**
     * 제한시간. 단위는 초.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Quest|Time", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float TimeLimit = 0.0f;

    /**
     * PoseHold Quest에서 요구하는 자세.
     *
     * 다른 Quest에서는 사용하지 않는다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Quest|Pose")
    EDMPoseType RequiredPose = EDMPoseType::Default;

    // 퀘스트 완료 보상은 없다.
    // 시간 초과 사이렌은 퀘스트 종류와 무관한 공통 패널티이므로
    // 사운드 설정은 ADMPlayerCharacter가 가진다.
};

/**
 * 하나의 글로벌 Quest Run을 나타내는 공개 가능한 상태.
 *
 * 권위 원본은 UDMQuestDirector가 가진다.
 * 이후 GameState에는 이 값의 Public Projection만 전달한다.
 */
USTRUCT(BlueprintType)
struct DONTMELT_API FDMQuestRuntimeState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DM|Quest")
    int32 RunId = 0;

    UPROPERTY(BlueprintReadOnly, Category = "DM|Quest")
    FGameplayTag QuestTag;

    UPROPERTY(BlueprintReadOnly, Category = "DM|Quest|Time")
    double StartServerTime = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "DM|Quest|Time")
    double EndServerTime = 0.0;

    bool IsActive() const
    {
        return RunId > 0 && QuestTag.IsValid() && EndServerTime > StartServerTime;
    }
};

/**
 * 한 플레이어의 Quest 진행 상태.
 *
 * 실제 SSOT는 UDMQuestProgressComponent가 가진다.
 */
USTRUCT(BlueprintType)
struct DONTMELT_API FDMQuestPlayerProgress
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DM|Quest")
    int32 RunId = 0;

    /**
     * Count 또는 Seconds.
     */
    UPROPERTY(BlueprintReadOnly, Category = "DM|Quest|Progress")
    float ProgressValue = 0.0f;

    /*
     * AccumulatedDuration / ContinuousDuration
     * 행동이 현재 진행 중일 때의 서버 시작 시각.
     *
     * 0이면 현재 Duration 관찰 중이 아니다.
     *
     * Owner HUD는 이 값과 ServerWorldTime을 이용하여
     * 매 프레임 복제 없이 현재 진행도를 표시할 수 있다.
     */
    UPROPERTY(BlueprintReadOnly, Category = "DM|Quest|Progress")
    double ObservationStartServerTime = 0.0;

    /*
     * TargetValue를 실제로 달성한 논리적 서버 시각.
     *
     * Completion Rank의 서버 권위 기준으로 사용한다.
     * Rank 자체는 여기서 계산하지 않는다.
     */
    UPROPERTY(BlueprintReadOnly, Category = "DM|Quest|Progress")
    double CompletionServerTime = 0.0;

    UPROPERTY(BlueprintReadOnly, Category = "DM|Quest|Progress")
    EDMQuestParticipantState State = EDMQuestParticipantState::Inactive;

    /**
     * 제한시간 안에 완료한 경우 1~4.
     *
     * 순위가 없는 상태에서는 0.
     */
    UPROPERTY(BlueprintReadOnly, Category = "DM|Quest|Progress")
    int32 Rank = 0;
};