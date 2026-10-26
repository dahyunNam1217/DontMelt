
#include "Characters/Components/DMCharacterVFXComponent.h"

#include "Characters/DMJellyCharacterBase.h"

#include "Components/SkeletalMeshComponent.h"
#include "Engine/World.h"
#include "NiagaraComponent.h"
#include "NiagaraFunctionLibrary.h"
#include "NiagaraSystem.h"

namespace DMCharacterVFX
{
    // Skeleton에 생성할 Socket 이름과 일치해야 한다.
    const FName AttackSocket(TEXT("FX_Attack_Front"));
    const FName NPCAlertSocket(TEXT("FX_Alert_Right"));
    const FName StunSocket(TEXT("FX_Stun_Head"));
    const FName DefenseSocket(TEXT("FX_Defense_Back"));
}

#pragma region Lifecycle

UDMCharacterVFXComponent::UDMCharacterVFXComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    // Niagara는 각 실행 환경에서 로컬로 생성한다.
    SetIsReplicatedByDefault(false);
}

void UDMCharacterVFXComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    // 캐릭터가 제거될 때 남아 있는 지속형 이펙트를 종료한다.
    StopDefenseEffect();
    SetStunEffectActive(false);

    Super::EndPlay(EndPlayReason);
}

#pragma endregion

#pragma region VFX Internal

bool UDMCharacterVFXComponent::CanPresentLocally() const
{
    const UWorld* World = GetWorld();

    return World != nullptr && World->GetNetMode() != NM_DedicatedServer;
}

USkeletalMeshComponent* UDMCharacterVFXComponent::GetOwnerMesh() const
{
    ADMJellyCharacterBase* OwnerCharacter = Cast<ADMJellyCharacterBase>(GetOwner());

    if (!IsValid(OwnerCharacter))
    {
        return nullptr;
    }

    return OwnerCharacter->GetMesh();
}

void UDMCharacterVFXComponent::SpawnOneShot(
    UNiagaraSystem* Effect, const FName SocketName, const FVector LocalOffset, const FRotator LocalRotation
) const
{
    if (!CanPresentLocally() || !IsValid(Effect))
    {
        return;
    }

    USkeletalMeshComponent* MeshComponent = GetOwnerMesh();

    if (!IsValid(MeshComponent))
    {
        return;
    }

    // 잘못된 Socket에 이펙트가 생성되지 않도록 확인한다.
    if (!MeshComponent->DoesSocketExist(SocketName))
    {
        UE_LOG(
            LogTemp, Warning, TEXT("[%s] VFX Socket not found: %s"), *GetNameSafe(GetOwner()), *SocketName.ToString()
        );

        return;
    }

    // 일회성 이펙트는 자동 반환 풀링을 사용한다.
    UNiagaraFunctionLibrary::SpawnSystemAttached(
        Effect,
        MeshComponent,
        SocketName,
        LocalOffset,
        LocalRotation,
        EAttachLocation::KeepRelativeOffset,
        true,
        true,
        ENCPoolMethod::AutoRelease,
        true
    );
}

#pragma endregion

#pragma region VFX Presentation

void UDMCharacterVFXComponent::PlayAttackEffect()
{
    SpawnOneShot(AttackEffect.Get(), DMCharacterVFX::AttackSocket, AttackLocalOffset, AttackLocalRotation);
}

void UDMCharacterVFXComponent::PlayNPCAlertEffect()
{
    SpawnOneShot(NPCAlertEffect.Get(), DMCharacterVFX::NPCAlertSocket, AlertLocalOffset, AlertLocalRotation);
}

void UDMCharacterVFXComponent::PlayDefenseEffect()
{
    StopDefenseEffect();
    if (!CanPresentLocally() || !IsValid(DefenseEffect.Get()))
    {
        return;
    }

    USkeletalMeshComponent* MeshComponent = GetOwnerMesh();
    if (!IsValid(MeshComponent) || !MeshComponent->DoesSocketExist(DMCharacterVFX::DefenseSocket))
    {
        return;
    }

    ActiveDefenseEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
        DefenseEffect.Get(), MeshComponent, DMCharacterVFX::DefenseSocket,
        DefenseLocalOffset, DefenseLocalRotation, EAttachLocation::KeepRelativeOffset,
        false, true, ENCPoolMethod::None, true
    );

    if (ActiveDefenseEffect.IsValid())
    {
        GetWorld()->GetTimerManager().SetTimer(
            DefenseEffectEndTimer, this, &ThisClass::StopDefenseEffect,
            FMath::Max(0.05f, DefenseEffectMaxDuration), false
        );
    }
}

void UDMCharacterVFXComponent::StopDefenseEffect()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DefenseEffectEndTimer);
    }

    if (UNiagaraComponent* ExistingEffect = ActiveDefenseEffect.Get())
    {
        ExistingEffect->DeactivateImmediate();
        ExistingEffect->DestroyComponent();
    }
    ActiveDefenseEffect.Reset();
}

void UDMCharacterVFXComponent::SetStunEffectActive(const bool bActive)
{
    if (!bActive)
    {
        // 기존 스턴 이펙트가 있으면 종료한다.
        if (UNiagaraComponent* ExistingEffect = ActiveStunEffect.Get())
        {
            // Infinite Loop Niagara도 상태 종료와 동시에 화면에서 제거한다.
            ExistingEffect->DeactivateImmediate();
            ExistingEffect->DestroyComponent();
        }

        ActiveStunEffect.Reset();
        return;
    }

    // 같은 상태가 반복 전달되어도 중복 생성하지 않는다.
    if (ActiveStunEffect.IsValid())
    {
        return;
    }

    if (!CanPresentLocally() || !IsValid(StunEffect.Get()))
    {
        return;
    }

    USkeletalMeshComponent* MeshComponent = GetOwnerMesh();

    if (!IsValid(MeshComponent))
    {
        return;
    }

    if (!MeshComponent->DoesSocketExist(DMCharacterVFX::StunSocket))
    {
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("[%s] Stun VFX Socket not found: %s"),
            *GetNameSafe(GetOwner()),
            *DMCharacterVFX::StunSocket.ToString()
        );

        return;
    }

    // 스턴이 끝날 때까지 유지하는 Niagara를 생성한다.
    ActiveStunEffect = UNiagaraFunctionLibrary::SpawnSystemAttached(
        StunEffect.Get(),
        MeshComponent,
        DMCharacterVFX::StunSocket,
        StunLocalOffset,
        StunLocalRotation,
        EAttachLocation::KeepRelativeOffset,
        true,
        true,
        ENCPoolMethod::None,
        false
    );
}

#pragma endregion
