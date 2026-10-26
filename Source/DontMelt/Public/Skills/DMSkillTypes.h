#pragma once

#include "CoreMinimal.h"
#include "GameplayTagContainer.h"
#include "DMSkillTypes.generated.h"

class UTexture2D;

/**
 * 상대 대상 Skill을 사용할 때
 * Server가 해석할 대상 선택 방식.
 *
 * 실제 대상 Player를 Client가 지정하지 않는다.
 */
UENUM(BlueprintType)
enum class EDMSkillTargetMode : uint8
{
    RandomEligible UMETA(DisplayName = "Random Eligible"),

    LeadingEligible UMETA(DisplayName = "Leading Eligible")
};

/**
 * Skill 사용 요청이 차단된 Gameplay 이유.
 *
 * 내부 구성 오류를 나타내는 Enum이 아니라
 * 정상 Gameplay 상태에서 사용 불가능한 이유만 표현한다.
 */
UENUM(BlueprintType)
enum class EDMActionBlockReason : uint8
{
    None UMETA(DisplayName = "None"),

    MatchPhase UMETA(DisplayName = "Match Phase"),

    LifeState UMETA(DisplayName = "Life State"),

    Busy UMETA(DisplayName = "Busy"),

    Status UMETA(DisplayName = "Status"),

    Cooldown UMETA(DisplayName = "Cooldown"),

    CarryingRelic UMETA(DisplayName = "Carrying Relic"),

    Pose UMETA(DisplayName = "Pose"),

    NoTarget UMETA(DisplayName = "No Target")
};

/**
 * 공통 Gameplay Action 사용 가능 판정 결과.
 *
 * Skill 사용 가능 여부 판정에 사용한다.
 */
USTRUCT(BlueprintType)
struct DONTMELT_API FDMActionAvailabilityResult
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DM|Action")
    bool bAllowed = false;

    UPROPERTY(BlueprintReadOnly, Category = "DM|Action")
    EDMActionBlockReason BlockReason = EDMActionBlockReason::None;

    static FDMActionAvailabilityResult Allowed()
    {
        FDMActionAvailabilityResult Result;

        Result.bAllowed = true;
        Result.BlockReason = EDMActionBlockReason::None;

        return Result;
    }

    static FDMActionAvailabilityResult Blocked(const EDMActionBlockReason Reason)
    {
        FDMActionAvailabilityResult Result;

        Result.bAllowed = false;
        Result.BlockReason = Reason;

        return Result;
    }
};

/**
 * 하나의 Skill에 대한 불변 정의 데이터.
 *
 * Runtime cooldown / target / active effect 상태는
 * 이 Struct에 저장하지 않는다.
 */
USTRUCT(BlueprintType)
struct DONTMELT_API FDMSkillDefinition
{
    GENERATED_BODY()

    /**
     * Skill의 고유 Gameplay Tag.
     *
     * 예:
     * Skill.Invisibility
     * Skill.FootstepAmplify
     * Skill.VisionDistort
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Skill")
    FGameplayTag SkillTag;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Skill|UI")
    FText DisplayName;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Skill|UI")
    FText Description;

    /**
     * 스킬 HUD에 표시할 선택형 아이콘.
     * 미지정 시 UI에서 아이콘 영역을 숨긴다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Skill|UI")
    TObjectPtr<UTexture2D> Icon = nullptr;

    /**
     * Skill 사용 후 다시 사용할 수 있을 때까지의 시간.
     * 단위: seconds
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Skill|Time", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float Cooldown = 0.0f;

    /**
     * Skill의 기본 Gameplay Effect 지속시간.
     *
     * Skill별 연출 전환 시간은 이 값과 별도로 정의한다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Skill|Time", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float Duration = 0.0f;

    /**
     * true:
     * Skill 입력 후 IMC_SkillTargeting으로 진입한다.
     *
     * false:
     * 자기 자신에게 즉시 사용하는 Skill이다.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Skill|Targeting")
    bool bRequiresTargetSelection = false;

    /**
     * Relic을 운반 중인 Player가
     * 이 Skill을 사용할 수 있는지 여부.
     *
     * 현재:
     * Invisibility      = false
     * FootstepAmplify   = true
     * VisionDistort     = true
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|Skill|Restriction")
    bool bCanUseWhileCarryingRelic = true;
};

/**
 * 하나의 Skill cooldown의 Server 기준 런타임 상태.
 *
 * RemainingTime은 저장하지 않는다.
 *
 * Remaining =
 * EndServerTime - ServerWorldTime
 */
USTRUCT(BlueprintType)
struct DONTMELT_API FDMSkillCooldownEntry
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "DM|Skill|Cooldown")
    FGameplayTag SkillTag;

    UPROPERTY(BlueprintReadOnly, Category = "DM|Skill|Cooldown")
    double EndServerTime = 0.0;
};
