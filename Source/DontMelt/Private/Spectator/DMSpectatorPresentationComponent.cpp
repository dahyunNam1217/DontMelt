#include "Spectator/DMSpectatorPresentationComponent.h"

#include "Core/DMGameplayUtils.h"
#include "Core/DMLocalSettingsSubsystem.h"
#include "Engine/GameInstance.h"
#include "Framework/Match/DMMatchGameState.h"
#include "Framework/Match/DMMatchPlayerController.h"
#include "Characters/DMPlayerCharacter.h"
#include "Characters/Components/DMJellyPhysicsComponent.h"
#include "Input/DMInputConfigDataAsset.h"
#include "UI/DMSpectatorCountdownWidget.h"
#include "Spectator/DMRestrictedSpectatorPawn.h"
#include "Camera/CameraActor.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "InputActionValue.h"
#include "InputMappingContext.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMSpectatorPresentation, Log, All);

namespace
{
    constexpr float CorpseSpectatorPhaseDuration = 5.0f;

    constexpr float KillerSpectatorPhaseDuration = 5.0f;

    constexpr float FreeCameraSpectatorPhaseDuration = 15.0f;

    constexpr float KillerSpectatorPhaseEndTime = CorpseSpectatorPhaseDuration + KillerSpectatorPhaseDuration;

    constexpr float FreeCameraSpectatorPhaseEndTime = KillerSpectatorPhaseEndTime + FreeCameraSpectatorPhaseDuration;

    constexpr float FadeSpectatorPhaseDuration = 5.0f;

    constexpr float FadeSpectatorPhaseEndTime = FreeCameraSpectatorPhaseEndTime + FadeSpectatorPhaseDuration;

    /*
     * Timer가 정확히 정수 경계 직전에 실행돼
     * Ceil 결과가 이전 숫자로 남는 것을 방지하는
     * 아주 작은 Local Presentation 보정값.
     */
    constexpr float FadeCountdownBoundarySafetyOffset = 0.005f;

    constexpr int32 SpectatorCountdownWidgetZOrder = 100;

    constexpr float DeathCamWallPadding = 20.0f;
}

UDMSpectatorPresentationComponent::UDMSpectatorPresentationComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    SetIsReplicatedByDefault(false);
}

#pragma region Owner Access

ADMMatchPlayerController* UDMSpectatorPresentationComponent::GetOwnerController() const
{
    return GetOwner<ADMMatchPlayerController>();
}

FDMSpectatorPresentationSettings UDMSpectatorPresentationComponent::GetPresentationSettings() const
{
    const ADMMatchPlayerController* OwnerController = GetOwnerController();

    return OwnerController != nullptr ? OwnerController->GetSpectatorPresentationSettings()
                                      : FDMSpectatorPresentationSettings();
}

bool UDMSpectatorPresentationComponent::IsLocalController() const
{
    const ADMMatchPlayerController* OwnerController = GetOwnerController();

    return OwnerController != nullptr && OwnerController->IsLocalController();
}

APawn* UDMSpectatorPresentationComponent::GetPawn() const
{
    const ADMMatchPlayerController* OwnerController = GetOwnerController();

    return OwnerController != nullptr ? OwnerController->GetPawn() : nullptr;
}

template <class T>
T* UDMSpectatorPresentationComponent::GetPawn() const
{
    return Cast<T>(GetPawn());
}

FRotator UDMSpectatorPresentationComponent::GetControlRotation() const
{
    const ADMMatchPlayerController* OwnerController = GetOwnerController();

    return OwnerController != nullptr ? OwnerController->GetControlRotation() : FRotator::ZeroRotator;
}

AActor* UDMSpectatorPresentationComponent::GetViewTarget() const
{
    const ADMMatchPlayerController* OwnerController = GetOwnerController();

    return OwnerController != nullptr ? OwnerController->GetViewTarget() : nullptr;
}

void UDMSpectatorPresentationComponent::SetViewTarget(AActor* NewViewTarget) const
{
    if (ADMMatchPlayerController* OwnerController = GetOwnerController())
    {
        OwnerController->SetViewTarget(NewViewTarget);
    }
}

void UDMSpectatorPresentationComponent::SetViewTargetWithBlend(
    AActor* NewViewTarget,
    const float BlendTime,
    const EViewTargetBlendFunction BlendFunc,
    const float BlendExp,
    const bool bLockOutgoing
) const
{
    if (ADMMatchPlayerController* OwnerController = GetOwnerController())
    {
        OwnerController->SetViewTargetWithBlend(NewViewTarget, BlendTime, BlendFunc, BlendExp, bLockOutgoing);
    }
}

APlayerCameraManager* UDMSpectatorPresentationComponent::GetOwnerCameraManager() const
{
    const ADMMatchPlayerController* OwnerController = GetOwnerController();

    return OwnerController != nullptr ? OwnerController->PlayerCameraManager.Get() : nullptr;
}

UInputComponent* UDMSpectatorPresentationComponent::GetOwnerInputComponent() const
{
    const ADMMatchPlayerController* OwnerController = GetOwnerController();

    return OwnerController != nullptr ? OwnerController->InputComponent.Get() : nullptr;
}

UEnhancedInputLocalPlayerSubsystem* UDMSpectatorPresentationComponent::GetEnhancedInputSubsystem() const
{
    return DMGameplay::GetLocalInputSubsystem(GetOwnerController());
}

#pragma endregion

#pragma region Sequence

void UDMSpectatorPresentationComponent::ReceiveSpectatorContext(const FDMSpectatorContext& SpectatorContext)
{
    LocalSpectatorContext = SpectatorContext;

    // 이전 사망 시퀀스의 관전 단계를 새 시퀀스에 넘기지 않는다.
    LocalSpectatorPhase = EDMSpectatorPhase::Corpse;

    SetSpectatorInformationRestricted(true);
}

void UDMSpectatorPresentationComponent::FinishSequenceForRespawn(ADMPlayerCharacter* NewPlayerCharacter)
{
    UWorld* World = GetWorld();

    if (IsValid(World))
    {
        /*
         * 이미 새 Gameplay Pawn이 왔으므로
         * 과거 Spectator callback은 모두 무효화한다.
         */
        World->GetTimerManager().ClearTimer(LocalSpectatorPhaseTimerHandle);

        World->GetTimerManager().ClearTimer(LocalSpectatorCountdownTimerHandle);
    }

    /*
     * Spectator Input은 더 이상 필요 없다.
     */
    ExitSpectatorInputMode();

    /*
     * 30초 전에 Killer 관련 Delegate가
     * 어떤 예외로 남아 있어도 정리한다.
     */
    ClearObservedKillerViewBindings();

    /*
     * ==================================================
     * Camera Handoff
     * ==================================================
     *
     * Black Fade가 아직 유지된 상태에서
     * 먼저 새 Gameplay Pawn으로 ViewTarget을 확정한다.
     */
    SetViewTarget(NewPlayerCharacter);

    /*
     * 새 Pawn이 ViewTarget이 된 이후
     * 옛 Local Camera Actor들을 제거한다.
     */
    CleanupLocalRestrictedSpectatorPawn();

    CleanupLocalDeathCamera();

    /*
     * ==================================================
     * Fade / Widget Cleanup
     * ==================================================
     *
     * 반드시 마지막에 수행한다.
     *
     * 이 시점에는 이미 화면 뒤쪽 Camera가
     * 새 Gameplay Pawn을 보고 있으므로,
     * Black이 풀렸을 때 죽은 Pawn/FreeCamera가
     * 순간적으로 노출되지 않는다.
     */
    CleanupLocalSpectatorFadePresentation();

    /*
     * 이전 사망 Sequence의 Snapshot은
     * 다음 사망까지 필요 없다.
     */
    LocalSpectatorContext = FDMSpectatorContext();

    SetSpectatorInformationRestricted(false);
}

void UDMSpectatorPresentationComponent::StopForMatchResult()
{
    UWorld* World = GetWorld();

    if (IsValid(World))
    {
        World->GetTimerManager().ClearTimer(LocalSpectatorPhaseTimerHandle);

        World->GetTimerManager().ClearTimer(LocalSpectatorCountdownTimerHandle);
    }

    // 처치자의 사망·제거 이벤트가 관전 카메라를 다시 바꾸지 않도록 한다.
    ClearObservedKillerViewBindings();

    if (bSpectatorInputModeActive)
    {
        ExitSpectatorInputMode();
    }

    // 반복 호출되어도 안전하게 Fade와 카운트다운을 정리한다.
    CleanupLocalSpectatorFadePresentation();
}

void UDMSpectatorPresentationComponent::ShutdownPresentation()
{
    ExitSpectatorInputMode();

    CleanupLocalRestrictedSpectatorPawn();

    CleanupLocalSpectatorFadePresentation();

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(LocalSpectatorPhaseTimerHandle);
    }

    ClearObservedKillerViewBindings();

    CleanupLocalDeathCamera();
}

void UDMSpectatorPresentationComponent::ClearPresentationDelegates()
{
    SpectatorInformationRestrictionChangedDelegate.Clear();

    LocalSpectatorPhaseChangedDelegate.Clear();
}

bool UDMSpectatorPresentationComponent::HasActiveSpectatorPresentation() const
{
    return bSpectatorInformationRestricted || bSpectatorInputModeActive || IsValid(LocalDeathCameraActor.Get())
        || LocalRestrictedSpectatorPawn.IsValid() || LocalObservedKillerPawn.IsValid() || bLocalSpectatorFadeActive
        || IsValid(SpectatorCountdownWidget.Get());
}

bool UDMSpectatorPresentationComponent::ShouldBlockSpectatorPresentation() const
{
    // 결과 화면에 진입했다면 GameState를 잃어도 관전을 재개하지 않는다.
    const ADMMatchPlayerController* OwnerController = GetOwnerController();

    if (OwnerController != nullptr && OwnerController->IsMatchResultPresentationStarted())
    {
        return true;
    }

    const UWorld* World = GetWorld();

    const ADMMatchGameState* MatchGameState = IsValid(World) ? World->GetGameState<ADMMatchGameState>() : nullptr;

    // GameState가 없다는 사실만으로 경기 종료를 판정하지 않는다.
    if (!IsValid(MatchGameState))
    {
        return false;
    }

    const EDMMatchPhase Phase = MatchGameState->GetCurrentPhase();

    if (Phase == EDMMatchPhase::Finished || Phase == EDMMatchPhase::Aborted)
    {
        return true;
    }

    // 종료 결과와 Phase의 수신 순서에 의존하지 않는다.
    return MatchGameState->GetMatchEndReason() != EDMMatchEndReason::None;
}

#pragma endregion

#pragma region Presentation

void UDMSpectatorPresentationComponent::SetSpectatorInformationRestricted(const bool bRestricted)
{
    if (!IsLocalController())
    {
        return;
    }

    if (bSpectatorInformationRestricted == bRestricted)
    {
        return;
    }

    bSpectatorInformationRestricted = bRestricted;

    SpectatorInformationRestrictionChangedDelegate.Broadcast(bSpectatorInformationRestricted);

    UE_LOG(
        LogDMSpectatorPresentation,
        Log,
        TEXT("Spectator information restriction changed. Restricted=%s"),
        bSpectatorInformationRestricted ? TEXT("true") : TEXT("false")
    );
}

double UDMSpectatorPresentationComponent::GetRemainingSpectatorTimeSeconds() const
{
    if (!IsLocalController() || !bSpectatorInformationRestricted
        || LocalSpectatorContext.SpectatorStartServerTime <= 0.0)
    {
        return 0.0;
    }

    // 관전 Fade 구간의 종료 시각을 그대로 재사용한다.
    return FMath::Max(0.0, static_cast<double>(FadeSpectatorPhaseEndTime) - GetElapsedSpectatorTime());
}

void UDMSpectatorPresentationComponent::InitializeSpectatorInput(ADMPlayerCharacter* PlayerCharacter)
{
    if (!IsLocalController() || !IsValid(PlayerCharacter))
    {
        return;
    }

    UDMInputConfigDataAsset* InputConfig = PlayerCharacter->GetInputConfigDataAsset();

    if (!ensureMsgf(IsValid(InputConfig), TEXT("Spectator Input initialization failed: InputConfig is invalid.")))
    {
        return;
    }

    LocalInputConfig = InputConfig;

    /*
     * Controller InputComponent는 Match 동안 유지되므로
     * 같은 Action을 Respawn마다 다시 Binding하지 않는다.
     */
    if (bSpectatorInputBindingsInitialized)
    {
        return;
    }

    UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(GetOwnerInputComponent());

    if (!ensureMsgf(
            IsValid(EnhancedInputComponent),
            TEXT(
                "Spectator Input initialization failed: PlayerController InputComponent is not UEnhancedInputComponent."
            )
        ))
    {
        return;
    }

    if (!ensureMsgf(
            IsValid(LocalInputConfig->MoveAction) && IsValid(LocalInputConfig->LookAction),
            TEXT("Spectator Input initialization failed: MoveAction or LookAction is not assigned.")
        ))
    {
        return;
    }

    EnhancedInputComponent->BindAction(
        LocalInputConfig->MoveAction, ETriggerEvent::Triggered, this, &ThisClass::InputSpectatorMove
    );

    EnhancedInputComponent->BindAction(
        LocalInputConfig->LookAction, ETriggerEvent::Triggered, this, &ThisClass::InputSpectatorLook
    );

    bSpectatorInputBindingsInitialized = true;

    UE_LOG(
        LogDMSpectatorPresentation,
        Log,
        TEXT("Spectator Input bindings initialized. Move=%s, Look=%s"),
        *GetNameSafe(LocalInputConfig->MoveAction),
        *GetNameSafe(LocalInputConfig->LookAction)
    );
}

bool UDMSpectatorPresentationComponent::EnterSpectatorInputMode()
{
    if (!IsLocalController() || ShouldBlockSpectatorPresentation() || !IsValid(LocalInputConfig)
        || !IsValid(LocalInputConfig->SpectatorMappingContext.Get()))
    {
        return false;
    }

    UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetEnhancedInputSubsystem();

    if (!IsValid(InputSubsystem))
    {
        return false;
    }

    FModifyContextOptions ModifyOptions;

    ModifyOptions.bIgnoreAllPressedKeysUntilRelease = true;

    /*
     * ==================================================
     * Existing Gameplay Context Cleanup
     * ==================================================
     */

    if (IsValid(LocalInputConfig->GameplayMappingContext.Get()))
    {
        InputSubsystem->RemoveMappingContext(LocalInputConfig->GameplayMappingContext, ModifyOptions);
    }

    if (IsValid(LocalInputConfig->PoseMappingContext.Get()))
    {
        InputSubsystem->RemoveMappingContext(LocalInputConfig->PoseMappingContext, ModifyOptions);
    }

    if (IsValid(LocalInputConfig->InteractionMappingContext.Get()))
    {
        InputSubsystem->RemoveMappingContext(LocalInputConfig->InteractionMappingContext, ModifyOptions);
    }

    if (IsValid(LocalInputConfig->SkillTargetingMappingContext.Get()))
    {
        InputSubsystem->RemoveMappingContext(LocalInputConfig->SkillTargetingMappingContext, ModifyOptions);
    }

    /*
     * 동일 Context 중복 등록 방지.
     */
    InputSubsystem->RemoveMappingContext(LocalInputConfig->SpectatorMappingContext, ModifyOptions);

    /*
     * 죽은 Character의 IA_Move / IA_Look Binding이
     * 같은 Action에 반응하지 않도록
     * Pawn InputComponent를 Stack에서 제거한다.
     */
    if (ADMPlayerCharacter* DeadCharacter = GetPawn<ADMPlayerCharacter>())
    {
        DeadCharacter->DisableInput(GetOwnerController());
    }

    InputSubsystem->AddMappingContext(LocalInputConfig->SpectatorMappingContext, 0, ModifyOptions);

    bSpectatorInputModeActive = true;

    UE_LOG(LogDMSpectatorPresentation, Log, TEXT("Spectator Input mode entered."));

    return true;
}

void UDMSpectatorPresentationComponent::ExitSpectatorInputMode()
{
    if (!IsLocalController())
    {
        return;
    }

    if (IsValid(LocalInputConfig) && IsValid(LocalInputConfig->SpectatorMappingContext.Get()))
    {
        if (UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetEnhancedInputSubsystem())
        {
            FModifyContextOptions ModifyOptions;

            ModifyOptions.bIgnoreAllPressedKeysUntilRelease = true;

            InputSubsystem->RemoveMappingContext(LocalInputConfig->SpectatorMappingContext, ModifyOptions);
        }
    }

    bSpectatorInputModeActive = false;

    UE_LOG(LogDMSpectatorPresentation, Log, TEXT("Spectator Input mode exited."));
}

void UDMSpectatorPresentationComponent::InputSpectatorMove(const FInputActionValue& InputActionValue)
{
    if (!bSpectatorInputModeActive || LocalSpectatorPhase != EDMSpectatorPhase::FreeCamera
        || !LocalRestrictedSpectatorPawn.IsValid())
    {
        return;
    }

    const FVector2D MovementInput = InputActionValue.Get<FVector2D>();

    if (MovementInput.IsNearlyZero())
    {
        return;
    }

    ADMRestrictedSpectatorPawn* RestrictedSpectatorPawn = LocalRestrictedSpectatorPawn.Get();

    if (!IsValid(RestrictedSpectatorPawn))
    {
        return;
    }

    FVector DesiredMovement = RestrictedSpectatorPawn->GetActorForwardVector() * MovementInput.Y
        + RestrictedSpectatorPawn->GetActorRightVector() * MovementInput.X;

    const float InputMagnitude = FMath::Min(DesiredMovement.Size(), 1.0f);

    if (InputMagnitude <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    DesiredMovement.Normalize();

    RestrictedSpectatorPawn->AddRestrictedMovementInput(DesiredMovement, InputMagnitude);
}

void UDMSpectatorPresentationComponent::InputSpectatorLook(const FInputActionValue& InputActionValue)
{
    if (!bSpectatorInputModeActive || LocalSpectatorPhase != EDMSpectatorPhase::FreeCamera
        || !LocalRestrictedSpectatorPawn.IsValid() || !IsValid(GetOwnerCameraManager()))
    {
        return;
    }

    const FVector2D LookInput = InputActionValue.Get<FVector2D>();

    if (LookInput.IsNearlyZero())
    {
        return;
    }

    ADMRestrictedSpectatorPawn* RestrictedSpectatorPawn = LocalRestrictedSpectatorPawn.Get();

    float Sensitivity = 1.0f;
    if (UGameInstance* Instance = GetWorld() ? GetWorld()->GetGameInstance() : nullptr)
    {
        if (const UDMLocalSettingsSubsystem* Settings = Instance->GetSubsystem<UDMLocalSettingsSubsystem>())
        {
            Sensitivity = Settings->GetMouseSensitivity();
        }
    }
    Sensitivity = FMath::IsFinite(Sensitivity) ? FMath::Clamp(Sensitivity, 0.1f, 3.0f) : 1.0f;

    FRotator NewRotation = RestrictedSpectatorPawn->GetActorRotation();
    NewRotation.Yaw = FRotator::NormalizeAxis(NewRotation.Yaw + LookInput.X * Sensitivity);

    // 관전 Pawn의 Actor Pitch는 일반 Controller Pitch와 부호 해석이 달라 상하 반전을 보정한다.
    const float DesiredPitch = FRotator::NormalizeAxis(NewRotation.Pitch + LookInput.Y * Sensitivity);

    NewRotation.Pitch =
        FMath::Clamp(DesiredPitch, GetOwnerCameraManager()->ViewPitchMin, GetOwnerCameraManager()->ViewPitchMax);

    NewRotation.Roll = 0.0f;

    RestrictedSpectatorPawn->SetActorRotation(NewRotation);
}

double UDMSpectatorPresentationComponent::GetElapsedSpectatorTime() const
{
    const UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return 0.0;
    }

    const AGameStateBase* GameState = World->GetGameState();

    if (!IsValid(GameState) || LocalSpectatorContext.SpectatorStartServerTime <= 0.0)
    {
        return 0.0;
    }

    return FMath::Max(0.0, GameState->GetServerWorldTimeSeconds() - LocalSpectatorContext.SpectatorStartServerTime);
}

void UDMSpectatorPresentationComponent::BeginCorpseSpectatorPhase()
{
    const FDMSpectatorPresentationSettings Settings = GetPresentationSettings();

    if (!IsLocalController() || ShouldBlockSpectatorPresentation())
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    /*
     * 같은 Controller에서 이전 관전 Presentation이 남아 있으면
     * 먼저 정리한다.
     */
    World->GetTimerManager().ClearTimer(LocalSpectatorPhaseTimerHandle);

    ClearObservedKillerViewBindings();

    CleanupLocalRestrictedSpectatorPawn();

    CleanupLocalDeathCamera();

    CleanupLocalSpectatorFadePresentation();

    const double ElapsedSpectatorTime = GetElapsedSpectatorTime();

    const float RemainingCorpseTime =
        FMath::Max(0.0f, CorpseSpectatorPhaseDuration - static_cast<float>(ElapsedSpectatorTime));

    /*
     * RPC가 5초 이상 늦게 도착했다면
     * Corpse Camera를 잠깐 보여주지 않는다.
     *
     * Corpse 구간이 이미 끝난 시점이라면
     * KillerView 단계로 즉시 진행한다.
     */
    if (RemainingCorpseTime <= KINDA_SMALL_NUMBER)
    {
        HandleCorpseSpectatorPhaseExpired();

        return;
    }

    LocalSpectatorPhase = EDMSpectatorPhase::Corpse;

    // DeathCam 단계로 진입했음을 로컬 관전 HUD에 전달한다.
    LocalSpectatorPhaseChangedDelegate.Broadcast(LocalSpectatorPhase);

    /*
     * ==================================================
     * Corpse Focus
     * ==================================================
     */
    ADMPlayerCharacter* CorpseCharacter = GetPawn<ADMPlayerCharacter>();

    FVector FocusLocation = LocalSpectatorContext.DeathLocation;

    if (IsValid(CorpseCharacter))
    {
        USkeletalMeshComponent* CorpseMesh = CorpseCharacter->GetMesh();

        if (IsValid(CorpseMesh))
        {
            /*
             * 기본 View가 1인칭이었다면
             * 기존 Camera Policy 때문에 자기 Mesh가
             * OwnerNoSee일 수 있다.
             *
             * DeathCam에서는 자기 시체를 보여줘야 한다.
             */
            CorpseMesh->SetOwnerNoSee(false);

            /*
             * Capsule 위치가 아니라 현재 SkeletalMesh Bounds를
             * DeathCam 중심으로 사용한다.
             *
             * 특정 "pelvis" Bone 이름에 의존하지 않는다.
             */
            const FBoxSphereBounds CorpseBounds = CorpseMesh->CalcBounds(CorpseMesh->GetComponentTransform());

            if (!CorpseBounds.Origin.ContainsNaN())
            {
                FocusLocation = CorpseBounds.Origin;
            }
        }
    }

    /*
     * ==================================================
     * DeathCam 방향
     * ==================================================
     *
     * 가능하면 사망 직전 Local Camera가 있던 방향을
     * 수평 기준으로 유지한다.
     */
    FVector CurrentCameraLocation = FocusLocation - FVector::ForwardVector * Settings.DeathCamDistance;

    if (IsValid(GetOwnerCameraManager()))
    {
        CurrentCameraLocation = GetOwnerCameraManager()->GetCameraLocation();
    }

    FVector HorizontalCameraDirection = CurrentCameraLocation - FocusLocation;

    HorizontalCameraDirection.Z = 0.0f;

    if (!HorizontalCameraDirection.Normalize())
    {
        if (IsValid(CorpseCharacter))
        {
            HorizontalCameraDirection = -CorpseCharacter->GetActorForwardVector();
        }
        else
        {
            HorizontalCameraDirection = -GetControlRotation().Vector();
        }

        HorizontalCameraDirection.Z = 0.0f;

        if (!HorizontalCameraDirection.Normalize())
        {
            HorizontalCameraDirection = -FVector::ForwardVector;
        }
    }

    FVector DesiredCameraLocation = FocusLocation + HorizontalCameraDirection * Settings.DeathCamDistance
        + FVector(0.0f, 0.0f, Settings.DeathCamHeight);

    /*
     * ==================================================
     * Camera Wall Collision
     * ==================================================
     *
     * Focus → Camera 위치 사이에 Camera Channel을
     * 막는 구조물이 있으면 그 앞까지만 이동한다.
     */
    FCollisionQueryParams CameraTraceParams(SCENE_QUERY_STAT(DMDeathCameraTrace), false);

    if (IsValid(CorpseCharacter))
    {
        CameraTraceParams.AddIgnoredActor(CorpseCharacter);
    }

    FHitResult CameraHit;

    if (World->LineTraceSingleByChannel(CameraHit, FocusLocation, DesiredCameraLocation, ECC_Camera, CameraTraceParams))
    {
        DesiredCameraLocation = CameraHit.Location + CameraHit.ImpactNormal * DeathCamWallPadding;
    }

    const FRotator DeathCameraRotation = (FocusLocation - DesiredCameraLocation).Rotation();

    /*
     * ==================================================
     * Local Camera Actor
     * ==================================================
     */
    FActorSpawnParameters SpawnParameters;

    SpawnParameters.Owner = GetOwnerController();

    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    LocalDeathCameraActor = World->SpawnActor<ACameraActor>(
        ACameraActor::StaticClass(), DesiredCameraLocation, DeathCameraRotation, SpawnParameters
    );

    if (!IsValid(LocalDeathCameraActor))
    {
        UE_LOG(LogDMSpectatorPresentation, Error, TEXT("DeathCam failed: Local CameraActor could not be spawned."));

        return;
    }

    /*
     * Gameplay Camera → DeathCam.
     */
    SetViewTargetWithBlend(LocalDeathCameraActor, Settings.DeathCamBlendTime, VTBlend_Cubic, 2.0f, true);

    /*
     * Corpse Phase의 남은 시간만 Timer를 설정한다.
     *
     * Server Respawn Timer와는 독립적인
     * Local Presentation Timer다.
     */
    World->GetTimerManager().SetTimer(
        LocalSpectatorPhaseTimerHandle, this, &ThisClass::HandleCorpseSpectatorPhaseExpired, RemainingCorpseTime, false
    );

    UE_LOG(
        LogDMSpectatorPresentation,
        Log,
        TEXT("Corpse DeathCam started. Remaining=%.3f, Focus=%s, Camera=%s"),
        RemainingCorpseTime,
        *FocusLocation.ToCompactString(),
        *DesiredCameraLocation.ToCompactString()
    );
}

void UDMSpectatorPresentationComponent::HandleCorpseSpectatorPhaseExpired()
{
    if (!IsLocalController())
    {
        return;
    }

    UE_LOG(LogDMSpectatorPresentation, Log, TEXT("Corpse Spectator Phase expired. Starting KillerView."));

    BeginKillerViewSpectatorPhase();
}

void UDMSpectatorPresentationComponent::BeginKillerViewSpectatorPhase()
{
    const FDMSpectatorPresentationSettings Settings = GetPresentationSettings();

    if (!IsLocalController() || ShouldBlockSpectatorPresentation())
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    World->GetTimerManager().ClearTimer(LocalSpectatorPhaseTimerHandle);

    ClearObservedKillerViewBindings();

    const double ElapsedSpectatorTime = GetElapsedSpectatorTime();

    const float RemainingKillerViewTime =
        FMath::Max(0.0f, KillerSpectatorPhaseEndTime - static_cast<float>(ElapsedSpectatorTime));

    /*
     * KillerView 구간이 이미 끝났다면
     * FreeCamera 단계로 즉시 진행한다.
     */
    if (RemainingKillerViewTime <= KINDA_SMALL_NUMBER)
    {
        HandleKillerViewSpectatorPhaseExpired();

        return;
    }

    LocalSpectatorPhase = EDMSpectatorPhase::KillerView;

    // 공격자 관찰 구간으로 변경되었음을 알린다.
    LocalSpectatorPhaseChangedDelegate.Broadcast(LocalSpectatorPhase);

    APawn* KillerPawn = LocalSpectatorContext.KillerPawn.Get();

    ADMPlayerCharacter* KillerCharacter = Cast<ADMPlayerCharacter>(KillerPawn);

    /*
     * Killer Actor 자체가 이미 없으면
     * DeathCam을 그대로 유지한다.
     */
    if (!IsValid(KillerCharacter))
    {
        FallbackKillerViewToDeathCamera(TEXT("Killer Pawn is invalid."));
    }
    else
    {
        UDMJellyPhysicsComponent* KillerPhysicsComponent = KillerCharacter->GetJellyPhysicsComponent();

        /*
         * Killer가 5초 DeathCam 구간 동안
         * 이미 실제 사망했다면
         * KillerView로 넘어가지 않는다.
         */
        if (IsValid(KillerPhysicsComponent) && KillerPhysicsComponent->IsDeathPresentationActive())
        {
            FallbackKillerViewToDeathCamera(TEXT("Killer is already in Death Presentation."));
        }
        else
        {
            LocalObservedKillerPawn = KillerPawn;

            if (IsValid(KillerPhysicsComponent))
            {
                LocalObservedKillerPhysicsComponent = KillerPhysicsComponent;

                KillerPhysicsComponent->OnDeathPresentationStarted().AddUObject(
                    this, &ThisClass::HandleObservedKillerDeathPresentationStarted
                );
            }

            KillerPawn->OnDestroyed.AddDynamic(this, &ThisClass::HandleObservedKillerPawnDestroyed);

            /*
             * Killer Pawn 자체를 ViewTarget으로 사용한다.
             *
             * ADMPlayerCharacter에는 FollowCamera가 있으므로
             * Live World의 해당 Pawn Camera를 사용한다.
             */
            SetViewTargetWithBlend(KillerPawn, Settings.KillerViewBlendTime, VTBlend_Cubic, 2.0f, true);

            UE_LOG(
                LogDMSpectatorPresentation,
                Log,
                TEXT("KillerView started. Killer=%s, Remaining=%.3f"),
                *GetNameSafe(KillerPawn),
                RemainingKillerViewTime
            );
        }
    }

    /*
     * Killer가 없어 fallback 중이어도
     * 5~10초 시간 구간 자체는 그대로 소비한다.
     */
    World->GetTimerManager().SetTimer(
        LocalSpectatorPhaseTimerHandle,
        this,
        &ThisClass::HandleKillerViewSpectatorPhaseExpired,
        RemainingKillerViewTime,
        false
    );
}

void UDMSpectatorPresentationComponent::HandleObservedKillerDeathPresentationStarted()
{
    if (LocalSpectatorPhase != EDMSpectatorPhase::KillerView)
    {
        return;
    }

    FallbackKillerViewToDeathCamera(TEXT("Observed Killer started Death Presentation."));
}

void UDMSpectatorPresentationComponent::HandleObservedKillerPawnDestroyed(AActor* DestroyedActor)
{
    if (LocalSpectatorPhase != EDMSpectatorPhase::KillerView)
    {
        return;
    }

    FallbackKillerViewToDeathCamera(TEXT("Observed Killer Pawn was destroyed."));
}

void UDMSpectatorPresentationComponent::FallbackKillerViewToDeathCamera(const TCHAR* Reason)
{
    const FDMSpectatorPresentationSettings Settings = GetPresentationSettings();

    if (!IsLocalController() || ShouldBlockSpectatorPresentation())
    {
        return;
    }

    ClearObservedKillerViewBindings();

    AActor* FallbackViewTarget = nullptr;

    if (IsValid(LocalDeathCameraActor))
    {
        FallbackViewTarget = LocalDeathCameraActor;
    }
    else
    {
        /*
         * 매우 늦은 RPC 등으로 DeathCam Actor가
         * 만들어지지 않은 예외 상황의 마지막 fallback.
         */
        FallbackViewTarget = GetPawn();
    }

    if (IsValid(FallbackViewTarget) && GetViewTarget() != FallbackViewTarget)
    {
        SetViewTargetWithBlend(FallbackViewTarget, Settings.KillerViewBlendTime, VTBlend_Cubic, 2.0f, true);
    }

    UE_LOG(
        LogDMSpectatorPresentation,
        Log,
        TEXT("KillerView fallback activated. Reason=%s, Fallback=%s"),
        Reason != nullptr ? Reason : TEXT("Unknown"),
        *GetNameSafe(FallbackViewTarget)
    );
}

void UDMSpectatorPresentationComponent::ClearObservedKillerViewBindings()
{
    if (LocalObservedKillerPhysicsComponent.IsValid())
    {
        LocalObservedKillerPhysicsComponent->OnDeathPresentationStarted().RemoveAll(this);
    }

    if (LocalObservedKillerPawn.IsValid())
    {
        LocalObservedKillerPawn->OnDestroyed.RemoveDynamic(this, &ThisClass::HandleObservedKillerPawnDestroyed);
    }

    LocalObservedKillerPhysicsComponent.Reset();

    LocalObservedKillerPawn.Reset();
}

void UDMSpectatorPresentationComponent::HandleKillerViewSpectatorPhaseExpired()
{
    if (!IsLocalController())
    {
        return;
    }

    ClearObservedKillerViewBindings();

    UE_LOG(LogDMSpectatorPresentation, Log, TEXT("KillerView Spectator Phase expired. Starting FreeCamera."));

    BeginFreeCameraSpectatorPhase();
}

void UDMSpectatorPresentationComponent::BeginFreeCameraSpectatorPhase()
{
    const FDMSpectatorPresentationSettings Settings = GetPresentationSettings();

    if (!IsLocalController() || ShouldBlockSpectatorPresentation())
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    World->GetTimerManager().ClearTimer(LocalSpectatorPhaseTimerHandle);

    ExitSpectatorInputMode();

    CleanupLocalRestrictedSpectatorPawn();

    const double ElapsedSpectatorTime = GetElapsedSpectatorTime();

    const float RemainingFreeCameraTime =
        FMath::Max(0.0f, FreeCameraSpectatorPhaseEndTime - static_cast<float>(ElapsedSpectatorTime));

    /*
     * Client RPC / frame 지연 때문에 이미
     * Spectator 시작 후 25초가 지났다면
     * FreeCamera를 뒤늦게 시작하지 않는다.
     */
    if (RemainingFreeCameraTime <= KINDA_SMALL_NUMBER)
    {
        HandleFreeCameraSpectatorPhaseExpired();

        return;
    }

    LocalSpectatorPhase = EDMSpectatorPhase::FreeCamera;

    // 제한형 자유 카메라 단계로 전환되었음을 알린다.
    LocalSpectatorPhaseChangedDelegate.Broadcast(LocalSpectatorPhase);

    /*
     * 현재 실제로 화면에 보이는 Camera Transform을
     * FreeCamera 시작점으로 사용한다.
     *
     * 정상:
     * Killer Live View의 현재 위치
     *
     * Killer fallback:
     * DeathCam의 현재 위치
     */
    FVector StartCameraLocation = LocalSpectatorContext.DeathLocation;

    FRotator StartCameraRotation = GetControlRotation();

    if (IsValid(GetOwnerCameraManager()))
    {
        StartCameraLocation = GetOwnerCameraManager()->GetCameraLocation();

        StartCameraRotation = GetOwnerCameraManager()->GetCameraRotation();
    }

    FActorSpawnParameters SpawnParameters;

    SpawnParameters.Owner = GetOwnerController();

    SpawnParameters.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;

    ADMRestrictedSpectatorPawn* NewRestrictedSpectatorPawn = World->SpawnActor<ADMRestrictedSpectatorPawn>(
        ADMRestrictedSpectatorPawn::StaticClass(), StartCameraLocation, StartCameraRotation, SpawnParameters
    );

    if (IsValid(NewRestrictedSpectatorPawn))
    {
        LocalRestrictedSpectatorPawn = NewRestrictedSpectatorPawn;

        NewRestrictedSpectatorPawn->SetMaximumMovementSpeed(Settings.FreeCameraMaxSpeed);

        /*
         * Spawn 지점 자체가 현재 Camera Transform과 같으므로
         * Blend 없이 즉시 ViewTarget을 교체해도
         * 시각적인 위치 Jump가 발생하지 않는다.
         */
        SetViewTarget(NewRestrictedSpectatorPawn);

        /*
         * ViewTarget이 새 FreeCamera로 교체된 뒤에야
         * 기존 DeathCam Actor를 제거한다.
         *
         * Killer가 없는 fallback 상태에서도 안전하다.
         */
        CleanupLocalDeathCamera();

        if (!EnterSpectatorInputMode())
        {
            UE_LOG(LogDMSpectatorPresentation, Error, TEXT("FreeCamera started, but Spectator Input mode failed."));
        }

        UE_LOG(
            LogDMSpectatorPresentation,
            Log,
            TEXT("FreeCamera started. Remaining=%.3f, Location=%s, Rotation=%s, MaxSpeed=%.1f"),
            RemainingFreeCameraTime,
            *StartCameraLocation.ToCompactString(),
            *StartCameraRotation.ToCompactString(),
            Settings.FreeCameraMaxSpeed
        );
    }
    else
    {
        UE_LOG(
            LogDMSpectatorPresentation,
            Error,
            TEXT("FreeCamera Pawn spawn failed. Current ViewTarget will be retained.")
        );
    }

    /*
     * Pawn Spawn에 실패하더라도
     * 전체 Spectator 시간표는 바꾸지 않는다.
     */
    World->GetTimerManager().SetTimer(
        LocalSpectatorPhaseTimerHandle,
        this,
        &ThisClass::HandleFreeCameraSpectatorPhaseExpired,
        RemainingFreeCameraTime,
        false
    );
}

void UDMSpectatorPresentationComponent::HandleFreeCameraSpectatorPhaseExpired()
{
    if (!IsLocalController())
    {
        return;
    }

    /*
     * 25초부터는 카메라를 더 움직이지 못한다.
     *
     * Pawn/ViewTarget 자체는 아직 유지한다.
     */
    ExitSpectatorInputMode();

    UE_LOG(LogDMSpectatorPresentation, Log, TEXT("FreeCamera Spectator Phase expired. Starting Fade."));

    BeginFadeSpectatorPhase();
}

void UDMSpectatorPresentationComponent::BeginFadeSpectatorPhase()
{
    const FDMSpectatorPresentationSettings Settings = GetPresentationSettings();

    if (!IsLocalController() || ShouldBlockSpectatorPresentation())
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    World->GetTimerManager().ClearTimer(LocalSpectatorPhaseTimerHandle);

    World->GetTimerManager().ClearTimer(LocalSpectatorCountdownTimerHandle);

    /*
     * 혹시 입력 Context가 남아 있다면
     * Fade 진입과 동시에 확실히 제거한다.
     */
    ExitSpectatorInputMode();

    LocalSpectatorPhase = EDMSpectatorPhase::Fade;

    // 마지막 5초는 기존 SpectatorCountdownWidget이 표시한다.
    LocalSpectatorPhaseChangedDelegate.Broadcast(LocalSpectatorPhase);

    const double ElapsedSpectatorTime = GetElapsedSpectatorTime();

    const float RemainingFadeTime =
        FMath::Max(0.0f, FadeSpectatorPhaseEndTime - static_cast<float>(ElapsedSpectatorTime));

    /*
     * 이미 ServerTime 기준 30초를 넘겼다면
     * 뒤늦게 Fade를 새로 시작하지 않는다.
     */
    if (RemainingFadeTime <= KINDA_SMALL_NUMBER)
    {
        HandleFadeSpectatorPhaseExpired();

        return;
    }

    /*
     * ==================================================
     * Camera Fade
     * ==================================================
     *
     * Audio는 Fade하지 않는다.
     *
     * Dead Global Voice와
     * 일반 Audio 수신을 끊지 않기 위해서다.
     */
    if (IsValid(GetOwnerCameraManager()))
    {
        const float ActualFadeDuration = FMath::Min(Settings.SpectatorFadeToBlackDuration, RemainingFadeTime);

        GetOwnerCameraManager()->StartCameraFade(0.0f, 1.0f, ActualFadeDuration, FLinearColor::Black, false, true);

        bLocalSpectatorFadeActive = true;
    }

    /*
     * ==================================================
     * Countdown Widget
     * ==================================================
     */
    if (!IsValid(SpectatorCountdownWidget))
    {
        if (Settings.SpectatorCountdownWidgetClass)
        {
            SpectatorCountdownWidget =
                CreateWidget<UDMSpectatorCountdownWidget>(GetOwnerController(), Settings.SpectatorCountdownWidgetClass);

            if (IsValid(SpectatorCountdownWidget))
            {
                SpectatorCountdownWidget->AddToViewport(SpectatorCountdownWidgetZOrder);
            }
        }
        else
        {
            UE_LOG(LogDMSpectatorPresentation, Warning, TEXT("Spectator Countdown Widget Class is not assigned."));
        }
    }

    LocalDisplayedFadeCountdown = INDEX_NONE;

    UpdateFadeSpectatorCountdown();

    /*
     * 전체 30초 시점까지 남은 시간만 설정한다.
     */
    World->GetTimerManager().SetTimer(
        LocalSpectatorPhaseTimerHandle, this, &ThisClass::HandleFadeSpectatorPhaseExpired, RemainingFadeTime, false
    );

    UE_LOG(LogDMSpectatorPresentation, Log, TEXT("Spectator Fade started. Remaining=%.3f"), RemainingFadeTime);
}

void UDMSpectatorPresentationComponent::UpdateFadeSpectatorCountdown()
{
    if (!IsLocalController() || ShouldBlockSpectatorPresentation() || LocalSpectatorPhase != EDMSpectatorPhase::Fade)
    {
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    World->GetTimerManager().ClearTimer(LocalSpectatorCountdownTimerHandle);

    const double ElapsedSpectatorTime = GetElapsedSpectatorTime();

    const float RemainingFadeTime =
        FMath::Max(0.0f, FadeSpectatorPhaseEndTime - static_cast<float>(ElapsedSpectatorTime));

    if (RemainingFadeTime <= KINDA_SMALL_NUMBER)
    {
        if (LocalDisplayedFadeCountdown != 0)
        {
            LocalDisplayedFadeCountdown = 0;

            if (IsValid(SpectatorCountdownWidget))
            {
                SpectatorCountdownWidget->SetCountdownValue(0);
            }
        }

        return;
    }

    const int32 CountdownValue = FMath::Clamp(FMath::CeilToInt(RemainingFadeTime - KINDA_SMALL_NUMBER), 1, 5);

    if (CountdownValue != LocalDisplayedFadeCountdown)
    {
        LocalDisplayedFadeCountdown = CountdownValue;

        if (IsValid(SpectatorCountdownWidget))
        {
            SpectatorCountdownWidget->SetCountdownValue(CountdownValue);
        }

        UE_LOG(LogDMSpectatorPresentation, Log, TEXT("Spectator Respawn Countdown: %d"), CountdownValue);
    }

    /*
     * 1은 마지막 숫자이므로
     * 추가 숫자 경계 Timer가 필요 없다.
     *
     * 30초 종료는 Phase Timer가 담당한다.
     */
    if (CountdownValue <= 1)
    {
        return;
    }

    const float NextBoundaryRemaining = static_cast<float>(CountdownValue - 1);

    const float TimeUntilNextBoundary =
        FMath::Max(0.01f, RemainingFadeTime - NextBoundaryRemaining + FadeCountdownBoundarySafetyOffset);

    World->GetTimerManager().SetTimer(
        LocalSpectatorCountdownTimerHandle, this, &ThisClass::UpdateFadeSpectatorCountdown, TimeUntilNextBoundary, false
    );
}

void UDMSpectatorPresentationComponent::HandleFadeSpectatorPhaseExpired()
{
    if (!IsLocalController())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(LocalSpectatorCountdownTimerHandle);
    }

    LocalDisplayedFadeCountdown = 0;

    if (IsValid(SpectatorCountdownWidget))
    {
        SpectatorCountdownWidget->SetCountdownValue(0);
    }

    UE_LOG(
        LogDMSpectatorPresentation,
        Log,
        TEXT("Spectator Fade Phase expired. Waiting for server-confirmed respawn handoff.")
    );

    /*
     * 중요:
     *
     * 여기서 StopCameraFade() 하지 않는다.
     * 여기서 FreeCamera Pawn을 Destroy하지 않는다.
     *
     * Server Respawn이 실제로 완료되기 전에
     * 죽은 Pawn 화면이 잠깐 보이는 것을 방지한다.
     *
     * 실제 Respawn 성공이 Server에서 확정될 때까지
     * Black 상태를 유지한다
     */
}

void UDMSpectatorPresentationComponent::CleanupLocalSpectatorFadePresentation()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(LocalSpectatorCountdownTimerHandle);
    }

    if (bLocalSpectatorFadeActive && IsValid(GetOwnerCameraManager()))
    {
        GetOwnerCameraManager()->StopCameraFade();
    }

    bLocalSpectatorFadeActive = false;

    LocalDisplayedFadeCountdown = INDEX_NONE;

    if (IsValid(SpectatorCountdownWidget))
    {
        SpectatorCountdownWidget->RemoveFromParent();

        SpectatorCountdownWidget = nullptr;
    }
}

void UDMSpectatorPresentationComponent::CleanupLocalDeathCamera()
{
    if (!IsValid(LocalDeathCameraActor))
    {
        LocalDeathCameraActor = nullptr;

        return;
    }

    LocalDeathCameraActor->Destroy();

    LocalDeathCameraActor = nullptr;
}

void UDMSpectatorPresentationComponent::CleanupLocalRestrictedSpectatorPawn()
{
    ADMRestrictedSpectatorPawn* RestrictedSpectatorPawn = LocalRestrictedSpectatorPawn.Get();

    if (!IsValid(RestrictedSpectatorPawn))
    {
        LocalRestrictedSpectatorPawn.Reset();

        return;
    }

    /*
     * 현재 ViewTarget인 상태로 Actor부터 파괴하면
     * CameraManager가 잘못된 Target을 잠깐 볼 수 있다.
     *
     * 현재 정상 Pawn이 존재하면 우선 그쪽으로 되돌린다.
     */
    if (GetViewTarget() == RestrictedSpectatorPawn)
    {
        if (APawn* ControlledPawn = GetPawn())
        {
            SetViewTarget(ControlledPawn);
        }
    }

    RestrictedSpectatorPawn->Destroy();

    LocalRestrictedSpectatorPawn.Reset();
}

#pragma endregion
