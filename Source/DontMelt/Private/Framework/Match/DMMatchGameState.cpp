#include "Framework/Match/DMMatchGameState.h"

#include "Core/DMMatchConstants.h"

#include "Core/DMGameInstance.h"
#include "Engine/World.h"
#include "Framework/Match/DMMatchPlayerController.h"

#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMMatchGameState, Log, All);

#pragma region Queries

int32 ADMMatchGameState::GetCurrentPlayerStateCount() const
{
    return PlayerArray.Num();
}

double ADMMatchGameState::GetRemainingQuestTime() const
{
    if (!QuestPublicState.IsActive())
    {
        return 0.0;
    }

    const double RemainingTime = QuestPublicState.EndServerTime - GetServerWorldTimeSeconds();

    return FMath::Max(0.0, RemainingTime);
}

double ADMMatchGameState::GetRemainingRandomEventWarningTime() const
{
    if (!RandomEventPublicState.IsScheduledOrActive())
    {
        return 0.0;
    }

    const double CurrentServerTime = GetServerWorldTimeSeconds();

    if (CurrentServerTime >= RandomEventPublicState.StartServerTime)
    {
        return 0.0;
    }

    return FMath::Max(0.0, RandomEventPublicState.StartServerTime - CurrentServerTime);
}

double ADMMatchGameState::GetRemainingRandomEventTime() const
{
    if (!RandomEventPublicState.IsScheduledOrActive())
    {
        return 0.0;
    }

    const double CurrentServerTime = GetServerWorldTimeSeconds();

    if (CurrentServerTime < RandomEventPublicState.StartServerTime)
    {
        return 0.0;
    }

    const double RemainingTime = RandomEventPublicState.EndServerTime - CurrentServerTime;

    return FMath::Max(0.0, RemainingTime);
}

double ADMMatchGameState::GetRemainingPhaseTime() const
{
    if (PhaseState.PhaseEndServerTime <= 0.0)
    {
        return 0.0;
    }

    const double RemainingTime = PhaseState.PhaseEndServerTime - GetServerWorldTimeSeconds();

    return FMath::Max(0.0, RemainingTime);
}

double ADMMatchGameState::GetRemainingRegularTime() const
{
    if (PhaseState.CurrentPhase != EDMMatchPhase::Playing)
    {
        return 0.0;
    }

    return GetRemainingPhaseTime();
}

double ADMMatchGameState::GetRemainingOvertimeTime() const
{
    if (PhaseState.CurrentPhase != EDMMatchPhase::Overtime)
    {
        return 0.0;
    }

    return GetRemainingPhaseTime();
}

#pragma endregion

#pragma region Server Mutators

bool ADMMatchGameState::TrySetPhaseState(const FDMMatchPhaseState& NewPhaseState)
{
    if (!HasAuthority())
    {
        return false;
    }

    if (!ensureMsgf(
            NewPhaseState.PhaseEndServerTime >= 0.0,
            TEXT("TrySetPhaseState rejected: PhaseEndServerTime cannot be negative.")
        ))
    {
        return false;
    }

    const bool bSamePhase = PhaseState.CurrentPhase == NewPhaseState.CurrentPhase;

    const bool bSameEndTime = FMath::IsNearlyEqual(PhaseState.PhaseEndServerTime, NewPhaseState.PhaseEndServerTime);

    if (bSamePhase && bSameEndTime)
    {
        return true;
    }

    PhaseState = NewPhaseState;
    HandlePhaseStateChanged();

    return true;
}

void ADMMatchGameState::SetStartingPlayerCount(const int32 NewStartingPlayerCount)
{
    if (!HasAuthority())
    {
        return;
    }

    if (!ensureMsgf(
            DMMatch::IsValidPlayerCount(NewStartingPlayerCount),
            TEXT("SetStartingPlayerCount rejected: invalid count %d."),
            NewStartingPlayerCount
        ))
    {
        return;
    }

    if (StartingPlayerCount == NewStartingPlayerCount)
    {
        return;
    }

    if (StartingPlayerCount != 0)
    {
        UE_LOG(
            LogDMMatchGameState,
            Warning,
            TEXT("SetStartingPlayerCount rejected: count is already frozen. Current=%d, Requested=%d"),
            StartingPlayerCount,
            NewStartingPlayerCount
        );

        return;
    }

    StartingPlayerCount = NewStartingPlayerCount;
    HandleStartingPlayerCountChanged();
}

void ADMMatchGameState::SetRequiredRelicCount(const int32 NewRequiredRelicCount)
{
    if (!HasAuthority())
    {
        return;
    }

    if (!ensureMsgf(
            NewRequiredRelicCount > 0,
            TEXT("SetRequiredRelicCount rejected: count must be positive. Requested=%d"),
            NewRequiredRelicCount
        ))
    {
        return;
    }

    if (RequiredRelicCount == NewRequiredRelicCount)
    {
        return;
    }

    /*
     * Match 시작 시 한 번 확정한 뒤
     * 경기 도중 변경하지 않는다.
     */
    if (RequiredRelicCount != 0)
    {
        UE_LOG(
            LogDMMatchGameState,
            Warning,
            TEXT("SetRequiredRelicCount rejected: count is already frozen. Current=%d, Requested=%d"),
            RequiredRelicCount,
            NewRequiredRelicCount
        );

        return;
    }

    RequiredRelicCount = NewRequiredRelicCount;

    HandleRequiredRelicCountChanged();
}

bool ADMMatchGameState::TrySetMatchResult(const FDMMatchResult& NewMatchResult)
{
    if (!HasAuthority())
    {
        return false;
    }

    if (!ensureMsgf(
            NewMatchResult.EndReason != EDMMatchEndReason::None,
            TEXT("TrySetMatchResult rejected: None is not a final result.")
        ))
    {
        return false;
    }

    if (!ensureMsgf(
            !(NewMatchResult.WinningBaseSlot != INDEX_NONE && !DMMatch::IsValidBaseSlot(NewMatchResult.WinningBaseSlot)
            ),
            TEXT("TrySetMatchResult rejected: WinningBaseSlot is invalid. BaseSlot=%d"),
            NewMatchResult.WinningBaseSlot
        ))
    {
        return false;
    }

    if (!ensureMsgf(
            NewMatchResult.ResultServerTime >= 0.0,
            TEXT("TrySetMatchResult rejected: ResultServerTime cannot be negative.")
        ))
    {
        return false;
    }

    /*
     * Match Result는 한 번만 Commit할 수 있다.
     */
    if (MatchResult.EndReason != EDMMatchEndReason::None)
    {
        UE_LOG(LogDMMatchGameState, Warning, TEXT("TrySetMatchResult rejected: result is already frozen."));

        return false;
    }

    MatchResult = NewMatchResult;

    HandleMatchResultChanged();

    return true;
}

void ADMMatchGameState::SetQuestPublicState(const FDMQuestRuntimeState& NewQuestPublicState)
{
    if (!HasAuthority())
    {
        return;
    }

    const bool bClearingState = NewQuestPublicState.RunId == 0 && !NewQuestPublicState.QuestTag.IsValid()
        && FMath::IsNearlyZero(NewQuestPublicState.StartServerTime)
        && FMath::IsNearlyZero(NewQuestPublicState.EndServerTime);

    if (!bClearingState)
    {
        if (!ensureMsgf(
                NewQuestPublicState.RunId > 0 && NewQuestPublicState.QuestTag.IsValid()
                    && NewQuestPublicState.StartServerTime >= 0.0
                    && NewQuestPublicState.EndServerTime > NewQuestPublicState.StartServerTime,
                TEXT("SetQuestPublicState rejected: invalid public state.")
            ))
        {
            return;
        }
    }

    const bool bSameState = QuestPublicState.RunId == NewQuestPublicState.RunId
        && QuestPublicState.QuestTag == NewQuestPublicState.QuestTag
        && FMath::IsNearlyEqual(QuestPublicState.StartServerTime, NewQuestPublicState.StartServerTime)
        && FMath::IsNearlyEqual(QuestPublicState.EndServerTime, NewQuestPublicState.EndServerTime);

    if (bSameState)
    {
        return;
    }

    QuestPublicState = NewQuestPublicState;

    HandleQuestPublicStateChanged();
}

void ADMMatchGameState::SetRandomEventPublicState(const FDMRandomEventRuntimeState& NewRandomEventPublicState)
{
    if (!HasAuthority())
    {
        return;
    }

    const bool bClearingState = !NewRandomEventPublicState.EventTag.IsValid()
        && FMath::IsNearlyZero(NewRandomEventPublicState.WarningStartServerTime)
        && FMath::IsNearlyZero(NewRandomEventPublicState.StartServerTime)
        && FMath::IsNearlyZero(NewRandomEventPublicState.EndServerTime);

    if (!bClearingState)
    {
        const bool bInvalidState = !NewRandomEventPublicState.EventTag.IsValid()
            || NewRandomEventPublicState.WarningStartServerTime < 0.0
            || NewRandomEventPublicState.StartServerTime < NewRandomEventPublicState.WarningStartServerTime
            || NewRandomEventPublicState.EndServerTime <= NewRandomEventPublicState.StartServerTime;

        if (!ensureMsgf(!bInvalidState, TEXT("SetRandomEventPublicState rejected: invalid public state.")))
        {
            return;
        }
    }

    const bool bSameState = RandomEventPublicState.EventTag == NewRandomEventPublicState.EventTag
        && FMath::IsNearlyEqual(RandomEventPublicState.WarningStartServerTime,
                                NewRandomEventPublicState.WarningStartServerTime)
        && FMath::IsNearlyEqual(RandomEventPublicState.StartServerTime, NewRandomEventPublicState.StartServerTime)
        && FMath::IsNearlyEqual(RandomEventPublicState.EndServerTime, NewRandomEventPublicState.EndServerTime);

    if (bSameState)
    {
        return;
    }

    RandomEventPublicState = NewRandomEventPublicState;

    HandleRandomEventPublicStateChanged();
}

void ADMMatchGameState::SetRelicRevealState(const FDMRelicRevealState& NewState)
{
    if (!HasAuthority())
    {
        return;
    }

    const bool bClearing = NewState.EndServerTime == 0.0 && NewState.Entries.IsEmpty();

    if (!bClearing)
    {
        if (!NewState.HasPayload() || NewState.EndServerTime <= GetServerWorldTimeSeconds()
            || NewState.Entries.Num() > 5)
        {
            return;
        }

        // 완전한 위치 목록만 허용한다. 동일한 마커 ID도 거부한다.
        TSet<int32> SeenIds;
        for (const FDMRelicRevealEntry& Entry : NewState.Entries)
        {
            if (Entry.MarkerId < 0 || Entry.WorldLocation.ContainsNaN() || SeenIds.Contains(Entry.MarkerId))
            {
                return;
            }
            SeenIds.Add(Entry.MarkerId);
        }
    }

    if (bClearing && !RelicRevealState.HasPayload())
    {
        return;
    }

    RelicRevealState = NewState;
    HandleRelicRevealChanged();
    ForceNetUpdate();
}

void ADMMatchGameState::SetOvertimeIntroState(const FDMOvertimeIntroState& NewState)
{
    if (!HasAuthority())
    {
        return;
    }

    const bool bClearing = NewState.IntroEndServerTime == 0.0 && NewState.BaseCounts.IsEmpty();

    if (!bClearing)
    {
        // 연장전 진입 Phase에서만 공개 데이터를 받을 수 있다.
        if (GetCurrentPhase() != EDMMatchPhase::OvertimeIntro || !NewState.HasPayload()
            || NewState.BaseCounts.Num() != StartingPlayerCount
            || !FMath::IsNearlyEqual(NewState.IntroEndServerTime, PhaseState.PhaseEndServerTime, 0.01))
        {
            return;
        }

        TSet<int32> SeenBaseSlots;

        for (const FDMOvertimeBaseCount& Entry : NewState.BaseCounts)
        {
            if (!DMMatch::IsValidBaseSlot(Entry.BaseSlot) || Entry.RegisteredRelicCount < 0
                || Entry.RegisteredRelicCount > 5 || SeenBaseSlots.Contains(Entry.BaseSlot))
            {
                return;
            }

            SeenBaseSlots.Add(Entry.BaseSlot);
        }
    }
    else if (!OvertimeIntroState.HasPayload())
    {
        // 이미 비어 있다면 불필요한 복제를 반복하지 않는다.
        return;
    }

    OvertimeIntroState = NewState;

    // Listen Server에는 RepNotify가 호출되지 않으므로 직접 알린다.
    HandleOvertimeIntroStateChanged();

    ForceNetUpdate();
}

#pragma endregion

#pragma region Replication Callbacks

void ADMMatchGameState::OnRep_RelicRevealState()
{
    HandleRelicRevealChanged();
}

void ADMMatchGameState::OnRep_OvertimeIntroState()
{
    // 원격 클라이언트에서 복제된 공개 정보의 변경을 알린다.
    HandleOvertimeIntroStateChanged();
}

void ADMMatchGameState::OnRep_PhaseState()
{
    HandlePhaseStateChanged();
}

void ADMMatchGameState::OnRep_StartingPlayerCount()
{
    HandleStartingPlayerCountChanged();
}

void ADMMatchGameState::OnRep_RequiredRelicCount()
{
    HandleRequiredRelicCountChanged();
}

void ADMMatchGameState::OnRep_MatchResult()
{
    HandleMatchResultChanged();
}

void ADMMatchGameState::OnRep_QuestPublicState()
{
    HandleQuestPublicStateChanged();
}

void ADMMatchGameState::OnRep_RandomEventPublicState()
{
    HandleRandomEventPublicStateChanged();
}

#pragma endregion

#pragma region Internal Notifications

void ADMMatchGameState::HandleRelicRevealChanged()
{
    RelicRevealChangedDelegate.Broadcast(this, RelicRevealState);
}

void ADMMatchGameState::HandleOvertimeIntroStateChanged()
{
    OvertimeIntroStateChangedDelegate.Broadcast(this, OvertimeIntroState);
}

void ADMMatchGameState::HandlePhaseStateChanged()
{
    // 기존 복제 Phase를 이용해 각 실행 환경의 음악만 갱신한다.
    // 클라이언트가 Gameplay Phase를 새로 계산하거나 서버 상태를 변경하지 않는다.
    if (UWorld* World = GetWorld())
    {
        if (UDMGameInstance* GameInstance = Cast<UDMGameInstance>(World->GetGameInstance()))
        {
            GameInstance->RefreshMusicForWorld(World);
        }
    }

    PhaseStateChangedDelegate.Broadcast(this, PhaseState);
}

void ADMMatchGameState::HandleStartingPlayerCountChanged()
{
    StartingPlayerCountChangedDelegate.Broadcast(this, StartingPlayerCount);
}

void ADMMatchGameState::HandleRequiredRelicCountChanged()
{
    RequiredRelicCountChangedDelegate.Broadcast(this, RequiredRelicCount);
}

void ADMMatchGameState::HandleMatchResultChanged()
{
    UWorld* World = GetWorld();

    UDMGameInstance* GameInstance = IsValid(World) ? Cast<UDMGameInstance>(World->GetGameInstance()) : nullptr;

    // 결과 UI에 알리기 전에 보관한다.
    // 서버 확정과 클라이언트 수신이 모두 이 경로를 사용한다.
    bool bResultCached = false;

    if (IsValid(GameInstance))
    {
        // 보관 성공 여부를 결과 ACK 전송 조건으로 사용한다.
        bResultCached = GameInstance->CacheFinalMatchResult(this);
    }
    else if (MatchResult.EndReason != EDMMatchEndReason::None)
    {
        UE_LOG(
            LogDMMatchGameState, Error, TEXT("Failed to store match result: check that UDMGameInstance is configured.")
        );
    }

    // 원격 클라이언트는 결과를 로컬에 보관한 뒤에만 서버에 ACK를 보낸다.
    // Authority(Listen 호스트)는 ACK 대상이 아니며,
    // 같은 ResultId 중복 전송은 NotifyMatchResultCached() 내부에서 걸러진다.
    if (bResultCached && !HasAuthority() && MatchResult.ResultId.IsValid() && IsValid(World))
    {
        if (ADMMatchPlayerController* LocalController =
                Cast<ADMMatchPlayerController>(World->GetFirstPlayerController()))
        {
            LocalController->NotifyMatchResultCached(MatchResult.ResultId);
        }
    }

    MatchResultChangedDelegate.Broadcast(this, MatchResult);
}

void ADMMatchGameState::HandleQuestPublicStateChanged()
{
    QuestPublicStateChangedDelegate.Broadcast(this, QuestPublicState);
}

void ADMMatchGameState::HandleRandomEventPublicStateChanged()
{
    // 서버가 확정한 이벤트 시작·종료 시각을 이용해 각 로컬 음악의 전환 예약을 갱신한다.
    if (UWorld* World = GetWorld())
    {
        if (UDMGameInstance* GameInstance = Cast<UDMGameInstance>(World->GetGameInstance()))
        {
            GameInstance->RefreshMusicForWorld(World);
        }
    }

    RandomEventPublicStateChangedDelegate.Broadcast(this, RandomEventPublicState);
}

#pragma endregion

#pragma region Replication

void ADMMatchGameState::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME(ADMMatchGameState, PhaseState);

    DOREPLIFETIME(ADMMatchGameState, StartingPlayerCount);

    DOREPLIFETIME(ADMMatchGameState, RequiredRelicCount);

    DOREPLIFETIME(ADMMatchGameState, MatchResult);

    DOREPLIFETIME(ADMMatchGameState, QuestPublicState);

    DOREPLIFETIME(ADMMatchGameState, RandomEventPublicState);

    DOREPLIFETIME(ADMMatchGameState, RelicRevealState);

    // 연장전 진입 시에만 채워지는 진영별 등록 유물 개수.
    DOREPLIFETIME(ADMMatchGameState, OvertimeIntroState);
}

#pragma endregion
