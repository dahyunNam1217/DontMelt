
#include "UI/DMCombatHUDWidget.h"

#include "Characters/Components/DMCombatComponent.h"

#include "Components/TextBlock.h"
#include "Components/ProgressBar.h"
#include "Engine/World.h"

namespace
{
    // 쿨다운이 진행 중일 때만 약 10Hz로 표시를 갱신한다.
    constexpr float CombatHUDRefreshInterval = 0.1f;

    // 표시값과 진행 바에 사용할 최소 오차 범위.
    constexpr float CombatCooldownEpsilon = 0.01f;
}

#pragma region Data Source

void UDMCombatHUDWidget::SetCombatComponent(UDMCombatComponent* InCombatComponent)
{
    if (BoundCombatComponent.Get() != InCombatComponent)
    {
        // 이전 Pawn의 CombatComponent 연결을 제거한다.
        if (UDMCombatComponent* OldCombat = BoundCombatComponent.Get())
        {
            OldCombat->OnCombatCooldownChanged().RemoveAll(this);
        }

        BoundCombatComponent = InCombatComponent;

        // 새 Pawn의 CombatComponent 변경 알림을 구독한다.
        if (IsValid(InCombatComponent))
        {
            InCombatComponent->OnCombatCooldownChanged().AddUObject(this, &ThisClass::HandleCooldownChanged);
        }
    }

    // 최초 구독 시점에도 현재 원본 값을 즉시 반영한다.
    RefreshCooldowns();
}

void UDMCombatHUDWidget::SetPresentationActive(const bool bActive)
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

    RefreshCooldowns();
}

#pragma endregion

#pragma region Cooldown Presentation

void UDMCombatHUDWidget::HandleCooldownChanged()
{
    RefreshCooldowns();
}

void UDMCombatHUDWidget::RefreshCooldowns()
{
    // 표시되지 않는 HUD는 쿨다운 숫자를 갱신하지 않는다.
    if (!bPresentationActive)
    {
        StopDisplayTimer();
        return;
    }

    if (!IsValid(TXT_AttackCooldown) || !IsValid(PB_AttackCooldown) || !IsValid(TXT_DefenseCooldown)
        || !IsValid(PB_DefenseCooldown))
    {
        StopDisplayTimer();
        return;
    }

    const UDMCombatComponent* Combat = BoundCombatComponent.Get();

    if (!IsValid(Combat))
    {
        TXT_AttackCooldown->SetText(FText::FromString(TEXT("--")));

        TXT_DefenseCooldown->SetText(FText::FromString(TEXT("--")));

        PB_AttackCooldown->SetPercent(0.0f);
        PB_DefenseCooldown->SetPercent(0.0f);

        StopDisplayTimer();
        return;
    }

    // 쿨다운 계산은 기존 CombatComponent Query를 사용한다.
    const float AttackRemaining = Combat->GetRemainingAttackCooldown();

    const float DefenseRemaining = Combat->GetRemainingDefenseCooldown();

    const float AttackDuration = Combat->GetAttackCooldownDuration();

    const float DefenseDuration = Combat->GetDefenseCooldownDuration();

    const bool bAttackCoolingDown = AttackRemaining > CombatCooldownEpsilon;

    const bool bDefenseCoolingDown = DefenseRemaining > CombatCooldownEpsilon;

    // 공격 쿨다운 표시.
    if (bAttackCoolingDown)
    {
        const float DisplayTime = FMath::CeilToInt(AttackRemaining * 10.0f) * 0.1f;

        TXT_AttackCooldown->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), DisplayTime)));
    }
    else
    {
        TXT_AttackCooldown->SetText(NSLOCTEXT("DMCombatHUD", "AttackReady", "준비"));
    }

    // 방어 쿨다운 표시.
    if (bDefenseCoolingDown)
    {
        const float DisplayTime = FMath::CeilToInt(DefenseRemaining * 10.0f) * 0.1f;

        TXT_DefenseCooldown->SetText(FText::FromString(FString::Printf(TEXT("%.1f"), DisplayTime)));
    }
    else
    {
        TXT_DefenseCooldown->SetText(NSLOCTEXT("DMCombatHUD", "DefenseReady", "준비"));
    }

    // 1.0은 사용 준비 완료, 0.0은 방금 사용한 상태다.
    const float AttackPercent = bAttackCoolingDown
        ? FMath::Clamp(1.0f - AttackRemaining / FMath::Max(AttackDuration, CombatCooldownEpsilon), 0.0f, 1.0f)
        : 1.0f;

    const float DefensePercent = bDefenseCoolingDown
        ? FMath::Clamp(1.0f - DefenseRemaining / FMath::Max(DefenseDuration, CombatCooldownEpsilon), 0.0f, 1.0f)
        : 1.0f;

    PB_AttackCooldown->SetPercent(AttackPercent);

    PB_DefenseCooldown->SetPercent(DefensePercent);

    // 쿨다운이 모두 끝났으면 로컬 표시 Timer도 종료한다.
    if (!bAttackCoolingDown && !bDefenseCoolingDown)
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

    // 변경 알림이 중복되어도 Timer는 하나만 유지한다.
    if (!TimerManager.IsTimerActive(DisplayTimerHandle))
    {
        TimerManager.SetTimer(DisplayTimerHandle, this, &ThisClass::RefreshCooldowns, CombatHUDRefreshInterval, true);
    }
}

void UDMCombatHUDWidget::StopDisplayTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DisplayTimerHandle);
    }

    DisplayTimerHandle.Invalidate();
}

#pragma endregion

#pragma region Lifecycle

void UDMCombatHUDWidget::NativeDestruct()
{
    SetPresentationActive(false);

    SetCombatComponent(nullptr);

    StopDisplayTimer();

    Super::NativeDestruct();
}

#pragma endregion
