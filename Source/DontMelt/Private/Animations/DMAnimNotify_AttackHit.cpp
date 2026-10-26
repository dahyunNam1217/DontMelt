#include "Animations/DMAnimNotify_AttackHit.h"

#include "Characters/Components/DMCombatComponent.h"

#include "Components/SkeletalMeshComponent.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMAttackHitNotify, Log, All);

void UDMAnimNotify_AttackHit::Notify(
    USkeletalMeshComponent* MeshComp, UAnimSequenceBase* Animation, const FAnimNotifyEventReference& EventReference
)
{
    Super::Notify(MeshComp, Animation, EventReference);

    if (MeshComp == nullptr)
    {
        return;
    }

    AActor* OwnerActor = MeshComp->GetOwner();

    if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
    {
        return;
    }

    UDMCombatComponent* CombatComponent = OwnerActor->FindComponentByClass<UDMCombatComponent>();

    if (CombatComponent == nullptr)
    {
        UE_LOG(
            LogDMAttackHitNotify,
            Error,
            TEXT("[DM Attack Hit Notify] CombatComponent not found. Owner=%s"),
            *GetNameSafe(OwnerActor)
        );

        return;
    }

    CombatComponent->HandleAttackHitNotify();
}