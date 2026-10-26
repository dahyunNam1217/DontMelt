#pragma once

#include "CoreMinimal.h"

#include "Components/ActorComponent.h"
#include "Interaction/DMInteractionTypes.h"
#include "Framework/Match/DMMatchPlayerState.h"

#include "DMInteractionComponent.generated.h"

class ADMPlayerCharacter;
class ADMRelic;
class ADMRelicManager;
class ADMRelicChest;

DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnInteractionSessionChanged, const FDMInteractionSession&);

/**
 * PlayerCharacter의 공통 Interaction 파이프라인.
 *
 * 담당:
 * - 로컬 후보 탐색
 * - 서버 요청
 * - 진행 Session
 * - 거리/LOS/상태 검증
 * - 취소
 * - Relic Reservation 연결
 *
 * Relic 상태 변경 자체는 담당하지 않는다.
 */
UCLASS(ClassGroup = (DontMelt), meta = (BlueprintSpawnableComponent))
class DONTMELT_API UDMInteractionComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDMInteractionComponent();

#pragma region Public API

    /**
     * 로컬 Player 입력 진입점.
     */
    void RequestInteraction();

    /**
     * 로컬 UI에 표시할 상호작용 후보를 조회한다.
     * 서버 요청이나 상호작용 상태 변경은 수행하지 않는다.
     */
    AActor* GetLocalInteractionPromptCandidate();

    /**
     * 현재 열린 Chest에서 Space Confirm 입력을 전달한다.
     *
     * Client는 Chest Action 의도만 전달하고,
     * 실제 Steal/Deposit 판정은 서버가 한다.
     */
    void RequestChestAction(ADMRelicChest* Chest);

    /**
     * 이동 입력 등에 의해 현재 Interaction을 취소한다.
     */
    void RequestCancelInteraction();

    /**
     * 서버 Death Commit 전용 Interaction Cleanup.
     *
     * 진행 중 Session / Reservation만 정리한다.
     * DepositFailure / Death Relic Drop은 발생시키지 않는다.
     */
    void ForceCleanupForDeath();

    UFUNCTION(BlueprintPure, Category = "DM|Interaction")
    bool IsInteractionActive() const
    {
        return OwnerSession.IsActive();
    }

    /**
     * 서버 승인 대기 중 상태까지 포함해서
     * Gameplay Input을 막아야 하는지 반환한다.
     */
    bool IsInteractionInputLocked() const
    {
        return bLocalInteractionRequestPending || OwnerSession.IsActive();
    }

    UFUNCTION(BlueprintPure, Category = "DM|Interaction")
    float GetInteractionProgress01() const;

    const FDMInteractionSession& GetOwnerSession() const
    {
        return OwnerSession;
    }

    FDMOnInteractionSessionChanged& OnInteractionSessionChanged()
    {
        return InteractionSessionChangedDelegate;
    }

#pragma endregion

protected:
#pragma region Lifecycle

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

private:
#pragma region Local Candidate

    AActor* FindLocalInteractionCandidate(bool bWriteLog = true);

    void SubmitLocalInteractionRequest(AActor* TargetActor, bool bRequestChestAction);

#pragma endregion

#pragma region Server Request

    UFUNCTION(Server, Reliable)
    void ServerRequestInteraction(AActor* TargetActor, int32 RequestId, bool bRequestChestAction);

    UFUNCTION(Server, Reliable)
    void ServerCancelInteraction(int32 RequestId);

    UFUNCTION(Client, Reliable)
    void ClientRejectInteraction(int32 RequestId);

    UFUNCTION(Client, Reliable)
    void ClientFinishInteraction(int32 RequestId);

#pragma endregion

#pragma region Server Validation

    bool ValidateCommonServerRules(AActor* TargetActor, FString& OutFailureReason) const;

    bool HasServerLineOfSightTo(AActor* TargetActor) const;

    void ServerValidateActiveInteraction();

    void ServerHandleInteractionCompleted();

#pragma endregion

#pragma region Session

    void BeginServerInteraction(
        AActor* TargetActor,
        EDMInteractionType InteractionType,
        float Duration,
        ADMRelic* ReservedRelic,
        int32 ReservationId,
        int32 RequestId
    );

    /**
     * Interaction 진행 중 서버 검증 실패 처리.
     *
     * DepositRelic 실패라면
     * 운반 중 Relic을 바닥에 Drop한 뒤 종료한다.
     *
     * 단, 사용자가 Space를 놓은 단순 입력 취소는
     * 이 함수를 사용하지 않는다.
     */
    void EndServerInteractionAsFailure(const FString& Reason);

    void EndServerInteraction(bool bCompleted, const FString& Reason);

    UFUNCTION()
    void OnRep_OwnerSession();

#pragma endregion

#pragma region Helpers

    ADMPlayerCharacter* GetOwnerPlayerCharacter() const;

    ADMRelicManager* ResolveRelicManager();

    void SetServerMovementLocked(bool bLocked);

#pragma endregion

#pragma region Configuration

    /**
     * 기획서에 구체적인 유효 Interaction 거리는 아직 없다.
     *
     * 250cm는 개발용 초기값이며
     * 플레이테스트 후 조정한다.
     */
    UPROPERTY(EditDefaultsOnly, Category = "DM|Interaction", meta = (ClampMin = "50.0"))
    float InteractionRange = 250.0f;

    /**
     * Local Interaction 후보가
     * 플레이어 전방 어느 정도 안에 있어야 하는지 결정한다.
     *
     * 0.35는 약 전방 좌우 69도 범위다.
     *
     * Pitch는 검사하지 않으므로
     * 바닥의 Relic을 정확히 내려다볼 필요가 없다.
     */
    UPROPERTY(EditDefaultsOnly, Category = "DM|Interaction", meta = (ClampMin = "-1.0", ClampMax = "1.0"))
    float InteractionFacingDotThreshold = 0.35f;

    /**
     * Active Interaction 동안 서버가
     * 거리/LOS/대상 상태를 재검증하는 주기.
     *
     * Tick 대신 짧은 수명의 Timer를 사용한다.
     */
    UPROPERTY(EditDefaultsOnly, Category = "DM|Interaction", meta = (ClampMin = "0.05", ClampMax = "1.0"))
    float ServerValidationInterval = 0.1f;

#pragma endregion

#pragma region Replicated Owner Session

    UPROPERTY(ReplicatedUsing = OnRep_OwnerSession, VisibleInstanceOnly, Category = "DM|Interaction")
    FDMInteractionSession OwnerSession;

#pragma endregion

#pragma region Delegates

    FDMOnInteractionSessionChanged InteractionSessionChangedDelegate;

#pragma endregion

#pragma region Local State

    TWeakObjectPtr<AActor> LocalFocusedActor;

    int32 NextLocalRequestId = 1;

    int32 PendingRequestId = INDEX_NONE;

    bool bLocalInteractionRequestPending = false;

    bool bLocalCancelRequested = false;

#pragma endregion

#pragma region Server Runtime State

    TWeakObjectPtr<ADMRelicManager> CachedRelicManager;

    TWeakObjectPtr<ADMRelic> ActiveReservedRelic;

    int32 ActiveReservationId = INDEX_NONE;

    int32 LastServerProcessedRequestId = 0;

    FTimerHandle CompletionTimerHandle;

    FTimerHandle ValidationTimerHandle;

    bool bMovementModeSaved = false;

    uint8 SavedMovementMode = 0;

    uint8 SavedCustomMovementMode = 0;

#pragma endregion
};