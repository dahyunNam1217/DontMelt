
#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "TimerManager.h"
#include "GameplayTagContainer.h"

#include "DMGameRootWidget.generated.h"

class UCanvasPanel;
class UBorder;
class UDMInteractionComponent;
class UDMInteractionPromptWidget;
class UDMChestPanelWidget;
class UDMSkillTargetPanelWidget;
class UDMGameplayHUDWidget;
class UDMQuestHUDWidget;
class UDMRandomEventHUDWidget;
class UDMStatusHUDWidget;
class UDMPoseHUDWidget;
class UDMIngameMenuWidget;
class UDMAnnouncementWidget;
class UDMRelicRevealHUDWidget;
class UDMOvertimeIntroWidget;
class UDMSpectatorHUDWidget;
class UDMMatchResultWidget;
class ADMMatchGameState;
class ADMMatchPlayerState;
class ADMMatchPlayerController;
class ADMPlayerCharacter;
class UDMVoiceStatusWidget;

struct FDMMatchPhaseState;
enum class EDMPlayerLifeState : uint8;

/**
 * Match UI의 최상위 Presentation Root.
 *
 * Gameplay 데이터는 소유하지 않는다.
 * 레이어 구조와 로컬 Visibility만 관리한다.
 */
UCLASS(Abstract, Blueprintable)
class DONTMELT_API UDMGameRootWidget : public UUserWidget
{
    GENERATED_BODY()

public:
#pragma region UI Data Sources

    void RebindDataSources(
        ADMMatchPlayerController* InController,
        ADMMatchGameState* InGameState,
        ADMMatchPlayerState* InPlayerState,
        ADMPlayerCharacter* InPawn
    );

    void ShutdownDataSources();

    // 필수 BindWidget이 모두 연결되었는지 컨트롤러 생성 직후 확인한다.
    bool HasRequiredBindings() const;

#pragma endregion

#pragma region Ingame Menu

    UDMIngameMenuWidget* GetIngameMenuWidget() const
    {
        return IngameMenu.Get();
    }

    // Controller의 로컬 메뉴 상태를 UI에 반영한다.
    void RefreshIngameMenuPresentation();

#pragma endregion

protected:
    virtual void NativeConstruct() override;

    virtual void NativeDestruct() override;

#pragma region UI Layers

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCanvasPanel> RootCanvas = nullptr;

    // 생존·관전과 무관하게 표시할 로컬 마이크 상태다.
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMVoiceStatusWidget> VoiceStatus = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCanvasPanel> GameplayLayer = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMGameplayHUDWidget> GameplayHUD = nullptr;

    /** 평상시 F 상호작용 입력 안내 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMInteractionPromptWidget> InteractionPrompt = nullptr;

    /** 로컬 플레이어의 상태이상 표시 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMStatusHUDWidget> StatusHUD = nullptr;

    /** 로컬 플레이어의 자세 선택 상태를 표시한다. */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMPoseHUDWidget> PoseHUD = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCanvasPanel> ActivityLayer = nullptr;

    /** 퀘스트 발생 중에만 표시되는 활동 정보 HUD */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMQuestHUDWidget> QuestHUD = nullptr;

    /** 랜덤 이벤트 예고 및 진행 상태 HUD */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMRandomEventHUDWidget> RandomEventHUD = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCanvasPanel> ModalLayer = nullptr;

    /** 상자 및 대상 선택 모달이 공유할 검정 배경 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UBorder> BRD_DimBackground = nullptr;

    /** 상자 확인 전용 모달 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMChestPanelWidget> ChestPanel = nullptr;

    /** 대상 지정 스킬을 위한 공용 모달 패널 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMSkillTargetPanelWidget> SkillTargetPanel = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCanvasPanel> AnnouncementLayer = nullptr;

    /** 퀘스트·이벤트·피해 경고의 공통 알림 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMAnnouncementWidget> AnnouncementHUD = nullptr;

    /** 정규 경기 종료 60초 전 3초 유물 위치 공개 지도. */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMRelicRevealHUDWidget> RelicRevealHUD = nullptr;

    /** 연장전 진입 시 공개되는 진영별 등록 유물 개수 UI */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMOvertimeIntroWidget> OvertimeIntroWidget = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCanvasPanel> SpectatorLayer = nullptr;

    /** 로컬 사망·관전 단계와 부활 대기시간을 표시한다. */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMSpectatorHUDWidget> SpectatorHUD = nullptr;

    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UCanvasPanel> MenuLayer = nullptr;

    /** Esc로 열리는 로컬 인게임 메뉴 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMIngameMenuWidget> IngameMenu = nullptr;

    /** 서버 확정 경기 결과를 모든 참가자에게 보여주는 화면 */
    UPROPERTY(meta = (BindWidget))
    TObjectPtr<UDMMatchResultWidget> MatchResultWidget = nullptr;

#pragma endregion

private:
#pragma region UI Data Binding

    void RefreshRootVisibility();

    /** 현재 승인된 상자 상태를 모달에 반영한다. */
    void RefreshChestModal(bool bAllowGameplayUI);

    /** 대상 선택 상태에 따라 공용 모달을 표시한다. */
    void RefreshSkillTargetingModal(bool bAllowGameplayUI);

    /** 상호작용 안내의 로컬 갱신을 시작하거나 중지한다. */
    void UpdateInteractionPromptPolling(bool bEnable);

    /** 현재 로컬 상호작용 후보를 읽어 안내를 갱신한다. */
    void RefreshInteractionPrompt();

    /** 안내용 로컬 Timer를 정리한다. */
    void StopInteractionPromptPolling();

    /** 기존 Controller의 상자 열기·닫기 알림을 받는다. */
    void HandleChestInteractionChanged(bool bOpen, int32 BaseSlot, int32 RegisteredRelicCount);

    /** 기존 Controller의 스킬 대상 선택 상태 변경 알림 */
    void HandleSkillTargetingStateChanged(bool bTargeting, FGameplayTag SkillTag);

    void HandlePhaseStateChanged(ADMMatchGameState* Source, FDMMatchPhaseState NewState);

    void HandleLifeStateChanged(ADMMatchPlayerState* Source, EDMPlayerLifeState NewState);

    /** 유물 운반 상태 변경 시 열린 상자의 안내를 갱신한다. */
    void HandleCarryStateChanged(ADMMatchPlayerState* Source, bool bIsCarryingRelic);

    void HandleSpectatorRestrictionChanged(bool bRestricted);

    TWeakObjectPtr<ADMMatchPlayerController> BoundController;

    TWeakObjectPtr<ADMMatchGameState> BoundGameState;

    TWeakObjectPtr<ADMMatchPlayerState> BoundPlayerState;

    TWeakObjectPtr<ADMPlayerCharacter> BoundPawn;

    FTimerHandle InteractionPromptTimerHandle;

#pragma endregion
};
