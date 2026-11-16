#include "Experiments/DMAnonymousPoCPlayerPawn.h"

#include "Camera/CameraTypes.h"
#include "Components/InputComponent.h"
#include "Engine/LocalPlayer.h"
#include "EnhancedInputComponent.h"
#include "EnhancedInputSubsystems.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "InputAction.h"
#include "InputActionValue.h"
#include "InputCoreTypes.h"
#include "InputMappingContext.h"
#include "InputModifiers.h"
#include "Math/RotationMatrix.h"

ADMAnonymousPoCPlayerPawn::ADMAnonymousPoCPlayerPawn(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    bAlwaysRelevant = false;
    bOnlyRelevantToOwner = true;
    bNetUseOwnerRelevancy = false;
    SetReplicates(true);
    SetReplicateMovement(true);
}

bool ADMAnonymousPoCPlayerPawn::IsNetRelevantFor(
    const AActor* RealViewer, const AActor* /*ViewTarget*/, const FVector& /*SrcLocation*/
) const
{
    // 거리·ViewTarget이 소유 제한을 우회하지 않게 한다. 엔진 참조 역경로는 별도 검증한다.
    return IsValid(RealViewer) && RealViewer == GetController();
}

void ADMAnonymousPoCPlayerPawn::PawnClientRestart()
{
    // 엔진의 재시작·이동 예측은 유지하되 본 게임 입력 에셋·HUD·관전 연결은 실행하지 않는다.
    ACharacter::PawnClientRestart();
    const APlayerController* Player = Cast<APlayerController>(GetController());
    ULocalPlayer* LocalPlayer = IsValid(Player) ? Player->GetLocalPlayer() : nullptr;
    if (IsLocallyControlled() && IsValid(LocalPlayer) && IsValid(LocalMapping))
    {
        UEnhancedInputLocalPlayerSubsystem* Input = LocalPlayer->GetSubsystem<UEnhancedInputLocalPlayerSubsystem>();
        if (IsValid(Input))
        {
            if (BoundInputSubsystem.IsValid())
            {
                BoundInputSubsystem->RemoveMappingContext(LocalMapping);
            }
            BoundInputSubsystem = Input;
            Input->AddMappingContext(LocalMapping, 0);
        }
    }
}

void ADMAnonymousPoCPlayerPawn::SetupPlayerInputComponent(UInputComponent* PlayerInputComponent)
{
    ACharacter::SetupPlayerInputComponent(PlayerInputComponent);
    UEnhancedInputComponent* Enhanced = Cast<UEnhancedInputComponent>(PlayerInputComponent);
    if (!ensureMsgf(IsValid(Enhanced), TEXT("PoC requires an EnhancedInputComponent.")))
    {
        return;
    }
    if (!IsValid(LocalMapping))
    {
        LocalMapping = NewObject<UInputMappingContext>(this);
        ForwardAction = NewObject<UInputAction>(this);
        RightAction = NewObject<UInputAction>(this);
        YawAction = NewObject<UInputAction>(this);
        PitchAction = NewObject<UInputAction>(this);
        ForwardAction->ValueType = EInputActionValueType::Axis1D;
        RightAction->ValueType = EInputActionValueType::Axis1D;
        YawAction->ValueType = EInputActionValueType::Axis1D;
        PitchAction->ValueType = EInputActionValueType::Axis1D;
        LocalMapping->MapKey(ForwardAction, EKeys::W);
        LocalMapping->MapKey(ForwardAction, EKeys::S).Modifiers.Add(NewObject<UInputModifierNegate>(LocalMapping));
        LocalMapping->MapKey(RightAction, EKeys::D);
        LocalMapping->MapKey(RightAction, EKeys::A).Modifiers.Add(NewObject<UInputModifierNegate>(LocalMapping));
        LocalMapping->MapKey(YawAction, EKeys::MouseX);
        LocalMapping->MapKey(PitchAction, EKeys::MouseY);
    }
    Enhanced->BindAction(ForwardAction, ETriggerEvent::Triggered, this, &ThisClass::InputForward);
    Enhanced->BindAction(RightAction, ETriggerEvent::Triggered, this, &ThisClass::InputRight);
    Enhanced->BindAction(YawAction, ETriggerEvent::Triggered, this, &ThisClass::InputYaw);
    Enhanced->BindAction(PitchAction, ETriggerEvent::Triggered, this, &ThisClass::InputPitch);
    // 점프·Sprint는 눌림/해제만 전달하며 기존 이동 Component를 사용한다.
    PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Pressed, this, &ThisClass::BeginSprint);
    PlayerInputComponent->BindKey(EKeys::LeftShift, IE_Released, this, &ThisClass::EndSprint);
    PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Pressed, this, &ThisClass::BeginJump);
    PlayerInputComponent->BindKey(EKeys::SpaceBar, IE_Released, this, &ThisClass::EndJump);
}

void ADMAnonymousPoCPlayerPawn::CalcCamera(float /*DeltaTime*/, FMinimalViewInfo& OutResult)
{
    OutResult.Location = GetPawnViewLocation();
    OutResult.Rotation = GetViewRotation();
    OutResult.FOV = 90.0f;
}

void ADMAnonymousPoCPlayerPawn::MoveForward(const float Value)
{
    if (IsLocallyControlled() && IsValid(Controller))
    {
        AddMovementInput(FRotationMatrix(FRotator(0.0f, Controller->GetControlRotation().Yaw, 0.0f))
            .GetUnitAxis(EAxis::X), Value);
    }
}

void ADMAnonymousPoCPlayerPawn::MoveRight(const float Value)
{
    if (IsLocallyControlled() && IsValid(Controller))
    {
        AddMovementInput(FRotationMatrix(FRotator(0.0f, Controller->GetControlRotation().Yaw, 0.0f))
            .GetUnitAxis(EAxis::Y), Value);
    }
}

void ADMAnonymousPoCPlayerPawn::InputForward(const FInputActionValue& Value) { MoveForward(Value.Get<float>()); }
void ADMAnonymousPoCPlayerPawn::InputRight(const FInputActionValue& Value) { MoveRight(Value.Get<float>()); }
void ADMAnonymousPoCPlayerPawn::InputYaw(const FInputActionValue& Value) { LookYaw(Value.Get<float>()); }
void ADMAnonymousPoCPlayerPawn::InputPitch(const FInputActionValue& Value) { LookPitch(Value.Get<float>()); }
void ADMAnonymousPoCPlayerPawn::LookYaw(const float Value) { AddControllerYawInput(Value); }
void ADMAnonymousPoCPlayerPawn::LookPitch(const float Value) { AddControllerPitchInput(-Value); }
void ADMAnonymousPoCPlayerPawn::BeginSprint() { SetSprintRequested(true); }
void ADMAnonymousPoCPlayerPawn::EndSprint() { ClearSprintRequest(); }
void ADMAnonymousPoCPlayerPawn::BeginJump() { Jump(); }
void ADMAnonymousPoCPlayerPawn::EndJump() { StopJumping(); }

void ADMAnonymousPoCPlayerPawn::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (BoundInputSubsystem.IsValid() && IsValid(LocalMapping))
    {
        BoundInputSubsystem->RemoveMappingContext(LocalMapping);
    }
    BoundInputSubsystem.Reset();
    ClearSprintRequest();
    Super::EndPlay(EndPlayReason);
}
