
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"

#include "DMPoseHUDWidget.generated.h"

class ADMPlayerCharacter;
class UDMPoseComponent;

/**
 * 로컬 자세 모드와 서버가 확정한 현재 자세를 표시한다.
 *
 * 입력 처리·자세 변경·퀘스트 이벤트는 수행하지 않는다.
 */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMPoseHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
#pragma region Data Source

    void SetPlayerCharacter(ADMPlayerCharacter* InCharacter);

    void SetPresentationActive(bool bActive);

#pragma endregion

protected:
    virtual void NativeConstruct() override;
    virtual void NativeDestruct() override;

#pragma region Widget References

    UPROPERTY(meta = (BindWidgetOptional))
    TObjectPtr<UUserWidget> PoseRadialMenu = nullptr;

    UFUNCTION(BlueprintImplementableEvent, Category = "DM|Pose")
    void ApplyPoseRadialHighlight(UUserWidget* RadialMenu, int32 PoseIndex);

#pragma endregion

private:
#pragma region Presentation

    void HandleLocalPoseInputModeChanged(bool bActive);

    void HandlePosePresentationChanged();

    void RefreshPresentation();

    TWeakObjectPtr<ADMPlayerCharacter> BoundCharacter;
    TWeakObjectPtr<UDMPoseComponent> BoundPoseComponent;

    bool bPresentationActive = false;

#pragma endregion
};
