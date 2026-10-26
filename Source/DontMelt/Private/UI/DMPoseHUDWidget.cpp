
#include "UI/DMPoseHUDWidget.h"

#include "Characters/DMPlayerCharacter.h"
#include "Characters/Components/DMPoseComponent.h"

#include "Components/TextBlock.h"

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
        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    // 자세 입력 모드일 때만 선택 UI를 표시한다.
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    const UDMPoseComponent* PoseComponent = BoundPoseComponent.Get();

    const bool bServerConfirmed = IsValid(PoseComponent) && PoseComponent->IsPoseLocked();

    const EDMPoseType CurrentPose = IsValid(PoseComponent) ? PoseComponent->GetPoseType() : EDMPoseType::Default;

    if (IsValid(TXT_ModeStatus))
    {
        TXT_ModeStatus->SetText(
            bServerConfirmed ? NSLOCTEXT("DMPoseHUD", "PoseModeActive", "자세 선택 모드")
                             : NSLOCTEXT("DMPoseHUD", "PoseModePending", "서버 자세 상태 확인 중")
        );
    }

    // 실제 적용된 자세만 선택 상태로 강조한다.
    const bool bFlat = bServerConfirmed && CurrentPose == EDMPoseType::Flat;

    const bool bCrouched = bServerConfirmed && CurrentPose == EDMPoseType::Crouched;

    const bool bStar = bServerConfirmed && CurrentPose == EDMPoseType::Star;

    if (IsValid(TXT_PoseFlat))
    {
        TXT_PoseFlat->SetRenderOpacity(bFlat ? 1.0f : 0.45f);
    }

    if (IsValid(TXT_PoseCrouched))
    {
        TXT_PoseCrouched->SetRenderOpacity(bCrouched ? 1.0f : 0.45f);
    }

    if (IsValid(TXT_PoseStar))
    {
        TXT_PoseStar->SetRenderOpacity(bStar ? 1.0f : 0.45f);
    }

    if (!IsValid(TXT_CurrentPose))
    {
        return;
    }

    if (!bServerConfirmed)
    {
        TXT_CurrentPose->SetText(NSLOCTEXT("DMPoseHUD", "PosePending", "적용 대기"));
        return;
    }

    FText PoseName;

    switch (CurrentPose)
    {
    case EDMPoseType::Flat:
        PoseName = NSLOCTEXT("DMPoseHUD", "Flat", "납작");
        break;

    case EDMPoseType::Crouched:
        PoseName = NSLOCTEXT("DMPoseHUD", "Crouched", "웅크림");
        break;

    case EDMPoseType::Star:
        PoseName = NSLOCTEXT("DMPoseHUD", "Star", "별 자세");
        break;

    default:
        PoseName = NSLOCTEXT("DMPoseHUD", "Default", "기본 자세");
        break;
    }

    TXT_CurrentPose->SetText(FText::Format(NSLOCTEXT("DMPoseHUD", "CurrentPoseFormat", "현재 자세: {0}"), PoseName));
}

#pragma endregion
