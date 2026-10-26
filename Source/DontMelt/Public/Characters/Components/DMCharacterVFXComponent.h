
#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "DMCharacterVFXComponent.generated.h"

class UNiagaraSystem;
class UNiagaraComponent;
class USkeletalMeshComponent;
struct FTimerHandle;

/**
 * Player와 NPC가 공유하는 로컬 VFX 표현 컴포넌트.
 *
 * Gameplay 판정과 상태를 소유하지 않으며
 * Niagara 생성 및 종료만 담당한다.
 */
UCLASS(ClassGroup = (DontMelt))
class DONTMELT_API UDMCharacterVFXComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDMCharacterVFXComponent();

#pragma region VFX Presentation

    /** 공격 시 일회성 이펙트를 재생한다. */
    void PlayAttackEffect();

    /** NPC 놀람 시 일회성 느낌표를 재생한다. */
    void PlayNPCAlertEffect();

    /** 방어 시 일회성 이펙트를 재생한다. */
    void PlayDefenseEffect();

    /** 복제된 스턴 표현 상태에 따라 별 이펙트를 시작하거나 종료한다. */
    void SetStunEffectActive(bool bActive);

#pragma endregion

protected:
#pragma region Lifecycle

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#pragma endregion

private:
#pragma region VFX Internal

    bool CanPresentLocally() const;

    USkeletalMeshComponent* GetOwnerMesh() const;

    void SpawnOneShot(UNiagaraSystem* Effect, FName SocketName, FVector LocalOffset, FRotator LocalRotation) const;

    void StopDefenseEffect();

#pragma endregion

#pragma region VFX Configuration

    UPROPERTY(EditDefaultsOnly, Category = "DontMelt|VFX|Combat")
    TObjectPtr<UNiagaraSystem> AttackEffect = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "DontMelt|VFX|NPC")
    TObjectPtr<UNiagaraSystem> NPCAlertEffect = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "DontMelt|VFX|Status")
    TObjectPtr<UNiagaraSystem> StunEffect = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "DontMelt|VFX|Combat")
    TObjectPtr<UNiagaraSystem> DefenseEffect = nullptr;

    // Socket 기준 상대 위치·회전. NPC와 플레이어가 사용하는 동일 Skeleton 기준으로 조정한다.
    UPROPERTY(EditDefaultsOnly, Category = "DontMelt|VFX|Offset")
    FVector AttackLocalOffset = FVector::ZeroVector;

    UPROPERTY(EditDefaultsOnly, Category = "DontMelt|VFX|Offset")
    FRotator AttackLocalRotation = FRotator::ZeroRotator;

    UPROPERTY(EditDefaultsOnly, Category = "DontMelt|VFX|Offset")
    FVector AlertLocalOffset = FVector::ZeroVector;

    UPROPERTY(EditDefaultsOnly, Category = "DontMelt|VFX|Offset")
    FRotator AlertLocalRotation = FRotator::ZeroRotator;

    UPROPERTY(EditDefaultsOnly, Category = "DontMelt|VFX|Offset")
    FVector DefenseLocalOffset = FVector::ZeroVector;

    UPROPERTY(EditDefaultsOnly, Category = "DontMelt|VFX|Offset")
    FRotator DefenseLocalRotation = FRotator::ZeroRotator;

    UPROPERTY(EditDefaultsOnly, Category = "DontMelt|VFX|Offset")
    FVector StunLocalOffset = FVector::ZeroVector;

    UPROPERTY(EditDefaultsOnly, Category = "DontMelt|VFX|Offset")
    FRotator StunLocalRotation = FRotator::ZeroRotator;

    // 방어 Niagara가 Infinite Loop여도 일정 시간 뒤 강제로 종료한다.
    UPROPERTY(EditDefaultsOnly, Category = "DontMelt|VFX|Combat", meta = (ClampMin = "0.05"))
    float DefenseEffectMaxDuration = 0.8f;

#pragma endregion

#pragma region Local Runtime

    /** 생성된 지속형 스턴 이펙트를 종료하기 위한 로컬 참조. */
    TWeakObjectPtr<UNiagaraComponent> ActiveStunEffect;
    TWeakObjectPtr<UNiagaraComponent> ActiveDefenseEffect;
    FTimerHandle DefenseEffectEndTimer;

#pragma endregion
};
