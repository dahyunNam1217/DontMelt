#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DMSizeAmplificationComponent.generated.h"

USTRUCT()
struct FDMSizeAmplificationRepState
{
    GENERATED_BODY()

    UPROPERTY()
    double StartServerTime = 0.0;

    UPROPERTY()
    double EndServerTime = 0.0;

    UPROPERTY()
    float TargetScale = 1.0f;

    UPROPERTY()
    float TransitionDuration = 0.0f;

    bool IsActive() const
    {
        return EndServerTime > StartServerTime && TargetScale > 1.0f;
    }
};

/**
 * Player 전용 크기 증폭의 Capsule·Mesh·Camera 전환을 담당한다.
 * Gameplay 활성 여부의 원본은 CharacterStateComponent의 Status.SizeAmplified다.
 */
UCLASS(ClassGroup = (DontMelt), meta = (BlueprintSpawnableComponent))
class DONTMELT_API UDMSizeAmplificationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDMSizeAmplificationComponent();

    /** 서버에서 공간 검증 후 크기 증폭을 시작한다. */
    bool TryStartSizeAmplification(float Duration, float TargetScale, float TransitionDuration);

    /** 서버에서 상태와 표현을 즉시 원본 크기로 복구한다. */
    bool StopSizeAmplification(bool bRemoveStatus);

protected:
    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

private:
    UFUNCTION()
    void OnRep_SizeState();

    UFUNCTION(NetMulticast, Reliable)
    void MulticastRestoreOriginalSize();

    void HandleStatusChanged();
    void UpdateSizeTransition();
    void StartTransitionTimer();
    void StopTransitionTimer();
    void CacheOriginalValues();
    bool RestoreOriginalValues();
    bool TryApplyCapsuleDimensions(float Radius, float HalfHeight, bool bAllowVerifiedFallback);
    bool RollbackCapsuleTransition(
        const FVector& PreviousLocation, const FQuat& PreviousRotation, float PreviousRadius, float PreviousHalfHeight
    );
    void ResetAutonomousProxyPrediction();
    bool CanUseCapsuleSize(float Radius, float HalfHeight, float CenterOffsetZ) const;
    bool ApplyScaleAlpha(float ScaleAlpha, bool bCheckCollision);
    float CalculateScaleAtServerTime(double ServerTime) const;

    UPROPERTY(ReplicatedUsing = OnRep_SizeState, Transient)
    FDMSizeAmplificationRepState SizeState;

    FTransform OriginalMeshRelativeTransform = FTransform::Identity;
    float OriginalCapsuleRadius = 0.0f;
    float OriginalCapsuleHalfHeight = 0.0f;
    bool bOriginalValuesCached = false;
    bool bStoppingEffect = false;
    FTimerHandle TransitionTimerHandle;
};
