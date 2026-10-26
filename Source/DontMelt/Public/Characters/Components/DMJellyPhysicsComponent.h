#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DMJellyPhysicsComponent.generated.h"

DECLARE_MULTICAST_DELEGATE(FDMOnDeathPresentationStarted);

/**
 * Jelly Character의 물리 표현을 담당한다.
 *
 * Death / Ragdoll / FakeDeath 등의
 * 물리적 Presentation을 담당하지만,
 * Gameplay 상태의 SSOT는 소유하지 않는다.
 */
UCLASS(ClassGroup = (DontMelt), meta = (BlueprintSpawnableComponent))
class DONTMELT_API UDMJellyPhysicsComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDMJellyPhysicsComponent();

#pragma region Death Presentation

    /**
     * 서버에서 Death Ragdoll 표현을 시작한다.
     *
     * LifeState를 변경하지 않으며
     * Gameplay Death 판정을 수행하지 않는다.
     */
    bool StartDeathRagdollPresentation();

    /**
     * 이 Client에서 실제 Death Presentation이
     * 이미 시작되었는지 반환한다.
     *
     * Gameplay LifeState의 원본이 아니라
     * Local Presentation 상태다.
     */
    bool IsDeathPresentationActive() const
    {
        return bDeathPresentationActive;
    }

    /**
     * 실제 Death Presentation 시작 Local Event.
     *
     * FakeDeath와 구분하기 위해
     * 실제 Death Ragdoll 경로에서만 Broadcast한다.
     */
    FDMOnDeathPresentationStarted& OnDeathPresentationStarted()
    {
        return DeathPresentationStartedDelegate;
    }

#pragma endregion

private:
#pragma region Death Presentation Internal

    UFUNCTION(NetMulticast, Reliable)
    void MulticastStartDeathRagdollPresentation();

    void ApplyDeathRagdollPresentation();

    /**
     * 실제 Death Presentation Multicast를
     * 이 Client가 받은 상태.
     *
     * Gameplay Death SSOT가 아니다.
     */
    bool bDeathPresentationActive = false;

    FDMOnDeathPresentationStarted DeathPresentationStartedDelegate;

#pragma endregion
};