#pragma once

#include "CoreMinimal.h"
#include "GameFramework/SpectatorPawn.h"
#include "DMRestrictedSpectatorPawn.generated.h"

class UCameraComponent;

/**
 * Don’t Melt 사망 관전의 10~25초 구간에서 사용하는
 * Local Restricted Free Camera Pawn.
 *
 * Gameplay Pawn이 아니며:
 *
 * - Server Authority 데이터를 소유하지 않는다.
 * - Replication하지 않는다.
 * - PlayerCharacter를 대체하지 않는다.
 * - Respawn 판정에 관여하지 않는다.
 *
 * 벽 / 바닥 / 주요 구조물과 충돌하는
 * 제한형 자유 카메라 이동만 담당한다.
 */
UCLASS()
class DONTMELT_API ADMRestrictedSpectatorPawn : public ASpectatorPawn
{
    GENERATED_BODY()

public:
    ADMRestrictedSpectatorPawn(const FObjectInitializer& ObjectInitializer);

#pragma region Spectator Movement

    /**
     * MatchPlayerController가 Enhanced Input을
     * World Direction으로 변환한 뒤 호출한다.
     *
     * 이 Pawn은 Possess하지 않으며
     * MovementComponent Input 누적을 사용하지 않는다.
     *
     * Local Actor Transform을 Sweep 이동하여
     * WorldStatic / WorldDynamic 구조물과 충돌한다.
     */
    void AddRestrictedMovementInput(const FVector& WorldDirection, float ScaleValue);

    /**
     * 자유 카메라의 최대 이동속도를 설정한다.
     *
     * MatchPlayerController가 소유한
     * Spectator Presentation 설정값을 전달받는다.
     */
    void SetMaximumMovementSpeed(float InMaxSpeed);

#pragma endregion

private:
#pragma region Components

    /**
     * FreeCamera의 실제 View를 제공한다.
     *
     * Controller ControlRotation에 의존하지 않고
     * Pawn 자체 Transform을 따라간다.
     */
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DM|Spectator", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCameraComponent> SpectatorCameraComponent = nullptr;

#pragma endregion

#pragma region Movement Runtime

    /**
     * Local FreeCamera의 초당 최대 이동거리.
     *
     * Gameplay 데이터가 아니며
     * MatchPlayerController에서 FreeCamera 진입 시 설정한다.
     */
    float MaximumMovementSpeed = 0.0f;

#pragma endregion
};