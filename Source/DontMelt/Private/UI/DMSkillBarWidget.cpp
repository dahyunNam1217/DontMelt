
#include "UI/DMSkillBarWidget.h"

#include "UI/DMSkillSlotWidget.h"

#include "Framework/Match/DMMatchPlayerState.h"
#include "Skills/DMSkillComponent.h"
#include "Data/DMSkillSetDataAsset.h"
#include "Core/DMGameplayTags.h"

#include "Engine/World.h"

namespace
{
    // 쿨다운이 진행 중일 때만 화면을 갱신한다.
    constexpr float SkillHUDRefreshInterval = 0.1f;
}

#pragma region Data Source

void UDMSkillBarWidget::SetPlayerState(ADMMatchPlayerState* InPlayerState)
{
    UDMSkillComponent* NewSkillComponent = IsValid(InPlayerState) ? InPlayerState->GetSkillComponent() : nullptr;

    if (BoundPlayerState.Get() == InPlayerState && BoundSkillComponent.Get() == NewSkillComponent)
    {
        RefreshDefinitions();
        RefreshCooldowns();
        return;
    }

    StopDisplayTimer();

    // 이전 SkillComponent의 변경 알림을 해제한다.
    if (UDMSkillComponent* OldSkillComponent = BoundSkillComponent.Get())
    {
        OldSkillComponent->OnSkillReadModelChanged().RemoveAll(this);
    }

    BoundPlayerState = InPlayerState;
    BoundSkillComponent = NewSkillComponent;

    if (IsValid(NewSkillComponent))
    {
        NewSkillComponent->OnSkillReadModelChanged().AddUObject(this, &ThisClass::HandleSkillReadModelChanged);
    }

    // Delegate 구독 이후 현재 상태를 즉시 읽는다.
    RefreshDefinitions();
    RefreshCooldowns();
}

void UDMSkillBarWidget::SetPresentationActive(const bool bActive)
{
    if (bPresentationActive == bActive)
    {
        return;
    }

    bPresentationActive = bActive;

    if (!bPresentationActive)
    {
        StopDisplayTimer();
        return;
    }

    RefreshDefinitions();
    RefreshCooldowns();
}

#pragma endregion

#pragma region Data Binding

void UDMSkillBarWidget::HandleSkillReadModelChanged()
{
    // 스킬 정의와 종료 시각이 변경되었을 때만 호출된다.
    RefreshDefinitions();
    RefreshCooldowns();
}

void UDMSkillBarWidget::RefreshDefinitions()
{
    const UDMSkillComponent* SkillComponent = BoundSkillComponent.Get();

    const UDMSkillSetDataAsset* SkillSet = IsValid(SkillComponent) ? SkillComponent->GetSkillSetData() : nullptr;

    // 세 슬롯은 같은 Widget 클래스를 재사용한다.

    const auto ApplySlot = [SkillSet](UDMSkillSlotWidget* SkillSlotWidget, const FGameplayTag& Tag, const TCHAR* Key)
    {
        if (!IsValid(SkillSlotWidget))
        {
            return;
        }

        const FDMSkillDefinition* Definition = IsValid(SkillSet) ? SkillSet->FindSkillDefinition(Tag) : nullptr;

        SkillSlotWidget->ApplyDefinition(Definition, FText::FromString(Key));
    };

    ApplySlot(SkillSlot1, DMGameplayTags::Skill_Invisibility, TEXT("1"));

    ApplySlot(SkillSlot2, DMGameplayTags::Skill_FootstepAmplify, TEXT("2"));

    ApplySlot(SkillSlot3, DMGameplayTags::Skill_VisionDistort, TEXT("3"));
}

void UDMSkillBarWidget::RefreshCooldowns()
{
    if (!bPresentationActive)
    {
        StopDisplayTimer();
        return;
    }

    const UDMSkillComponent* SkillComponent = BoundSkillComponent.Get();

    const UDMSkillSetDataAsset* SkillSet = IsValid(SkillComponent) ? SkillComponent->GetSkillSetData() : nullptr;

    const bool bSystemReady =
        IsValid(SkillComponent) && SkillComponent->IsSkillSystemInitialized() && IsValid(SkillSet);

    const auto RefreshSlot =
        [SkillComponent, SkillSet, bSystemReady](UDMSkillSlotWidget* SkillSlotWidget, const FGameplayTag& Tag) -> bool
    {
        if (!IsValid(SkillSlotWidget))
        {
            return false;
        }

        const FDMSkillDefinition* Definition = bSystemReady ? SkillSet->FindSkillDefinition(Tag) : nullptr;

        // 복제된 쿨다운 항목까지 준비되었는지 확인한다.
        const bool bHasCooldownEntry = bSystemReady
            && SkillComponent->GetSkillCooldownEntries().ContainsByPredicate(
                [&Tag](const FDMSkillCooldownEntry& Entry)
                {
                    return Entry.SkillTag == Tag;
                }
            );

        const bool bDataReady = Definition != nullptr && bHasCooldownEntry;

        const float Remaining = bDataReady ? SkillComponent->GetRemainingSkillCooldown(Tag) : 0.0f;

        const float Duration = Definition ? Definition->Cooldown : 0.0f;

        SkillSlotWidget->UpdateCooldown(Remaining, Duration, bDataReady);

        return bDataReady && Remaining > 0.01f;
    };

    const bool bSlot1Cooling = RefreshSlot(SkillSlot1, DMGameplayTags::Skill_Invisibility);

    const bool bSlot2Cooling = RefreshSlot(SkillSlot2, DMGameplayTags::Skill_FootstepAmplify);

    const bool bSlot3Cooling = RefreshSlot(SkillSlot3, DMGameplayTags::Skill_VisionDistort);

    if (!bSlot1Cooling && !bSlot2Cooling && !bSlot3Cooling)
    {
        StopDisplayTimer();
        return;
    }

    UWorld* World = GetWorld();

    if (!IsValid(World))
    {
        return;
    }

    FTimerManager& TimerManager = World->GetTimerManager();

    // 이미 실행 중이면 새 타이머를 만들지 않는다.
    if (!TimerManager.IsTimerActive(DisplayTimerHandle))
    {
        TimerManager.SetTimer(DisplayTimerHandle, this, &ThisClass::RefreshCooldowns, SkillHUDRefreshInterval, true);
    }
}

#pragma endregion

#pragma region Presentation Timer

void UDMSkillBarWidget::StopDisplayTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DisplayTimerHandle);
    }

    DisplayTimerHandle.Invalidate();
}

#pragma endregion

#pragma region Lifecycle

void UDMSkillBarWidget::NativeDestruct()
{
    // 화면 종료 시 모든 타이머와 외부 Delegate를 해제한다.
    SetPresentationActive(false);
    SetPlayerState(nullptr);
    StopDisplayTimer();

    Super::NativeDestruct();
}

#pragma endregion
