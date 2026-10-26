#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "DMNPCPopulationDirector.generated.h"

class ADMNPCCharacter;
class ATargetPoint;

/**
 * Match 단위 NPC Population 서버 관리자.
 *
 * 책임:
 * - NPC Spawn Point 수집
 * - Match 시작 NPC 생성
 * - 실제 Spawn된 NPC Registry 관리
 * - Population 초기화 실패 시 생성 NPC 정리
 *
 * 개별 NPC의 행동 판단은 담당하지 않는다.
 * 해당 책임은 ADMJellyAIController가 담당한다.
 */
UCLASS()
class DONTMELT_API UDMNPCPopulationDirector : public UObject
{
    GENERATED_BODY()

public:
#pragma region Initialization

    /**
     * 서버에서 이번 Match의 NPC Population을 생성한다.
     *
     * InTargetNPCCount:
     * GameMode에서 이미 Snapshot된 최종 NPC 수.
     *
     * InNPCClass:
     * 실제 Spawn할 NPC Character Class.
     */
    bool InitializePopulation(int32 InTargetNPCCount, TSubclassOf<ADMNPCCharacter> InNPCClass);

    /**
     * Director가 생성한 NPC를 모두 제거하고
     * Runtime Registry를 초기화한다.
     *
     * Match 시작 초기화 실패 Rollback에 사용한다.
     */
    void ShutdownPopulation();

#pragma endregion

#pragma region Query

    /**
     * 현재 유효한 Registry NPC 수를 반환한다.
     */
    int32 GetRegisteredNPCCount() const;

#pragma endregion

#pragma region Reaction

    /**
     * 한 번의 NPC 오인 공격 사건을 처리한다.
     *
     * 직접 맞은 NPC는 항상 반응하며,
     * Registry의 주변 NPC 중 가까운 순서로
     * 제한된 수만 추가 반응시킨다.
     */
    void HandleWrongTargetAttack(
        const FVector& AttackOrigin,
        const TArray<ADMNPCCharacter*>& HitNPCTargets,
        float ReactionRadius,
        int32 MaxNearbyResponderCount,
        float FleeDistance
    );

    /**
     * Global Panic Event를
     * 현재 Registry의 모든 NPC에게 전달한다.
     */
    void HandlePanicEvent();

    /**
     * 실제 플레이어 사망 후 현재 경기가 계속될 때만 호출한다.
     * 근처 NPC를 거리순으로 선별하고 AIController에 비웃음 반응을 요청한다.
     */
    void HandleNearbyPlayerDeath(
        const FVector& DeathLocation, float ReactionRadius, int32 MaxResponderCount, float MockDuration
    );

#pragma endregion

private:
#pragma region Reaction Internal

    bool
    DispatchWrongTargetReaction(ADMNPCCharacter* NPCCharacter, const FVector& AttackOrigin, float FleeDistance) const;

#pragma endregion

#pragma region Spawn

    /**
     * 현재 World에서
     * DM_NPCSpawn Actor Tag를 가진 TargetPoint를 수집한다.
     */
    bool CollectSpawnPoints(TArray<ATargetPoint*>& OutSpawnPoints) const;

    /**
     * Registry에 존재하는 모든 유효 NPC를 Destroy한다.
     */
    void DestroyRegisteredNPCs();

#pragma endregion

#pragma region Runtime State

    /**
     * 이번 Match가 요구하는 NPC 수.
     *
     * GameMode의 ResolvedNPCCountSnapshot을
     * 초기화 시 한 번 전달받는다.
     */
    int32 TargetNPCCount = 0;

    /**
     * 실제 서버에서 Spawn된 NPC Registry.
     *
     * Actor 수명 자체는 World가 관리하므로
     * Registry는 강한 소유권이 필요하지 않다.
     */
    TArray<TWeakObjectPtr<ADMNPCCharacter>> RegisteredNPCs;

#pragma endregion
};