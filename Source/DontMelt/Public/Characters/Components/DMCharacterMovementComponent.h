#pragma once

#include "CoreMinimal.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "DMCharacterMovementComponent.generated.h"

DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnAuthoritativeSprintStateChanged, bool);

/**
 * Don't Melt Character 전용 MovementComponent.
 *
 * 기본 CharacterMovement의 네트워크 Prediction을 유지하면서
 * 프로젝트 전용 이동 의도(Sprint)를 SavedMove에 확장한다.
 */
UCLASS()
class DONTMELT_API UDMCharacterMovementComponent : public UCharacterMovementComponent
{
    GENERATED_BODY()

public:
    void SetSprintRequested(bool bRequested);

    bool IsSprintRequested() const;

    FDMOnAuthoritativeSprintStateChanged& OnAuthoritativeSprintStateChanged()
    {
        return AuthoritativeSprintStateChangedDelegate;
    }

    virtual float GetMaxSpeed() const override;
    virtual bool CanAttemptJump() const override;
    virtual FVector ConstrainAnimRootMotionVelocity(
        const FVector& RootMotionVelocity, const FVector& CurrentVelocity) const override;
#if WITH_EDITOR
    virtual void OnCharacterStuckInGeometry(const FHitResult* Hit) override;
    virtual void HandleImpact(const FHitResult& Hit, float TimeSlice = 0.f,
        const FVector& MoveDelta = FVector::ZeroVector) override;
#endif

    virtual FNetworkPredictionData_Client* GetPredictionData_Client() const override;

protected:
    virtual void OnMovementUpdated(float DeltaSeconds, const FVector& OldLocation, const FVector& OldVelocity) override;

    virtual void UpdateFromCompressedFlags(uint8 Flags) override;

private:
#if WITH_EDITOR
    // 충돌 처리에는 사용하지 않는 진단 출력 제한 상태.
    double NPCDiagnosticLastImpactTime = -1.0;
    uint32 NPCDiagnosticSuppressedImpacts = 0;
#endif
    bool IsAuthoritativeSprintActive() const;
    bool IsEmoteMovementLocked() const;

    bool bWasAuthoritativeSprinting = false;

    FDMOnAuthoritativeSprintStateChanged AuthoritativeSprintStateChangedDelegate;

    /**
     * 현재 Sprint 이동 요청.
     *
     * Replicated Property가 아니며
     * CharacterMovement SavedMove를 통해 서버에 전달된다.
     */
    bool bWantsToSprint = false;
};