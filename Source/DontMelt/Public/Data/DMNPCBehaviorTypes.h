#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "DMNPCBehaviorTypes.generated.h"

/**
 * NPC의 일반 위장 행동 선택에 사용하는 데이터 프로필.
 *
 * DataTable의 RowName을 Behavior Profile ID의
 * 단일 원본으로 사용한다.
 *
 * 이 Struct는 정의 데이터만 보관하며
 * NPC의 현재 런타임 행동 상태를 저장하지 않는다.
 */
USTRUCT(BlueprintType)
struct DONTMELT_API FDMNPCBehaviorProfileRow : public FTableRowBase
{
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|AI|Behavior", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float WalkWeight = 40.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|AI|Behavior", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float IdleWeight = 10.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|AI|Behavior", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float SprintWeight = 15.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|AI|Behavior", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float JumpWeight = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|AI|Behavior", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float EmoteWeight = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|AI|Behavior", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float FakeAttackWeight = 5.0f;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|AI|Behavior", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float RelicApproachWeight = 20.0f;

    /**
     * Idle 행동을 선택했을 때
     * 다음 행동 선택까지 기다리는 시간.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|AI|Behavior", meta = (ClampMin = "0.1", UIMin = "0.1"))
    float IdleDuration = 1.5f;

    /**
     * 기본 군중 이동 목적지를 탐색할 반경.
     *
     * 단위는 Unreal Unit(cm).
     */
    UPROPERTY(
        EditAnywhere, BlueprintReadOnly, Category = "DM|AI|Behavior", meta = (ClampMin = "100.0", UIMin = "100.0")
    )
    float RoamRadius = 1000.0f;

    /**
     * Walk / Sprint Nav 이동의
     * 목적지 도착 허용 반경.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|AI|Behavior", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float MoveAcceptanceRadius = 80.0f;

    /**
     * NPC가 유물 또는 상자 앞에 도착한 뒤
     * 실제 상호작용을 하는 척 머무르는 시간.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|AI|Behavior", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float RelicApproachWaitDuration = 2.0f;

    /**
     * Global Panic 명령 수신 후
     * 실제 반응을 시작하기 전 최소 지연.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|AI|Panic", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float PanicDelayMin = 1.0f;

    /**
     * Global Panic 반응 최대 지연.
     */
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DM|AI|Panic", meta = (ClampMin = "0.0", UIMin = "0.0"))
    float PanicDelayMax = 3.0f;

    /**
     * Panic 시작 시 Scream Presentation을
     * 요청할 확률. 0~1.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|AI|Panic",
        meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0")
    )
    float PanicScreamChance = 0.35f;

    /**
     * Panic 시작 시 Jump할 확률. 0~1.
     */
    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category = "DM|AI|Panic",
        meta = (ClampMin = "0.0", ClampMax = "1.0", UIMin = "0.0", UIMax = "1.0")
    )
    float PanicJumpChance = 0.25f;
};