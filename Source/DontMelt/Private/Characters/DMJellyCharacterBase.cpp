#include "Characters/DMJellyCharacterBase.h"

#include "Characters/Components/DMCharacterMovementComponent.h"
#include "Characters/Components/DMCharacterStateComponent.h"
#include "Characters/Components/DMJellyPhysicsComponent.h"
#include "Characters/Components/DMCharacterAudioComponent.h"
#include "Characters/Components/DMCharacterVFXComponent.h"
#include "Components/SkeletalMeshComponent.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimInstance.h"

#include "GameFramework/CharacterMovementComponent.h"

#pragma region Lifecycle

ADMJellyCharacterBase::ADMJellyCharacterBase(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer.SetDefaultSubobjectClass<UDMCharacterMovementComponent>(
          ACharacter::CharacterMovementComponentName
      ))
{
    CharacterStateComponent = CreateDefaultSubobject<UDMCharacterStateComponent>(TEXT("CharacterStateComponent"));

    CharacterAudioComponent = CreateDefaultSubobject<UDMCharacterAudioComponent>(TEXT("CharacterAudioComponent"));

    JellyPhysicsComponent = CreateDefaultSubobject<UDMJellyPhysicsComponent>(TEXT("JellyPhysicsComponent"));

    CharacterVFXComponent = CreateDefaultSubobject<UDMCharacterVFXComponent>(TEXT("CharacterVFXComponent"));

    bUseControllerRotationPitch = false;
    bUseControllerRotationYaw = false;
    bUseControllerRotationRoll = false;

    UCharacterMovementComponent* MovementComponent = GetCharacterMovement();

    if (MovementComponent == nullptr)
    {
        return;
    }

    MovementComponent->bOrientRotationToMovement = true;

    MovementComponent->bUseControllerDesiredRotation = false;
}

void ADMJellyCharacterBase::PostInitializeComponents()
{
    Super::PostInitializeComponents();

    UCharacterMovementComponent* MovementComponent = GetCharacterMovement();

    if (MovementComponent == nullptr)
    {
        return;
    }

    /*
     * Blueprint Default까지 적용된 WalkSpeed를
     * CharacterMovement의 기본 최대 속도에 동기화한다.
     */
    MovementComponent->MaxWalkSpeed = WalkSpeed;
}

#pragma endregion

#pragma region Component Access

UDMJellyPhysicsComponent* ADMJellyCharacterBase::GetJellyPhysicsComponent() const
{
    /*
     * 정상적인 Native Default Subobject 포인터라면
     * 그대로 반환한다.
     *
     * Blueprint가 오래된 Native Component 참조를
     * 보유하고 있는 경우 CDO/Archetype Component를
     * 가리키거나 nullptr일 수 있으므로 Owner까지 검증한다.
     */
    if (IsValid(JellyPhysicsComponent) && JellyPhysicsComponent->GetOwner() == this)
    {
        return JellyPhysicsComponent.Get();
    }

    /*
     * 현재 Runtime Character Instance에 실제로
     * 부착되어 있는 Component를 다시 찾는다.
     */
    return FindComponentByClass<UDMJellyPhysicsComponent>();
}

#pragma endregion

#pragma region Movement API

void ADMJellyCharacterBase::ClearSprintRequest()
{
    SetSprintRequested(false);
}

float ADMJellyCharacterBase::GetWalkSpeed() const
{
    return WalkSpeed;
}

float ADMJellyCharacterBase::GetSprintSpeed() const
{
    return SprintSpeed;
}

bool ADMJellyCharacterBase::CanSprint() const
{
    return true;
}

#pragma endregion

#pragma region Emote API

void ADMJellyCharacterBase::RequestEmote(const FName EmoteId)
{
    if (CharacterStateComponent == nullptr)
    {
        return;
    }

    CharacterStateComponent->RequestEmote(EmoteId);
}

void ADMJellyCharacterBase::RequestBeginEmote(const FName EmoteId)
{
    // 실제 검증과 ActionState 전환은 StateComponent의 Server RPC가 담당한다.
    if (CharacterStateComponent == nullptr)
    {
        return;
    }

    CharacterStateComponent->RequestBeginEmote(EmoteId);
}

void ADMJellyCharacterBase::RequestEndEmote()
{
    if (CharacterStateComponent == nullptr)
    {
        return;
    }

    CharacterStateComponent->RequestEndEmote();
}

#pragma endregion

#pragma region Movement Internal

void ADMJellyCharacterBase::SetSprintRequested(const bool bEnableSprint)
{
    UDMCharacterMovementComponent* MovementComponent = Cast<UDMCharacterMovementComponent>(GetCharacterMovement());

    if (MovementComponent == nullptr)
    {
        return;
    }

    MovementComponent->SetSprintRequested(bEnableSprint);
}

#pragma endregion

#pragma region Combat Animation

bool ADMJellyCharacterBase::CanPlayAttackMontage() const
{
    if (AttackMontage == nullptr)
    {
        return false;
    }

    const USkeletalMeshComponent* MeshComponent = GetMesh();

    if (MeshComponent == nullptr)
    {
        return false;
    }

    return MeshComponent->GetAnimInstance() != nullptr;
}

float ADMJellyCharacterBase::PlayAttackMontage()
{
    if (!CanPlayAttackMontage())
    {
        return 0.0f;
    }

    return PlayAnimMontage(AttackMontage, 1.0f);
}

void ADMJellyCharacterBase::StopAttackMontage()
{
    if (AttackMontage == nullptr)
    {
        return;
    }

    StopAnimMontage(AttackMontage);
}

#pragma endregion
