#pragma once

#include "CoreMinimal.h"
#include "DMCharacterTypes.generated.h"

/**
 * 플레이어가 취할 수 있는 현재 자세.
 *
 * 자세는 동시에 하나만 존재하므로
 * UDMPoseComponent가 이 Enum을 단일 원본으로 소유한다.
 */
UENUM(BlueprintType)
enum class EDMPoseType : uint8
{
    Default UMETA(DisplayName = "Default"),
    Flat UMETA(DisplayName = "Flat"),
    Crouched UMETA(DisplayName = "Crouched"),
    Star UMETA(DisplayName = "Star")
};

UENUM(BlueprintType)
enum class EDMActionRequest : uint8
{
    Move UMETA(DisplayName = "Move"),
    Sprint UMETA(DisplayName = "Sprint"),
    Jump UMETA(DisplayName = "Jump"),
    Attack UMETA(DisplayName = "Attack"),
    Defense UMETA(DisplayName = "Defense"),
    Interact UMETA(DisplayName = "Interact"),
    Skill UMETA(DisplayName = "Skill"),
    Emote UMETA(DisplayName = "Emote"),
    PoseChange UMETA(DisplayName = "Pose Change")
};

/**
 * 현재 Character가 수행 중인
 * 상호 배타적 주요 행동.
 *
 * 동시에 하나만 존재한다.
 */
UENUM(BlueprintType)
enum class EDMActionState : uint8
{
    None UMETA(DisplayName = "None"),
    Attack UMETA(DisplayName = "Attack"),
    Defense UMETA(DisplayName = "Defense"),
    Interact UMETA(DisplayName = "Interact"),
    Emote UMETA(DisplayName = "Emote"),
    FakeDeath UMETA(DisplayName = "Fake Death"),
    SkillCast UMETA(DisplayName = "Skill Cast")
};