#include "Characters/Components/DMSizeAmplificationComponent.h"

#include "Characters/DMPlayerCharacter.h"
#include "Characters/Components/DMCharacterStateComponent.h"
#include "Core/DMGameplayTags.h"
#include "Core/DMGameplayUtils.h"

#include "Components/CapsuleComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

namespace
{
    constexpr float SizeTransitionUpdateInterval = 1.0f / 30.0f;
    constexpr float CapsuleSizeTolerance = 0.1f;
    constexpr float CapsuleLocationTolerance = 1.0f;
}

UDMSizeAmplificationComponent::UDMSizeAmplificationComponent()
{
    PrimaryComponentTick.bCanEverTick = false;
    SetIsReplicatedByDefault(true);
}

void UDMSizeAmplificationComponent::BeginPlay()
{
    Super::BeginPlay();
    CacheOriginalValues();

    if (ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner()))
    {
        if (UDMCharacterStateComponent* StateComponent = OwnerCharacter->GetCharacterStateComponent())
        {
            StateComponent->OnStatusChanged().AddUObject(this, &ThisClass::HandleStatusChanged);
        }
    }

    OnRep_SizeState();
}

void UDMSizeAmplificationComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner()))
    {
        if (UDMCharacterStateComponent* StateComponent = OwnerCharacter->GetCharacterStateComponent())
        {
            StateComponent->OnStatusChanged().RemoveAll(this);
        }
    }

    StopTransitionTimer();
    RestoreOriginalValues();
    Super::EndPlay(EndPlayReason);
}

void UDMSizeAmplificationComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);
    DOREPLIFETIME(UDMSizeAmplificationComponent, SizeState);
}

bool UDMSizeAmplificationComponent::TryStartSizeAmplification(
    const float Duration, const float TargetScale, const float TransitionDuration
)
{
    ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());
    UDMCharacterStateComponent* StateComponent =
        IsValid(OwnerCharacter) ? OwnerCharacter->GetCharacterStateComponent() : nullptr;

    if (!IsValid(OwnerCharacter) || !OwnerCharacter->HasAuthority() || !IsValid(StateComponent)
        || Duration <= 0.0f || TargetScale <= 1.0f || TransitionDuration <= 0.0f
        || Duration < TransitionDuration * 2.0f || SizeState.IsActive()
        || StateComponent->HasStatus(DMGameplayTags::Status_SizeAmplified))
    {
        return false;
    }

    CacheOriginalValues();
    if (!bOriginalValuesCached)
    {
        return false;
    }

    const float TargetRadius = OriginalCapsuleRadius * TargetScale;
    const float TargetHalfHeight = OriginalCapsuleHalfHeight * TargetScale;
    const float CenterOffsetZ = TargetHalfHeight - OriginalCapsuleHalfHeight;

    if (!CanUseCapsuleSize(TargetRadius, TargetHalfHeight, CenterOffsetZ))
    {
        return false;
    }

    if (!StateComponent->ApplyTimedStatus(DMGameplayTags::Status_SizeAmplified, Duration))
    {
        return false;
    }

    SizeState.StartServerTime = DMGameplay::GetServerTimeSeconds(this);
    SizeState.EndServerTime = SizeState.StartServerTime + static_cast<double>(Duration);
    SizeState.TargetScale = TargetScale;
    SizeState.TransitionDuration = TransitionDuration;

    ApplyScaleAlpha(1.0f, false);
    StartTransitionTimer();
    OwnerCharacter->ForceNetUpdate();
    return true;
}

bool UDMSizeAmplificationComponent::StopSizeAmplification(const bool bRemoveStatus)
{
    ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());
    if (!IsValid(OwnerCharacter) || !OwnerCharacter->HasAuthority() || bStoppingEffect)
    {
        return false;
    }

    TGuardValue<bool> StopGuard(bStoppingEffect, true);
    StopTransitionTimer();
    const bool bRestoredOriginalValues = RestoreOriginalValues();

    if (bRemoveStatus)
    {
        if (UDMCharacterStateComponent* StateComponent = OwnerCharacter->GetCharacterStateComponent();
            IsValid(StateComponent) && StateComponent->HasStatus(DMGameplayTags::Status_SizeAmplified))
        {
            StateComponent->RemoveStatus(DMGameplayTags::Status_SizeAmplified);
        }
    }

    SizeState = FDMSizeAmplificationRepState();
    MulticastRestoreOriginalSize();
    OwnerCharacter->ForceNetUpdate();
    return bRestoredOriginalValues;
}

void UDMSizeAmplificationComponent::OnRep_SizeState()
{
    CacheOriginalValues();
    ResetAutonomousProxyPrediction();

    if (!SizeState.IsActive())
    {
        StopTransitionTimer();
        RestoreOriginalValues();
        return;
    }

    UpdateSizeTransition();
}

void UDMSizeAmplificationComponent::MulticastRestoreOriginalSize_Implementation()
{
    StopTransitionTimer();
    RestoreOriginalValues();
    ResetAutonomousProxyPrediction();
}

void UDMSizeAmplificationComponent::HandleStatusChanged()
{
    ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());
    UDMCharacterStateComponent* StateComponent =
        IsValid(OwnerCharacter) ? OwnerCharacter->GetCharacterStateComponent() : nullptr;

    if (IsValid(OwnerCharacter) && OwnerCharacter->HasAuthority() && IsValid(StateComponent)
        && !StateComponent->HasStatus(DMGameplayTags::Status_SizeAmplified) && SizeState.IsActive())
    {
        StopSizeAmplification(false);
    }
}

void UDMSizeAmplificationComponent::UpdateSizeTransition()
{
    if (!SizeState.IsActive())
    {
        StopTransitionTimer();
        RestoreOriginalValues();
        return;
    }

    const double ServerTime = DMGameplay::GetServerTimeSeconds(this);
    if (ServerTime >= SizeState.EndServerTime)
    {
        if (AActor* OwnerActor = GetOwner(); IsValid(OwnerActor) && OwnerActor->HasAuthority())
        {
            StopSizeAmplification(true);
        }
        else
        {
            StopTransitionTimer();
            RestoreOriginalValues();
        }
        return;
    }

    const float Scale = CalculateScaleAtServerTime(ServerTime);
    const bool bCheckCollision = GetOwner() != nullptr && GetOwner()->HasAuthority();

    if (!ApplyScaleAlpha(Scale, bCheckCollision) && bCheckCollision)
    {
        StopSizeAmplification(true);
        return;
    }

    const double Elapsed = ServerTime - SizeState.StartServerTime;
    const double Remaining = SizeState.EndServerTime - ServerTime;

    if (Elapsed < static_cast<double>(SizeState.TransitionDuration)
        || Remaining <= static_cast<double>(SizeState.TransitionDuration))
    {
        StartTransitionTimer();
        return;
    }

    // 최대 크기 유지 구간에는 반복 갱신하지 않고 축소 시작 시점만 예약한다.
    StopTransitionTimer();
    if (UWorld* World = GetWorld())
    {
        const float DelayUntilShrink = static_cast<float>(Remaining - SizeState.TransitionDuration);
        World->GetTimerManager().SetTimer(
            TransitionTimerHandle, this, &ThisClass::UpdateSizeTransition, FMath::Max(DelayUntilShrink, 0.001f), false
        );
    }
}

void UDMSizeAmplificationComponent::StartTransitionTimer()
{
    UWorld* World = GetWorld();
    if (!IsValid(World) || World->GetTimerManager().IsTimerActive(TransitionTimerHandle))
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        TransitionTimerHandle, this, &ThisClass::UpdateSizeTransition, SizeTransitionUpdateInterval, false
    );
}

void UDMSizeAmplificationComponent::StopTransitionTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(TransitionTimerHandle);
    }
    TransitionTimerHandle.Invalidate();
}

void UDMSizeAmplificationComponent::CacheOriginalValues()
{
    if (bOriginalValuesCached)
    {
        return;
    }

    const ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());
    const UCapsuleComponent* Capsule = IsValid(OwnerCharacter) ? OwnerCharacter->GetCapsuleComponent() : nullptr;
    const USkeletalMeshComponent* Mesh = IsValid(OwnerCharacter) ? OwnerCharacter->GetMesh() : nullptr;

    if (!IsValid(Capsule) || !IsValid(Mesh))
    {
        return;
    }

    OriginalCapsuleRadius = Capsule->GetUnscaledCapsuleRadius();
    OriginalCapsuleHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
    OriginalMeshRelativeTransform = Mesh->GetRelativeTransform();
    bOriginalValuesCached = OriginalCapsuleRadius > 0.0f && OriginalCapsuleHalfHeight > 0.0f;
}

bool UDMSizeAmplificationComponent::RestoreOriginalValues()
{
    if (!bOriginalValuesCached)
    {
        return false;
    }

    ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());
    if (!IsValid(OwnerCharacter))
    {
        return false;
    }

    UCapsuleComponent* Capsule = OwnerCharacter->GetCapsuleComponent();
    USkeletalMeshComponent* Mesh = OwnerCharacter->GetMesh();

    bool bCapsuleRestored = false;

    if (IsValid(Capsule))
    {
        if (OwnerCharacter->HasAuthority())
        {
            bCapsuleRestored = TryApplyCapsuleDimensions(
                OriginalCapsuleRadius, OriginalCapsuleHalfHeight, true
            );

            if (!bCapsuleRestored)
            {
                /*
                 * 기준 위치까지 복구할 수 없어도 확대된 충돌 형상을 사망·교체 이후 남기지 않는다.
                 * 현재 확대 Capsule이 있던 중심은 더 작은 원본 Capsule에도 안전한 위치다.
                 */
                Capsule->SetCapsuleSize(OriginalCapsuleRadius, OriginalCapsuleHalfHeight, true);
            }
        }
        else
        {
            // 위치 기준점은 서버 CharacterMovement 보정이 전달하고 Proxy는 충돌 형상만 맞춘다.
            Capsule->SetCapsuleSize(OriginalCapsuleRadius, OriginalCapsuleHalfHeight, true);
            bCapsuleRestored = true;
        }
    }

    if (IsValid(Mesh))
    {
        Mesh->SetRelativeTransform(OriginalMeshRelativeTransform);
    }

    OwnerCharacter->ApplySizeAmplificationCameraScale(1.0f);
    return bCapsuleRestored;
}

bool UDMSizeAmplificationComponent::TryApplyCapsuleDimensions(
    const float Radius, const float HalfHeight, const bool bAllowVerifiedFallback
)
{
    ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());
    UCapsuleComponent* Capsule = IsValid(OwnerCharacter) ? OwnerCharacter->GetCapsuleComponent() : nullptr;

    if (!IsValid(OwnerCharacter) || !OwnerCharacter->HasAuthority() || !IsValid(Capsule))
    {
        return false;
    }

    const FVector PreviousLocation = Capsule->GetComponentLocation();
    const FQuat PreviousRotation = Capsule->GetComponentQuat();
    const float PreviousRadius = Capsule->GetUnscaledCapsuleRadius();
    const float PreviousHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
    const float CenterDelta = HalfHeight - PreviousHalfHeight;
    const FVector TargetLocation = PreviousLocation + FVector(0.0f, 0.0f, CenterDelta);

    if (!CanUseCapsuleSize(Radius, HalfHeight, CenterDelta))
    {
        return false;
    }

    const bool bShrinking = HalfHeight < PreviousHalfHeight || Radius < PreviousRadius;
    if (bShrinking)
    {
        // 축소 후 아래로 이동해야 기존 큰 Capsule의 바닥 충돌에 Sweep이 막히지 않는다.
        Capsule->SetCapsuleSize(Radius, HalfHeight, true);
    }

    FHitResult Hit;
    Capsule->MoveComponent(
        TargetLocation - PreviousLocation,
        PreviousRotation,
        true,
        &Hit,
        EMoveComponentFlags::MOVECOMP_NoFlags,
        ETeleportType::TeleportPhysics
    );

    const bool bReachedTarget = Capsule->GetComponentLocation().Equals(TargetLocation, CapsuleLocationTolerance);
    if (Hit.bBlockingHit || !bReachedTarget)
    {
        const bool bRolledBack = RollbackCapsuleTransition(
            PreviousLocation, PreviousRotation, PreviousRadius, PreviousHalfHeight
        );

        if (!bRolledBack || !bAllowVerifiedFallback
            || !CanUseCapsuleSize(Radius, HalfHeight, CenterDelta))
        {
            return false;
        }

        /*
         * Sweep이 바닥 접촉으로 부분 이동했지만 목표 형상이 별도 Overlap 검사에서 안전하면
         * CharacterMovement의 Crouch/UnCrouch와 같은 방식으로 검증된 위치를 확정한다.
         */
        Capsule->SetCapsuleSize(Radius, HalfHeight, true);
        Capsule->MoveComponent(
            TargetLocation - Capsule->GetComponentLocation(),
            PreviousRotation,
            false,
            nullptr,
            EMoveComponentFlags::MOVECOMP_NoFlags,
            ETeleportType::TeleportPhysics
        );

        if (!Capsule->GetComponentLocation().Equals(TargetLocation, CapsuleLocationTolerance))
        {
            // 원본 크기는 반드시 복구하되 안전했던 현재 위치를 유지한다.
            Capsule->SetCapsuleSize(Radius, HalfHeight, true);
            return false;
        }

        return true;
    }

    if (!bShrinking)
    {
        Capsule->SetCapsuleSize(Radius, HalfHeight, true);
    }
    return true;
}

bool UDMSizeAmplificationComponent::RollbackCapsuleTransition(
    const FVector& PreviousLocation,
    const FQuat& PreviousRotation,
    const float PreviousRadius,
    const float PreviousHalfHeight
)
{
    ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());
    UCapsuleComponent* Capsule = IsValid(OwnerCharacter) ? OwnerCharacter->GetCapsuleComponent() : nullptr;

    if (!IsValid(Capsule))
    {
        return false;
    }

    const float CurrentRadius = Capsule->GetUnscaledCapsuleRadius();
    const float CurrentHalfHeight = Capsule->GetUnscaledCapsuleHalfHeight();
    const bool bCurrentShapeFitsInsidePrevious = CurrentRadius <= PreviousRadius + CapsuleSizeTolerance
        && CurrentHalfHeight <= PreviousHalfHeight + CapsuleSizeTolerance;

    if (!bCurrentShapeFitsInsidePrevious)
    {
        Capsule->SetCapsuleSize(PreviousRadius, PreviousHalfHeight, true);
    }

    Capsule->MoveComponent(
        PreviousLocation - Capsule->GetComponentLocation(),
        PreviousRotation,
        false,
        nullptr,
        EMoveComponentFlags::MOVECOMP_NoFlags,
        ETeleportType::TeleportPhysics
    );

    if (bCurrentShapeFitsInsidePrevious)
    {
        Capsule->SetCapsuleSize(PreviousRadius, PreviousHalfHeight, true);
    }

    return Capsule->GetComponentLocation().Equals(PreviousLocation, CapsuleLocationTolerance)
        && Capsule->GetComponentQuat().Equals(PreviousRotation, KINDA_SMALL_NUMBER)
        && FMath::IsNearlyEqual(Capsule->GetUnscaledCapsuleRadius(), PreviousRadius, CapsuleSizeTolerance)
        && FMath::IsNearlyEqual(Capsule->GetUnscaledCapsuleHalfHeight(), PreviousHalfHeight, CapsuleSizeTolerance);
}

void UDMSizeAmplificationComponent::ResetAutonomousProxyPrediction()
{
    ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());
    if (!IsValid(OwnerCharacter) || OwnerCharacter->HasAuthority() || !OwnerCharacter->IsLocallyControlled())
    {
        return;
    }

    if (UCharacterMovementComponent* MovementComponent = OwnerCharacter->GetCharacterMovement())
    {
        // Capsule 형상이 바뀌기 전 SavedMove를 폐기하고 서버 위치 보정을 새 기준으로 받는다.
        MovementComponent->ResetPredictionData_Client();
    }
}

bool UDMSizeAmplificationComponent::CanUseCapsuleSize(
    const float Radius, const float HalfHeight, const float CenterOffsetZ
) const
{
    const ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());
    const UCapsuleComponent* Capsule = IsValid(OwnerCharacter) ? OwnerCharacter->GetCapsuleComponent() : nullptr;
    const UWorld* World = GetWorld();

    if (!IsValid(Capsule) || !IsValid(World))
    {
        return false;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DMSizeAmplification), false, OwnerCharacter);
    const FVector TestLocation = Capsule->GetComponentLocation() + FVector(0.0f, 0.0f, CenterOffsetZ);
    const FCollisionShape TestShape = FCollisionShape::MakeCapsule(
        FMath::Max(0.0f, Radius - CapsuleSizeTolerance),
        FMath::Max(Radius, HalfHeight - CapsuleSizeTolerance)
    );

    return !World->OverlapBlockingTestByProfile(
        TestLocation, Capsule->GetComponentQuat(), Capsule->GetCollisionProfileName(), TestShape, QueryParams
    );
}

bool UDMSizeAmplificationComponent::ApplyScaleAlpha(const float Scale, const bool bCheckCollision)
{
    if (!bOriginalValuesCached)
    {
        return false;
    }

    ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());
    UCapsuleComponent* Capsule = IsValid(OwnerCharacter) ? OwnerCharacter->GetCapsuleComponent() : nullptr;
    USkeletalMeshComponent* Mesh = IsValid(OwnerCharacter) ? OwnerCharacter->GetMesh() : nullptr;

    if (!IsValid(Capsule) || !IsValid(Mesh))
    {
        return false;
    }

    const float ClampedScale = FMath::Clamp(Scale, 1.0f, SizeState.TargetScale);
    const float TargetRadius = OriginalCapsuleRadius * ClampedScale;
    const float TargetHalfHeight = OriginalCapsuleHalfHeight * ClampedScale;
    if (bCheckCollision)
    {
        if (!TryApplyCapsuleDimensions(TargetRadius, TargetHalfHeight, false))
        {
            return false;
        }
    }
    else
    {
        /*
         * Proxy가 중심 위치까지 독립적으로 이동하면 서버 ReplicatedMovement와 중복된다.
         * 서버가 확정한 위치 보정을 따르고 Capsule 형상만 같은 진행률로 갱신한다.
         */
        Capsule->SetCapsuleSize(TargetRadius, TargetHalfHeight, true);
    }

    FTransform MeshTransform = OriginalMeshRelativeTransform;
    MeshTransform.SetScale3D(OriginalMeshRelativeTransform.GetScale3D() * ClampedScale);
    FVector MeshLocation = OriginalMeshRelativeTransform.GetLocation();
    MeshLocation.Z *= ClampedScale;
    MeshTransform.SetLocation(MeshLocation);
    Mesh->SetRelativeTransform(MeshTransform);

    OwnerCharacter->ApplySizeAmplificationCameraScale(ClampedScale);
    return true;
}

float UDMSizeAmplificationComponent::CalculateScaleAtServerTime(const double ServerTime) const
{
    const double Elapsed = ServerTime - SizeState.StartServerTime;
    const double Remaining = SizeState.EndServerTime - ServerTime;

    if (Elapsed < static_cast<double>(SizeState.TransitionDuration))
    {
        return FMath::Lerp(1.0f, SizeState.TargetScale,
            FMath::Clamp(static_cast<float>(Elapsed / SizeState.TransitionDuration), 0.0f, 1.0f));
    }

    if (Remaining < static_cast<double>(SizeState.TransitionDuration))
    {
        return FMath::Lerp(1.0f, SizeState.TargetScale,
            FMath::Clamp(static_cast<float>(Remaining / SizeState.TransitionDuration), 0.0f, 1.0f));
    }

    return SizeState.TargetScale;
}
