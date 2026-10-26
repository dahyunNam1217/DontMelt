#include "Events/DMBlackoutController.h"

#include "Core/DMGameplayTags.h"
#include "Framework/Match/DMMatchGameState.h"

#include "Components/LightComponentBase.h"
#include "Components/PointLightComponent.h"
#include "Components/DirectionalLightComponent.h"
#include "Components/SkyLightComponent.h"

#include "Engine/World.h"

#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"

#include "Kismet/GameplayStatics.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMBlackoutController, Log, All);

ADMBlackoutController::ADMBlackoutController()
{
    PrimaryActorTick.bCanEverTick = false;

    SetReplicates(false);
}

#pragma region Lifecycle

void ADMBlackoutController::BeginPlay()
{
    Super::BeginPlay();

    // 일반 조명 OFF 대상과 메인 조명 DIM 대상을 각각 수집한다.
    CacheManagedLights();
    CacheDimLights();

    if (!TryBindGameState())
    {
        GetWorldTimerManager().SetTimer(
            GameStateBindRetryTimerHandle,
            FTimerDelegate::CreateWeakLambda(
                this,
                [this]()
                {
                    TryBindGameState();
                }
            ),
            0.25f,
            true
        );
    }
}

void ADMBlackoutController::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ADMMatchGameState* MatchGameState = BoundMatchGameState.Get())
    {
        MatchGameState->OnRandomEventPublicStateChanged().RemoveAll(this);
    }

    GetWorldTimerManager().ClearTimer(GameStateBindRetryTimerHandle);

    ResetBlackoutPresentation();

    Super::EndPlay(EndPlayReason);
}

#pragma endregion

#pragma region GameState Binding

bool ADMBlackoutController::TryBindGameState()
{
    if (BoundMatchGameState.IsValid())
    {
        GetWorldTimerManager().ClearTimer(GameStateBindRetryTimerHandle);

        return true;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return false;
    }

    ADMMatchGameState* MatchGameState = World->GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        return false;
    }

    MatchGameState->OnRandomEventPublicStateChanged().RemoveAll(this);

    MatchGameState->OnRandomEventPublicStateChanged().AddUObject(
        this, &ADMBlackoutController::HandleRandomEventPublicStateChanged
    );

    BoundMatchGameState = MatchGameState;

    GetWorldTimerManager().ClearTimer(GameStateBindRetryTimerHandle);

    /*
     * Binding 전에 이미 Event Projection이
     * 존재했을 가능성까지 즉시 동기화한다.
     */
    HandleRandomEventPublicStateChanged(MatchGameState, MatchGameState->GetRandomEventPublicState());

    UE_LOG(LogDMBlackoutController, Log, TEXT("BlackoutController bound to MatchGameState."));

    return true;
}

void ADMBlackoutController::HandleRandomEventPublicStateChanged(
    ADMMatchGameState* MatchGameState, const FDMRandomEventRuntimeState EventState
)
{
    if (!IsValid(MatchGameState))
    {
        return;
    }

    if (EventState.EventTag != DMGameplayTags::RandomEvent_Blackout)
    {
        /*
         * Blackout가 Clear됐거나
         * 다른 Random Event가 시작된 경우
         * 모든 로컬 정전 연출을 원상복구한다.
         */
        ResetBlackoutPresentation();

        return;
    }

    ScheduleBlackoutPresentation(EventState);
}

#pragma endregion

#pragma region Light Cache

void ADMBlackoutController::ApplyDimLights()
{
    for (const FManagedDimLight& Entry : ManagedDimLights)
    {
        ULightComponentBase* LightComponent = Entry.LightComponent.Get();

        if (!IsValid(LightComponent))
        {
            continue;
        }

        const float Ratio =
            FMath::Clamp(Entry.bSkyLight ? SkyLightIntensityRatio : DirectionalLightIntensityRatio, 0.0f, 1.0f);

        // 현재 밝기가 아니라 캐시된 원본 밝기를 기준으로 계산한다.
        const float NewIntensity = Entry.OriginalIntensity * Ratio;

        if (USkyLightComponent* SkyLight = Cast<USkyLightComponent>(LightComponent))
        {
            SkyLight->SetIntensity(NewIntensity);
        }
        else if (UDirectionalLightComponent* DirectionalLight = Cast<UDirectionalLightComponent>(LightComponent))
        {
            DirectionalLight->SetIntensity(NewIntensity);
        }
    }
}

void ADMBlackoutController::RestoreDimLights()
{
    for (const FManagedDimLight& Entry : ManagedDimLights)
    {
        ULightComponentBase* LightComponent = Entry.LightComponent.Get();

        if (!IsValid(LightComponent))
        {
            continue;
        }

        // 정전 시작 전에 저장했던 원래 밝기로 되돌린다.
        if (USkyLightComponent* SkyLight = Cast<USkyLightComponent>(LightComponent))
        {
            SkyLight->SetIntensity(Entry.OriginalIntensity);
        }
        else if (UDirectionalLightComponent* DirectionalLight = Cast<UDirectionalLightComponent>(LightComponent))
        {
            DirectionalLight->SetIntensity(Entry.OriginalIntensity);
        }
    }
}

void ADMBlackoutController::CacheManagedLights()
{
    ManagedLights.Reset();

    if (BlackoutLightActorTag.IsNone())
    {
        UE_LOG(LogDMBlackoutController, Warning, TEXT("Blackout light Actor Tag is None."));

        return;
    }

    TArray<AActor*> TaggedActors;

    UGameplayStatics::GetAllActorsWithTag(this, BlackoutLightActorTag, TaggedActors);

    for (AActor* TaggedActor : TaggedActors)
    {
        if (!IsValid(TaggedActor))
        {
            continue;
        }

        TArray<ULightComponentBase*> LightComponents;

        TaggedActor->GetComponents<ULightComponentBase>(LightComponents);

        for (ULightComponentBase* LightComponent : LightComponents)
        {
            if (!IsValid(LightComponent))
            {
                continue;
            }

            FManagedBlackoutLight Entry;

            Entry.LightComponent = LightComponent;

            Entry.bOriginalVisible = LightComponent->IsVisible();

            ManagedLights.Add(Entry);
        }
    }

    UE_LOG(
        LogDMBlackoutController,
        Log,
        TEXT("Blackout light cache completed. TaggedActors=%d, Lights=%d"),
        TaggedActors.Num(),
        ManagedLights.Num()
    );
}

void ADMBlackoutController::CacheDimLights()
{
    ManagedDimLights.Reset();

    if (BlackoutDimLightActorTag.IsNone())
    {
        return;
    }

    TArray<AActor*> TaggedActors;

    // 메인 조명 전용 Actor Tag를 기준으로 수집한다.
    UGameplayStatics::GetAllActorsWithTag(this, BlackoutDimLightActorTag, TaggedActors);

    for (AActor* TaggedActor : TaggedActors)
    {
        if (!IsValid(TaggedActor))
        {
            continue;
        }

        // 동일한 조명이 OFF와 DIM에 중복 등록되는 것을 방지한다.
        if (TaggedActor->ActorHasTag(BlackoutLightActorTag))
        {
            UE_LOG(
                LogDMBlackoutController,
                Warning,
                TEXT("Blackout light has both OFF and DIM tags: %s"),
                *GetNameSafe(TaggedActor)
            );

            continue;
        }

        TArray<ULightComponentBase*> LightComponents;

        TaggedActor->GetComponents<ULightComponentBase>(LightComponents);

        for (ULightComponentBase* LightComponent : LightComponents)
        {
            if (!IsValid(LightComponent))
            {
                continue;
            }

            const bool bIsDirectional = LightComponent->IsA<UDirectionalLightComponent>();

            const bool bIsSkyLight = LightComponent->IsA<USkyLightComponent>();

            // 메인 조명 두 종류만 DIM 대상으로 허용한다.
            if (!bIsDirectional && !bIsSkyLight)
            {
                continue;
            }

            // 런타임에 밝기를 변경할 수 있도록 Movable 조명만 허용한다.
            if (!LightComponent->IsMovable())
            {
                UE_LOG(
                    LogDMBlackoutController,
                    Warning,
                    TEXT("Blackout DIM requires a Movable light: %s"),
                    *GetNameSafe(TaggedActor)
                );

                continue;
            }

            FManagedDimLight Entry;

            Entry.LightComponent = LightComponent;
            Entry.OriginalIntensity = LightComponent->Intensity;
            Entry.bSkyLight = bIsSkyLight;

            ManagedDimLights.Add(Entry);
        }
    }

    UE_LOG(
        LogDMBlackoutController,
        Log,
        TEXT("Blackout dim lights cached. Actors=%d, Lights=%d"),
        TaggedActors.Num(),
        ManagedDimLights.Num()
    );
}

void ADMBlackoutController::RestoreManagedLights()
{
    for (const FManagedBlackoutLight& Entry : ManagedLights)
    {
        ULightComponentBase* LightComponent = Entry.LightComponent.Get();

        if (!IsValid(LightComponent))
        {
            continue;
        }

        LightComponent->SetVisibility(Entry.bOriginalVisible, true);
    }
}

void ADMBlackoutController::SetManagedLightsOff()
{
    for (const FManagedBlackoutLight& Entry : ManagedLights)
    {
        ULightComponentBase* LightComponent = Entry.LightComponent.Get();

        if (!IsValid(LightComponent))
        {
            continue;
        }

        LightComponent->SetVisibility(false, true);
    }
}

#pragma endregion

#pragma region Blackout Presentation

void ADMBlackoutController::ScheduleBlackoutPresentation(const FDMRandomEventRuntimeState& EventState)
{
    ResetBlackoutPresentation();

    if (!EventState.EventTag.IsValid() || EventState.EventTag != DMGameplayTags::RandomEvent_Blackout)
    {
        return;
    }

    // 첫 캐싱 시 메인 조명이 아직 없었던 경우 재탐색한다.
    if (ManagedDimLights.IsEmpty())
    {
        CacheDimLights();
    }

    const double ServerNow = GetCurrentServerTime();

    if (ServerNow <= 0.0 || EventState.EndServerTime <= ServerNow)
    {
        return;
    }

    ActiveBlackoutStartServerTime = EventState.StartServerTime;

    ActiveBlackoutEndServerTime = EventState.EndServerTime;

    /*
     * Event를 늦게 수신했는데 이미 Active Window라면
     * 즉시 정전 상태로 진입한다.
     */
    if (ServerNow >= ActiveBlackoutStartServerTime)
    {
        BeginBlackout();
    }
    else
    {
        const double FlickerStartServerTime = ActiveBlackoutStartServerTime - static_cast<double>(FlickerLeadTime);

        const double FlickerStartDelay = FlickerStartServerTime - ServerNow;

        if (FlickerStartDelay <= 0.0)
        {
            BeginFlicker();
        }
        else
        {
            GetWorldTimerManager().SetTimer(
                FlickerStartTimerHandle,
                this,
                &ADMBlackoutController::BeginFlicker,
                static_cast<float>(FlickerStartDelay),
                false
            );
        }

        const double BlackoutStartDelay = ActiveBlackoutStartServerTime - ServerNow;

        GetWorldTimerManager().SetTimer(
            BlackoutStartTimerHandle,
            this,
            &ADMBlackoutController::BeginBlackout,
            FMath::Max(0.01f, static_cast<float>(BlackoutStartDelay)),
            false
        );
    }

    const double BlackoutEndDelay = ActiveBlackoutEndServerTime - ServerNow;

    GetWorldTimerManager().SetTimer(
        BlackoutEndTimerHandle,
        this,
        &ADMBlackoutController::EndBlackout,
        FMath::Max(0.01f, static_cast<float>(BlackoutEndDelay)),
        false
    );
}

void ADMBlackoutController::BeginFlicker()
{
    if (bBlackoutActive || FlickerInterval <= 0.0f)
    {
        return;
    }

    if (ManagedLights.IsEmpty())
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(FlickerStartTimerHandle);

    bFlickerLightsOff = false;

    /*
     * 기다리지 않고 첫 Flicker를 즉시 수행한다.
     */
    HandleFlickerTick();

    GetWorldTimerManager().SetTimer(
        FlickerTimerHandle, this, &ADMBlackoutController::HandleFlickerTick, FlickerInterval, true
    );

    UE_LOG(LogDMBlackoutController, Verbose, TEXT("Blackout flicker started."));
}

void ADMBlackoutController::HandleFlickerTick()
{
    if (bBlackoutActive)
    {
        return;
    }

    bFlickerLightsOff = !bFlickerLightsOff;

    if (bFlickerLightsOff)
    {
        SetManagedLightsOff();
    }
    else
    {
        RestoreManagedLights();
    }
}

void ADMBlackoutController::BeginBlackout()
{
    if (bBlackoutActive)
    {
        return;
    }

    GetWorldTimerManager().ClearTimer(FlickerStartTimerHandle);

    GetWorldTimerManager().ClearTimer(FlickerTimerHandle);

    GetWorldTimerManager().ClearTimer(BlackoutStartTimerHandle);

    bFlickerLightsOff = false;

    bBlackoutActive = true;

    // 일반 조명은 완전히 끈다.
    SetManagedLightsOff();

    // DirectionalLight와 SkyLight는 설정 비율만큼 어둡게 한다.
    ApplyDimLights();

    if (!ManagedLights.IsEmpty())
    {
        CreateLocalVisibilityLight();
    }

    UE_LOG(LogDMBlackoutController, Log, TEXT("Blackout presentation started. ManagedLights=%d"), ManagedLights.Num());
}

void ADMBlackoutController::EndBlackout()
{
    if (!bBlackoutActive)
    {
        /*
         * Warning Cancel 또는 timing mismatch에서도
         * 반드시 원본 조명은 복원한다.
         */
        ResetBlackoutPresentation();

        return;
    }

    UE_LOG(LogDMBlackoutController, Log, TEXT("Blackout presentation finished."));

    ResetBlackoutPresentation();
}

void ADMBlackoutController::ResetBlackoutPresentation()
{
    GetWorldTimerManager().ClearTimer(FlickerStartTimerHandle);

    GetWorldTimerManager().ClearTimer(FlickerTimerHandle);

    GetWorldTimerManager().ClearTimer(BlackoutStartTimerHandle);

    GetWorldTimerManager().ClearTimer(BlackoutEndTimerHandle);

    // 일반 조명의 Visibility를 원래 상태로 복구한다.
    RestoreManagedLights();

    // 메인 조명의 원래 Intensity를 복구한다.
    RestoreDimLights();

    DestroyLocalVisibilityLight();

    ActiveBlackoutStartServerTime = 0.0;

    ActiveBlackoutEndServerTime = 0.0;

    bFlickerLightsOff = false;

    bBlackoutActive = false;
}

#pragma endregion

#pragma region Local Visibility Light

void ADMBlackoutController::CreateLocalVisibilityLight()
{
    if (IsValid(LocalVisibilityLight))
    {
        return;
    }

    APlayerController* LocalPlayerController = UGameplayStatics::GetPlayerController(this, 0);

    if (!IsValid(LocalPlayerController) || !LocalPlayerController->IsLocalController())
    {
        return;
    }

    APawn* LocalPawn = LocalPlayerController->GetPawn();

    if (!IsValid(LocalPawn) || !IsValid(LocalPawn->GetRootComponent()))
    {
        return;
    }

    LocalVisibilityLight = NewObject<UPointLightComponent>(LocalPawn, TEXT("DMBlackoutLocalVisibilityLight"));

    if (!IsValid(LocalVisibilityLight))
    {
        return;
    }

    LocalPawn->AddInstanceComponent(LocalVisibilityLight);

    LocalVisibilityLight->SetupAttachment(LocalPawn->GetRootComponent());

    LocalVisibilityLight->SetMobility(EComponentMobility::Movable);

    LocalVisibilityLight->SetIntensity(LocalVisibilityLightIntensity);

    LocalVisibilityLight->SetAttenuationRadius(LocalVisibilityLightRadius);

    LocalVisibilityLight->SetRelativeLocation(LocalVisibilityLightOffset);

    LocalVisibilityLight->RegisterComponent();

    LocalVisibilityLight->SetVisibility(true, true);
}

void ADMBlackoutController::DestroyLocalVisibilityLight()
{
    if (!IsValid(LocalVisibilityLight))
    {
        LocalVisibilityLight = nullptr;

        return;
    }

    LocalVisibilityLight->DestroyComponent();

    LocalVisibilityLight = nullptr;
}

#pragma endregion

#pragma region Helpers

double ADMBlackoutController::GetCurrentServerTime() const
{
    const ADMMatchGameState* MatchGameState = BoundMatchGameState.Get();

    if (!IsValid(MatchGameState))
    {
        return 0.0;
    }

    return MatchGameState->GetServerWorldTimeSeconds();
}

#pragma endregion