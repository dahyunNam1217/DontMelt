#include "Characters/Components/DMCharacterAudioComponent.h"

#include "Characters/DMJellyCharacterBase.h"
#include "Characters/DMNPCCharacter.h"
#include "Characters/DMPlayerCharacter.h"
#include "Characters/Components/DMCharacterStateComponent.h"
#include "Characters/Components/DMJellyPhysicsComponent.h"

#include "Engine/World.h"
#include "GameFramework/CharacterMovementComponent.h"
#include "Kismet/GameplayStatics.h"
#include "Sound/SoundAttenuation.h"
#include "Sound/SoundBase.h"

UDMCharacterAudioComponent::UDMCharacterAudioComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    /*
     * 이 Component는 로컬 Audio Presentation 전용이다.
     *
     * Footstep Event / Volume 값을
     * Network Replication하지 않는다.
     */
    SetIsReplicatedByDefault(false);
}

#pragma region Lifecycle

void UDMCharacterAudioComponent::BeginPlay()
{
    Super::BeginPlay();

    ADMJellyCharacterBase* OwnerCharacter = Cast<ADMJellyCharacterBase>(GetOwner());

    if (!IsValid(OwnerCharacter))
    {
        return;
    }

    // 기존 Emote Multicast에 연결한다. 새로운 RPC는 생성하지 않는다.
    if (UDMCharacterStateComponent* StateComponent = OwnerCharacter->GetCharacterStateComponent())
    {
        StateComponent->OnEmotePresentationRequested.AddUniqueDynamic(
            this, &ThisClass::HandleEmotePresentationRequested
        );
    }

    // 실제 사망 래그돌의 기존 로컬 이벤트만 사용한다.
    if (UDMJellyPhysicsComponent* PhysicsComponent = OwnerCharacter->GetJellyPhysicsComponent())
    {
        PhysicsComponent->OnDeathPresentationStarted().AddUObject(
            this, &ThisClass::HandleDeathPresentationStarted
        );
    }
}

void UDMCharacterAudioComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (ADMJellyCharacterBase* OwnerCharacter = Cast<ADMJellyCharacterBase>(GetOwner()))
    {
        if (UDMCharacterStateComponent* StateComponent = OwnerCharacter->GetCharacterStateComponent())
        {
            StateComponent->OnEmotePresentationRequested.RemoveDynamic(
                this, &ThisClass::HandleEmotePresentationRequested
            );
        }

        if (UDMJellyPhysicsComponent* PhysicsComponent = OwnerCharacter->GetJellyPhysicsComponent())
        {
            PhysicsComponent->OnDeathPresentationStarted().RemoveAll(this);
        }
    }

    Super::EndPlay(EndPlayReason);
}

#pragma endregion

bool UDMCharacterAudioComponent::CanPlayFootstepLocally() const
{
    if (!IsValid(FootstepSound.Get()))
    {
        return false;
    }

    const UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return false;
    }

    /*
     * Dedicated Server에서는 Audio Presentation을
     * 실행하지 않는다.
     */
    if (World->GetNetMode() == NM_DedicatedServer)
    {
        return false;
    }

    // NPC와 Player가 동일 AnimBP를 사용해도, 발소리는 실제 플레이어만 재생한다.
    // 기존 3D 거리 감쇠와 플레이어 대상 발소리 증폭 스킬은 유지한다.
    if (!IsValid(Cast<ADMPlayerCharacter>(GetOwner())))
    {
        return false;
    }

    const ADMJellyCharacterBase* OwnerCharacter = Cast<ADMJellyCharacterBase>(GetOwner());

    if (!IsValid(OwnerCharacter))
    {
        return false;
    }

    const UCharacterMovementComponent* MovementComponent = OwnerCharacter->GetCharacterMovement();

    /*
     * 공중에서 Blend 때문에 Footstep Notify가
     * 뒤늦게 들어오는 경우를 방지한다.
     */
    if (MovementComponent != nullptr && MovementComponent->IsFalling())
    {
        return false;
    }

    FVector HorizontalVelocity = OwnerCharacter->GetVelocity();

    HorizontalVelocity.Z = 0.0f;

    /*
     * Idle 전환 Blend 구간에서 남은 Notify 때문에
     * 정지한 Character가 발소리를 내는 것을 방지한다.
     */
    if (HorizontalVelocity.IsNearlyZero(1.0f))
    {
        return false;
    }

    return true;
}

float UDMCharacterAudioComponent::GetResolvedFootstepVolumeMultiplier() const
{
    float Result = FMath::Max(0.0f, BaseFootstepVolumeMultiplier);

    /*
     * Source 자신의 상태.
     */
    if (bInvisibilityPresentationActive)
    {
        Result *= FMath::Max(0.0f, InvisibilityFootstepVolumeMultiplier);
    }

    /*
     * 현재 Local Listener에게만 적용되는
     * caster-specific FootstepAmplify.
     *
     * 이 bool은 Network Replication되지 않는다.
     */
    if (bFootstepAmplifyListenerPresentationActive)
    {
        Result *= FMath::Max(0.0f, FootstepAmplifyListenerVolumeMultiplier);
    }

    return Result;
}

void UDMCharacterAudioComponent::HandleFootstepNotify()
{
    if (!CanPlayFootstepLocally())
    {
        return;
    }

    const AActor* OwnerActor = GetOwner();

    if (!IsValid(OwnerActor))
    {
        return;
    }

    const float VolumeMultiplier = GetResolvedFootstepVolumeMultiplier();

    if (VolumeMultiplier <= 0.0f)
    {
        return;
    }

    /*
     * Fire-and-forget Local Audio.
     *
     * 이 호출은 Network Replication을 하지 않는다.
     * 각 Client가 자기 World의 Animation Notify를 통해
     * 독립적으로 발소리를 재생한다.
     */
    UGameplayStatics::PlaySoundAtLocation(
        this,
        FootstepSound.Get(),
        OwnerActor->GetActorLocation(),
        OwnerActor->GetActorRotation(),
        VolumeMultiplier,
        1.0f,
        0.0f,
        FootstepAttenuation.Get()
    );
}

#pragma region Combat Audio

void UDMCharacterAudioComponent::PlayAttackSound()
{
    PlayCombatSound(AttackSound.Get());
}

void UDMCharacterAudioComponent::PlayDefenseSound()
{
    PlayCombatSound(DefenseSound.Get());
}

void UDMCharacterAudioComponent::PlayCombatSound(USoundBase* Sound) const
{
    if (!IsValid(Sound))
    {
        return;
    }

    const UWorld* World = GetWorld();

    // 전용 서버에서는 오디오를 재생하지 않는다.
    if (World == nullptr || World->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    const ADMJellyCharacterBase* OwnerCharacter = Cast<ADMJellyCharacterBase>(GetOwner());

    if (!IsValid(OwnerCharacter))
    {
        return;
    }

    // 몽타주가 동기화된 각 실행 환경에서만 3D 사운드를 재생한다.
    // 이 함수는 네트워크 요청을 보내거나 공격·방어를 판정하지 않는다.
    UGameplayStatics::PlaySoundAtLocation(
        this,
        Sound,
        OwnerCharacter->GetActorLocation(),
        OwnerCharacter->GetActorRotation(),
        1.0f,
        1.0f,
        0.0f,
        CombatAttenuation.Get()
    );
}

#pragma endregion

#pragma region Reaction Audio

void UDMCharacterAudioComponent::HandleEmotePresentationRequested(const FName EmoteId)
{
    // 플레이어 Emote까지 비명 소리가 확장되지 않도록 NPC만 처리한다.
    if (Cast<ADMNPCCharacter>(GetOwner()) == nullptr)
    {
        return;
    }

    if (EmoteId == FName(TEXT("Scream")))
    {
        PlayReactionSound(NPCScreamSound.Get());
    }
    else if (EmoteId == FName(TEXT("Laugh")))
    {
        PlayReactionSound(NPCMockLaughSound.Get());
    }
}

void UDMCharacterAudioComponent::PlayStunSound()
{
    // 스턴 시작 여부는 상태 컴포넌트가 판정해 이 함수를 호출한다.
    PlayReactionSound(StunSound.Get());
}

void UDMCharacterAudioComponent::HandleDeathPresentationStarted()
{
    // NPC 래그돌과는 분리하고 실제 플레이어 사망만 재생한다.
    if (Cast<ADMPlayerCharacter>(GetOwner()) != nullptr)
    {
        PlayReactionSound(DeathSound.Get());
    }
}

void UDMCharacterAudioComponent::PlayReactionSound(USoundBase* Sound) const
{
    if (!IsValid(Sound))
    {
        return;
    }

    const UWorld* World = GetWorld();

    if (World == nullptr || World->GetNetMode() == NM_DedicatedServer)
    {
        return;
    }

    const ADMJellyCharacterBase* OwnerCharacter = Cast<ADMJellyCharacterBase>(GetOwner());

    if (!IsValid(OwnerCharacter))
    {
        return;
    }

    // 별도 감쇠를 연결하지 않았다면 이미 쓰는 전투 거리 감쇠를 사용한다.
   USoundAttenuation* Attenuation = IsValid(ReactionAttenuation.Get())
        ? ReactionAttenuation.Get()
        : CombatAttenuation.Get();

    // 사운드 자체는 로컬에서만 재생한다. 네트워크 판정이나 RPC는 없다.
    UGameplayStatics::PlaySoundAtLocation(
        this,
        Sound,
        OwnerCharacter->GetActorLocation(),
        OwnerCharacter->GetActorRotation(),
        1.0f,
        1.0f,
        0.0f,
        Attenuation
    );
}

#pragma endregion
