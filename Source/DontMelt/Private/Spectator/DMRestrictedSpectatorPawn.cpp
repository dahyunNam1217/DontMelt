#include "Spectator/DMRestrictedSpectatorPawn.h"

#include "Camera/CameraComponent.h"
#include "Components/SphereComponent.h"
#include "Engine/World.h"
#include "GameFramework/PawnMovementComponent.h"

ADMRestrictedSpectatorPawn::ADMRestrictedSpectatorPawn(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    /*
     * ==================================================
     * Local-only Spectator Pawn
     * ==================================================
     *
     * 이 Pawn은 사망한 Client에서만 생성할
     * Presentation Actor다.
     */
    SetReplicates(false);

    SetReplicateMovement(false);

    /*
     * ADefaultPawn의 legacy/default input binding을
     * 사용하지 않는다.
     */
    bAddDefaultMovementBindings = false;

    /*
     * 이 Pawn은 Possess하지 않는다.
     *
     * MovementComponent의 Controller 기반 Tick 이동 대신
     * Enhanced Input 발생 시 직접 Swept Movement를 수행한다.
     */
    if (UPawnMovementComponent* SpectatorMovementComponent = GetMovementComponent())
    {
        SpectatorMovementComponent->SetComponentTickEnabled(false);
    }

    /*
     * ==================================================
     * Collision
     * ==================================================
     *
     * ASpectatorPawn → ADefaultPawn에서 제공되는
     * Sphere Collision을 그대로 재사용한다.
     */
    SetActorEnableCollision(true);

    USphereComponent* SpectatorCollision = GetCollisionComponent();

    if (IsValid(SpectatorCollision))
    {
        /*
         * 물리 시뮬레이션은 필요 없고
         * Movement Sweep 충돌만 필요하므로 QueryOnly.
         */
        SpectatorCollision->SetCollisionEnabled(ECollisionEnabled::QueryOnly);

        /*
         * 일반 Pawn 계열 World Collision 설정과
         * 호환되도록 Pawn Object Type을 사용한다.
         */
        SpectatorCollision->SetCollisionObjectType(ECC_Pawn);

        /*
         * 기본은 모두 Ignore.
         *
         * 자유 카메라를 막아야 하는
         * 월드 Geometry만 명시적으로 Block한다.
         */
        SpectatorCollision->SetCollisionResponseToAllChannels(ECR_Ignore);

        /*
         * 벽 / 바닥 / 천장 / 고정 구조물.
         */
        SpectatorCollision->SetCollisionResponseToChannel(ECC_WorldStatic, ECR_Block);

        /*
         * 문 / 움직이는 구조물 등
         * WorldDynamic Geometry.
         */
        SpectatorCollision->SetCollisionResponseToChannel(ECC_WorldDynamic, ECR_Block);

        /*
         * 단순 관전 카메라이므로
         * Overlap Event는 필요 없다.
         */
        SpectatorCollision->SetGenerateOverlapEvents(false);
    }

    /*
     * ==================================================
     * Camera
     * ==================================================
     */
    SpectatorCameraComponent = CreateDefaultSubobject<UCameraComponent>(TEXT("SpectatorCameraComponent"));

    SpectatorCameraComponent->SetupAttachment(GetRootComponent());

    /*
     * 이 Pawn은 Controller가 Possess하지 않는
     * Local View Actor로 사용할 예정이므로
     * Controller ControlRotation에 의존하지 않는다.
     *
     * Pawn Actor Rotation 자체를
     * Look Input으로 변경한다.
     */
    SpectatorCameraComponent->bUsePawnControlRotation = false;
}

void ADMRestrictedSpectatorPawn::AddRestrictedMovementInput(const FVector& WorldDirection, const float ScaleValue)
{
    if (MaximumMovementSpeed <= KINDA_SMALL_NUMBER || WorldDirection.IsNearlyZero() || FMath::IsNearlyZero(ScaleValue))
    {
        return;
    }

    const UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    const FVector SafeDirection = WorldDirection.GetSafeNormal();

    const float SafeScaleValue = FMath::Clamp(ScaleValue, -1.0f, 1.0f);

    const FVector MovementDelta = SafeDirection * SafeScaleValue * MaximumMovementSpeed * World->GetDeltaSeconds();

    if (MovementDelta.IsNearlyZero())
    {
        return;
    }

    FHitResult SweepHit;

    AddActorWorldOffset(MovementDelta, true, &SweepHit, ETeleportType::None);

    /*
     * 벽에 사선으로 부딪혔을 때
     * 완전히 멈추지 않고 남은 이동량을
     * 벽의 접평면 방향으로 투영한다.
     */
    if (SweepHit.IsValidBlockingHit() && SweepHit.Time < 1.0f)
    {
        const FVector RemainingDelta = MovementDelta * (1.0f - SweepHit.Time);

        const FVector SlideDelta = FVector::VectorPlaneProject(RemainingDelta, SweepHit.ImpactNormal);

        if (!SlideDelta.IsNearlyZero())
        {
            AddActorWorldOffset(SlideDelta, true, nullptr, ETeleportType::None);
        }
    }
}

void ADMRestrictedSpectatorPawn::SetMaximumMovementSpeed(const float InMaxSpeed)
{
    MaximumMovementSpeed = FMath::Max(0.0f, InMaxSpeed);
}