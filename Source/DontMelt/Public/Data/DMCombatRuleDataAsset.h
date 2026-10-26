#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DMCombatRuleDataAsset.generated.h"

/**
 * Don't Melt 전투 시스템의 불변 규칙과
 * 플레이테스트 조정값을 정의한다.
 *
 * Runtime Combat 상태는 저장하지 않는다.
 */
UCLASS(BlueprintType)
class DONTMELT_API UDMCombatRuleDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
#pragma region Attack Query

    float GetAttackRange() const
    {
        return AttackRange;
    }

    float GetAttackCooldown() const
    {
        return AttackCooldown;
    }

#pragma endregion

#pragma region Attack Policy Query

    bool IsAttackBlockedByWorldGeometry() const
    {
        return bAttackBlockedByWorldGeometry;
    }

#pragma endregion

#pragma region Penalty Query

    float GetWrongTargetStunDuration() const
    {
        return WrongTargetStunDuration;
    }

#pragma endregion

#pragma region Defense Query

    float GetDefenseDuration() const
    {
        return DefenseDuration;
    }

    float GetDefenseCooldown() const
    {
        return DefenseCooldown;
    }

    float GetDefenseArcAngle() const
    {
        return DefenseArcAngle;
    }

    int32 GetDefenseBlockCount() const
    {
        return DefenseBlockCount;
    }

#pragma endregion

#pragma region Respawn Protection Query

    float GetSpawnProtectionDuration() const
    {
        return SpawnProtectionDuration;
    }

#pragma endregion

private:
#pragma region Attack Settings

    /**
     * 공격 판정 거리.
     *
     * 기획상 초기 범위는 120~150cm지만
     * 단일 확정값은 아직 결정되지 않았다.
     *
     * /*---구현필요*/

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Combat|Attack",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "120.0", UIMax = "150.0")
    )
    float AttackRange = 0.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Combat|Attack",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0")
    )
    float AttackCooldown = 8.0f;

#pragma endregion

#pragma region Attack Policy Settings

    UPROPERTY(
        EditAnywhere, BlueprintReadOnly, Category = "DM|Combat|Attack Policy", meta = (AllowPrivateAccess = "true")
    )
    bool bAttackBlockedByWorldGeometry = true;

#pragma endregion

#pragma region Penalty Settings

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Combat|Penalty",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0")
    )
    float WrongTargetStunDuration = 3.0f;

#pragma endregion

#pragma region Defense Settings

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Combat|Defense",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0")
    )
    float DefenseDuration = 1.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Combat|Defense",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0")
    )
    float DefenseCooldown = 10.0f;

    /**
     * 방어자의 정면을 중심으로 한
     * 전체 방어 각도(Degree).
     *
     * 예:
     * 120도라면 정면 기준
     * 좌우 각각 60도까지 방어한다.
     *
     * 정확한 최종 수치는
     * 플레이테스트 후 결정한다.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Combat|Defense",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", ClampMax = "180.0")
    )
    float DefenseArcAngle = 0.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Combat|Defense",
        meta = (AllowPrivateAccess = "true", ClampMin = "1")
    )
    int32 DefenseBlockCount = 1;

#pragma endregion

#pragma region Respawn Protection Settings

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|Combat|Respawn",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0")
    )
    float SpawnProtectionDuration = 3.0f;

#pragma endregion
};