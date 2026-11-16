#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "Engine/NetSerialization.h"
#include "Characters/DMCharacterTypes.h"
#include "DMAnonymousPresentationActor.generated.h"

class APlayerController;
class UStaticMeshComponent;

/** 판정 원본이나 신원 참조를 포함하지 않는 실험용 공개 스냅샷. */
USTRUCT(BlueprintType)
struct FDMAnonymousPresentationState
{
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly)
    FVector_NetQuantize100 Location = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    FRotator Rotation = FRotator::ZeroRotator;

    UPROPERTY(BlueprintReadOnly)
    FVector_NetQuantize10 Velocity = FVector::ZeroVector;

    UPROPERTY(BlueprintReadOnly)
    EDMActionState ActionState = EDMActionState::None;

    UPROPERTY(BlueprintReadOnly)
    bool bFalling = false;

    UPROPERTY(BlueprintReadOnly)
    double SampleServerTime = 0.0;
};

/** 모든 실험 소스에 동일한 네이티브 클래스를 사용한다. 실제 Pawn은 보관하지 않는다. */
UCLASS(NotBlueprintable)
class DONTMELT_API ADMAnonymousPresentationActor : public AActor
{
    GENERATED_BODY()

public:
    ADMAnonymousPresentationActor();
    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;
    virtual bool IsNetRelevantFor(
        const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation
    ) const override;
    virtual void Tick(float DeltaSeconds) override;

    void SetPublicState(const FDMAnonymousPresentationState& State);
    void SetExcludedViewer(APlayerController* Controller);

    UFUNCTION(BlueprintPure, Category = "DM|Experiment")
    FDMAnonymousPresentationState GetPublicState() const { return PublicState; }

private:
    UFUNCTION()
    void OnRep_PublicState();

    UPROPERTY(VisibleAnywhere)
    TObjectPtr<UStaticMeshComponent> Body;

    UPROPERTY(ReplicatedUsing = OnRep_PublicState)
    FDMAnonymousPresentationState PublicState;

    // 서버에서만 설정하는 자기 표현 제외 대상. 복제·소유·부착 관계로 사용하지 않는다.
    TWeakObjectPtr<APlayerController> ExcludedViewer;
    bool bReceivedState = false;
};
