#pragma once

#include "CoreMinimal.h"
#include "Characters/DMPlayerCharacter.h"
#include "DMAnonymousPoCPlayerPawn.generated.h"

class UInputAction;
class UInputMappingContext;
class UEnhancedInputLocalPlayerSubsystem;
struct FInputActionValue;

/** 실제 플레이어 Pawn. 본 게임을 바꾸지 않고 소유 연결만 허용하는 실험이다. */
UCLASS()
class DONTMELT_API ADMAnonymousPoCPlayerPawn : public ADMPlayerCharacter
{
    GENERATED_BODY()

public:
    ADMAnonymousPoCPlayerPawn(const FObjectInitializer& ObjectInitializer);
    virtual bool IsNetRelevantFor(
        const AActor* RealViewer, const AActor* ViewTarget, const FVector& SrcLocation
    ) const override;
    virtual void PawnClientRestart() override;
    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;
    virtual void CalcCamera(float DeltaTime, FMinimalViewInfo& OutResult) override;

protected:
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

private:
    void InputForward(const FInputActionValue& Value);
    void InputRight(const FInputActionValue& Value);
    void InputYaw(const FInputActionValue& Value);
    void InputPitch(const FInputActionValue& Value);
    void MoveForward(float Value);
    void MoveRight(float Value);
    void LookYaw(float Value);
    void LookPitch(float Value);
    void BeginSprint();
    void EndSprint();
    void BeginJump();
    void EndJump();

    // 로컬 입력용 런타임 객체이며 에셋 파일·Config·복제 상태를 만들지 않는다.
    UPROPERTY(Transient)
    TObjectPtr<UInputMappingContext> LocalMapping;
    UPROPERTY(Transient)
    TObjectPtr<UInputAction> ForwardAction;
    UPROPERTY(Transient)
    TObjectPtr<UInputAction> RightAction;
    UPROPERTY(Transient)
    TObjectPtr<UInputAction> YawAction;
    UPROPERTY(Transient)
    TObjectPtr<UInputAction> PitchAction;
    TWeakObjectPtr<UEnhancedInputLocalPlayerSubsystem> BoundInputSubsystem;
};
