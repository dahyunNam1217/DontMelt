#include "UI/DMRelicHUDWidget.h"

#include "Characters/DMPlayerCharacter.h"
#include "Characters/Components/DMInteractionComponent.h"

#include "Framework/Match/DMMatchPlayerController.h"
#include "Framework/Match/DMMatchPlayerState.h"

#include "Engine/World.h"
#include "TimerManager.h"

namespace
{
    /*
     * Gameplay Tick이 아니다.
     *
     * Interaction 진행 UI가 실제로 보이는
     * 2초 동안에만 약 20Hz로 Percent를 갱신한다.
     */
    constexpr float InteractionProgressUpdateInterval = 0.05f;
}

void UDMRelicHUDWidget::InitializeRelicHUD(ADMMatchPlayerController* InController, ADMPlayerCharacter* InCharacter)
{
    ShutdownRelicHUD();

    if (!IsValid(InController) || !IsValid(InCharacter))
    {
        return;
    }

    OwningMatchController = InController;

    ADMMatchPlayerState* PlayerState = InCharacter->GetPlayerState<ADMMatchPlayerState>();

    if (IsValid(PlayerState))
    {
        BoundPlayerState = PlayerState;

        PlayerState->OnCarryStateChanged().AddUObject(this, &ThisClass::HandleCarryStateChanged);

        /*
         * Delegate가 다음에 올 때까지 기다리지 않고
         * 현재 값을 즉시 HUD에 투영한다.
         */
        BP_SetCarryState(PlayerState->IsCarryingRelic());
    }
    else
    {
        BP_SetCarryState(false);
    }

    UDMInteractionComponent* InteractionComponent = InCharacter->FindComponentByClass<UDMInteractionComponent>();

    if (IsValid(InteractionComponent))
    {
        BoundInteractionComponent = InteractionComponent;

        InteractionComponent->OnInteractionSessionChanged().AddUObject(
            this, &ThisClass::HandleInteractionSessionChanged
        );

        HandleInteractionSessionChanged(InteractionComponent->GetOwnerSession());
    }
    else
    {
        BP_SetInteractionProgress(false, EDMInteractionType::PickupRelic, 0.0f);
    }

    InController->OnChestInteractionChanged().AddUObject(this, &ThisClass::HandleChestInteractionChanged);

    /*
     * 현재 Chest가 이미 열려 있을 가능성도 있으므로
     * 초기 상태를 즉시 동기화한다.
     */
    HandleChestInteractionChanged(
        InController->IsChestInteractionOpen(),
        InController->GetInspectedBaseSlot(),
        InController->GetInspectedRegisteredRelicCount()
    );
}

void UDMRelicHUDWidget::ShutdownRelicHUD()
{
    StopInteractionProgressUpdates();

    if (ADMMatchPlayerState* PlayerState = BoundPlayerState.Get())
    {
        PlayerState->OnCarryStateChanged().RemoveAll(this);
    }

    if (UDMInteractionComponent* InteractionComponent = BoundInteractionComponent.Get())
    {
        InteractionComponent->OnInteractionSessionChanged().RemoveAll(this);
    }

    if (ADMMatchPlayerController* Controller = OwningMatchController.Get())
    {
        Controller->OnChestInteractionChanged().RemoveAll(this);
    }

    OwningMatchController.Reset();

    BoundPlayerState.Reset();

    BoundInteractionComponent.Reset();
}

void UDMRelicHUDWidget::NativeDestruct()
{
    ShutdownRelicHUD();

    Super::NativeDestruct();
}

void UDMRelicHUDWidget::HandleCarryStateChanged(ADMMatchPlayerState* PlayerState, const bool bIsCarryingRelic)
{
    if (PlayerState != BoundPlayerState.Get())
    {
        return;
    }

    /*
     * Chest를 열어둔 동안에는
     * Carry 안내를 화면에서 숨긴다.
     *
     * Gameplay Carry 상태 자체를
     * 변경하는 것은 아니다.
     */
    if (ADMMatchPlayerController* Controller = OwningMatchController.Get())
    {
        if (Controller->IsChestInteractionOpen())
        {
            BP_SetCarryState(false);

            return;
        }
    }

    BP_SetCarryState(bIsCarryingRelic);
}

void UDMRelicHUDWidget::HandleInteractionSessionChanged(const FDMInteractionSession& Session)
{
    // 바닥 유물 획득만 기존 유물 HUD에서 표시한다.
    // 등록과 훔치기 진행률은 ChestPanel이 담당한다.
    if (!Session.IsActive() || Session.InteractionType != EDMInteractionType::PickupRelic)
    {
        StopInteractionProgressUpdates();

        BP_SetInteractionProgress(false, ActiveInteractionType, 0.0f);

        return;
    }

    // 현재 획득 상호작용 종류를 저장한다.
    ActiveInteractionType = Session.InteractionType;

    // 상호작용 시작 시 현재 진행률을 즉시 표시한다.
    UpdateInteractionProgress();

    // 획득 진행 중에만 로컬 표시 타이머를 실행한다.
    StartInteractionProgressUpdates();
}

void UDMRelicHUDWidget::HandleChestInteractionChanged(
    const bool bOpen, const int32 /*BaseSlot*/, const int32 /*RegisteredRelicCount*/
)
{
    // 상자를 열면 운반 안내를 가리되 실제 운반 상태는 변경하지 않는다.
    if (bOpen)
    {
        BP_SetCarryState(false);
        return;
    }

    // 상자를 닫으면 현재 PlayerState의 운반 상태를 다시 읽는다.
    const ADMMatchPlayerState* PlayerState = BoundPlayerState.Get();

    BP_SetCarryState(IsValid(PlayerState) && PlayerState->IsCarryingRelic());
}

void UDMRelicHUDWidget::StartInteractionProgressUpdates()
{
    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    FTimerManager& TimerManager = World->GetTimerManager();

    if (TimerManager.IsTimerActive(InteractionProgressTimerHandle))
    {
        return;
    }

    TimerManager.SetTimer(
        InteractionProgressTimerHandle,
        this,
        &ThisClass::UpdateInteractionProgress,
        InteractionProgressUpdateInterval,
        true
    );
}

void UDMRelicHUDWidget::StopInteractionProgressUpdates()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(InteractionProgressTimerHandle);
    }
}

void UDMRelicHUDWidget::UpdateInteractionProgress()
{
    UDMInteractionComponent* InteractionComponent = BoundInteractionComponent.Get();

    if (!IsValid(InteractionComponent) || !InteractionComponent->IsInteractionActive())
    {
        StopInteractionProgressUpdates();

        BP_SetInteractionProgress(false, ActiveInteractionType, 0.0f);

        return;
    }

    const FDMInteractionSession& Session = InteractionComponent->GetOwnerSession();

    // 진행 도중 상호작용 종류가 변경되었다면
    // 기존 유물 HUD의 진행률을 즉시 제거한다.
    if (Session.InteractionType != EDMInteractionType::PickupRelic)
    {
        StopInteractionProgressUpdates();

        BP_SetInteractionProgress(false, ActiveInteractionType, 0.0f);

        return;
    }

    ActiveInteractionType = Session.InteractionType;

    // 현재 원본 Session에서 진행률을 읽어 표시한다.
    BP_SetInteractionProgress(true, ActiveInteractionType, InteractionComponent->GetInteractionProgress01());
}
