#include "UI/DMMatchResultWidget.h"

#include "Core/DMGameplayUtils.h"
#include "Core/DMMatchConstants.h"

#include "UI/DMPlayerResultRowWidget.h"

#include "Framework/Match/DMMatchGameState.h"
#include "Framework/Match/DMMatchPlayerState.h"
#include "Framework/Match/DMMatchPlayerController.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/Widget.h"
#include "Components/VerticalBoxSlot.h"

#include "Engine/World.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMMatchResultWidget, Log, All);

namespace
{
    constexpr float ResultRefreshInterval = 0.25f;

    constexpr double AutoReturnSeconds = 30.0;

    bool IsTerminalPhase(const EDMMatchPhase Phase)
    {
        return Phase == EDMMatchPhase::Finished || Phase == EDMMatchPhase::Aborted;
    }

    bool HasServerConfirmedWinner(const FDMMatchResult& Result)
    {
        const bool bVictoryReason = Result.EndReason == EDMMatchEndReason::RelicVictory
            || Result.EndReason == EDMMatchEndReason::EliminationVictory
            || Result.EndReason == EDMMatchEndReason::RegularTimeVictory
            || Result.EndReason == EDMMatchEndReason::OvertimeVictory;

        return bVictoryReason && DMMatch::IsValidBaseSlot(Result.WinningBaseSlot);
    }
}

#pragma region Lifecycle

void UDMMatchResultWidget::NativeConstruct()
{
    Super::NativeConstruct();

    if (IsValid(BTN_ReturnToLobby))
    {
        BTN_ReturnToLobby->OnClicked.AddUniqueDynamic(this, &ThisClass::OnReturnToLobbyClicked);
    }

    if (IsValid(BTN_QuitGame))
    {
        BTN_QuitGame->OnClicked.AddUniqueDynamic(this, &ThisClass::OnQuitGameClicked);
    }

    if (IsValid(BTN_RematchReady))
    {
        BTN_RematchReady->OnClicked.AddUniqueDynamic(this, &ThisClass::OnRematchReadyClicked);
    }

    if (IsValid(BTN_RestartMatch))
    {
        BTN_RestartMatch->OnClicked.AddUniqueDynamic(this, &ThisClass::OnRestartMatchClicked);
    }

    bScoreboardDirty = true;

    RefreshPresentation();
}

void UDMMatchResultWidget::NativeDestruct()
{
    SetPresentationActive(false);
    SetDataSources(nullptr, nullptr);

    if (IsValid(BTN_ReturnToLobby))
    {
        BTN_ReturnToLobby->OnClicked.RemoveDynamic(this, &ThisClass::OnReturnToLobbyClicked);
    }

    if (IsValid(BTN_QuitGame))
    {
        BTN_QuitGame->OnClicked.RemoveDynamic(this, &ThisClass::OnQuitGameClicked);
    }

    if (IsValid(BTN_RematchReady))
    {
        BTN_RematchReady->OnClicked.RemoveDynamic(this, &ThisClass::OnRematchReadyClicked);
    }

    if (IsValid(BTN_RestartMatch))
    {
        BTN_RestartMatch->OnClicked.RemoveDynamic(this, &ThisClass::OnRestartMatchClicked);
    }

    Super::NativeDestruct();
}

#pragma endregion

#pragma region Data Source

void UDMMatchResultWidget::SetDataSources(ADMMatchGameState* InGameState, ADMMatchPlayerState* InPlayerState)
{
    const bool bGameStateChanged = BoundGameState.Get() != InGameState;

    const bool bPlayerStateChanged = BoundPlayerState.Get() != InPlayerState;

    if (!bGameStateChanged && !bPlayerStateChanged)
    {
        RefreshPresentation();
        return;
    }

    if (ADMMatchGameState* OldGameState = BoundGameState.Get())
    {
        OldGameState->OnPhaseStateChanged().RemoveAll(this);
        OldGameState->OnMatchResultChanged().RemoveAll(this);
    }

    if (ADMMatchPlayerState* OldPlayerState = BoundPlayerState.Get())
    {
        OldPlayerState->OnBaseSlotChanged().RemoveAll(this);
    }

    ClearReadySources();

    if (bGameStateChanged)
    {
        CachedLocalBaseSlot = INDEX_NONE;
        LastRemainingSeconds = INDEX_NONE;
    }

    BoundGameState = InGameState;
    BoundPlayerState = InPlayerState;

    bScoreboardDirty = true;

    if (IsValid(InGameState))
    {
        InGameState->OnPhaseStateChanged().AddUObject(this, &ThisClass::HandlePhaseStateChanged);

        InGameState->OnMatchResultChanged().AddUObject(this, &ThisClass::HandleMatchResultChanged);
    }

    if (IsValid(InPlayerState))
    {
        InPlayerState->OnBaseSlotChanged().AddUObject(this, &ThisClass::HandleBaseSlotChanged);

        if (InPlayerState->HasAssignedBaseSlot())
        {
            CachedLocalBaseSlot = InPlayerState->GetBaseSlot();
        }
    }

    RefreshPresentation();
}

void UDMMatchResultWidget::SetPresentationActive(const bool bActive)
{
    bPresentationActive = bActive;

    RefreshPresentation();
}

#pragma endregion

#pragma region Data Binding

void UDMMatchResultWidget::HandlePhaseStateChanged(
    ADMMatchGameState* Source, FDMMatchPhaseState /*NewState*/
)
{
    if (Source != BoundGameState.Get())
    {
        return;
    }

    RefreshPresentation();
}

void UDMMatchResultWidget::HandleMatchResultChanged(
    ADMMatchGameState* Source, FDMMatchResult /*NewResult*/
)
{
    if (Source != BoundGameState.Get())
    {
        return;
    }

    bScoreboardDirty = true;

    RefreshPresentation();
}

void UDMMatchResultWidget::HandleBaseSlotChanged(ADMMatchPlayerState* Source, const int32 NewBaseSlot)
{
    if (Source != BoundPlayerState.Get())
    {
        return;
    }

    if (DMMatch::IsValidBaseSlot(NewBaseSlot))
    {
        CachedLocalBaseSlot = NewBaseSlot;
    }

    RefreshPresentation();
}

void UDMMatchResultWidget::HandleRematchReadyChanged(
    ADMMatchPlayerState* Source, bool /*bNewReady*/
)
{
    if (!bPresentationActive || !IsValid(Source))
    {
        return;
    }

    RefreshActionButtons();
}

bool UDMMatchResultWidget::RefreshReadySources()
{
    TArray<TWeakObjectPtr<ADMMatchPlayerState>> NewPlayerStates;

    if (ADMMatchGameState* GameState = BoundGameState.Get())
    {
        for (ADMMatchPlayerState* MatchPlayerState : DMGameplay::GetMatchPlayerStates(GameState))
        {
            NewPlayerStates.AddUnique(TWeakObjectPtr<ADMMatchPlayerState>(MatchPlayerState));
        }
    }

    if (NewPlayerStates == ReadyPlayerStates)
    {
        return false;
    }

    ClearReadySources();

    ReadyPlayerStates = MoveTemp(NewPlayerStates);

    for (const TWeakObjectPtr<ADMMatchPlayerState>& WeakState : ReadyPlayerStates)
    {
        if (ADMMatchPlayerState* PlayerState = WeakState.Get())
        {
            PlayerState->OnRematchReadyChanged().AddUObject(this, &ThisClass::HandleRematchReadyChanged);
        }
    }

    return true;
}

void UDMMatchResultWidget::ClearReadySources()
{
    for (const TWeakObjectPtr<ADMMatchPlayerState>& WeakState : ReadyPlayerStates)
    {
        if (ADMMatchPlayerState* PlayerState = WeakState.Get())
        {
            PlayerState->OnRematchReadyChanged().RemoveAll(this);
        }
    }

    ReadyPlayerStates.Reset();
}

#pragma endregion

#pragma region Button Actions

void UDMMatchResultWidget::OnReturnToLobbyClicked()
{
    ADMMatchPlayerController* Controller = Cast<ADMMatchPlayerController>(GetOwningPlayer());

    if (!bPresentationActive || !IsValid(Controller) || !Controller->IsLocalController()
        || Controller->GetNetMode() != NM_ListenServer || !IsValid(BTN_ReturnToLobby)
        || !BTN_ReturnToLobby->GetIsEnabled())
    {
        return;
    }

    Controller->ServerRequestReturnToLobby();
}

void UDMMatchResultWidget::OnQuitGameClicked()
{
    ADMMatchPlayerController* Controller = Cast<ADMMatchPlayerController>(GetOwningPlayer());

    if (!bPresentationActive || !IsValid(Controller) || !Controller->IsLocalController() || !IsValid(BTN_QuitGame)
        || !BTN_QuitGame->GetIsEnabled())
    {
        return;
    }

    // 애플리케이션을 종료하지 않고 메인 메뉴 패널로 복귀한다.
    Controller->LeaveMatchToTitle(true);
}

void UDMMatchResultWidget::OnRematchReadyClicked()
{
    ADMMatchPlayerController* Controller = Cast<ADMMatchPlayerController>(GetOwningPlayer());

    ADMMatchPlayerState* PlayerState = BoundPlayerState.Get();

    if (!bPresentationActive || !IsValid(Controller) || !Controller->IsLocalController()
        || Controller->GetNetMode() != NM_Client || !IsValid(PlayerState) || !IsValid(BTN_RematchReady)
        || !BTN_RematchReady->GetIsEnabled())
    {
        return;
    }

    // 복제된 현재 준비 상태를 기준으로 변경을 요청한다.
    Controller->ServerSetRematchReady(!PlayerState->IsRematchReady());
}

void UDMMatchResultWidget::OnRestartMatchClicked()
{
    ADMMatchPlayerController* Controller = Cast<ADMMatchPlayerController>(GetOwningPlayer());

    if (!bPresentationActive || !IsValid(Controller) || !Controller->IsLocalController()
        || Controller->GetNetMode() != NM_ListenServer || !IsValid(BTN_RestartMatch)
        || !BTN_RestartMatch->GetIsEnabled())
    {
        return;
    }

    // 실제 준비 상태와 이동 가능 여부는 서버에서 다시 검사한다.
    Controller->ServerRequestRestartMatch();
}

#pragma endregion

#pragma region Presentation

void UDMMatchResultWidget::RefreshPresentation()
{
    ADMMatchGameState* GameState = BoundGameState.Get();

    if (!bPresentationActive || !IsValid(GameState) || !IsTerminalPhase(GameState->GetCurrentPhase()))
    {
        StopRefreshTimer();
        ClearReadySources();

        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    if (!IsValid(TXT_ResultTitle) || !IsValid(TXT_ResultDetail) || !IsValid(ResultScoreboardPanel)
        || !IsValid(PlayerResultRows) || !IsValid(TXT_AutoReturnCountdown) || !IsValid(BTN_ReturnToLobby)
        || !IsValid(BTN_QuitGame) || !IsValid(BTN_RematchReady) || !IsValid(BTN_RestartMatch)
        || !IsValid(TXT_RematchReady) || !IsValid(TXT_RematchReadyStatus))
    {
        StopRefreshTimer();
        ClearReadySources();

        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    if (ADMMatchPlayerState* PlayerState = BoundPlayerState.Get())
    {
        if (PlayerState->HasAssignedBaseSlot())
        {
            CachedLocalBaseSlot = PlayerState->GetBaseSlot();
        }
    }

    const FDMMatchResult Result = GameState->GetMatchResult();

    const bool bHasWinner = GameState->GetCurrentPhase() == EDMMatchPhase::Finished && HasServerConfirmedWinner(Result);
    // 정상 종료라면 승자가 없는 무승부·전원 탈락에서도 통계표를 표시한다.
    const bool bHasScoreboard = GameState->GetCurrentPhase() == EDMMatchPhase::Finished
        && Result.EndReason != EDMMatchEndReason::None && !Result.PlayerResults.IsEmpty();

    FText TitleText;
    FText DetailText;

    if (Result.EndReason == EDMMatchEndReason::None)
    {
        TitleText = NSLOCTEXT("DMMatchResult", "WaitingTitle", "결과 확인 중");

        DetailText = NSLOCTEXT("DMMatchResult", "WaitingDetail", "서버의 최종 경기 결과를 기다리고 있습니다.");
    }
    else if (bHasWinner)
    {
        if (DMMatch::IsValidBaseSlot(CachedLocalBaseSlot))
        {
            // 서버가 지정한 승리 진영을 로컬 플레이어에게 맞춰 표시한다.
            const bool bLocalPlayerWon = CachedLocalBaseSlot == Result.WinningBaseSlot;

            TitleText = bLocalPlayerWon ? NSLOCTEXT("DMMatchResult", "VictoryTitle", "승리")
                                        : NSLOCTEXT("DMMatchResult", "DefeatTitle", "패배");
        }
        else
        {
            TitleText = NSLOCTEXT("DMMatchResult", "LocalPlayerPending", "결과 확인 중");
        }
    }
    else
    {
        switch (Result.EndReason)
        {
        case EDMMatchEndReason::OvertimeDraw:

            TitleText = NSLOCTEXT("DMMatchResult", "OvertimeDrawTitle", "무승부");

            DetailText = NSLOCTEXT(
                "DMMatchResult",
                "OvertimeDrawDetail",
                "연장전 종료 시 등록 유물 최다 참가자가 동률이어서 승자 없이 종료되었습니다."
            );
            break;

        case EDMMatchEndReason::AllDefeated:

            TitleText = NSLOCTEXT("DMMatchResult", "AllDefeatedTitle", "전원 탈락");

            DetailText =
                NSLOCTEXT("DMMatchResult", "AllDefeatedDetail", "모든 참가자가 탈락하여 경기가 종료되었습니다.");
            break;

        case EDMMatchEndReason::PlayerDisconnect:

            TitleText = NSLOCTEXT("DMMatchResult", "DisconnectedTitle", "경기 중단");

            DetailText =
                NSLOCTEXT("DMMatchResult", "PlayerDisconnected", "참가자의 연결이 종료되어 경기가 중단되었습니다.");
            break;

        case EDMMatchEndReason::HostLeft:

            TitleText = NSLOCTEXT("DMMatchResult", "HostLeftTitle", "경기 중단");

            DetailText =
                NSLOCTEXT("DMMatchResult", "HostLeftDetail", "호스트의 연결이 종료되어 경기가 중단되었습니다.");
            break;

        case EDMMatchEndReason::InternalError:
            TitleText = NSLOCTEXT("DMMatchResult", "InternalErrorTitle", "경기 중단");
            DetailText = NSLOCTEXT(
                "DMMatchResult", "InternalErrorDetail", "서버에서 경기 결과를 처리하지 못해 경기가 중단되었습니다."
            );
            break;

        case EDMMatchEndReason::RegularTimeVictory:

            TitleText = NSLOCTEXT("DMMatchResult", "RegularTimeEndedTitle", "경기 종료");

            DetailText = NSLOCTEXT(
                "DMMatchResult", "RegularTimeNoWinner", "정규 시간이 종료되었으며 승리자가 확정되지 않았습니다."
            );
            break;

        case EDMMatchEndReason::OvertimeVictory:

            TitleText = NSLOCTEXT("DMMatchResult", "OvertimeEndedTitle", "경기 종료");

            DetailText =
                NSLOCTEXT("DMMatchResult", "OvertimeNoWinner", "연장전이 종료되었으며 승리자가 확정되지 않았습니다.");
            break;

        default:

            TitleText = NSLOCTEXT("DMMatchResult", "UnknownFinishTitle", "경기 종료");

            DetailText = NSLOCTEXT(
                "DMMatchResult", "UnknownFinishDetail", "정상적인 승리자가 확정되지 않은 상태로 경기가 종료되었습니다."
            );
            break;
        }
    }

    TXT_ResultTitle->SetText(TitleText);
    TXT_ResultDetail->SetText(DetailText);

    ResultScoreboardPanel->SetVisibility(
        bHasScoreboard ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed
    );

    TXT_ResultDetail->SetVisibility(bHasWinner ? ESlateVisibility::Collapsed : ESlateVisibility::SelfHitTestInvisible);

    if (bScoreboardDirty)
    {
        if (bHasScoreboard)
        {
            RebuildPlayerRows(Result);
        }
        else
        {
            PlayerResultRows->ClearChildren();
        }

        bScoreboardDirty = false;
    }

    SetVisibility(ESlateVisibility::SelfHitTestInvisible);

    StartRefreshTimer();
    HandleRefreshTimer();
    RefreshActionButtons();
}

void UDMMatchResultWidget::RebuildPlayerRows(const FDMMatchResult& Result)
{
    PlayerResultRows->ClearChildren();

    if (Result.PlayerResults.IsEmpty())
    {
        UE_LOG(LogDMMatchResultWidget, Warning, TEXT("Participant stats list for the victory result is empty."));

        return;
    }

    if (!ensureMsgf(
            PlayerResultRowClass && !PlayerResultRowClass->HasAnyClassFlags(CLASS_Abstract),
            TEXT("PlayerResultRowClass must be set to WBP_DMPlayerResultRow.")
        ))
    {
        return;
    }

    APlayerController* OwningController = GetOwningPlayer();

    if (!IsValid(OwningController))
    {
        return;
    }

    for (const FDMMatchResultEntry& Entry : Result.PlayerResults)
    {
        UDMPlayerResultRowWidget* Row = CreateWidget<UDMPlayerResultRowWidget>(OwningController, PlayerResultRowClass);

        if (!IsValid(Row))
        {
            UE_LOG(LogDMMatchResultWidget, Error, TEXT("Failed to create player result row."));

            continue;
        }

        // 결과 Row가 부모 VerticalBox의 전체 가로 폭을 사용하도록 한다.
        if (UVerticalBoxSlot* RowSlot = PlayerResultRows->AddChildToVerticalBox(Row))
        {
            RowSlot->SetHorizontalAlignment(HAlign_Fill);
            RowSlot->SetVerticalAlignment(VAlign_Fill);
        }

        Row->SetResultEntry(Entry);

        Row->SetVisibility(ESlateVisibility::SelfHitTestInvisible);
    }
}

void UDMMatchResultWidget::RefreshActionButtons()
{
    if (!bPresentationActive || !IsValid(BTN_ReturnToLobby) || !IsValid(BTN_QuitGame) || !IsValid(BTN_RematchReady)
        || !IsValid(BTN_RestartMatch) || !IsValid(TXT_RematchReady) || !IsValid(TXT_RematchReadyStatus))
    {
        return;
    }

    ADMMatchPlayerController* Controller = Cast<ADMMatchPlayerController>(GetOwningPlayer());

    const ADMMatchGameState* GameState = BoundGameState.Get();

    const ADMMatchPlayerState* LocalPlayerState = BoundPlayerState.Get();

    const bool bLocalControllerValid = IsValid(Controller) && Controller->IsLocalController();

    const bool bIsHost = bLocalControllerValid && Controller->GetNetMode() == NM_ListenServer;

    const bool bIsClient = bLocalControllerValid && Controller->GetNetMode() == NM_Client;

    const bool bLocalPlayerValid =
        bLocalControllerValid && IsValid(LocalPlayerState) && LocalPlayerState->HasAssignedBaseSlot();

    const bool bTerminal = IsValid(GameState) && IsTerminalPhase(GameState->GetCurrentPhase());

    const FDMMatchResult Result = IsValid(GameState) ? GameState->GetMatchResult() : FDMMatchResult();

    const bool bResultReceived = bTerminal && Result.EndReason != EDMMatchEndReason::None;

    const bool bBeforeDeadline =
        bResultReceived && GameState->GetServerWorldTimeSeconds() < Result.ResultServerTime + AutoReturnSeconds;

    const bool bLocalReady = IsValid(LocalPlayerState) && LocalPlayerState->IsRematchReady();

    // 수동 로비 복귀는 호스트에게만 표시한다.
    BTN_ReturnToLobby->SetVisibility(bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    BTN_ReturnToLobby->SetIsEnabled(bIsHost && bLocalPlayerValid && bBeforeDeadline);

    // 모든 로컬 참가자는 메인 메뉴로 나갈 수 있다.
    BTN_QuitGame->SetVisibility(ESlateVisibility::Visible);
    BTN_QuitGame->SetIsEnabled(bLocalControllerValid);

    // 준비 버튼은 일반 참가자에게만 표시한다.
    BTN_RematchReady->SetVisibility(bIsClient ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    BTN_RematchReady->SetIsEnabled(bIsClient && bLocalPlayerValid && bBeforeDeadline);

    TXT_RematchReady->SetText(
        bLocalReady ? NSLOCTEXT("DMMatchResult", "CancelReady", "준비 취소")
                    : NSLOCTEXT("DMMatchResult", "Ready", "준비")
    );

    const int32 ExpectedPlayerCount = IsValid(GameState) ? GameState->GetStartingPlayerCount() : 0;

    bool bAllClientsReady = bIsHost && bLocalPlayerValid && bBeforeDeadline
        && DMMatch::IsValidPlayerCount(ExpectedPlayerCount) && IsValid(GameState)
        && GameState->PlayerArray.Num() == ExpectedPlayerCount && ReadyPlayerStates.Num() == ExpectedPlayerCount;

    bool bFoundHost = false;
    int32 ClientCount = 0;
    TArray<FString> ReadyLines;

    if (bIsHost)
    {
        for (const TWeakObjectPtr<ADMMatchPlayerState>& WeakState : ReadyPlayerStates)
        {
            const ADMMatchPlayerState* PlayerState = WeakState.Get();

            if (!IsValid(PlayerState))
            {
                bAllClientsReady = false;
                continue;
            }

            // 호스트 화면에서는 자신의 PlayerState를 제외하면 된다.
            // 호스트 식별을 위한 별도 복제 변수를 추가하지 않는다.
            if (PlayerState == LocalPlayerState)
            {
                bFoundHost = true;
                continue;
            }

            ++ClientCount;

            const bool bPlayerReady = PlayerState->IsRematchReady();

            if (!bPlayerReady)
            {
                bAllClientsReady = false;
            }

            ReadyLines.Add(FString::Printf(
                TEXT("%s : %s"), *PlayerState->GetPlayerName(), bPlayerReady ? TEXT("준비") : TEXT("대기")
            ));
        }
    }

    bAllClientsReady = bAllClientsReady && bFoundHost && ClientCount == ExpectedPlayerCount - 1;

    TXT_RematchReadyStatus->SetVisibility(bIsHost ? ESlateVisibility::HitTestInvisible : ESlateVisibility::Collapsed);

    TXT_RematchReadyStatus->SetText(FText::FromString(FString::Join(ReadyLines, TEXT("\n"))));

    // 모든 일반 참가자가 준비한 경우에만 호스트에게 표시한다.
    BTN_RestartMatch->SetVisibility(bAllClientsReady ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

    BTN_RestartMatch->SetIsEnabled(bAllClientsReady);
}

#pragma endregion

#pragma region Presentation Timer

void UDMMatchResultWidget::StartRefreshTimer()
{
    UWorld* World = GetWorld();

    if (!IsValid(World) || World->GetTimerManager().IsTimerActive(RefreshTimerHandle))
    {
        return;
    }

    World->GetTimerManager().SetTimer(
        RefreshTimerHandle, this, &ThisClass::HandleRefreshTimer, ResultRefreshInterval, true
    );
}

void UDMMatchResultWidget::StopRefreshTimer()
{
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(RefreshTimerHandle);
    }

    LastRemainingSeconds = INDEX_NONE;
}

void UDMMatchResultWidget::HandleRefreshTimer()
{
    ADMMatchGameState* GameState = BoundGameState.Get();

    if (!bPresentationActive || !IsValid(GameState) || !IsTerminalPhase(GameState->GetCurrentPhase()))
    {
        StopRefreshTimer();
        ClearReadySources();

        SetVisibility(ESlateVisibility::Collapsed);
        return;
    }

    if (!IsValid(TXT_AutoReturnCountdown))
    {
        return;
    }

    // 별도 참가자 감시 타이머를 만들지 않고
    // 카운트다운 갱신 시 현재 참가자 목록도 확인한다.
    const bool bParticipantsChanged = RefreshReadySources();

    const FDMMatchResult Result = GameState->GetMatchResult();

    int32 RemainingSeconds = INDEX_NONE;

    if (Result.EndReason != EDMMatchEndReason::None)
    {
        const double RemainingTime =
            FMath::Max(0.0, Result.ResultServerTime + AutoReturnSeconds - GameState->GetServerWorldTimeSeconds());

        RemainingSeconds = FMath::CeilToInt(RemainingTime);
    }

    const bool bCountdownChanged = RemainingSeconds != LastRemainingSeconds;

    if (bCountdownChanged)
    {
        LastRemainingSeconds = RemainingSeconds;

        if (RemainingSeconds == INDEX_NONE)
        {
            TXT_AutoReturnCountdown->SetText(FText::GetEmpty());
        }
        else if (RemainingSeconds > 0)
        {
            TXT_AutoReturnCountdown->SetText(FText::Format(
                NSLOCTEXT("DMMatchResult", "AutoReturnCountdown", "{0}초 후 로비로 이동"),
                FText::AsNumber(RemainingSeconds)
            ));
        }
        else
        {
            TXT_AutoReturnCountdown->SetText(
                NSLOCTEXT("DMMatchResult", "AutoReturnDeadlineReached", "자동 로비 복귀 시간이 되었습니다.")
            );
        }
    }

    if (bParticipantsChanged || bCountdownChanged)
    {
        RefreshActionButtons();
    }
}

#pragma endregion