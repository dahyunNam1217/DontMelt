// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "Characters/DMJellyCharacterBase.h"

#include "Engine/DataTable.h"
#include "DMNPCCharacter.generated.h"

class UMaterialInterface;
class UMaterialInstanceDynamic;
class UStaticMeshComponent;

/**
 * NPC 젤리 인간 Character.
 *
 * 플레이어 전용 Combat, Interaction, Pose Component를 소유하지 않는다.
 */

UCLASS()
class DONTMELT_API ADMNPCCharacter : public ADMJellyCharacterBase
{
    GENERATED_BODY()

public:
    ADMNPCCharacter(const FObjectInitializer& ObjectInitializer);

    /** 정전 전용 Material을 로컬 Mesh 슬롯에 적용한다. */
    bool BeginBlackoutMaterialPresentation(
        UMaterialInterface* BlackoutMaterial,
        FName FadeParameterName,
        float InitialFade,
        const TArray<int32>& MaterialSlots
    );

    /** 이미 생성된 정전 MID의 Fade 값만 변경한다. */
    void SetBlackoutMaterialFade(float FadeValue);

    /** 정전 MID가 유지된 슬롯만 원래 Material로 복구한다. */
    void EndBlackoutMaterialPresentation();

#pragma region AI Configuration Query

    const FDataTableRowHandle& GetBehaviorProfileHandle() const
    {
        return BehaviorProfile;
    }

#pragma endregion

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

public:
#pragma region NPC Alert Presentation

    /**
     * 서버가 승인한 NPC 놀람 연출을 관련 클라이언트에 전달한다.
     *
     * Gameplay 상태를 변경하지 않는 일회성 시각 연출이다.
     * 서버의 AIController에서만 호출한다.
     */
    UFUNCTION(NetMulticast, Unreliable)
    void MulticastPlayAlertEffect();

    /** BP Timeline 완료 시 호출하여 느낌표를 다시 숨긴다. */
    UFUNCTION(BlueprintCallable, Category = "DM|Presentation|Mistaken Attack")
    void FinishMistakenAttackExclamationPresentation();

    UFUNCTION(BlueprintImplementableEvent, Category = "DM|Presentation|Mistaken Attack")
    void BP_PlayMistakenAttackExclamationPresentation();

    UFUNCTION(BlueprintImplementableEvent, Category = "DM|Presentation|Mistaken Attack")
    void BP_StopMistakenAttackExclamationPresentation();

#pragma endregion


private:
    void StopMistakenAttackExclamationPresentation();

    /** BP Viewport에서 Mesh·Socket·상대 Transform을 편집한다. */
    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category = "DM|Presentation|Mistaken Attack",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UStaticMeshComponent> MistakenAttackExclamation = nullptr;

    /** 슬롯별 정전 시작 직전 Material. */
    UPROPERTY(Transient)
    TMap<int32, TObjectPtr<UMaterialInterface>> BlackoutOriginalMaterials;

    /** Fade 값을 갱신할 슬롯별 정전 MID. */
    UPROPERTY(Transient)
    TMap<int32, TObjectPtr<UMaterialInstanceDynamic>> BlackoutDynamicMaterials;

    FName ActiveBlackoutFadeParameterName = NAME_None;

#pragma region AI Configuration Data

    /**
     * 이 NPC Archetype이 사용할
     * Behavior DataTable Row.
     *
     * 런타임 행동 상태가 아니라
     * AI 정의 데이터 연결 정보다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|AI|Behavior", meta = (AllowPrivateAccess = "true"))
    FDataTableRowHandle BehaviorProfile;

#pragma endregion
};
