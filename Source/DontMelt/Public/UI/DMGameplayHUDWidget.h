
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"

#include "DMGameplayHUDWidget.generated.h"

class UTextBlock;
class ADMMatchGameState;
class UDMCombatComponent;
class UDMCombatHUDWidget;
class UDMRelicHUDWidget;
class ADMMatchPlayerController;
class ADMPlayerCharacter;
class ADMMatchPlayerState;
class UDMSkillBarWidget;

/**
 * Match Phase and remaining-time presentation.
 *
 * Does not own gameplay time or match state.
 * Uses the replicated GameState as its read model.
 */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMGameplayHUDWidget : public UUserWidget
{
    GENERATED_BODY()

public:
#pragma region Data Source

    void SetMatchGameState(ADMMatchGameState* InGameState);

    /** 현재 Pawn의 전투 쿨다운 원본을 자식 HUD에 전달한다. */
    void SetCombatComponent(UDMCombatComponent* InCombatComponent);

    /** 현재 소유 PlayerState의 스킬 데이터를 SkillBar에 전달한다. */
    void SetSkillPlayerState(ADMMatchPlayerState* InPlayerState);

    /**
     * 현재 로컬 Controller와 Pawn을 기존 유물 HUD에 전달한다.
     * Pawn이 없으면 이전 Delegate 및 Timer 연결을 정리한다.
     */
    void SetRelicHUDSources(ADMMatchPlayerController* InController, ADMPlayerCharacter* InCharacter);

    void SetPresentationActive(bool bActive);

    void RefreshFromMatchState();

#pragma endregion

protected:
    virtual void NativeDestruct() override;

#pragma region Widget References

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_MatchPhase = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UTextBlock> TXT_RemainingTime = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMCombatHUDWidget> CombatHUD = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMSkillBarWidget> SkillBar = nullptr;

    /** 기존 유물 HUD를 재사용하는 단일 자식 Widget */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMRelicHUDWidget> RelicHUD = nullptr;

#pragma endregion

private:
#pragma region Presentation Timer

    void StopDisplayTimer();

    TWeakObjectPtr<ADMMatchGameState> BoundGameState;

    FTimerHandle DisplayTimerHandle;

    // Local presentation lifecycle only.
    bool bPresentationActive = false;

#pragma endregion
};
