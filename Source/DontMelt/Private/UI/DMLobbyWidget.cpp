
#include "UI/DMLobbyWidget.h"

#include "Core/DMMatchConstants.h"

#include "UI/DMLobbyPlayerRowWidget.h"
#include "UI/DMSettingsWidget.h"
#include "Framework/Lobby/DMLobbyGameState.h"
#include "Framework/Lobby/DMLobbyPlayerController.h"
#include "Framework/Lobby/DMLobbyPlayerState.h"

#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Engine/World.h"
#include "Algo/Sort.h"

#pragma region Initialization

void UDMLobbyWidget::InitializeLobby(ADMLobbyPlayerController* InController)
{
    BoundController = InController;

    RefreshLobby();
}

void UDMLobbyWidget::RefreshLobby()
{
    TryBindGameState();

    RefreshPlayerList();
    RefreshControls();
}

bool UDMLobbyWidget::IsGameStateBound() const
{
    return IsValid(BoundGameState.Get());
}

#pragma endregion

#pragma region Lifecycle

void UDMLobbyWidget::NativeOnInitialized()
{
    Super::NativeOnInitialized();

    if (IsValid(BTN_Ready.Get()))
    {
        BTN_Ready->OnClicked.AddUniqueDynamic(this, &UDMLobbyWidget::HandleReadyClicked);
    }

    if (IsValid(BTN_StartMatch.Get()))
    {
        BTN_StartMatch->OnClicked.AddUniqueDynamic(this, &UDMLobbyWidget::HandleStartMatchClicked);
    }

    if (IsValid(BTN_LeaveLobby.Get()))
    {
        BTN_LeaveLobby->OnClicked.AddUniqueDynamic(this, &UDMLobbyWidget::HandleLeaveLobbyClicked);
    }

    if (IsValid(BTN_Settings.Get()))
    {
        BTN_Settings->OnClicked.AddUniqueDynamic(this, &UDMLobbyWidget::HandleSettingsClicked);
    }

    if (IsValid(SettingsPanel.Get()))
    {
        SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);

        // 변경 확인 절차를 통과한 뒤에만 호출된다.
        SettingsPanel->OnSettingsClosed().AddUObject(this, &UDMLobbyWidget::HandleSettingsClosed);
    }
}

void UDMLobbyWidget::NativeDestruct()
{
    UnbindGameState();

    BoundController.Reset();

    // SettingsPanel은 이 위젯의 자식이라 수명이 같다.
    // 바인딩은 NativeOnInitialized에서 한 번만 하므로 여기서 해제하지 않는다.
    // 해제하면 같은 인스턴스가 다시 Construct될 때 닫힘 처리가 연결되지 않는다.

    Super::NativeDestruct();
}

#pragma endregion

#pragma region Data Binding

void UDMLobbyWidget::TryBindGameState()
{
    UWorld* World = GetWorld();

    ADMLobbyGameState* GameState = IsValid(World) ? World->GetGameState<ADMLobbyGameState>() : nullptr;

    if (BoundGameState.Get() == GameState)
    {
        return;
    }

    UnbindGameState();

    BoundGameState = GameState;

    if (IsValid(GameState))
    {
        GameState->OnLobbyDataChanged().AddUObject(this, &UDMLobbyWidget::HandleLobbyDataChanged);
    }
}

void UDMLobbyWidget::UnbindGameState()
{
    UnbindPlayerStates();

    ADMLobbyGameState* GameState = BoundGameState.Get();

    if (IsValid(GameState))
    {
        GameState->OnLobbyDataChanged().RemoveAll(this);
    }

    BoundGameState.Reset();
}

void UDMLobbyWidget::BindPlayerStates()
{
    UnbindPlayerStates();

    ADMLobbyGameState* GameState = BoundGameState.Get();

    if (!IsValid(GameState))
    {
        return;
    }

    for (APlayerState* PlayerState : GameState->PlayerArray)
    {
        ADMLobbyPlayerState* LobbyPlayerState = Cast<ADMLobbyPlayerState>(PlayerState);

        if (!IsValid(LobbyPlayerState))
        {
            continue;
        }

        LobbyPlayerState->OnReadyChanged().AddUObject(this, &UDMLobbyWidget::HandlePlayerReadyChanged);

        LobbyPlayerState->OnHostChanged().AddUObject(this, &UDMLobbyWidget::HandlePlayerHostChanged);

        BoundPlayerStates.Add(LobbyPlayerState);
    }
}

void UDMLobbyWidget::UnbindPlayerStates()
{
    for (const TWeakObjectPtr<ADMLobbyPlayerState>& PlayerState : BoundPlayerStates)
    {
        ADMLobbyPlayerState* LobbyPlayerState = PlayerState.Get();

        if (!IsValid(LobbyPlayerState))
        {
            continue;
        }

        LobbyPlayerState->OnReadyChanged().RemoveAll(this);
        LobbyPlayerState->OnHostChanged().RemoveAll(this);
    }

    BoundPlayerStates.Reset();
}

void UDMLobbyWidget::HandleLobbyDataChanged()
{
    RefreshPlayerList();
    RefreshControls();
}

void UDMLobbyWidget::HandlePlayerReadyChanged(ADMLobbyPlayerState* ChangedPlayerState, bool bNewReady)
{
    RefreshControls();
}

void UDMLobbyWidget::HandlePlayerHostChanged(ADMLobbyPlayerState* ChangedPlayerState, bool bNewHost)
{
    RefreshControls();
}

#pragma endregion

#pragma region Presentation

void UDMLobbyWidget::RefreshPlayerList()
{
    if (!IsValid(VB_PlayerList.Get()))
    {
        return;
    }

    VB_PlayerList->ClearChildren();

    UnbindPlayerStates();

    ADMLobbyGameState* GameState = BoundGameState.Get();

    if (!IsValid(GameState) || !PlayerRowWidgetClass)
    {
        RefreshControls();
        return;
    }

    // PlayerArray의 복제 순서와 무관하게 표시 순서를 정한다.
    TArray<ADMLobbyPlayerState*> SortedPlayers;

    for (APlayerState* PlayerState : GameState->PlayerArray)
    {
        ADMLobbyPlayerState* LobbyPlayerState = Cast<ADMLobbyPlayerState>(PlayerState);

        if (IsValid(LobbyPlayerState))
        {
            SortedPlayers.Add(LobbyPlayerState);
        }
    }

    // 이름을 기준으로 정렬하고 동일한 이름은 PlayerId로 구분한다.
    Algo::Sort(
        SortedPlayers,
        [](const ADMLobbyPlayerState* A, const ADMLobbyPlayerState* B)
        {
            const int32 NameComparison = A->GetPlayerName().Compare(B->GetPlayerName(), ESearchCase::IgnoreCase);

            if (NameComparison != 0)
            {
                return NameComparison < 0;
            }

            return A->GetPlayerId() < B->GetPlayerId();
        }
    );

    for (ADMLobbyPlayerState* LobbyPlayerState : SortedPlayers)
    {
        UDMLobbyPlayerRowWidget* PlayerRow =
            CreateWidget<UDMLobbyPlayerRowWidget>(GetOwningPlayer(), PlayerRowWidgetClass);

        if (!IsValid(PlayerRow))
        {
            continue;
        }

        PlayerRow->SetLobbyPlayerState(LobbyPlayerState);

        // 모든 참가자 Row가 동일한 가로 폭을 사용하도록 한다.
        if (UVerticalBoxSlot* RowSlot = VB_PlayerList->AddChildToVerticalBox(PlayerRow))
        {
            RowSlot->SetHorizontalAlignment(HAlign_Fill);
            RowSlot->SetVerticalAlignment(VAlign_Fill);
        }
    }

    BindPlayerStates();
}

void UDMLobbyWidget::RefreshControls()
{
    ADMLobbyGameState* GameState = BoundGameState.Get();

    ADMLobbyPlayerController* Controller = BoundController.Get();

    ADMLobbyPlayerState* LocalPlayerState =
        IsValid(Controller) ? Controller->GetPlayerState<ADMLobbyPlayerState>() : nullptr;

    const bool bHasGameState = IsValid(GameState);
    const bool bHasLocalPlayerState = IsValid(LocalPlayerState);

    const int32 PlayerCount = bHasGameState ? GameState->PlayerArray.Num() : 0;

    const int32 MinPlayers = bHasGameState ? GameState->GetMinPlayerCount() : 0;

    const int32 MaxPlayers = bHasGameState ? GameState->GetMaxPlayerCount() : 0;

    const bool bIsHost = bHasLocalPlayerState && LocalPlayerState->IsHost();

    const bool bIsReady = bHasLocalPlayerState && LocalPlayerState->IsReady();

    if (IsValid(TXT_PlayerCount.Get()))
    {
        TXT_PlayerCount->SetText(FText::FromString(
            FString::Printf(TEXT("%d / %s"), PlayerCount, MaxPlayers > 0 ? *FString::FromInt(MaxPlayers) : TEXT("?"))
        ));
    }

    if (IsValid(TXT_RuleSummary.Get()))
    {
        TXT_RuleSummary->SetText(
            MinPlayers > 0 && MaxPlayers > 0
                ? FText::FromString(FString::Printf(TEXT("최소 %d명 / 최대 %d명"), MinPlayers, MaxPlayers))
                : FText::FromString(TEXT("로비 규칙 동기화 중..."))
        );
    }

    if (IsValid(TXT_ReadyButton.Get()))
    {
        TXT_ReadyButton->SetText(FText::FromString(bIsReady ? TEXT("준비 취소") : TEXT("준비")));
    }

    if (IsValid(BTN_Ready.Get()))
    {
        BTN_Ready->SetIsEnabled(bHasLocalPlayerState);
    }

    bool bAllReady = PlayerCount > 0;

    if (bHasGameState)
    {
        for (APlayerState* PlayerState : GameState->PlayerArray)
        {
            const ADMLobbyPlayerState* LobbyPlayerState = Cast<ADMLobbyPlayerState>(PlayerState);

            if (!IsValid(LobbyPlayerState) || !LobbyPlayerState->IsReady())
            {
                bAllReady = false;
                break;
            }
        }
    }
    else
    {
        bAllReady = false;
    }

    const bool bCanStart = bIsHost && MinPlayers >= DMMatch::MinPlayers && MaxPlayers <= DMMatch::MaxPlayers
        && MaxPlayers >= MinPlayers && PlayerCount >= MinPlayers && PlayerCount <= MaxPlayers && bAllReady;

    if (IsValid(BTN_StartMatch.Get()))
    {
        BTN_StartMatch->SetVisibility(bIsHost ? ESlateVisibility::Visible : ESlateVisibility::Collapsed);

        BTN_StartMatch->SetIsEnabled(bCanStart);
    }

    if (IsValid(TXT_LobbyNotice.Get()))
    {
        FString Notice;

        if (!bHasGameState || !bHasLocalPlayerState)
        {
            Notice = TEXT("로비 정보 동기화 중...");
        }
        else if (MinPlayers < DMMatch::MinPlayers || MaxPlayers < MinPlayers)
        {
            Notice = TEXT("게임 규칙 동기화 중...");
        }
        else if (PlayerCount < MinPlayers)
        {
            Notice = FString::Printf(TEXT("게임 시작까지 %d명 더 필요합니다."), MinPlayers - PlayerCount);
        }
        else if (!bAllReady)
        {
            Notice = TEXT("모든 플레이어가 준비하면 시작할 수 있습니다.");
        }
        else if (bIsHost)
        {
            Notice = TEXT("모든 플레이어가 준비했습니다. 게임을 시작하세요.");
        }
        else
        {
            Notice = TEXT("방장이 게임을 시작하기를 기다리는 중입니다.");
        }

        TXT_LobbyNotice->SetText(FText::FromString(Notice));
    }
}

#pragma endregion

#pragma region Button Actions

void UDMLobbyWidget::HandleReadyClicked()
{
    ADMLobbyPlayerController* Controller = BoundController.Get();

    if (!IsValid(Controller))
    {
        return;
    }

    const ADMLobbyPlayerState* PlayerState = Controller->GetPlayerState<ADMLobbyPlayerState>();

    if (!IsValid(PlayerState))
    {
        return;
    }

    Controller->RequestSetReady(!PlayerState->IsReady());
}

void UDMLobbyWidget::HandleStartMatchClicked()
{
    ADMLobbyPlayerController* Controller = BoundController.Get();

    if (!IsValid(Controller))
    {
        return;
    }

    const ADMLobbyPlayerState* PlayerState = Controller->GetPlayerState<ADMLobbyPlayerState>();

    if (!IsValid(PlayerState) || !PlayerState->IsHost())
    {
        return;
    }

    Controller->RequestStartMatch();
}

void UDMLobbyWidget::HandleLeaveLobbyClicked()
{
    ADMLobbyPlayerController* Controller = BoundController.Get();

    if (IsValid(Controller))
    {
        Controller->LeaveLobby();
    }
}

void UDMLobbyWidget::HandleSettingsClicked()
{
    if (!IsValid(SettingsPanel.Get()))
    {
        return;
    }

    SettingsPanel->OpenSettings();
}

void UDMLobbyWidget::HandleSettingsClosed()
{
    if (IsValid(SettingsPanel.Get()))
    {
        SettingsPanel->SetVisibility(ESlateVisibility::Collapsed);
    }

    // 설정창에 있던 포커스를 로비 설정 버튼으로 돌려놓는다.
    if (IsValid(BTN_Settings.Get()))
    {
        BTN_Settings->SetKeyboardFocus();
    }
}

#pragma endregion
