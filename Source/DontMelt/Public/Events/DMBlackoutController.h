#pragma once

#include "CoreMinimal.h"
#include "Events/DMRandomEventTypes.h"
#include "GameFramework/Info.h"
#include "TimerManager.h"

#include "DMBlackoutController.generated.h"

class ADMMatchGameState;

class ULightComponentBase;
class UPointLightComponent;

/**
 * Blackout Event의 로컬 월드 표현을 담당한다.
 *
 * RandomEventDirector의 권위 상태를 직접 소유하지 않는다.
 * MatchGameState의 Public Event Projection을 관찰하여
 * 각 World Instance에서 조명 연출만 수행한다.
 */
UCLASS(Blueprintable)
class DONTMELT_API ADMBlackoutController : public AInfo
{
    GENERATED_BODY()

public:
    ADMBlackoutController();

protected:
#pragma region Lifecycle

    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#pragma endregion

private:
    struct FManagedBlackoutLight
    {
        TWeakObjectPtr<ULightComponentBase> LightComponent;

        bool bOriginalVisible = true;
    };

    /**
     * 정전 중 밝기만 감소시키는 메인 조명 정보.
     */
    struct FManagedDimLight
    {
        TWeakObjectPtr<ULightComponentBase> LightComponent;

        float OriginalIntensity = 0.0f;

        bool bSkyLight = false;
    };

#pragma region GameState Binding

    bool TryBindGameState();

    void HandleRandomEventPublicStateChanged(ADMMatchGameState* MatchGameState, FDMRandomEventRuntimeState EventState);

#pragma endregion

#pragma region Light Cache

    void CacheManagedLights();

    void RestoreManagedLights();

    void SetManagedLightsOff();

    /** 밝기 감소 대상 메인 조명을 수집한다. */
    void CacheDimLights();

    /** 메인 조명의 밝기를 정전 비율로 변경한다. */
    void ApplyDimLights();

    /** 메인 조명의 원래 밝기를 복구한다. */
    void RestoreDimLights();

#pragma endregion

#pragma region Blackout Presentation

    void ScheduleBlackoutPresentation(const FDMRandomEventRuntimeState& EventState);

    void BeginFlicker();

    void HandleFlickerTick();

    void BeginBlackout();

    void EndBlackout();

    void ResetBlackoutPresentation();

#pragma endregion

#pragma region Local Visibility Light

    void CreateLocalVisibilityLight();

    void DestroyLocalVisibilityLight();

#pragma endregion

#pragma region Helpers

    double GetCurrentServerTime() const;

#pragma endregion

#pragma region Configuration

    /**
     * Blackout의 영향을 받을 Light Actor Tag.
     *
     * 이 Tag가 붙은 Actor 내부의
     * 모든 LightComponentBase를 관리한다.
     */
    UPROPERTY(EditAnywhere, Category = "DM|Blackout|Lights")
    FName BlackoutLightActorTag = TEXT("DM.BlackoutLight");

    /**
     * 밝기를 낮출 메인 조명에 부여하는 Actor Tag.
     * 일반 조명 OFF 태그와 구분한다.
     */
    UPROPERTY(EditAnywhere, Category = "DM|Blackout|Lights")
    FName BlackoutDimLightActorTag = TEXT("DM.BlackoutDimLight");

    /** 정전 중 DirectionalLight 밝기 비율. */
    UPROPERTY(EditAnywhere, Category = "DM|Blackout|Lights", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float DirectionalLightIntensityRatio = 0.10f;

    /** 정전 중 SkyLight 밝기 비율. */
    UPROPERTY(EditAnywhere, Category = "DM|Blackout|Lights", meta = (ClampMin = "0.0", ClampMax = "1.0"))
    float SkyLightIntensityRatio = 0.15f;

    /**
     * Event 실제 시작 몇 초 전부터
     * Flicker를 시작할지 결정한다.
     */
    UPROPERTY(EditAnywhere, Category = "DM|Blackout|Flicker", meta = (ClampMin = "0.0"))
    float FlickerLeadTime = 2.0f;

    /**
     * Flicker On/Off 간격.
     */
    UPROPERTY(EditAnywhere, Category = "DM|Blackout|Flicker", meta = (ClampMin = "0.05"))
    float FlickerInterval = 0.20f;

    /**
     * 정전 중 로컬 플레이어 주변의
     * 희미한 보조 조명 밝기.
     *
     * 맵 노출값에 따라 Editor에서 조절한다.
     */
    UPROPERTY(EditAnywhere, Category = "DM|Blackout|Local Light", meta = (ClampMin = "0.0"))
    float LocalVisibilityLightIntensity = 100.0f;

    /**
     * 1 Unreal Unit = 1 cm.
     *
     * 기본 200cm = 약 2m.
     */
    UPROPERTY(EditAnywhere, Category = "DM|Blackout|Local Light", meta = (ClampMin = "1.0"))
    float LocalVisibilityLightRadius = 200.0f;

    UPROPERTY(EditAnywhere, Category = "DM|Blackout|Local Light")
    FVector LocalVisibilityLightOffset = FVector(0.0f, 0.0f, 40.0f);

#pragma endregion

#pragma region Runtime State

    TWeakObjectPtr<ADMMatchGameState> BoundMatchGameState;

    TArray<FManagedBlackoutLight> ManagedLights;

    /** 원래 밝기를 복원하기 위한 로컬 조명 캐시. */
    TArray<FManagedDimLight> ManagedDimLights;

    UPROPERTY(Transient)
    TObjectPtr<UPointLightComponent> LocalVisibilityLight = nullptr;

    double ActiveBlackoutStartServerTime = 0.0;

    double ActiveBlackoutEndServerTime = 0.0;

    bool bFlickerLightsOff = false;

    bool bBlackoutActive = false;

    FTimerHandle GameStateBindRetryTimerHandle;

    FTimerHandle FlickerStartTimerHandle;

    FTimerHandle FlickerTimerHandle;

    FTimerHandle BlackoutStartTimerHandle;

    FTimerHandle BlackoutEndTimerHandle;

#pragma endregion
};