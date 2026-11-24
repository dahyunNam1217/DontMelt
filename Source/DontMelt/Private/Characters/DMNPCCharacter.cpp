
#include "Characters/DMNPCCharacter.h"

#include "Characters/DMJellyAIController.h"
#include "Components/SkeletalMeshComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Engine/EngineTypes.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Materials/MaterialInterface.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMNPCCharacter, Log, All);

ADMNPCCharacter::ADMNPCCharacter(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
    SetReplicates(true);
    SetReplicateMovement(true);

    AIControllerClass = ADMJellyAIController::StaticClass();

    AutoPossessAI = EAutoPossessAI::PlacedInWorldOrSpawned;

    MistakenAttackExclamation = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("MistakenAttackExclamation"));
    MistakenAttackExclamation->SetupAttachment(GetMesh(), TEXT("FX_Alert_Right"));
    MistakenAttackExclamation->SetCollisionEnabled(ECollisionEnabled::NoCollision);
    MistakenAttackExclamation->SetGenerateOverlapEvents(false);
    MistakenAttackExclamation->SetSimulatePhysics(false);
    MistakenAttackExclamation->SetCastShadow(false);
    MistakenAttackExclamation->SetHiddenInGame(true);
    MistakenAttackExclamation->SetVisibility(true, true);
}

bool ADMNPCCharacter::BeginBlackoutMaterialPresentation(
    UMaterialInterface* BlackoutMaterial,
    const FName FadeParameterName,
    const float InitialFade,
    const TArray<int32>& MaterialSlots
)
{
    USkeletalMeshComponent* MeshComponent = GetMesh();

    if (!IsValid(MeshComponent) || !IsValid(BlackoutMaterial) || FadeParameterName.IsNone())
    {
        return false;
    }

    float ExistingParameterValue = 0.0f;

    if (!BlackoutMaterial->GetScalarParameterValue(
            FHashedMaterialParameterInfo(FadeParameterName), ExistingParameterValue
        ))
    {
        UE_LOG(
            LogDMNPCCharacter,
            Warning,
            TEXT("Blackout material is missing scalar parameter. NPC=%s, Material=%s, Parameter=%s"),
            *GetName(),
            *GetNameSafe(BlackoutMaterial),
            *FadeParameterName.ToString()
        );
        return false;
    }

    EndBlackoutMaterialPresentation();

    TArray<int32, TInlineAllocator<8>> SlotsToApply;
    const int32 MaterialCount = MeshComponent->GetNumMaterials();

    if (MaterialSlots.IsEmpty())
    {
        for (int32 SlotIndex = 0; SlotIndex < MaterialCount; ++SlotIndex)
        {
            SlotsToApply.Add(SlotIndex);
        }
    }
    else
    {
        for (const int32 SlotIndex : MaterialSlots)
        {
            if (SlotIndex >= 0 && SlotIndex < MaterialCount)
            {
                SlotsToApply.AddUnique(SlotIndex);
            }
        }
    }

    ActiveBlackoutFadeParameterName = FadeParameterName;

    for (const int32 SlotIndex : SlotsToApply)
    {
        UMaterialInterface* OriginalMaterial = MeshComponent->GetMaterial(SlotIndex);

        UMaterialInstanceDynamic* DynamicMaterial =
            MeshComponent->CreateDynamicMaterialInstance(SlotIndex, BlackoutMaterial);

        if (!IsValid(DynamicMaterial))
        {
            continue;
        }

        BlackoutOriginalMaterials.Add(SlotIndex, OriginalMaterial);
        BlackoutDynamicMaterials.Add(SlotIndex, DynamicMaterial);

        DynamicMaterial->SetScalarParameterValue(ActiveBlackoutFadeParameterName, FMath::Clamp(InitialFade, 0.0f, 1.0f));
    }

    if (BlackoutDynamicMaterials.IsEmpty())
    {
        ActiveBlackoutFadeParameterName = NAME_None;
        return false;
    }

    return true;
}

void ADMNPCCharacter::SetBlackoutMaterialFade(const float FadeValue)
{
    if (ActiveBlackoutFadeParameterName.IsNone())
    {
        return;
    }

    const float ClampedFade = FMath::Clamp(FadeValue, 0.0f, 1.0f);

    for (const TPair<int32, TObjectPtr<UMaterialInstanceDynamic>>& Entry : BlackoutDynamicMaterials)
    {
        if (IsValid(Entry.Value))
        {
            Entry.Value->SetScalarParameterValue(ActiveBlackoutFadeParameterName, ClampedFade);
        }
    }
}

void ADMNPCCharacter::EndBlackoutMaterialPresentation()
{
    USkeletalMeshComponent* MeshComponent = GetMesh();

    if (IsValid(MeshComponent))
    {
        for (const TPair<int32, TObjectPtr<UMaterialInterface>>& Entry : BlackoutOriginalMaterials)
        {
            const TObjectPtr<UMaterialInstanceDynamic>* DynamicMaterial = BlackoutDynamicMaterials.Find(Entry.Key);

            // 정전 도중 다른 표현이 슬롯을 교체했다면 오래된 원본으로 덮어쓰지 않는다.
            if (DynamicMaterial != nullptr && MeshComponent->GetMaterial(Entry.Key) == DynamicMaterial->Get())
            {
                MeshComponent->SetMaterial(Entry.Key, Entry.Value);
            }
        }
    }

    BlackoutOriginalMaterials.Reset();
    BlackoutDynamicMaterials.Reset();
    ActiveBlackoutFadeParameterName = NAME_None;
}

void ADMNPCCharacter::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    StopMistakenAttackExclamationPresentation();
    EndBlackoutMaterialPresentation();

    Super::EndPlay(EndPlayReason);
}

#pragma region NPC Alert Presentation

void ADMNPCCharacter::MulticastPlayAlertEffect_Implementation()
{
    StopMistakenAttackExclamationPresentation();

    // BP 연출이 없으면 기본 Hidden 상태를 유지한다.
    if (!GetClass()->IsFunctionImplementedInScript(
            GET_FUNCTION_NAME_CHECKED(ADMNPCCharacter, BP_PlayMistakenAttackExclamationPresentation)
        ))
    {
        return;
    }

    BP_PlayMistakenAttackExclamationPresentation();
}

void ADMNPCCharacter::FinishMistakenAttackExclamationPresentation()
{
    if (IsValid(MistakenAttackExclamation))
    {
        MistakenAttackExclamation->SetHiddenInGame(true);
    }
}

void ADMNPCCharacter::StopMistakenAttackExclamationPresentation()
{
    if (GetClass()->IsFunctionImplementedInScript(
            GET_FUNCTION_NAME_CHECKED(ADMNPCCharacter, BP_StopMistakenAttackExclamationPresentation)
        ))
    {
        BP_StopMistakenAttackExclamationPresentation();
    }

    FinishMistakenAttackExclamationPresentation();
}

#pragma endregion
