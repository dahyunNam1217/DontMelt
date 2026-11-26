
#include "UI/DMPoseHUDWidget.h"

#include "Characters/DMPlayerCharacter.h"
#include "Characters/Components/DMPoseComponent.h"

#pragma region Lifecycle

void UDMPoseHUDWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // 초기 데이터가 연결되기 전에는 숨긴다.
    SetVisibility(ESlateVisibility::Collapsed);
}

void UDMPoseHUDWidget::NativeDestruct()
{
    SetPresentationActive(false);
    SetPlayerCharacter(nullptr);

    Super::NativeDestruct();
}

#pragma endregion

#pragma region Data Source

void UDMPoseHUDWidget::SetPlayerCharacter(ADMPlayerCharacter* InCharacter)
{
    UDMPoseComponent* NewPoseComponent =
        IsValid(InCharacter) ? InCharacter->FindComponentByClass<UDMPoseComponent>() : nullptr;

    if (BoundCharacter.Get() == InCharacter && BoundPoseComponent.Get() == NewPoseComponent)
    {
        RefreshPresentation();
        return;
    }

    // 이전 Pawn의 로컬 입력 모드 알림을 해제한다.
    if (ADMPlayerCharacter* OldCharacter = BoundCharacter.Get())
    {
        OldCharacter->OnLocalPoseInputModeChanged().RemoveAll(this);
    }

    // 이전 PoseComponent의 표시 알림을 해제한다.
    if (UDMPoseComponent* OldPose = BoundPoseComponent.Get())
    {
        OldPose->OnPosePresentationChanged().RemoveAll(this);
    }

    BoundCharacter = InCharacter;
    BoundPoseComponent = NewPoseComponent;

    if (IsValid(InCharacter))
    {
        InCharacter->OnLocalPoseInputModeChanged().AddUObject(this, &ThisClass::HandleLocalPoseInputModeChanged);
    }

    if (IsValid(NewPoseComponent))
    {
        NewPoseComponent->OnPosePresentationChanged().AddUObject(this, &ThisClass::HandlePosePresentationChanged);
    }

    // 리스폰 또는 늦은 데이터 연결에도 현재 상태를 표시한다.
    RefreshPresentation();
}

void UDMPoseHUDWidget::SetPresentationActive(const bool bActive)
{
    bPresentationActive = bActive;
    RefreshPresentation();
}

#pragma endregion

#pragma region Presentation

void UDMPoseHUDWidget::HandleLocalPoseInputModeChanged(bool /*bActive*/
)
{
    // 로컬 Input Context 상태를 다시 조회한다.
    RefreshPresentation();
}

void UDMPoseHUDWidget::HandlePosePresentationChanged()
{
    // 서버가 확정한 자세 또는 모드 상태를 다시 조회한다.
    RefreshPresentation();
}

void UDMPoseHUDWidget::RefreshPresentation()
{
    const ADMPlayerCharacter* Character = BoundCharacter.Get();

    if (!bPresentationActive || !IsValid(Character) || !Character->IsLocallyControlled()
        || !Character->IsPoseInputModeActive())
    {
        if (IsValid(PoseRadialMenu))
        {
            ApplyPoseRadialHighlight(PoseRadialMenu, 0);
        }
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    // 자세 입력 모드일 때만 선택 UI를 표시한다.
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    const UDMPoseComponent* PoseComponent = BoundPoseComponent.Get();

    const bool bServerConfirmed = IsValid(PoseComponent) && PoseComponent->IsPoseLocked();

    const EDMPoseType CurrentPose = IsValid(PoseComponent) ? PoseComponent->GetPoseType() : EDMPoseType::Default;

    if (IsValid(PoseRadialMenu))
    {
        int32 PoseIndex = 0;
        if (bServerConfirmed)
        {
            switch (CurrentPose)
            {
            case EDMPoseType::Flat: PoseIndex = 1; break;
            case EDMPoseType::Crouched: PoseIndex = 2; break;
            case EDMPoseType::Star: PoseIndex = 3; break;
            default: break;
            }
        }
        ApplyPoseRadialHighlight(PoseRadialMenu, PoseIndex);
    }
}

#pragma endregion
