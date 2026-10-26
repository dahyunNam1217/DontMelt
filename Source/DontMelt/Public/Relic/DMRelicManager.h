#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Relic/DMRelicTypes.h"
#include "DMRelicManager.generated.h"

class ADMMatchPlayerState;
class ADMRelic;
class ADMRelicChest;
class ADMRelicSpawnPoint;
class ADMPlayerCharacter;
class UDMInteractionComponent;
class UDMRelicRuleDataAsset;

struct FDMRelicRevealEntry;

DECLARE_DELEGATE(FDMOnRelicDepositCommitted);

DECLARE_DELEGATE_OneParam(FDMOnRelicStolen, ADMMatchPlayerState*);

struct FDMRelicInteractionReservation
{
    TWeakObjectPtr<UDMInteractionComponent> OwnerComponent;

    int32 ReservationId = INDEX_NONE;

    int32 RequestId = INDEX_NONE;
};

/**
 * 수동 Drop 직후
 * 같은 플레이어의 동일 Relic 재습득 제한.
 *
 * 서버 전용 런타임 데이터.
 */
struct FDMRelicRepickupRestriction
{
    TWeakObjectPtr<ADMMatchPlayerState> BlockedPlayerState;

    double UnlockServerTime = 0.0;
};

/**
 * Match 동안 모든 Relic을 중앙에서 조회하고 관리하는
 * 서버 전용 Manager.
 *
 * 개별 Relic의 WorldState는 복사해서 저장하지 않는다.
 * 실제 상태의 SSOT는 ADMRelic이다.
 */
UCLASS()
class DONTMELT_API ADMRelicManager : public AActor
{
    GENERATED_BODY()

public:
    ADMRelicManager();

#pragma region Initialization

    /**
     * Starting Roster 확정 후 GameMode가 한 번 호출한다.
     *
     * 참가 진영마다 Registered Relic 1개,
     * PublicStart에 공용 Relic 1개를 생성한다.
     */
    bool InitializeMatch(
        int32 StartingPlayerCount,
        int32 ExpectedTotalRelicCount,
        UDMRelicRuleDataAsset* InRelicRuleData,
        TSubclassOf<ADMRelic> InRelicClass,
        const TArray<ADMMatchPlayerState*>& MatchPlayers
    );

#pragma endregion

#pragma region Events

    /**
     * 서버에서 Relic Deposit Commit이 완전히 끝난 뒤
     * MatchGameMode에게 결과 재평가를 요청하는 알림.
     *
     * 승리 판정 자체는 RelicManager가 하지 않는다.
     */
    FDMOnRelicDepositCommitted& OnRelicDepositCommitted()
    {
        return RelicDepositCommittedDelegate;
    }

    FDMOnRelicStolen& OnRelicStolen()
    {
        return RelicStolenDelegate;
    }

#pragma endregion

#pragma region Queries

    /**
     * 특정 Base에 현재 등록 완료되어 있는 Relic 수를 계산한다.
     *
     * 별도 Count Cache를 저장하지 않고
     * 최대 5개의 Relic을 직접 조회한다.
     */
    int32 GetRegisteredRelicCount(int32 BaseSlot) const;

    int32 GetTotalRelicCount() const
    {
        return Relics.Num();
    }

    /**
     * 현재 Active Base에 연결된 Chest만 반환한다.
     *
     * 내부 Registry의 소유권은 RelicManager에 유지되며
     * 호출자는 임시 조회 결과만 받는다.
     */
    void GetActiveRelicChests(TArray<ADMRelicChest*>& OutChests) const;

    /**
     * 현재 월드에 실제 노출되어 있는 Relic만 반환한다.
     *
     * PublicSpawn / Dropped만 포함한다.
     */
    void GetWorldVisibleRelics(TArray<ADMRelic*>& OutRelics) const;

    /** 서버에서 현재 전체 유물의 위치를 완전한 Snapshot으로 수집한다. */
    bool BuildRelicRevealEntries(TArray<FDMRelicRevealEntry>& OutEntries) const;

    /**
     * RelicId로 Relic을 찾는다.
     *
     * 최대 5개이므로 별도의 ID Map을 만들지 않는다.
     */
    ADMRelic* FindRelicById(int32 RelicId) const;

    /**
     * 해당 PlayerState가 현재 운반 중인 Relic을 반환한다.
     *
     * Carrier 관계의 SSOT는 RelicManager의 Carrier Map이다.
     */
    ADMRelic* GetCarriedRelic(ADMMatchPlayerState* PlayerState) const;

    /**
     * 해당 Relic을 현재 운반 중인 PlayerState를 반환한다.
     */
    ADMMatchPlayerState* GetRelicCarrier(ADMRelic* Relic) const;

    /**
     * 해당 PlayerState가 현재 Relic을 운반 중인지 반환한다.
     */
    bool IsPlayerCarryingRelic(ADMMatchPlayerState* PlayerState) const
    {
        return GetCarriedRelic(PlayerState) != nullptr;
    }

#pragma endregion

#pragma region Interaction

    /**
     * Target을 현재 Player가 어떤 Interaction으로
     * 취급할지 서버에서 결정한다.
     */
    bool ResolveInteractionRequest(
        const ADMPlayerCharacter* Interactor,
        AActor* TargetActor,
        const UDMInteractionComponent* RequestingComponent,
        bool bRequestChestAction,
        EDMInteractionType& OutInteractionType,
        float& OutDuration,
        ADMRelic*& OutReservationRelic,
        FString& OutFailureReason
    ) const;

    bool TryAcquireInteractionReservation(
        ADMRelic* Relic, UDMInteractionComponent* OwnerComponent, int32 RequestId, int32& OutReservationId
    );

    bool IsInteractionReservationOwnedBy(
        ADMRelic* Relic, const UDMInteractionComponent* OwnerComponent, int32 ReservationId
    ) const;

    void ReleaseInteractionReservation(ADMRelic* Relic, UDMInteractionComponent* OwnerComponent, int32 ReservationId);

    /**
     * Chest Inspect 완료 시 서버 기준 RegisteredRelicCount를 계산한다.
     *
     * Count를 저장하지 않고 현재 Relic 상태를 직접 조회한다.
     */
    bool TryInspectChest(
        const ADMPlayerCharacter* Interactor,
        const ADMRelicChest* Chest,
        int32& OutBaseSlot,
        int32& OutRegisteredRelicCount,
        FString& OutFailureReason
    ) const;

    /**
     * PickupRelic / StealRelic의 최종 서버 Commit.
     *
     * 모든 유물 획득 경로는 이 함수에서
     * 최종적으로 Carried 상태로 변경된다.
     */
    bool TryCommitRelicAcquisition(
        ADMPlayerCharacter* Interactor,
        ADMRelic* Relic,
        EDMInteractionType InteractionType,
        const ADMRelicChest* SourceChest,
        FString& OutFailureReason
    );

    /**
     * Carrier가 운반 중인 Relic을 하나의 서버 Commit으로 Dropped로 전환한다.
     *
     * Manual / Death / Disconnect 모두 이 함수만 사용한다.
     */
    bool TryDropCarriedRelic(
        ADMMatchPlayerState* CarrierPlayerState,
        ADMPlayerCharacter* CarrierCharacter,
        EDMRelicDropReason DropReason,
        FString& OutFailureReason
    );

    /**m
     * 운반 중인 Relic을 자신의 Active Chest에 등록한다.
     *
     * 성공 시:
     * Carried -> Registered
     * Carrier 관계 제거
     * Owner Carry Projection 해제
     * RegisteredBaseSlot 설정
     */
    bool TryCommitRelicDeposit(
        ADMPlayerCharacter* Interactor,
        ADMRelic* Relic,
        const ADMRelicChest* DestinationChest,
        FString& OutFailureReason
    );

    void StartDepositProtection(ADMRelic* Relic);

    void FinishDepositProtection(TWeakObjectPtr<ADMRelic> Relic);

#pragma endregion

private:
#pragma region Initialization Internal

    bool CacheWorldActors();

    bool ValidateMatchSetup(
        int32 StartingPlayerCount, const TArray<ADMMatchPlayerState*>& MatchPlayers, FString& OutFailureReason
    ) const;

    bool SpawnInitialRelics(
        const TArray<ADMMatchPlayerState*>& MatchPlayers, int32 ExpectedTotalRelicCount, FString& OutFailureReason
    );

    bool FindUniqueChestByBaseSlot(int32 BaseSlot, ADMRelicChest*& OutChest) const;

    bool FindUniquePublicStartPoint(ADMRelicSpawnPoint*& OutSpawnPoint) const;

    ADMRelic* SpawnRelic(const FTransform& SpawnTransform, EDMRelicWorldState InitialState, int32 RegisteredBaseSlot);

    /**
     * 초기화 도중 실패했을 때
     * 이미 생성된 Relic을 모두 제거한다.
     */
    void RollbackSpawnedRelics();

    ADMRelic* FindStealableRelicForBase(int32 BaseSlot, const UDMInteractionComponent* RequestingComponent) const;

    bool
    IsSamePlayerRepickupBlocked(ADMMatchPlayerState* PlayerState, ADMRelic* Relic, double& OutRemainingSeconds) const;

    FTransform BuildDropTransform(
        const ADMPlayerCharacter* CarrierCharacter, const ADMRelic* Relic, EDMRelicDropReason DropReason
    ) const;

    /**
     * 수동 Drop 위치의 Relic Bounds가
     * Relic Chest Bounds와 겹치는지 검사한다.
     */
    bool IsManualDropOverlappingRelicChest(
        const FTransform& DropTransform, const ADMRelic* Relic, ADMRelicChest*& OutBlockingChest
    ) const;

#pragma endregion

#pragma region Recovery Internal

    /**
     * Match 동안 서버가 일정 주기로
     * PublicSpawn / Dropped Relic의 위치를 검증한다.
     */
    void StartRelicRecoveryMonitor();

    /**
     * 현재 월드에 노출된 Relic들의 위치를 검사한다.
     */
    void CheckRelicsForRecovery();

    /**
     * 특정 Transform이 Relic을 둘 수 있는
     * 기본적인 유효 월드 위치인지 검사한다.
     */
    bool IsRelicWorldTransformValid(const FTransform& Transform, FString& OutFailureReason) const;

    /**
     * Relic이 마지막 정상 위치에서
     * 비정상적으로 이동했는지 검사한다.
     */
    bool HasRelicMovedUnexpectedly(const ADMRelic* Relic, float& OutDistance) const;

    /**
     * 기준 위치와 가장 가까운 Recovery SpawnPoint를 찾는다.
     *
     * Recovery가 하나도 유효하지 않으면
     * PublicStart를 중앙 fallback으로 사용한다.
     */
    bool FindRecoveryTransform(
        const FVector& ReferenceLocation, FTransform& OutRecoveryTransform, FString& OutRecoverySource
    ) const;

    /**
     * 유효하지 않은 PublicSpawn / Dropped Relic을
     * 검증된 SpawnPoint로 실제 이동시킨다.
     */
    bool RecoverWorldRelic(ADMRelic* Relic, const FString& RecoveryReason);

#pragma endregion

#pragma region Runtime State

    /**
     * 실제 Relic Actor Registry.
     *
     * Relic의 WorldState를 별도로 복사하지 않는다.
     */
    UPROPERTY()
    TArray<TObjectPtr<ADMRelic>> Relics;

    /**
     * 맵에 배치된 Chest 참조.
     * Match 초기화 때 한 번 수집한다.
     */
    UPROPERTY()
    TArray<TObjectPtr<ADMRelicChest>> RelicChests;

    /**
     * 맵에 배치된 RelicSpawnPoint 참조.
     */
    UPROPERTY()
    TArray<TObjectPtr<ADMRelicSpawnPoint>> RelicSpawnPoints;

    UPROPERTY()
    TSubclassOf<ADMRelic> RelicClass;

    int32 NextRelicId = 0;

    bool bInitialized = false;

    UPROPERTY()
    TObjectPtr<UDMRelicRuleDataAsset> RelicRuleData = nullptr;

    /**
     * 개별 Relic별 현재 Interaction Reservation.
     *
     * Relic 자신의 상태로 저장하지 않는다.
     */
    TMap<TWeakObjectPtr<ADMRelic>, FDMRelicInteractionReservation> InteractionReservations;

    int32 NextReservationId = 1;

    /**
     * Relic -> Carrier 관계.
     *
     * Carrier 관계의 서버 권위 원본.
     */
    TMap<TWeakObjectPtr<ADMRelic>, TWeakObjectPtr<ADMMatchPlayerState>> RelicToCarrier;

    /**
     * Carrier -> Relic 역방향 Index.
     *
     * 한 PlayerState당 최대 하나의 Relic만 존재한다.
     */
    TMap<TWeakObjectPtr<ADMMatchPlayerState>, TWeakObjectPtr<ADMRelic>> CarrierToRelic;

    /**
     * 수동 Drop 직후 같은 플레이어의
     * 동일 Relic 재습득 제한.
     *
     * Tick/Timer를 사용하지 않고
     * ServerTime 비교로 만료 여부를 판정한다.
     */
    TMap<TWeakObjectPtr<ADMRelic>, FDMRelicRepickupRestriction> RepickupRestrictions;

    TMap<TWeakObjectPtr<ADMRelic>, FTimerHandle> DepositProtectionTimers;

    /**
     * Relic 위치 오류를 검사하는 서버 반복 Timer.
     *
     * Tick을 사용하지 않는다.
     */
    FTimerHandle RelicRecoveryCheckTimer;

    /**
     * Deposit Commit 완료 알림.
     *
     * RelicManager는 상태 변경까지만 담당하고
     * Match 종료 여부는 GameMode가 판단한다.
     */
    FDMOnRelicDepositCommitted RelicDepositCommittedDelegate;

    FDMOnRelicStolen RelicStolenDelegate;

#pragma endregion
};