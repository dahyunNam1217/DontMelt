
#include "UI/DMGameRootWidget.h"

#include "UI/DMGameplayHUDWidget.h"
#include "UI/DMQuestHUDWidget.h"
#include "UI/DMRandomEventHUDWidget.h"
#include "UI/DMStatusHUDWidget.h"
#include "UI/DMPoseHUDWidget.h"
#include "UI/DMAnnouncementWidget.h"
#include "UI/DMRelicRevealHUDWidget.h"
#include "UI/DMOvertimeIntroWidget.h"
#include "UI/DMSpectatorHUDWidget.h"
#include "UI/DMMatchResultWidget.h"
#include "UI/DMInteractionPromptWidget.h"
#include "UI/DMChestPanelWidget.h"
#include "UI/DMSkillTargetPanelWidget.h"
#include "UI/DMIngameMenuWidget.h"
#include "Framework/Match/DMMatchPlayerController.h"
#include "Framework/Match/DMMatchGameState.h"
#include "Framework/Match/DMMatchPlayerState.h"
#include "Characters/DMPlayerCharacter.h"
#include "Characters/Components/DMCombatComponent.h"
#include "Characters/Components/DMInteractionComponent.h"
#include "Characters/Components/DMCharacterStateComponent.h"

#include "Components/CanvasPanel.h"
#include "Components/Border.h"
#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMGameRootWidget, Log, All);

namespace
{
    // 실제 플레이 중에만 약 5Hz로 상호작용 후보를 확인한다.
    constexpr float InteractionPromptRefreshInterval = 0.2f;
}

bool UDMGameRootWidget::HasRequiredBindings() const
{
    return RootCanvas && GameplayLayer && GameplayHUD && StatusHUD && PoseHUD && ActivityLayer && QuestHUD
        && RandomEventHUD && ModalLayer && AnnouncementLayer && AnnouncementHUD && RelicRevealHUD
        && OvertimeIntroWidget && SpectatorLayer && SpectatorHUD && MenuLayer && MatchResultWidget
        && InteractionPrompt && BRD_DimBackground && ChestPanel && SkillTargetPanel && IngameMenu && VoiceStatus;
}

void UDMGameRootWidget::NativeConstruct()
{
    Super::NativeConstruct();

    // Root 자체는 마우스 입력을 가로채지 않는다.
    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    // WBP의 필수 레이어 연결 검증.

    if (!ensureMsgf(HasRequiredBindings(), TEXT("DMGameRootWidget: required WBP BindWidget missing.")))
    {
        return;
    }
    RootCanvas->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    GameplayLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    ActivityLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    ModalLayer->SetVisibility(ESlateVisibility::Collapsed);

    AnnouncementLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    SpectatorLayer->SetVisibility(ESlateVisibility::Collapsed);

    // 로컬 관전이 시작되기 전에는 관전 패널을 숨긴다.
    SpectatorHUD->SetPresentationActive(false);

    MenuLayer->SetVisibility(ESlateVisibility::Collapsed);

    // 매치 시작 시 Esc 메뉴는 표시하지 않는다.
    IngameMenu->SetVisibility(ESlateVisibility::Collapsed);

    // 서버 결과가 확인되기 전에는 결과 화면을 표시하지 않는다.
    MatchResultWidget->SetPresentationActive(false);

    GameplayHUD->SetPresentationActive(false);

    // 아직 로컬 Gameplay 상태가 확인되지 않았으므로 숨긴다.
    QuestHUD->SetPresentationActive(false);

    // 이벤트 공개 상태 확인 전에는 HUD를 숨긴다.
    RandomEventHUD->SetPresentationActive(false);

    // 초기 데이터 연결 전에는 상태 및 알림 HUD를 숨긴다.
    StatusHUD->SetPresentationActive(false);
    PoseHUD->SetPresentationActive(false);
    AnnouncementHUD->SetPresentationActive(false);

    RelicRevealHUD->SetPresentationActive(false);

    // 연장전 진입 데이터 연결 전에는 패널을 숨긴다.
    OvertimeIntroWidget->SetPresentationActive(false);

    // 시작 시 상호작용 안내와 상자 모달을 숨긴다.
    InteractionPrompt->SetCandidate(nullptr);

    BRD_DimBackground->SetVisibility(ESlateVisibility::Collapsed);

    ChestPanel->SetChestOpen(false);

    // 대상 선택 패널은 시작 시 숨긴 상태를 유지한다.
    SkillTargetPanel->HideTargetingSkill();
}

void UDMGameRootWidget::RebindDataSources(
    ADMMatchPlayerController* InController,
    ADMMatchGameState* InGameState,
    ADMMatchPlayerState* InPlayerState,
    ADMPlayerCharacter* InPawn
)
{
    if (!IsValid(InController))
    {
        ShutdownDataSources();
        RefreshRootVisibility();
        return;
    }

    const bool bSourcesChanged = BoundController.Get() != InController || BoundGameState.Get() != InGameState
        || BoundPlayerState.Get() != InPlayerState || BoundPawn.Get() != InPawn;

    // The same sources are already bound.

    if (!bSourcesChanged)
    {
        if (IsValid(GameplayHUD))
        {
            GameplayHUD->SetMatchGameState(InGameState);

            // 동일 PlayerState여도 스킬 데이터 준비 여부를 재확인한다.
            GameplayHUD->SetSkillPlayerState(InPlayerState);
        }

        // 랜덤 이벤트와 본인의 입력 반전 상태를 함께 연결한다.
        if (IsValid(RandomEventHUD))
        {
            RandomEventHUD->SetGameState(InGameState);
        }

        if (IsValid(RelicRevealHUD))
        {
            RelicRevealHUD->SetGameState(InGameState);
        }

        // 이미 같은 GameState여도 Intro 공개 정보를 다시 확인한다.
        if (IsValid(OvertimeIntroWidget))
        {
            OvertimeIntroWidget->SetGameState(InGameState);
        }

        // 동일 Controller라도 현재 관전 단계 연결을 재확인한다.
        if (IsValid(SpectatorHUD))
        {
            SpectatorHUD->SetController(InController);
        }

        // 동일한 원본이어도 결과·내 진영의 현재 상태를 확인한다.
        if (IsValid(MatchResultWidget))
        {
            MatchResultWidget->SetDataSources(InGameState, InPlayerState);
        }

        // 원본 Actor가 같아도 퀘스트 데이터가 준비되었는지 다시 확인한다.
        if (IsValid(QuestHUD))
        {
            QuestHUD->SetDataSources(InGameState, InPlayerState);
        }

        // Pawn은 같아도 Component가 준비된 시점을 다시 확인한다.
        if (IsValid(ChestPanel))
        {
            UDMInteractionComponent* Interaction =
                IsValid(InPawn) ? InPawn->FindComponentByClass<UDMInteractionComponent>() : nullptr;

            ChestPanel->SetInteractionComponent(Interaction);
        }

        // 같은 Pawn이어도 현재 상태 Component 연결을 재확인한다.
        if (IsValid(StatusHUD))
        {
            UDMCharacterStateComponent* StateComponent =
                IsValid(InPawn) ? InPawn->GetCharacterStateComponent() : nullptr;

            StatusHUD->SetDataSources(StateComponent, InController);
        }

        // 같은 Pawn이어도 자세 선택 UI의 현재 연결을 재확인한다.
        if (IsValid(PoseHUD))
        {
            PoseHUD->SetPlayerCharacter(InPawn);
        }

        // 개인 QuestProgress가 나중에 준비된 경우도 처리한다.
        if (IsValid(AnnouncementHUD))
        {
            AnnouncementHUD->SetDataSources(InController, InGameState, InPlayerState);
        }

        RefreshRootVisibility();

        return;
    }

    // Remove subscriptions from previous sources first.
    ShutdownDataSources();

    BoundController = InController;
    BoundGameState = InGameState;
    BoundPlayerState = InPlayerState;
    BoundPawn = InPawn;

    // 경기 결과는 GameState, 개인 승패 비교는 PlayerState를 사용한다.
    if (IsValid(MatchResultWidget))
    {
        MatchResultWidget->SetDataSources(InGameState, InPlayerState);
    }

    // 관전 카메라 단계의 원본은 로컬 PlayerController다.
    if (IsValid(SpectatorHUD))
    {
        SpectatorHUD->SetController(InController);
    }

    // 현재 Pawn의 상태이상 원본과 로컬 Controller를 전달한다.
    if (IsValid(StatusHUD))
    {
        UDMCharacterStateComponent* StateComponent = IsValid(InPawn) ? InPawn->GetCharacterStateComponent() : nullptr;

        StatusHUD->SetDataSources(StateComponent, InController);
    }

    if (IsValid(RelicRevealHUD))
    {
        RelicRevealHUD->SetGameState(InGameState);
    }

    // 진영별 공개 UI는 GameState의 공개 정보만 구독한다.
    if (IsValid(OvertimeIntroWidget))
    {
        OvertimeIntroWidget->SetGameState(InGameState);
    }

    // 현재 로컬 Pawn의 Pose 입력 모드 및 PoseComponent에 연결한다.
    if (IsValid(PoseHUD))
    {
        PoseHUD->SetPlayerCharacter(InPawn);
    }

    // 알림 Widget은 변경 이벤트를 직접 구독한다.
    if (IsValid(AnnouncementHUD))
    {
        AnnouncementHUD->SetDataSources(InController, InGameState, InPlayerState);
    }

    // 전체 퀘스트 정보와 본인 진행 정보 원본을 전달한다.
    if (IsValid(QuestHUD))
    {
        QuestHUD->SetDataSources(InGameState, InPlayerState);
    }

    // 서버 이벤트 정보와 현재 Pawn의 입력 반전 상태를 연결한다.
    if (IsValid(RandomEventHUD))
    {
        RandomEventHUD->SetGameState(InGameState);

        UDMCharacterStateComponent* StateComponent = IsValid(InPawn) ? InPawn->GetCharacterStateComponent() : nullptr;
    }

    // 상자 모달은 현재 Pawn의 상호작용 원본만 구독한다.
    if (IsValid(ChestPanel))
    {
        UDMInteractionComponent* Interaction =
            IsValid(InPawn) ? InPawn->FindComponentByClass<UDMInteractionComponent>() : nullptr;

        ChestPanel->SetInteractionComponent(Interaction);
    }

    if (IsValid(GameplayHUD))
    {
        GameplayHUD->SetMatchGameState(InGameState);

        // 현재 Pawn의 CombatComponent를 조회해 하위 HUD에 전달한다.
        UDMCombatComponent* CombatComponent =
            IsValid(InPawn) ? InPawn->FindComponentByClass<UDMCombatComponent>() : nullptr;

        GameplayHUD->SetCombatComponent(CombatComponent);

        // 스킬 쿨다운은 Pawn이 아닌 PlayerState에서 가져온다.
        GameplayHUD->SetSkillPlayerState(InPlayerState);

        // 기존 유물 HUD를 현재 Controller와 Pawn에 다시 연결한다.
        GameplayHUD->SetRelicHUDSources(InController, InPawn);
    }

    InController->OnSpectatorInformationRestrictionChanged().AddUObject(
        this, &ThisClass::HandleSpectatorRestrictionChanged
    );

    if (IsValid(InGameState))
    {
        InGameState->OnPhaseStateChanged().AddUObject(this, &ThisClass::HandlePhaseStateChanged);
    }

    // 상자 열기·닫기 알림을 Root가 직접 구독한다.
    InController->OnChestInteractionChanged().AddUObject(this, &ThisClass::HandleChestInteractionChanged);

    // 기존 Controller의 대상 선택 시작·종료 알림을 구독한다.
    InController->OnSkillTargetingStateChanged().AddUObject(this, &ThisClass::HandleSkillTargetingStateChanged);

    if (IsValid(InPlayerState))
    {
        InPlayerState->OnLifeStateChanged().AddUObject(this, &ThisClass::HandleLifeStateChanged);

        // 상자 내부 등록 안내를 현재 유물 운반 상태와 동기화한다.
        InPlayerState->OnCarryStateChanged().AddUObject(this, &ThisClass::HandleCarryStateChanged);
    }

    UE_LOG(
        LogDMGameRootWidget,
        Log,
        TEXT("UI sources rebound. GS=%s, PS=%s, Pawn=%s"),
        *GetNameSafe(InGameState),
        *GetNameSafe(InPlayerState),
        *GetNameSafe(InPawn)
    );

    // Do not wait for the next delegate event.
    RefreshRootVisibility();
}

void UDMGameRootWidget::ShutdownDataSources()
{
    // 기존 화면이 종료되거나 Pawn이 변경되면 안내를 정리한다.
    StopInteractionPromptPolling();

    // 기존 MatchResult 및 개인 BaseSlot 변경 구독을 정리한다.
    if (IsValid(MatchResultWidget))
    {
        MatchResultWidget->SetPresentationActive(false);
        MatchResultWidget->SetDataSources(nullptr, nullptr);
    }

    if (IsValid(InteractionPrompt))
    {
        InteractionPrompt->SetCandidate(nullptr);
    }

    if (IsValid(ChestPanel))
    {
        ChestPanel->SetChestOpen(false);
        ChestPanel->SetInteractionComponent(nullptr);
    }

    // 이전 Controller의 스킬 선택 모달 표시를 정리한다.
    if (IsValid(SkillTargetPanel))
    {
        SkillTargetPanel->HideTargetingSkill();
    }

    if (IsValid(BRD_DimBackground))
    {
        BRD_DimBackground->SetVisibility(ESlateVisibility::Collapsed);
    }

    if (IsValid(ModalLayer))
    {
        ModalLayer->SetVisibility(ESlateVisibility::Collapsed);
    }

    // Root 또는 PlayerState 교체 시 QuestHUD 구독과 타이머를 해제한다.
    if (IsValid(QuestHUD))
    {
        QuestHUD->SetPresentationActive(false);
        QuestHUD->SetDataSources(nullptr, nullptr);
    }

    // 이전 Pawn과 매치의 랜덤 이벤트 구독을 모두 해제한다.

    if (IsValid(RandomEventHUD))
    {
        // 랜덤 이벤트 HUD의 표시와 GameState 구독만 정리한다.
        RandomEventHUD->SetPresentationActive(false);
        RandomEventHUD->SetGameState(nullptr);
    }

    // 이전 매치의 공개 마커와 표시 타이머를 정리한다.
    // GameState 변경 알림 Delegate도 함께 해제한다.
    if (IsValid(RelicRevealHUD))
    {
        RelicRevealHUD->SetPresentationActive(false);
        RelicRevealHUD->SetGameState(nullptr);
    }

    // 이전 연장전 공개 정보를 정리하고 Delegate를 해제한다.
    if (IsValid(OvertimeIntroWidget))
    {
        OvertimeIntroWidget->SetPresentationActive(false);
        OvertimeIntroWidget->SetGameState(nullptr);
    }

    // 이전 관전 HUD의 갱신 Timer와 Controller 구독을 정리한다.
    if (IsValid(SpectatorHUD))
    {
        SpectatorHUD->SetPresentationActive(false);
        SpectatorHUD->SetController(nullptr);
    }

    // 이전 Pawn의 상태이상 알림 연결을 해제한다.
    if (IsValid(StatusHUD))
    {
        StatusHUD->SetPresentationActive(false);
        StatusHUD->SetDataSources(nullptr, nullptr);
    }

    // 이전 Pawn의 자세 입력·복제 표시 Delegate를 해제한다.
    if (IsValid(PoseHUD))
    {
        PoseHUD->SetPresentationActive(false);
        PoseHUD->SetPlayerCharacter(nullptr);
    }

    // 이전 Controller와 퀘스트·이벤트 알림 구독을 해제한다.
    if (IsValid(AnnouncementHUD))
    {
        AnnouncementHUD->SetPresentationActive(false);
        AnnouncementHUD->SetDataSources(nullptr, nullptr, nullptr);
    }

    if (IsValid(GameplayHUD))
    {
        GameplayHUD->SetPresentationActive(false);

        GameplayHUD->SetMatchGameState(nullptr);

        // Pawn 교체 시 기존 CombatComponent Delegate 연결을 해제한다.
        GameplayHUD->SetCombatComponent(nullptr);

        // 이전 PlayerState의 스킬 변경·운반 변경 알림을 해제한다.
        GameplayHUD->SetSkillPlayerState(nullptr);

        // 이전 Pawn의 상호작용·유물 운반 Delegate를 해제한다.
        GameplayHUD->SetRelicHUDSources(nullptr, nullptr);
    }

    if (ADMMatchPlayerController* Controller = BoundController.Get())
    {
        Controller->OnSpectatorInformationRestrictionChanged().RemoveAll(this);

        // 상자 모달의 이전 Controller 알림도 해제한다.
        Controller->OnChestInteractionChanged().RemoveAll(this);

        // 이전 Controller의 대상 선택 알림을 해제한다.
        Controller->OnSkillTargetingStateChanged().RemoveAll(this);
    }

    if (ADMMatchGameState* GameState = BoundGameState.Get())
    {
        GameState->OnPhaseStateChanged().RemoveAll(this);
    }

    if (ADMMatchPlayerState* PlayerState = BoundPlayerState.Get())
    {
        PlayerState->OnLifeStateChanged().RemoveAll(this);

        // 이전 PlayerState의 유물 운반 알림도 해제한다.
        PlayerState->OnCarryStateChanged().RemoveAll(this);
    }

    BoundController.Reset();
    BoundGameState.Reset();
    BoundPlayerState.Reset();
    BoundPawn.Reset();
}

void UDMGameRootWidget::RefreshIngameMenuPresentation()
{
    RefreshRootVisibility();
}

void UDMGameRootWidget::RefreshRootVisibility()
{
    if (!IsValid(GameplayLayer) || !IsValid(ActivityLayer) || !IsValid(ModalLayer) || !IsValid(AnnouncementLayer)
        || !IsValid(SpectatorLayer) || !IsValid(MenuLayer))
    {
        return;
    }

    ADMMatchPlayerController* Controller = BoundController.Get();

    const ADMMatchGameState* GameState = BoundGameState.Get();

    const ADMMatchPlayerState* PlayerState = BoundPlayerState.Get();

    const ADMPlayerCharacter* Pawn = BoundPawn.Get();

    bool bShowGameplay = false;

    if (IsValid(Controller) && IsValid(GameState) && IsValid(PlayerState) && IsValid(Pawn)
        && Controller->GetPawn() == Pawn)
    {
        const EDMMatchPhase Phase = GameState->GetCurrentPhase();

        const bool bMatchEnded = Phase == EDMMatchPhase::Finished || Phase == EDMMatchPhase::Aborted;

        const bool bAlive = PlayerState->GetLifeState() == EDMPlayerLifeState::Alive;

        const bool bRestricted = Controller->IsSpectatorInformationRestricted();

        bShowGameplay = !bMatchEnded && bAlive && !bRestricted;
    }

    const ESlateVisibility GameplayVisibility =
        bShowGameplay ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed;

    GameplayLayer->SetVisibility(GameplayVisibility);

    if (IsValid(GameplayHUD))
    {
        GameplayHUD->SetPresentationActive(bShowGameplay);
    }

    // 상태이상 표시는 본인이 생존하여 Gameplay 중일 때만 활성화한다.
    if (IsValid(StatusHUD))
    {
        StatusHUD->SetPresentationActive(bShowGameplay);
    }

    // 생존 중인 자신의 Gameplay 화면에서만 자세 선택을 표시한다.
    if (IsValid(PoseHUD))
    {
        PoseHUD->SetPresentationActive(bShowGameplay);
    }

    ActivityLayer->SetVisibility(GameplayVisibility);

    // 퀘스트 패널은 정상 Gameplay에서만 갱신한다.
    if (IsValid(QuestHUD))
    {
        QuestHUD->SetPresentationActive(bShowGameplay);
    }

    // 랜덤 이벤트는 정규 경기 중 생존 플레이어에게만 표시한다.
    if (IsValid(RandomEventHUD))
    {
        const bool bAllowRandomEventHUD =
            bShowGameplay && IsValid(GameState) && GameState->GetCurrentPhase() == EDMMatchPhase::Playing;

        RandomEventHUD->SetPresentationActive(bAllowRandomEventHUD);
    }

    // 관전 HUD는 현재 Pawn 유효성과 별개로 표시한다.
    // 사망 직후 Pawn 제거와 리스폰 교체 중에도 Controller는 유지된다.
    const bool bMatchTerminal = IsValid(GameState)
        && (GameState->GetCurrentPhase() == EDMMatchPhase::Finished
            || GameState->GetCurrentPhase() == EDMMatchPhase::Aborted);

    // 서버의 종료 판정이 인게임 메뉴보다 우선한다.
    if (bMatchTerminal && IsValid(Controller))
    {
        // Root를 이미 갱신 중이므로 재귀 갱신은 하지 않는다.
        if (Controller->IsIngameMenuOpen())
        {
            Controller->CloseIngameMenu(false);
        }

        Controller->PrepareLocalPresentationForMatchResult();
    }

    // Esc 메뉴는 오직 로컬 Controller 상태를 읽어 표시한다.
    const bool bShowIngameMenu = !bMatchTerminal && IsValid(Controller) && Controller->IsIngameMenuOpen();

    if (IsValid(IngameMenu.Get()))
    {
        IngameMenu->SetVisibility(bShowIngameMenu ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);
    }

    // 결과 화면 또는 인게임 메뉴 중 하나가 필요할 때 표시한다.
    MenuLayer->SetVisibility(
        bMatchTerminal || bShowIngameMenu ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed
    );

    if (IsValid(MatchResultWidget))
    {
        MatchResultWidget->SetPresentationActive(bMatchTerminal);
    }

    // 서버가 확정한 생명주기와 별도로,
    // 로컬 관전 정보 제한은 Handoff 완료까지 유지된다.
    const bool bShowSpectator =
        IsValid(Controller) && IsValid(GameState) && Controller->IsSpectatorInformationRestricted() && !bMatchTerminal;

    SpectatorLayer->SetVisibility(
        bShowSpectator ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed
    );

    if (IsValid(SpectatorHUD))
    {
        SpectatorHUD->SetPresentationActive(bShowSpectator);
    }

    AnnouncementLayer->SetVisibility(GameplayVisibility);

    // 정규 경기 종료 60초 전의 유물 위치 공개 HUD.
    // 생존 중이고 정보 공개가 허용된 Playing 단계에서만 활성화한다.
    if (IsValid(RelicRevealHUD))
    {
        const bool bAllowRelicReveal =
            bShowGameplay && IsValid(GameState) && GameState->GetCurrentPhase() == EDMMatchPhase::Playing;

        RelicRevealHUD->SetPresentationActive(bAllowRelicReveal);
    }

    // 연장전 Intro의 진영별 공개 UI는 생존 플레이어에게만 표시한다.
    if (IsValid(OvertimeIntroWidget))
    {
        const bool bAllowOvertimeIntro =
            bShowGameplay && IsValid(GameState) && GameState->GetCurrentPhase() == EDMMatchPhase::OvertimeIntro;

        OvertimeIntroWidget->SetPresentationActive(bAllowOvertimeIntro);
    }

    // 공통 알림 역시 기존 Root의 생존·관전 제한을 따른다.
    if (IsValid(AnnouncementHUD))
    {
        AnnouncementHUD->SetPresentationActive(bShowGameplay);
    }

    // 상자 상태와 생존·관전 제한을 함께 확인하여 모달을 갱신한다.
    RefreshChestModal(bShowGameplay);

    // 동일한 검정 배경 위에서 대상 선택 모달도 갱신한다.
    RefreshSkillTargetingModal(bShowGameplay);

    const bool bCanShowInteractionPrompt = bShowGameplay && IsValid(GameState)
        && (GameState->GetCurrentPhase() == EDMMatchPhase::Playing
            || GameState->GetCurrentPhase() == EDMMatchPhase::Overtime)
        && IsValid(Controller) && !Controller->IsChestInteractionOpen() && !Controller->IsSkillTargetingActive()
        && !Controller->IsIngameMenuOpen();

    // 모달을 열었거나 플레이할 수 없으면 후보 탐색을 중단한다.
    UpdateInteractionPromptPolling(bCanShowInteractionPrompt);
}

void UDMGameRootWidget::RefreshChestModal(const bool bAllowGameplayUI)
{
    const ADMMatchPlayerController* Controller = BoundController.Get();

    const ADMMatchPlayerState* PlayerState = BoundPlayerState.Get();

    const ADMMatchGameState* GameState = BoundGameState.Get();

    const bool bShowChest = bAllowGameplayUI && IsValid(Controller) && Controller->IsChestInteractionOpen();

    if (!bShowChest)
    {
        // 상자가 닫혀 있거나 관전 중이면 모달을 완전히 숨긴다.
        if (IsValid(ChestPanel))
        {
            ChestPanel->SetChestOpen(false);
        }

        if (IsValid(BRD_DimBackground))
        {
            BRD_DimBackground->SetVisibility(ESlateVisibility::Collapsed);
        }

        if (IsValid(ModalLayer))
        {
            ModalLayer->SetVisibility(ESlateVisibility::Collapsed);
        }

        return;
    }

    const bool bIsOwnChest = IsValid(PlayerState) && PlayerState->GetBaseSlot() == Controller->GetInspectedBaseSlot();

    const bool bIsCarryingRelic = IsValid(PlayerState) && PlayerState->IsCarryingRelic();

    const int32 RequiredRelicCount = IsValid(GameState) ? GameState->GetRequiredRelicCount() : 0;

    // 등록 개수는 Controller가 서버에서 받은 Snapshot만 사용한다.
    ChestPanel->SetChestSnapshot(
        bIsOwnChest, Controller->GetInspectedRegisteredRelicCount(), RequiredRelicCount, bIsCarryingRelic
    );

    BRD_DimBackground->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    ChestPanel->SetChestOpen(true);

    ModalLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UDMGameRootWidget::RefreshSkillTargetingModal(const bool bAllowGameplayUI)
{
    if (!IsValid(SkillTargetPanel))
    {
        return;
    }

    const ADMMatchPlayerController* Controller = BoundController.Get();

    // 상자 모달이 열려 있으면 상자 표시를 우선한다.
    const bool bShowTargeting = bAllowGameplayUI && IsValid(Controller) && Controller->IsSkillTargetingActive()
        && !Controller->IsChestInteractionOpen();

    if (!bShowTargeting)
    {
        // 상자 모달과 공용 배경은 여기서 닫지 않는다.
        SkillTargetPanel->HideTargetingSkill();
        return;
    }

    // 실제 대상이나 상대의 유물 수는 조회하지 않는다.
    SkillTargetPanel->ShowTargetingSkill(Controller->GetPendingTargetSkillTag(), BoundPlayerState.Get());

    // 상자 모달과 같은 반투명 검정 배경을 사용한다.
    BRD_DimBackground->SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    ModalLayer->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
}

void UDMGameRootWidget::UpdateInteractionPromptPolling(const bool bEnable)
{
    if (!bEnable)
    {
        StopInteractionPromptPolling();

        if (IsValid(InteractionPrompt))
        {
            InteractionPrompt->SetCandidate(nullptr);
        }

        return;
    }

    UWorld* World = GetWorld();

    if (!World)
    {
        return;
    }

    FTimerManager& TimerManager = World->GetTimerManager();

    if (!TimerManager.IsTimerActive(InteractionPromptTimerHandle))
    {
        TimerManager.SetTimer(
            InteractionPromptTimerHandle,
            this,
            &ThisClass::RefreshInteractionPrompt,
            InteractionPromptRefreshInterval,
            true
        );
    }

    // 타이머 첫 호출을 기다리지 않고 현재 안내를 즉시 표시한다.
    RefreshInteractionPrompt();
}

void UDMGameRootWidget::RefreshInteractionPrompt()
{
    if (!IsValid(InteractionPrompt))
    {
        return;
    }

    ADMPlayerCharacter* Pawn = BoundPawn.Get();

    const ADMMatchPlayerController* Controller = BoundController.Get();

    if (!IsValid(Pawn) || !IsValid(Controller) || Controller->IsSpectatorInformationRestricted()
        || Controller->IsChestInteractionOpen() || Controller->IsSkillTargetingActive())
    {
        InteractionPrompt->SetCandidate(nullptr);
        return;
    }

    UDMInteractionComponent* Interaction = Pawn->FindComponentByClass<UDMInteractionComponent>();

    AActor* Candidate = IsValid(Interaction) ? Interaction->GetLocalInteractionPromptCandidate() : nullptr;

    InteractionPrompt->SetCandidate(Candidate);
}

void UDMGameRootWidget::StopInteractionPromptPolling()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(InteractionPromptTimerHandle);
    }

    InteractionPromptTimerHandle.Invalidate();
}

void UDMGameRootWidget::HandleChestInteractionChanged(
    bool /*bOpen*/, int32 /*BaseSlot*/, int32 /*RegisteredRelicCount*/
)
{
    // Controller의 승인된 현재 상태를 읽어 모달을 갱신한다.
    RefreshRootVisibility();
}

void UDMGameRootWidget::HandleSkillTargetingStateChanged(
    bool /*bTargeting*/, FGameplayTag /*SkillTag*/
)
{
    // Controller의 현재 대상 선택 상태를 다시 읽는다.
    // 전달된 이벤트 값은 별도 Gameplay 상태로 저장하지 않는다.
    RefreshRootVisibility();
}

void UDMGameRootWidget::HandlePhaseStateChanged(
    ADMMatchGameState* Source, FDMMatchPhaseState /*NewState*/
)
{
    if (Source != BoundGameState.Get())
    {
        return;
    }

    RefreshRootVisibility();

    if (IsValid(GameplayHUD))
    {
        GameplayHUD->RefreshFromMatchState();
    }
}

void UDMGameRootWidget::HandleLifeStateChanged(
    ADMMatchPlayerState* Source, EDMPlayerLifeState /*NewState*/
)
{
    if (Source != BoundPlayerState.Get())
    {
        return;
    }

    RefreshRootVisibility();
}

void UDMGameRootWidget::HandleCarryStateChanged(
    ADMMatchPlayerState* Source, bool /*bIsCarryingRelic*/
)
{
    if (Source != BoundPlayerState.Get())
    {
        return;
    }

    // 현재 상태를 다시 읽어 열린 상자의 안내를 갱신한다.
    // 등록된 유물 개수는 서버가 보낸 기존 Snapshot을 유지한다.
    RefreshRootVisibility();
}

void UDMGameRootWidget::HandleSpectatorRestrictionChanged(bool /*bRestricted*/
)
{
    RefreshRootVisibility();
}

void UDMGameRootWidget::NativeDestruct()
{
    ShutdownDataSources();

    Super::NativeDestruct();
}
