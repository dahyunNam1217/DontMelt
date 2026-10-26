#include "Characters/DMPlayerCharacter.h"

#include "Core/DMGameplayUtils.h"

#include "Framework/Match/DMMatchPlayerController.h"
#include "Characters/Components/DMCombatComponent.h"
#include "Characters/Components/DMInteractionComponent.h"
#include "Characters/Components/DMPoseComponent.h"
#include "Characters/Components/DMCharacterStateComponent.h"
#include "Characters/Components/DMCharacterMovementComponent.h"
#include "Characters/Components/DMJellyPhysicsComponent.h"
#include "Input/DMInputConfigDataAsset.h"
#include "Animation/AnimMontage.h"
#include "Relic/DMRelicChest.h"
#include "Core/DMGameplayTags.h"
#include "Core/DMLocalSettingsSubsystem.h"
#include "Quests/DMQuestTypes.h"

#include "Camera/CameraComponent.h"
#include "Camera/PlayerCameraManager.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/AudioComponent.h"
#include "Engine/LocalPlayer.h"
#include "Engine/GameInstance.h"
#include "Kismet/GameplayStatics.h"
#include "Net/UnrealNetwork.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystemInterface.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/SpringArmComponent.h"
#include "InputActionValue.h"
#include "Math/RotationMatrix.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMPlayerCharacter, Log, All);

#pragma region Lifecycle

ADMPlayerCharacter::ADMPlayerCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    CameraBoom = CreateDefaultSubobject<USpringArmComponent>(TEXT("CameraBoom"));

    CameraBoom->SetupAttachment(GetRootComponent());

    CameraBoom->bUsePawnControlRotation = true;
    CameraBoom->bDoCollisionTest = true;

    FollowCamera = CreateDefaultSubobject<UCameraComponent>(TEXT("FollowCamera"));

    FollowCamera->SetupAttachment(CameraBoom, USpringArmComponent::SocketName);

    FollowCamera->bUsePawnControlRotation = false;

    CombatComponent = CreateDefaultSubobject<UDMCombatComponent>(TEXT("CombatComponent"));

    InteractionComponent = CreateDefaultSubobject<UDMInteractionComponent>(TEXT("InteractionComponent"));

    PoseComponent = CreateDefaultSubobject<UDMPoseComponent>(TEXT("PoseComponent"));

    // 사이렌 전용 컴포넌트를 하나만 만들고, 처음에는 재생하지 않는다.
    QuestSirenAudioComponent = CreateDefaultSubobject<UAudioComponent>(TEXT("QuestSirenAudioComponent"));

    QuestSirenAudioComponent->SetupAttachment(GetRootComponent());

    QuestSirenAudioComponent->bAutoActivate = false;
    QuestSirenAudioComponent->bAllowSpatialization = true;
}

void ADMPlayerCharacter::BeginPlay()
{
    Super::BeginPlay();

    if (HasAuthority())
    {
        if (UDMCharacterMovementComponent* MovementComponent =
                Cast<UDMCharacterMovementComponent>(GetCharacterMovement()))
        {
            MovementComponent->OnAuthoritativeSprintStateChanged().AddUObject(
                this, &ADMPlayerCharacter::HandleAuthoritativeSprintStateChanged
            );
        }

        if (PoseComponent != nullptr)
        {
            PoseComponent->OnPoseTypeChanged().AddUObject(this, &ADMPlayerCharacter::HandlePoseTypeChanged);
        }

        if (UDMCharacterStateComponent* StateComponent = GetCharacterStateComponent())
        {
            StateComponent->OnAuthoritativeEmoteStateChanged().AddUObject(
                this, &ADMPlayerCharacter::HandleAuthoritativeEmoteStateChanged
            );
        }
    }

    if (USkeletalMeshComponent* CharacterMesh = GetMesh())
    {
        /*
         * 플레이어 메시의 Animation Notify와 Bone 갱신 정책을 유지한다.
         *
         * 투명화는 머티리얼을 변경하며,
         * 카메라의 OwnerNoSee 설정은 별도로 유지한다.
         */
        CharacterMesh->VisibilityBasedAnimTickOption = EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones;
    }

    if (!ensureMsgf(CameraBoom != nullptr, TEXT("[%s] CameraBoom is null during BeginPlay."), *GetNameSafe(this)))
    {
        return;
    }

    /*
     * 기본 View는 항상 1인칭이다.
     * Pose 진입/종료는 View를 변경하지 않는다.
     */
    bIsFirstPersonView = true;

    if (IsLocallyControlled())
    {
        ApplyLocalCameraView();
    }

    ApplyViewRotationPolicy();
}

void ADMPlayerCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 사망·Respawn·퇴장·맵 이동 등으로 Character가 사라질 때 소리를 확실히 정지한다.
    if (IsValid(QuestSirenAudioComponent))
    {
        QuestSirenAudioComponent->Stop();
    }

    Super::EndPlay(EndPlayReason);
}

void ADMPlayerCharacter::OnJumped_Implementation()
{
    Super::OnJumped_Implementation();

    if (!HasAuthority())
    {
        return;
    }

    BroadcastQuestActionEvent(DMGameplayTags::QuestEvent_Jump);
}

void ADMPlayerCharacter::PawnClientRestart()
{
    Super::PawnClientRestart();

    // Spawn 실패 후 서버가 사망한 옛 Pawn을 임시로 재소유한 경우
    // 죽은 캐릭터의 Gameplay MappingContext를 다시 활성화하지 않는다.
    if (const UDMJellyPhysicsComponent* Physics = GetJellyPhysicsComponent();
        IsValid(Physics) && Physics->IsDeathPresentationActive())
    {
        PrepareLocalInputForSpectating();
        UE_LOG(LogDMPlayerCharacter, Warning, TEXT("Dead Pawn restart ignored for gameplay input. Pawn=%s"), *GetNameSafe(this));
        return;
    }

    UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetEnhancedInputSubsystem();

    if (!ensureMsgf(InputSubsystem != nullptr, TEXT("[%s] EnhancedInputSubsystem is null."), *GetNameSafe(this)))
    {
        return;
    }

    if (!ensureMsgf(InputConfig != nullptr, TEXT("[%s] InputConfig is null."), *GetNameSafe(this)))
    {
        return;
    }

    if (InputConfig->GameplayMappingContext == nullptr)
    {
        UE_LOG(
            LogDMPlayerCharacter,
            Log,
            TEXT("[%s] GameplayMappingContext is not assigned in InputConfig."),
            *GetNameSafe(this)
        );

        return;
    }

    ensureMsgf(
        InputConfig->PoseMappingContext != nullptr,
        TEXT("[%s] PoseMappingContext is not assigned in InputConfig."),
        *GetNameSafe(this)
    );

    bIsPoseInputMode = false;
    bIsInteractionInputMode = false;
    bIsSkillTargetingInputMode = false;
    // Pawn 입력 초기화 후 이전 자세 메뉴 표시를 정리한다.
    LocalPoseInputModeChangedDelegate.Broadcast(false);
    bIsFirstPersonView = true;

    if (Controller != nullptr)
    {
        FRotator InitialControlRotation = GetActorRotation();

        InitialControlRotation.Pitch = 0.0f;
        InitialControlRotation.Roll = 0.0f;

        Controller->SetControlRotation(InitialControlRotation);
    }

    FModifyContextOptions ModifyOptions;
    ModifyOptions.bIgnoreAllPressedKeysUntilRelease = true;

    InputSubsystem->RemoveMappingContext(InputConfig->GameplayMappingContext, ModifyOptions);

    if (InputConfig->PoseMappingContext != nullptr)
    {
        InputSubsystem->RemoveMappingContext(InputConfig->PoseMappingContext, ModifyOptions);
    }

    if (InputConfig->InteractionMappingContext != nullptr)
    {
        InputSubsystem->RemoveMappingContext(InputConfig->InteractionMappingContext, ModifyOptions);
    }

    if (InputConfig->SkillTargetingMappingContext != nullptr)
    {
        InputSubsystem->RemoveMappingContext(InputConfig->SkillTargetingMappingContext, ModifyOptions);
    }

    if (InputConfig->SpectatorMappingContext != nullptr)
    {
        InputSubsystem->RemoveMappingContext(InputConfig->SpectatorMappingContext, ModifyOptions);
    }

    InputSubsystem->AddMappingContext(InputConfig->GameplayMappingContext, 0);

    UE_LOG(LogDMPlayerCharacter, Log, TEXT("[%s] GameplayMappingContext added successfully."), *GetNameSafe(this));

    ApplyLocalCameraView();
    ApplyViewRotationPolicy();

    if (!HasAuthority())
    {
        ServerSetFirstPersonView(true);
    }

    /*
     * Local Pawn / Input 준비가 끝난 뒤
     * PlayerController가 HUD를 생성하고
     * 현재 Character의 Component와 연결한다.
     */
    if (ADMMatchPlayerController* MatchPlayerController = Cast<ADMMatchPlayerController>(GetController()))
    {
        /*
         * PlayerController는 Respawn에도 살아남으므로
         * 이전 Pawn에서 남은 Targeting Context를 정리한다.
         */
        MatchPlayerController->CancelSkillTargetingLocal();

        MatchPlayerController->InitializeSpectatorInput(this);

        MatchPlayerController->RefreshLocalVoiceModeForGameplayPawn(this);

        // 현재 Pawn과 HUD 데이터 제공자를 Root에 다시 연결한다.
        MatchPlayerController->RefreshUIDataSources();

        // 정상 입력 설정이 실제로 끝난 후에만 부활 인계 준비 완료로 인정한다.
        // 이 함수가 승인 RPC보다 먼저 실행돼도 Controller가 준비 상태를 보관한다.
        MatchPlayerController->NotifyLocalGameplayPawnReady(this);

        // Gameplay Mapping Context 재등록 후 뷰포트 입력을 복구한다.
        MatchPlayerController->RestoreGameplayMouseCapture();
    }
}

void ADMPlayerCharacter::PrepareLocalInputForSpectating()
{
    if (!IsLocallyControlled())
    {
        return;
    }

    /*
     * Mapping Context 존재 여부와 관계없이
     * Local runtime mode는 반드시 종료한다.
     */
    bIsPoseInputMode = false;

    // 관전 진입 시 기존 자세 메뉴도 즉시 닫는다.
    LocalPoseInputModeChangedDelegate.Broadcast(false);

    bIsInteractionInputMode = false;

    bIsSkillTargetingInputMode = false;

    ClearSprintRequest();

    /*
     * 죽은 Pawn InputComponent 자체도
     * Enhanced Input Subsystem 성공 여부와 무관하게 막는다.
     */
    if (APlayerController* PlayerController = Cast<APlayerController>(GetController()))
    {
        DisableInput(PlayerController);
    }

    /*
     * 아래부터는 Mapping Context Cleanup.
     */
    if (!IsValid(InputConfig))
    {
        return;
    }

    UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetEnhancedInputSubsystem();

    if (!IsValid(InputSubsystem))
    {
        return;
    }

    FModifyContextOptions ModifyOptions;

    ModifyOptions.bIgnoreAllPressedKeysUntilRelease = true;

    /*
     * ==================================================
     * Gameplay Context Cleanup
     * ==================================================
     */

    if (InputConfig->GameplayMappingContext != nullptr)
    {
        InputSubsystem->RemoveMappingContext(InputConfig->GameplayMappingContext, ModifyOptions);
    }

    if (InputConfig->PoseMappingContext != nullptr)
    {
        InputSubsystem->RemoveMappingContext(InputConfig->PoseMappingContext, ModifyOptions);
    }

    if (InputConfig->InteractionMappingContext != nullptr)
    {
        InputSubsystem->RemoveMappingContext(InputConfig->InteractionMappingContext, ModifyOptions);
    }

    if (InputConfig->SkillTargetingMappingContext != nullptr)
    {
        InputSubsystem->RemoveMappingContext(InputConfig->SkillTargetingMappingContext, ModifyOptions);
    }

    /*
     * 이전 Spectator Context가 예외적으로 남아 있어도
     * Sequence 시작 시에는 제거한다.
     *
     * 10초 FreeCamera 진입 때 Controller가 다시 추가한다.
     */
    if (InputConfig->SpectatorMappingContext != nullptr)
    {
        InputSubsystem->RemoveMappingContext(InputConfig->SpectatorMappingContext, ModifyOptions);
    }

    UE_LOG(LogDMPlayerCharacter, Log, TEXT("[%s] Local Gameplay input prepared for spectating."), *GetNameSafe(this));
}

void ADMPlayerCharacter::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    Super::SetupPlayerInputComponent(PlayerInputComponent);

    UEnhancedInputComponent* EnhancedInputComponent = Cast<UEnhancedInputComponent>(PlayerInputComponent);

    if (!ensureMsgf(
            EnhancedInputComponent != nullptr,
            TEXT("[%s] PlayerInputComponent is not UEnhancedInputComponent."),
            *GetNameSafe(this)
        ))
    {
        return;
    }

    if (!ensureMsgf(InputConfig != nullptr, TEXT("[%s] InputConfig is null while binding input."), *GetNameSafe(this)))
    {
        return;
    }

    // Sprint / Attack / Defense / Pose / ExitPose는 필수 입력이다.
    // 누락되면 ensure로 알리고, 나머지 입력 바인딩은 계속 진행한다.
    BindMovementInput(EnhancedInputComponent);

    BindCombatInput(EnhancedInputComponent);

    BindInteractionInput(EnhancedInputComponent);

    BindPoseAndEmoteInput(EnhancedInputComponent);

    // 스킬 입력은 MatchPlayerController가 처리하므로 마지막에 바인딩한다.
    BindSkillInput(EnhancedInputComponent);
}

// View / Look / Move / Jump / Sprint
void ADMPlayerCharacter::BindMovementInput(UEnhancedInputComponent* EnhancedInputComponent)
{
    if (InputConfig->ViewAction != nullptr)
    {
        EnhancedInputComponent->BindAction(
            InputConfig->ViewAction, ETriggerEvent::Started, this, &ADMPlayerCharacter::InputViewStarted
        );
    }

    if (InputConfig->LookAction != nullptr)
    {
        EnhancedInputComponent->BindAction(
            InputConfig->LookAction, ETriggerEvent::Triggered, this, &ADMPlayerCharacter::InputLook
        );
    }

    if (InputConfig->MoveAction != nullptr)
    {
        EnhancedInputComponent->BindAction(
            InputConfig->MoveAction, ETriggerEvent::Triggered, this, &ADMPlayerCharacter::InputMove
        );
    }

    if (InputConfig->JumpAction != nullptr)
    {
        EnhancedInputComponent->BindAction(
            InputConfig->JumpAction, ETriggerEvent::Started, this, &ADMPlayerCharacter::InputJumpStarted
        );

        EnhancedInputComponent->BindAction(
            InputConfig->JumpAction, ETriggerEvent::Completed, this, &ADMPlayerCharacter::InputJumpCompleted
        );

        EnhancedInputComponent->BindAction(
            InputConfig->JumpAction, ETriggerEvent::Canceled, this, &ADMPlayerCharacter::InputJumpCompleted
        );
    }

    if (ensureMsgf(InputConfig->SprintAction != nullptr, TEXT("InputConfig is missing required SprintAction.")))
    {
        EnhancedInputComponent->BindAction(
            InputConfig->SprintAction, ETriggerEvent::Started, this, &ADMPlayerCharacter::InputSprintStarted
        );

        EnhancedInputComponent->BindAction(
            InputConfig->SprintAction, ETriggerEvent::Completed, this, &ADMPlayerCharacter::InputSprintCompleted
        );

        EnhancedInputComponent->BindAction(
            InputConfig->SprintAction, ETriggerEvent::Canceled, this, &ADMPlayerCharacter::InputSprintCompleted
        );
    }
}

// Attack / Defense
void ADMPlayerCharacter::BindCombatInput(UEnhancedInputComponent* EnhancedInputComponent)
{
    if (ensureMsgf(InputConfig->AttackAction != nullptr, TEXT("InputConfig is missing required AttackAction.")))
    {
        EnhancedInputComponent->BindAction(
            InputConfig->AttackAction, ETriggerEvent::Started, this, &ThisClass::InputAttackStarted
        );
    }

    if (ensureMsgf(InputConfig->DefenseAction != nullptr, TEXT("InputConfig is missing required DefenseAction.")))
    {
        EnhancedInputComponent->BindAction(
            InputConfig->DefenseAction, ETriggerEvent::Started, this, &ThisClass::InputDefenseStarted
        );
    }
}

// Interact / DropRelic / ConfirmInteraction / CloseInteraction
void ADMPlayerCharacter::BindInteractionInput(UEnhancedInputComponent* EnhancedInputComponent)
{
    if (InputConfig->InteractAction != nullptr)
    {
        EnhancedInputComponent->BindAction(
            InputConfig->InteractAction, ETriggerEvent::Started, this, &ThisClass::InputInteractStarted
        );

        EnhancedInputComponent->BindAction(
            InputConfig->InteractAction, ETriggerEvent::Completed, this, &ThisClass::InputInteractCompleted
        );

        EnhancedInputComponent->BindAction(
            InputConfig->InteractAction, ETriggerEvent::Canceled, this, &ThisClass::InputInteractCompleted
        );
    }

    if (InputConfig->DropRelicAction != nullptr)
    {
        EnhancedInputComponent->BindAction(
            InputConfig->DropRelicAction, ETriggerEvent::Started, this, &ThisClass::InputDropRelicStarted
        );

        UE_LOG(
            LogDMPlayerCharacter,
            Log,
            TEXT("DropRelicAction bound successfully. Action=%s"),
            *GetNameSafe(InputConfig->DropRelicAction)
        );
    }

    if (InputConfig->ConfirmInteractionAction != nullptr)
    {
        EnhancedInputComponent->BindAction(
            InputConfig->ConfirmInteractionAction,
            ETriggerEvent::Started,
            this,
            &ThisClass::InputConfirmInteractionStarted
        );

        EnhancedInputComponent->BindAction(
            InputConfig->ConfirmInteractionAction,
            ETriggerEvent::Completed,
            this,
            &ThisClass::InputConfirmInteractionCompleted
        );

        EnhancedInputComponent->BindAction(
            InputConfig->ConfirmInteractionAction,
            ETriggerEvent::Canceled,
            this,
            &ThisClass::InputConfirmInteractionCompleted
        );

        UE_LOG(
            LogDMPlayerCharacter,
            Log,
            TEXT("ConfirmInteractionAction bound successfully. Action=%s"),
            *GetNameSafe(InputConfig->ConfirmInteractionAction)
        );
    }

    if (InputConfig->CloseInteractionAction != nullptr)
    {
        EnhancedInputComponent->BindAction(
            InputConfig->CloseInteractionAction, ETriggerEvent::Started, this, &ThisClass::InputCloseInteractionStarted
        );

        UE_LOG(
            LogDMPlayerCharacter,
            Log,
            TEXT("CloseInteractionAction bound successfully. Action=%s"),
            *GetNameSafe(InputConfig->CloseInteractionAction)
        );
    }
}

// Pose / ExitPose / Emote
void ADMPlayerCharacter::BindPoseAndEmoteInput(UEnhancedInputComponent* EnhancedInputComponent)
{
    if (ensureMsgf(InputConfig->PoseAction != nullptr, TEXT("InputConfig is missing required PoseAction.")))
    {
        EnhancedInputComponent->BindAction(
            InputConfig->PoseAction, ETriggerEvent::Started, this, &ThisClass::InputPoseStarted
        );
    }

    if (ensureMsgf(InputConfig->ExitPoseAction != nullptr, TEXT("InputConfig is missing required ExitPoseAction.")))
    {
        EnhancedInputComponent->BindAction(
            InputConfig->ExitPoseAction, ETriggerEvent::Started, this, &ThisClass::InputExitPoseStarted
        );
    }

    // 감정표현: 누르고 있는 동안 유지, 떼거나 취소되면 종료한다.
    if (InputConfig->EmoteAction != nullptr)
    {
        EnhancedInputComponent->BindAction(
            InputConfig->EmoteAction, ETriggerEvent::Started, this, &ThisClass::InputEmoteStarted
        );

        EnhancedInputComponent->BindAction(
            InputConfig->EmoteAction, ETriggerEvent::Completed, this, &ThisClass::InputEmoteCompleted
        );

        EnhancedInputComponent->BindAction(
            InputConfig->EmoteAction, ETriggerEvent::Canceled, this, &ThisClass::InputEmoteCompleted
        );
    }
    else
    {
        UE_LOG(LogDMPlayerCharacter, Warning, TEXT("EmoteAction binding skipped: EmoteAction is null."));
    }
}

// Skill 사용 / 대상 선택 / 대상 선택 취소
void ADMPlayerCharacter::BindSkillInput(UEnhancedInputComponent* EnhancedInputComponent)
{
    ADMMatchPlayerController* MatchPlayerController = Cast<ADMMatchPlayerController>(GetController());

    if (!IsValid(MatchPlayerController))
    {
        UE_LOG(LogDMPlayerCharacter, Error, TEXT("Skill input binding failed: MatchPlayerController is unavailable."));

        return;
    }

    if (InputConfig->SkillInvisibilityAction != nullptr)
    {
        EnhancedInputComponent->BindAction(
            InputConfig->SkillInvisibilityAction,
            ETriggerEvent::Started,
            MatchPlayerController,
            &ADMMatchPlayerController::InputSkillInvisibilityStarted
        );
    }

    if (InputConfig->SkillFootstepAmplifyAction != nullptr)
    {
        EnhancedInputComponent->BindAction(
            InputConfig->SkillFootstepAmplifyAction,
            ETriggerEvent::Started,
            MatchPlayerController,
            &ADMMatchPlayerController::InputSkillFootstepAmplifyStarted
        );
    }

    if (InputConfig->SkillVisionDistortAction != nullptr)
    {
        EnhancedInputComponent->BindAction(
            InputConfig->SkillVisionDistortAction,
            ETriggerEvent::Started,
            MatchPlayerController,
            &ADMMatchPlayerController::InputSkillVisionDistortStarted
        );
    }

    if (InputConfig->TargetRandomAction != nullptr)
    {
        EnhancedInputComponent->BindAction(
            InputConfig->TargetRandomAction,
            ETriggerEvent::Started,
            MatchPlayerController,
            &ADMMatchPlayerController::InputTargetRandomStarted
        );
    }

    if (InputConfig->TargetLeaderAction != nullptr)
    {
        EnhancedInputComponent->BindAction(
            InputConfig->TargetLeaderAction,
            ETriggerEvent::Started,
            MatchPlayerController,
            &ADMMatchPlayerController::InputTargetLeaderStarted
        );
    }

    if (InputConfig->CancelTargetingAction != nullptr)
    {
        EnhancedInputComponent->BindAction(
            InputConfig->CancelTargetingAction,
            ETriggerEvent::Started,
            MatchPlayerController,
            &ADMMatchPlayerController::InputCancelTargetingStarted
        );
    }
}

#pragma endregion

#pragma region Movement Input

void ADMPlayerCharacter::InputMove(const FInputActionValue& InputActionValue)
{
    FVector2D MovementInput = InputActionValue.Get<FVector2D>();

    if (MovementInput.IsNearlyZero())
    {
        return;
    }

    if (InteractionComponent != nullptr && InteractionComponent->IsInteractionInputLocked())
    {
        InteractionComponent->RequestCancelInteraction();

        ClearSprintRequest();

        return;
    }

    UDMCharacterStateComponent* StateComponent = GetCharacterStateComponent();

    if (StateComponent != nullptr && !StateComponent->CanPerformAction(EDMActionRequest::Move))
    {
        ClearSprintRequest();

        return;
    }

    /*
     * 이동 입력 반전 Event는
     * IA_Move의 2D Vector만 반전한다.
     */
    if (StateComponent != nullptr && StateComponent->HasStatus(DMGameplayTags::Status_InputReversed))
    {
        MovementInput *= -1.0f;
    }

    if (Controller == nullptr)
    {
        return;
    }

    const float MovementYaw = Controller->GetControlRotation().Yaw;

    const FRotator MovementRotation(0.0f, MovementYaw, 0.0f);

    const FVector ForwardDirection = FRotationMatrix(MovementRotation).GetUnitAxis(EAxis::X);

    const FVector RightDirection = FRotationMatrix(MovementRotation).GetUnitAxis(EAxis::Y);

    AddMovementInput(ForwardDirection, MovementInput.Y);

    AddMovementInput(RightDirection, MovementInput.X);
}

void ADMPlayerCharacter::InputJumpStarted()
{
    if (InteractionComponent != nullptr && InteractionComponent->IsInteractionInputLocked())
    {
        return;
    }

    UDMCharacterStateComponent* StateComponent = GetCharacterStateComponent();

    if (StateComponent != nullptr && !StateComponent->CanPerformAction(EDMActionRequest::Jump))
    {
        return;
    }

    Jump();
}

void ADMPlayerCharacter::InputJumpCompleted()
{
    StopJumping();
}

void ADMPlayerCharacter::InputSprintStarted()
{
    if (InteractionComponent != nullptr && InteractionComponent->IsInteractionInputLocked())
    {
        ClearSprintRequest();
        return;
    }

    if (!CanSprint())
    {
        ClearSprintRequest();
        return;
    }

    SetSprintRequested(true);
}

void ADMPlayerCharacter::InputSprintCompleted()
{
    SetSprintRequested(false);
}

bool ADMPlayerCharacter::CanSprint() const
{
    if (InteractionComponent != nullptr && InteractionComponent->IsInteractionInputLocked())
    {
        return false;
    }

    const UDMCharacterStateComponent* StateComponent = GetCharacterStateComponent();

    if (StateComponent != nullptr && !StateComponent->CanPerformAction(EDMActionRequest::Sprint))
    {
        return false;
    }

    if (PoseComponent != nullptr && !PoseComponent->IsActionAllowedByPose(EDMActionRequest::Sprint))
    {
        return false;
    }

    return true;
}

#pragma endregion

#pragma region Quest Event Bridge

void ADMPlayerCharacter::HandleAuthoritativeSprintStateChanged(const bool bIsSprinting)
{
    BroadcastQuestActionEvent(
        bIsSprinting ? DMGameplayTags::QuestEvent_Sprint_Start : DMGameplayTags::QuestEvent_Sprint_End
    );
}

void ADMPlayerCharacter::HandlePoseTypeChanged(const EDMPoseType OldPose, const EDMPoseType NewPose)
{
    /*
     * Changing Flat -> Crouched means:
     *
     * Pose.End(Flat)
     * Pose.Start(Crouched)
     */
    if (OldPose != EDMPoseType::Default)
    {
        BroadcastQuestActionEvent(DMGameplayTags::QuestEvent_Pose_End, OldPose);
    }

    if (NewPose != EDMPoseType::Default)
    {
        BroadcastQuestActionEvent(DMGameplayTags::QuestEvent_Pose_Start, NewPose);
    }
}

void ADMPlayerCharacter::HandleAuthoritativeEmoteStateChanged(const bool bStarted, const FName EmoteId)
{
    BroadcastQuestActionEvent(
        bStarted ? DMGameplayTags::QuestEvent_Emote_Start : DMGameplayTags::QuestEvent_Emote_End,
        EDMPoseType::Default,
        EmoteId
    );
}

void ADMPlayerCharacter::BroadcastQuestActionEvent(
    const FGameplayTag& EventTag, const EDMPoseType PoseType, const FName EmoteId
)
{
    if (!HasAuthority() || !EventTag.IsValid())
    {
        return;
    }

    FDMQuestActionEvent ActionEvent;

    ActionEvent.EventTag = EventTag;

    ActionEvent.PoseType = PoseType;

    ActionEvent.EmoteId = EmoteId;

    QuestActionEventDelegate.Broadcast(ActionEvent);
}

#pragma endregion

#pragma region Quest Failure Presentation

void ADMPlayerCharacter::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    // 주변 플레이어와 본인 모두 들어야 하므로 조건 없이 복제한다.
    DOREPLIFETIME(ADMPlayerCharacter, bQuestSirenActive);
}

void ADMPlayerCharacter::SetQuestSirenActive(const bool bActive)
{
    if (!HasAuthority() || bQuestSirenActive == bActive)
    {
        return;
    }

    bQuestSirenActive = bActive;

    // 시작과 종료가 늦게 반영되지 않도록 복제를 앞당긴다.
    ForceNetUpdate();

    // Listen Server Host에서는 OnRep가 호출되지 않으므로 직접 반영한다.
    RefreshQuestSirenPresentation();
}

void ADMPlayerCharacter::OnRep_QuestSirenActive()
{
    RefreshQuestSirenPresentation();
}

void ADMPlayerCharacter::RefreshQuestSirenPresentation()
{
    // 소리를 들을 사람이 없는 서버 인스턴스에서는 재생하지 않는다.
    if (GetNetMode() == NM_DedicatedServer || !IsValid(QuestSirenAudioComponent))
    {
        return;
    }

    if (!bQuestSirenActive)
    {
        if (QuestSirenAudioComponent->IsPlaying())
        {
            QuestSirenAudioComponent->Stop();
        }

        return;
    }

    if (!IsValid(QuestSirenSound))
    {
        UE_LOG(LogDMPlayerCharacter, Warning, TEXT("[%s] QuestSirenSound is not assigned."), *GetNameSafe(this));

        return;
    }

    // 루프가 아닌 에셋은 한 번만 재생되고 끝나므로 설정 오류를 알린다.
    if (!QuestSirenSound->IsLooping())
    {
        UE_LOG(
            LogDMPlayerCharacter,
            Warning,
            TEXT("[%s] QuestSirenSound is not looping: %s"),
            *GetNameSafe(this),
            *GetNameSafe(QuestSirenSound)
        );
    }

    /*
     * 투명화 상태에서도 사이렌 음량은 줄이지 않는다.
     *
     * 사이렌은 시간 초과 패널티이므로
     * 투명화 중에도 위치가 드러나는 것이 확정 기획이다.
     */

    // 재생 중에 SetSound를 호출하면 처음부터 다시 재생되므로 다를 때만 바꾼다.
    if (QuestSirenAudioComponent->Sound != QuestSirenSound)
    {
        QuestSirenAudioComponent->SetSound(QuestSirenSound);
    }

    if (IsValid(QuestSirenAttenuation))
    {
        QuestSirenAudioComponent->AttenuationSettings = QuestSirenAttenuation;
    }

    // 중복 OnRep나 재호출이 와도 이미 재생 중이면 다시 시작하지 않는다.
    if (!QuestSirenAudioComponent->IsPlaying())
    {
        QuestSirenAudioComponent->Play();
    }
}

#pragma endregion

#pragma region Combat Input

void ADMPlayerCharacter::InputAttackStarted()
{
    if (InteractionComponent != nullptr && InteractionComponent->IsInteractionInputLocked())
    {
        return;
    }

    if (!ensureMsgf(
            CombatComponent != nullptr, TEXT("[%s] Attack input failed: CombatComponent is null."), *GetNameSafe(this)
        ))
    {
        return;
    }

    CombatComponent->RequestAttack();
}

void ADMPlayerCharacter::InputDefenseStarted()
{
    if (InteractionComponent != nullptr && InteractionComponent->IsInteractionInputLocked())
    {
        return;
    }

    if (!ensureMsgf(
            CombatComponent != nullptr, TEXT("[%s] Defense input failed: CombatComponent is null."), *GetNameSafe(this)
        ))
    {
        return;
    }

    CombatComponent->RequestDefense();
}

#pragma endregion

#pragma region Interaction Input

void ADMPlayerCharacter::InputInteractStarted()
{
    if (InteractionComponent == nullptr)
    {
        return;
    }

    if (InteractionComponent->IsInteractionInputLocked())
    {
        return;
    }

    UDMCharacterStateComponent* StateComponent = GetCharacterStateComponent();

    if (StateComponent != nullptr && !StateComponent->CanPerformAction(EDMActionRequest::Interact))
    {
        return;
    }

    InteractionComponent->RequestInteraction();
}

void ADMPlayerCharacter::InputInteractCompleted()
{
    if (InteractionComponent == nullptr)
    {
        return;
    }

    InteractionComponent->RequestCancelInteraction();
}

void ADMPlayerCharacter::InputDropRelicStarted()
{
    /*
     * Chest Interaction Mode에서는
     * IMC_Gameplay 자체가 빠져 있지만
     * 서버/코드 호출까지 포함해 한 번 더 방어한다.
     */
    if (bIsInteractionInputMode)
    {
        return;
    }

    ADMMatchPlayerController* MatchPlayerController = Cast<ADMMatchPlayerController>(GetController());

    if (!IsValid(MatchPlayerController))
    {
        return;
    }

    MatchPlayerController->RequestDropCarriedRelic();
}

void ADMPlayerCharacter::InputConfirmInteractionStarted()
{
    if (!bIsInteractionInputMode || InteractionComponent == nullptr)
    {
        return;
    }

    if (InteractionComponent->IsInteractionInputLocked())
    {
        return;
    }

    ADMMatchPlayerController* MatchPlayerController = Cast<ADMMatchPlayerController>(GetController());

    if (!IsValid(MatchPlayerController))
    {
        return;
    }

    ADMRelicChest* OpenedChest = MatchPlayerController->GetOpenedChest();

    if (!IsValid(OpenedChest))
    {
        return;
    }

    InteractionComponent->RequestChestAction(OpenedChest);
}

void ADMPlayerCharacter::InputConfirmInteractionCompleted()
{
    if (!bIsInteractionInputMode || InteractionComponent == nullptr)
    {
        return;
    }

    InteractionComponent->RequestCancelInteraction();
}

void ADMPlayerCharacter::EnterChestInteractionInputMode()
{
    if (!IsLocallyControlled() || !IsValid(InputConfig))
    {
        return;
    }

    if (!ensureMsgf(
            InputConfig->InteractionMappingContext != nullptr && InputConfig->CloseInteractionAction != nullptr
                && InputConfig->ConfirmInteractionAction != nullptr,
            TEXT(
                "Cannot enter Interaction input mode: InteractionMappingContext, CloseInteractionAction, or ConfirmInteractionAction is not assigned."
            )
        ))
    {
        return;
    }

    if (bIsInteractionInputMode)
    {
        return;
    }

    UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetEnhancedInputSubsystem();

    if (!IsValid(InputSubsystem))
    {
        return;
    }

    FModifyContextOptions ModifyOptions;

    /*
     * Chest를 열기 위해 누른 F가
     * 새 IMC의 Close F로 즉시 이어지는 것을 막는다.
     */
    ModifyOptions.bIgnoreAllPressedKeysUntilRelease = true;

    if (InputConfig->GameplayMappingContext != nullptr)
    {
        InputSubsystem->RemoveMappingContext(InputConfig->GameplayMappingContext, ModifyOptions);
    }

    if (InputConfig->PoseMappingContext != nullptr)
    {
        InputSubsystem->RemoveMappingContext(InputConfig->PoseMappingContext, ModifyOptions);
    }

    if (InputConfig->InteractionMappingContext != nullptr)
    {
        /*
         * 중복 등록 방지.
         */
        InputSubsystem->RemoveMappingContext(InputConfig->InteractionMappingContext, ModifyOptions);

        InputSubsystem->AddMappingContext(InputConfig->InteractionMappingContext, 0, ModifyOptions);
    }

    bIsInteractionInputMode = true;

    UE_LOG(LogDMPlayerCharacter, Log, TEXT("Chest Interaction input mode entered."));
}

void ADMPlayerCharacter::ExitChestInteractionInputMode()
{
    if (!IsLocallyControlled() || !IsValid(InputConfig))
    {
        return;
    }

    if (!bIsInteractionInputMode)
    {
        return;
    }

    UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetEnhancedInputSubsystem();

    if (!IsValid(InputSubsystem))
    {
        return;
    }

    FModifyContextOptions ModifyOptions;

    ModifyOptions.bIgnoreAllPressedKeysUntilRelease = true;

    if (InputConfig->InteractionMappingContext != nullptr)
    {
        InputSubsystem->RemoveMappingContext(InputConfig->InteractionMappingContext, ModifyOptions);
    }

    if (InputConfig->GameplayMappingContext != nullptr)
    {
        InputSubsystem->RemoveMappingContext(InputConfig->GameplayMappingContext, ModifyOptions);

        InputSubsystem->AddMappingContext(InputConfig->GameplayMappingContext, 0, ModifyOptions);
    }

    bIsInteractionInputMode = false;

    UE_LOG(LogDMPlayerCharacter, Log, TEXT("Chest Interaction input mode exited."));
}

void ADMPlayerCharacter::InputCloseInteractionStarted()
{
    if (!bIsInteractionInputMode)
    {
        return;
    }

    if (InteractionComponent != nullptr && InteractionComponent->IsInteractionInputLocked())
    {
        InteractionComponent->RequestCancelInteraction();
    }

    ADMMatchPlayerController* MatchPlayerController = Cast<ADMMatchPlayerController>(GetController());

    if (!IsValid(MatchPlayerController))
    {
        return;
    }

    MatchPlayerController->CloseChestInteractionLocal();

    ExitChestInteractionInputMode();
}

#pragma endregion

#pragma region Camera

void ADMPlayerCharacter::InputViewStarted()
{
    if (!IsLocallyControlled() || CameraBoom == nullptr)
    {
        return;
    }

    bIsFirstPersonView = !bIsFirstPersonView;

    ApplyLocalCameraView();

    ApplyViewRotationPolicy();

    if (!HasAuthority())
    {
        ServerSetFirstPersonView(bIsFirstPersonView);
    }
}

void ADMPlayerCharacter::InputLook(const FInputActionValue& InputActionValue)
{
    // 로컬 캐릭터만 카메라 입력을 처리한다.
    if (!IsLocallyControlled() || !IsValid(Controller))
    {
        return;
    }

    const FVector2D LookInput = InputActionValue.Get<FVector2D>();

    if (LookInput.IsNearlyZero())
    {
        return;
    }

    float Sensitivity = 1.0f;

    if (UGameInstance* GameInstance = GetGameInstance())
    {
        const UDMLocalSettingsSubsystem* LocalSettings = GameInstance->GetSubsystem<UDMLocalSettingsSubsystem>();

        if (IsValid(LocalSettings))
        {
            Sensitivity = LocalSettings->GetMouseSensitivity();
        }
    }

    // 유효하지 않은 감도값이 회전에 반영되지 않도록 제한한다.
    if (!FMath::IsFinite(Sensitivity))
    {
        Sensitivity = 1.0f;
    }

    Sensitivity = FMath::Clamp(Sensitivity, 0.1f, 3.0f);

    // 입력 매핑에서는 Y축을 반전하지 않고,
    // 카메라 회전 방향은 이 함수에서만 결정한다.
    const float YawAmount = LookInput.X * Sensitivity;

    const float PitchAmount = -LookInput.Y * Sensitivity;

    // 문제 재현 시에만 VeryVerbose 로그를 활성화한다.
    UE_LOG(
        LogDMPlayerCharacter,
        VeryVerbose,
        TEXT("MouseLook RawX=%.3f RawY=%.3f Yaw=%.3f Pitch=%.3f"),
        LookInput.X,
        LookInput.Y,
        YawAmount,
        PitchAmount
    );

    AddControllerYawInput(YawAmount);
    AddControllerPitchInput(PitchAmount);
}

void ADMPlayerCharacter::ApplyLocalCameraView()
{
    if (!IsLocallyControlled() || CameraBoom == nullptr)
    {
        return;
    }

    CameraBoom->SetUsingAbsoluteRotation(false);

    CameraBoom->bUsePawnControlRotation = true;

    if (bIsFirstPersonView)
    {
        CameraBoom->TargetArmLength = 0.0f;

        CameraBoom->TargetOffset = FVector(0.0f, 0.0f, BaseEyeHeight);

        CameraBoom->SocketOffset = FVector::ZeroVector;

        if (GetMesh() != nullptr)
        {
            GetMesh()->SetOwnerNoSee(true);
        }

        return;
    }

    CameraBoom->TargetArmLength = ThirdPersonArmLength;

    CameraBoom->TargetOffset = ThirdPersonTargetOffset;

    CameraBoom->SocketOffset = ThirdPersonSocketOffset;

    if (GetMesh() != nullptr)
    {
        GetMesh()->SetOwnerNoSee(false);
    }
}

void ADMPlayerCharacter::ApplyViewRotationPolicy()
{
    UCharacterMovementComponent* MovementComponent = GetCharacterMovement();

    if (MovementComponent == nullptr)
    {
        return;
    }

    /*
     * Pose Mode의 몸 방향 고정은
     * 1인칭/3인칭 View 회전 정책보다 우선한다.
     */
    bool bPoseLocksBody = false;

    if (IsLocallyControlled())
    {
        /*
         * Owning Client는 Replication 왕복을 기다리지 않고
         * 로컬 Input Mode를 이용해 즉시 회전을 제한한다.
         */
        bPoseLocksBody = bIsPoseInputMode;
    }
    else if (PoseComponent != nullptr)
    {
        bPoseLocksBody = PoseComponent->IsPoseLocked();
    }

    if (bPoseLocksBody)
    {
        bUseControllerRotationYaw = false;

        MovementComponent->bOrientRotationToMovement = false;

        MovementComponent->bUseControllerDesiredRotation = false;

        return;
    }

    if (bIsFirstPersonView)
    {
        bUseControllerRotationYaw = true;

        MovementComponent->bOrientRotationToMovement = false;

        MovementComponent->bUseControllerDesiredRotation = false;

        return;
    }

    bUseControllerRotationYaw = false;

    MovementComponent->bOrientRotationToMovement = true;

    MovementComponent->bUseControllerDesiredRotation = false;
}

void ADMPlayerCharacter::ServerSetFirstPersonView_Implementation(const bool bEnableFirstPerson)
{
    bIsFirstPersonView = bEnableFirstPerson;

    // Server에는 Camera 표현이 아니라 Character 회전 정책만 필요하다.
    ApplyViewRotationPolicy();
}

void ADMPlayerCharacter::RefreshRotationPolicy()
{
    /*
     * Simulated Proxy의 Character Rotation은
     * Server에서 복제된 Transform을 표현하므로 직접 정책을 변경하지 않는다.
     */
    if (!HasAuthority() && !IsLocallyControlled())
    {
        return;
    }

    ApplyViewRotationPolicy();
}

#pragma endregion

#pragma region Pose

void ADMPlayerCharacter::InputPoseStarted()
{
    if (InteractionComponent != nullptr && InteractionComponent->IsInteractionInputLocked())
    {
        return;
    }

    const UDMCharacterStateComponent* StateComponent = GetCharacterStateComponent();

    if (!IsValid(StateComponent) || !StateComponent->CanPerformAction(EDMActionRequest::PoseChange))
    {
        /*
         * Server가 거부할 Pose 요청이라면
         * Local Mapping Context도 Pose Mode로
         * 변경하지 않는다.
         */
        return;
    }

    if (PoseComponent == nullptr)
    {
        return;
    }

    if (!bIsPoseInputMode)
    {
        if (!SetPoseInputMode(true))
        {
            UE_LOG(LogDMPlayerCharacter, Error, TEXT("[%s] Failed to enter Pose Input Mode."), *GetNameSafe(this));

            return;
        }

        /*
         * Pose Mode에서는 Sprint가 허용되지 않으므로
         * owning client의 Sprint 요청을 즉시 해제한다.
         */
        ClearSprintRequest();

        PoseComponent->RequestSetPoseModeActive(true);

        return;
    }

    // Pose Mode에서는 같은 IA_Pose가 LMB 자세 순환 입력으로 사용된다.
    PoseComponent->RequestCyclePose();
}

void ADMPlayerCharacter::InputExitPoseStarted()
{
    if (InteractionComponent != nullptr && InteractionComponent->IsInteractionInputLocked())
    {
        return;
    }
    if (!bIsPoseInputMode || PoseComponent == nullptr)
    {
        return;
    }

    if (!SetPoseInputMode(false))
    {
        return;
    }

    PoseComponent->RequestSetPoseModeActive(false);
}

bool ADMPlayerCharacter::SetPoseInputMode(const bool bEnablePoseMode)
{
    if (!IsLocallyControlled())
    {
        UE_LOG(LogDMPlayerCharacter, Error, TEXT("[%s] PoseMode failed: not locally controlled."), *GetNameSafe(this));

        return false;
    }

    if (!ensureMsgf(InputConfig != nullptr, TEXT("[%s] PoseMode failed: InputConfig is null."), *GetNameSafe(this)))
    {
        return false;
    }

    if (!ensureMsgf(
            InputConfig->GameplayMappingContext != nullptr,
            TEXT("[%s] PoseMode failed: GameplayMappingContext is null."),
            *GetNameSafe(this)
        ))
    {
        return false;
    }

    if (!ensureMsgf(
            InputConfig->PoseMappingContext != nullptr,
            TEXT("[%s] PoseMode failed: PoseMappingContext is null."),
            *GetNameSafe(this)
        ))
    {
        return false;
    }

    UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetEnhancedInputSubsystem();

    if (InputSubsystem == nullptr)
    {
        UE_LOG(
            LogDMPlayerCharacter,
            Error,
            TEXT("[%s] Pose input mode change failed: EnhancedInput subsystem is unavailable."),
            *GetNameSafe(this)
        );

        return false;
    }

    FModifyContextOptions ModifyOptions;
    ModifyOptions.bIgnoreAllPressedKeysUntilRelease = true;

    InputSubsystem->RemoveMappingContext(InputConfig->GameplayMappingContext, ModifyOptions);

    InputSubsystem->RemoveMappingContext(InputConfig->PoseMappingContext, ModifyOptions);

    if (bEnablePoseMode)
    {
        InputSubsystem->AddMappingContext(InputConfig->PoseMappingContext, 0, ModifyOptions);
    }
    else
    {
        InputSubsystem->AddMappingContext(InputConfig->GameplayMappingContext, 0, ModifyOptions);
    }

    bIsPoseInputMode = bEnablePoseMode;

    ApplyViewRotationPolicy();

    // 입력 Context 전환이 완료된 뒤 로컬 HUD에 알린다.
    LocalPoseInputModeChangedDelegate.Broadcast(bIsPoseInputMode);

    return true;
}

#pragma endregion

#pragma region Emote

void ADMPlayerCharacter::InputEmoteStarted()
{
    // 상호작용 입력 잠금 중에는 감정표현을 시작하지 않는다.
    if (InteractionComponent != nullptr && InteractionComponent->IsInteractionInputLocked())
    {
        return;
    }

    // 최종 허용 여부(사망·자세·행동 상태)는 서버의 CanRequestEmote()가 판정한다.
    bIsEmoteInputHeld = true;

    RequestBeginEmote(DefaultEmoteId);
}

void ADMPlayerCharacter::InputEmoteCompleted()
{
    // 시작 입력이 없었다면 종료 요청을 보내지 않는다.
    if (!bIsEmoteInputHeld)
    {
        return;
    }

    bIsEmoteInputHeld = false;

    RequestEndEmote();
}

#pragma endregion

#pragma region Skill Targeting Input Mode

bool ADMPlayerCharacter::EnterSkillTargetingInputMode()
{
    if (!IsLocallyControlled() || !IsValid(InputConfig))
    {
        return false;
    }

    /*
     * Skill Targeting은 Gameplay Mode에서만 진입한다.
     * Interaction / Pose와 중첩시키지 않는다.
     */
    if (bIsInteractionInputMode || bIsPoseInputMode)
    {
        return false;
    }

    if (!ensureMsgf(
            InputConfig->GameplayMappingContext != nullptr && InputConfig->SkillTargetingMappingContext != nullptr
                && InputConfig->TargetRandomAction != nullptr && InputConfig->TargetLeaderAction != nullptr
                && InputConfig->CancelTargetingAction != nullptr,
            TEXT("Cannot enter Skill Targeting mode: required Input Asset is not assigned.")
        ))
    {
        return false;
    }

    if (bIsSkillTargetingInputMode)
    {
        return true;
    }

    UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetEnhancedInputSubsystem();

    if (InputSubsystem == nullptr)
    {
        return false;
    }

    /*
     * Gameplay Context 제거 시 Sprint Completed가
     * 호출되지 않을 수 있으므로 요청을 먼저 정리한다.
     */
    ClearSprintRequest();

    FModifyContextOptions ModifyOptions;

    ModifyOptions.bIgnoreAllPressedKeysUntilRelease = true;

    InputSubsystem->RemoveMappingContext(InputConfig->GameplayMappingContext, ModifyOptions);

    if (InputConfig->PoseMappingContext != nullptr)
    {
        InputSubsystem->RemoveMappingContext(InputConfig->PoseMappingContext, ModifyOptions);
    }

    if (InputConfig->InteractionMappingContext != nullptr)
    {
        InputSubsystem->RemoveMappingContext(InputConfig->InteractionMappingContext, ModifyOptions);
    }

    /*
     * 중복 등록 방지.
     */
    InputSubsystem->RemoveMappingContext(InputConfig->SkillTargetingMappingContext, ModifyOptions);

    InputSubsystem->AddMappingContext(InputConfig->SkillTargetingMappingContext, 0, ModifyOptions);

    bIsSkillTargetingInputMode = true;

    UE_LOG(LogDMPlayerCharacter, Log, TEXT("Skill Targeting input mode entered."));

    return true;
}

void ADMPlayerCharacter::ExitSkillTargetingInputMode()
{
    if (!IsLocallyControlled() || !IsValid(InputConfig))
    {
        return;
    }

    if (!bIsSkillTargetingInputMode)
    {
        return;
    }

    UEnhancedInputLocalPlayerSubsystem* InputSubsystem = GetEnhancedInputSubsystem();

    if (InputSubsystem == nullptr)
    {
        return;
    }

    FModifyContextOptions ModifyOptions;

    ModifyOptions.bIgnoreAllPressedKeysUntilRelease = true;

    if (InputConfig->SkillTargetingMappingContext != nullptr)
    {
        InputSubsystem->RemoveMappingContext(InputConfig->SkillTargetingMappingContext, ModifyOptions);
    }

    if (InputConfig->GameplayMappingContext != nullptr)
    {
        /*
         * 중복 방지 후 다시 Gameplay Mode 복원.
         */
        InputSubsystem->RemoveMappingContext(InputConfig->GameplayMappingContext, ModifyOptions);

        InputSubsystem->AddMappingContext(InputConfig->GameplayMappingContext, 0, ModifyOptions);
    }

    bIsSkillTargetingInputMode = false;

    UE_LOG(LogDMPlayerCharacter, Log, TEXT("Skill Targeting input mode exited."));
}

#pragma endregion

#pragma region Internal Helpers

UEnhancedInputLocalPlayerSubsystem* ADMPlayerCharacter::GetEnhancedInputSubsystem() const
{
    return DMGameplay::GetLocalInputSubsystem(Cast<APlayerController>(GetController()));
}

#pragma endregion

#pragma region Defense Animation

bool ADMPlayerCharacter::CanPlayDefenseMontage() const
{
    if (DefenseMontage == nullptr)
    {
        return false;
    }

    const USkeletalMeshComponent* CharacterMesh = GetMesh();

    if (CharacterMesh == nullptr || CharacterMesh->GetAnimInstance() == nullptr)
    {
        return false;
    }

    return DefenseMontage->GetPlayLength() > KINDA_SMALL_NUMBER;
}

float ADMPlayerCharacter::PlayDefenseMontage(const float DesiredDuration)
{
    if (!CanPlayDefenseMontage() || DesiredDuration <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }

    const float MontageLength = DefenseMontage->GetPlayLength();

    if (MontageLength <= KINDA_SMALL_NUMBER)
    {
        return 0.0f;
    }

    /*
     * Montage 원본 길이에 관계없이
     * Gameplay DefenseDuration과
     * 재생 길이를 맞춘다.
     *
     * 예:
     * Montage 1.2초 / Defense 1.0초
     * → PlayRate 1.2
     */
    const float PlayRate = MontageLength / DesiredDuration;

    return PlayAnimMontage(DefenseMontage, PlayRate);
}

void ADMPlayerCharacter::StopDefenseMontage()
{
    if (DefenseMontage == nullptr)
    {
        return;
    }

    StopAnimMontage(DefenseMontage);
}

#pragma endregion
