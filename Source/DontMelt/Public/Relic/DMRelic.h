#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Interaction/DMInteractable.h"
#include "Relic/DMRelicTypes.h"
#include "DMRelic.generated.h"

class ADMRelicManager;
class UBoxComponent;
class USceneComponent;
class UStaticMeshComponent;

/**
 * 개별 유물의 서버 권위 상태와 월드 표현을 담당한다.
 *
 * Carrier 관계와 Interaction Reservation은 소유하지 않는다.
 */
UCLASS()
class DONTMELT_API ADMRelic : public AActor, public IDMInteractable
{
    GENERATED_BODY()

    friend class ADMRelicManager;

public:
    ADMRelic();

    virtual bool
    IsNetRelevantFor(const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation) const override;

    UFUNCTION(BlueprintPure, Category = "DM|Relic")
    int32 GetRelicId() const
    {
        return RelicId;
    }

    EDMRelicWorldState GetWorldState() const
    {
        return WorldState;
    }

    int32 GetRegisteredBaseSlot() const
    {
        return RegisteredBaseSlot;
    }

    const FTransform& GetLastValidWorldTransform() const
    {
        return LastValidWorldTransform;
    }

private:
    /**
     * RelicManager가 생성 직후 한 번 초기화한다.
     */
    void InitializeServer(
        int32 InRelicId,
        EDMRelicWorldState InInitialState,
        int32 InRegisteredBaseSlot,
        const FTransform& InInitialTransform
    );

    void SetWorldState(EDMRelicWorldState NewState);

    void SetRegisteredBaseSlot(int32 NewBaseSlot);

    void SetLastValidWorldTransform(const FTransform& NewTransform);

    bool ShouldBeVisibleInWorld() const;

    void RefreshWorldPresentation();

private:
    UPROPERTY(
        VisibleAnywhere, BlueprintReadOnly, Category = "DM|Relic|Components", meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<USceneComponent> SceneRoot;

    UPROPERTY(
        VisibleAnywhere, BlueprintReadOnly, Category = "DM|Relic|Components", meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UStaticMeshComponent> RelicMesh;

    UPROPERTY(
        VisibleAnywhere, BlueprintReadOnly, Category = "DM|Relic|Components", meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UBoxComponent> InteractionBounds;

    /** Match 내부에서 Relic을 구분하는 ID. */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "DM|Relic|State", meta = (AllowPrivateAccess = "true"))
    int32 RelicId = INDEX_NONE;

    /**
     * 개별 Relic 상태의 권위 원본.
     * 서버 Gameplay 판정용이며 일반 Client 공개 데이터가 아니다.
     */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "DM|Relic|State", meta = (AllowPrivateAccess = "true"))
    EDMRelicWorldState WorldState = EDMRelicWorldState::PublicSpawn;

    /**
     * Registered 또는 DepositProtected일 때 등록된 진영.
     * 그 외에는 INDEX_NONE.
     */
    UPROPERTY(VisibleInstanceOnly, BlueprintReadOnly, Category = "DM|Relic|State", meta = (AllowPrivateAccess = "true"))
    int32 RegisteredBaseSlot = INDEX_NONE;

    /**
     * Recovery 시 사용할 마지막 검증 완료 Transform.
     */
    FTransform LastValidWorldTransform = FTransform::Identity;
};