#include "Events/DMRandomEventDirector.h"

#include "Core/DMGameplayUtils.h"
#include "Core/DMMatchConstants.h"

#include "Characters/DMPlayerCharacter.h"
#include "Characters/Components/DMCharacterStateComponent.h"
#include "Core/DMGameplayTags.h"
#include "Data/DMMatchRuleDataAsset.h"
#include "Data/DMRandomEventSetDataAsset.h"
#include "Framework/Match/DMMatchGameMode.h"
#include "Framework/Match/DMMatchGameState.h"
#include "Framework/Match/DMMatchPlayerState.h"

#include "Engine/World.h"
#include "GameFramework/PlayerState.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMRandomEventDirector, Log, All);

#pragma region Lifecycle

bool UDMRandomEventDirector::Initialize(
    ADMMatchGameMode* InOwnerGameMode,
    const UDMMatchRuleDataAsset* InMatchRuleData,
    const UDMRandomEventSetDataAsset* InEventSetData,
    FDMRandomEventPublicStatePublisher InPublicStatePublisher
)
{
    if (!ensureMsgf(
            IsValid(InOwnerGameMode) && InOwnerGameMode->HasAuthority(),
            TEXT("Initialize failed: authoritative GameMode is required.")
        ))
    {
        return false;
    }

    if (!ensureMsgf(InPublicStatePublisher.IsBound(), TEXT("Initialize failed: public state publisher is not bound.")))
    {
        return false;
    }

    if (!ensureMsgf(
            IsValid(InMatchRuleData) && IsValid(InEventSetData),
            TEXT("Initialize failed: Random Event configuration is unavailable.")
        ))
    {
        return false;
    }

    OwnerGameMode = InOwnerGameMode;

    PublicStatePublisher = MoveTemp(InPublicStatePublisher);

    RandomEventMinCount = InMatchRuleData->RandomEventMinCount;

    RandomEventMaxCount = InMatchRuleData->RandomEventMaxCount;

    MinimumEventStartInterval = InMatchRuleData->RandomEventMinimumInterval;

    MatchDuration = InMatchRuleData->MatchDuration;

    EventAttemptTimes = {
        InMatchRuleData->FirstRandomEventTime,

        InMatchRuleData->SecondRandomEventTime,

        InMatchRuleData->ThirdRandomEventTime
    };

    if (!ValidateSchedule())
    {
        return false;
    }

    if (!CacheEventDefinitions(InEventSetData))
    {
        return false;
    }

    ActiveEventState = FDMRandomEventRuntimeState();

    ActiveDefinitionIndex = INDEX_NONE;

    ResolvedEventAttemptCount = 0;

    PreviousEventTag = FGameplayTag();

    LastStartedEventServerTime = 0.0;

    UE_LOG(
        LogDMRandomEventDirector,
        Log,
        TEXT("RandomEventDirector initialized. Definitions=%d, AttemptCountRange=%d~%d, MinimumInterval=%.3f"),
        CachedEventDefinitions.Num(),
        RandomEventMinCount,
        RandomEventMaxCount,
        MinimumEventStartInterval
    );

    return true;
}

bool UDMRandomEventDirector::StartPlaying()
{
    ADMMatchGameMode* MatchGameMode = OwnerGameMode.Get();

    ADMMatchGameState* MatchGameState = GetMatchGameState();

    if (!IsValid(MatchGameMode) || !MatchGameMode->HasAuthority() || !IsValid(MatchGameState))
    {
        return false;
    }

    if (MatchGameState->GetCurrentPhase() != EDMMatchPhase::Playing)
    {
        UE_LOG(LogDMRandomEventDirector, Warning, TEXT("StartPlaying rejected: Match phase is not Playing."));

        return false;
    }

    if (ResolvedEventAttemptCount > 0 || !EventAttemptTimerHandles.IsEmpty())
    {
        UE_LOG(
            LogDMRandomEventDirector, Warning, TEXT("StartPlaying rejected: Random Event schedule already started.")
        );

        return false;
    }

    UWorld* World = MatchGameMode->GetWorld();

    if (!IsValid(World))
    {
        return false;
    }

    ResolvedEventAttemptCount = FMath::RandRange(RandomEventMinCount, RandomEventMaxCount);

    EventAttemptTimerHandles.Reserve(ResolvedEventAttemptCount);

    for (int32 AttemptIndex = 0; AttemptIndex < ResolvedEventAttemptCount; ++AttemptIndex)
    {
        FTimerHandle TimerHandle;

        FTimerDelegate TimerDelegate;

        TimerDelegate.BindUObject(this, &UDMRandomEventDirector::HandleEventAttempt, AttemptIndex);

        World->GetTimerManager().SetTimer(TimerHandle, TimerDelegate, EventAttemptTimes[AttemptIndex], false);

        if (!World->GetTimerManager().IsTimerActive(TimerHandle))
        {
            UE_LOG(
                LogDMRandomEventDirector,
                Error,
                TEXT("Random Event attempt timer registration failed. Attempt=%d"),
                AttemptIndex
            );

            ClearEventAttemptTimers();

            ResolvedEventAttemptCount = 0;

            return false;
        }

        EventAttemptTimerHandles.Add(TimerHandle);
    }

    UE_LOG(
        LogDMRandomEventDirector,
        Log,
        TEXT("Random Event schedule started. ResolvedAttempts=%d"),
        ResolvedEventAttemptCount
    );

    return true;
}

void UDMRandomEventDirector::Shutdown()
{
    if (ActiveEventState.EventTag == DMGameplayTags::RandomEvent_InputReverse)
    {
        RemoveInputReverseEffect();
    }

    ClearEventAttemptTimers();

    ClearActiveEventTimers();

    if (ADMMatchGameMode* MatchGameMode = OwnerGameMode.Get())
    {
        if (MatchGameMode->HasAuthority())
        {
            PublishPublicState(FDMRandomEventRuntimeState());
        }
    }

    ActiveEventState = FDMRandomEventRuntimeState();

    ActiveDefinitionIndex = INDEX_NONE;

    ResolvedEventAttemptCount = 0;

    PreviousEventTag = FGameplayTag();

    LastStartedEventServerTime = 0.0;

    UE_LOG(LogDMRandomEventDirector, Log, TEXT("RandomEventDirector shutdown completed."));
}

#pragma endregion

#pragma region Initialization

bool UDMRandomEventDirector::CacheEventDefinitions(const UDMRandomEventSetDataAsset* EventSetData)
{
    if (!IsValid(EventSetData))
    {
        return false;
    }

    const TArray<FDMRandomEventDefinition>& SourceDefinitions = EventSetData->GetEventDefinitions();

    /*
     * 현재 기획상 Event는 정확히 3종이다.
     *
     * Blackout / Panic / InputReverse
     */
    if (!ensureMsgf(
            SourceDefinitions.Num() == 3,
            TEXT("Random Event Set must contain exactly 3 definitions. Current=%d"),
            SourceDefinitions.Num()
        ))
    {
        return false;
    }

    TSet<FGameplayTag> FoundTags;

    bool bHasBlackout = false;

    bool bHasPanic = false;

    bool bHasInputReverse = false;

    CachedEventDefinitions.Reset();

    CachedEventDefinitions.Reserve(SourceDefinitions.Num());

    for (const FDMRandomEventDefinition& Definition : SourceDefinitions)
    {
        if (!ValidateEventDefinition(Definition))
        {
            CachedEventDefinitions.Reset();

            return false;
        }

        if (!ensureMsgf(
                !FoundTags.Contains(Definition.EventTag),
                TEXT("Duplicate Random Event Tag: %s"),
                *Definition.EventTag.ToString()
            ))
        {
            CachedEventDefinitions.Reset();

            return false;
        }

        FoundTags.Add(Definition.EventTag);

        if (Definition.EventTag == DMGameplayTags::RandomEvent_Blackout)
        {
            bHasBlackout = true;
        }
        else if (Definition.EventTag == DMGameplayTags::RandomEvent_Panic)
        {
            bHasPanic = true;
        }
        else if (Definition.EventTag == DMGameplayTags::RandomEvent_InputReverse)
        {
            bHasInputReverse = true;
        }
        else
        {
            UE_LOG(
                LogDMRandomEventDirector,
                Error,
                TEXT("Unsupported Random Event Tag: %s"),
                *Definition.EventTag.ToString()
            );

            CachedEventDefinitions.Reset();

            return false;
        }

        CachedEventDefinitions.Add(Definition);
    }

    if (!ensureMsgf(
            bHasBlackout && bHasPanic && bHasInputReverse,
            TEXT("Random Event Set must contain Blackout, Panic and InputReverse.")
        ))
    {
        CachedEventDefinitions.Reset();

        return false;
    }

    return true;
}

bool UDMRandomEventDirector::ValidateEventDefinition(const FDMRandomEventDefinition& Definition) const
{
    if (!Definition.EventTag.IsValid())
    {
        return false;
    }

    if (Definition.WarningDuration < 0.0f)
    {
        return false;
    }

    if (Definition.Duration <= 0.0f)
    {
        return false;
    }

    return true;
}

bool UDMRandomEventDirector::ValidateSchedule() const
{
    if (!ensureMsgf(
            FMath::IsWithinInclusive(RandomEventMinCount, 2, 3) && FMath::IsWithinInclusive(RandomEventMaxCount, 2, 3)
                && RandomEventMinCount <= RandomEventMaxCount,
            TEXT("Invalid Random Event attempt count range. Min=%d, Max=%d"),
            RandomEventMinCount,
            RandomEventMaxCount
        ))
    {
        return false;
    }

    if (MatchDuration <= 0.0f)
    {
        return false;
    }

    /*
     * 기획상 Event 간 최소 간격은
     * 120초 이상이어야 한다.
     */
    if (!ensureMsgf(
            MinimumEventStartInterval >= 120.0f,
            TEXT("Random Event minimum interval must be at least 120 seconds. Current=%.3f"),
            MinimumEventStartInterval
        ))
    {
        return false;
    }

    if (EventAttemptTimes.Num() < RandomEventMaxCount)
    {
        return false;
    }

    float PreviousAttemptTime = -1.0f;

    for (int32 Index = 0; Index < RandomEventMaxCount; ++Index)
    {
        const float AttemptTime = EventAttemptTimes[Index];

        if (!ensureMsgf(
                AttemptTime > 0.0f && AttemptTime < MatchDuration,
                TEXT("Invalid Random Event attempt time. Index=%d, Time=%.3f"),
                Index,
                AttemptTime
            ))
        {
            return false;
        }

        if (PreviousAttemptTime >= 0.0f)
        {
            const float Interval = AttemptTime - PreviousAttemptTime;

            if (!ensureMsgf(
                    Interval + KINDA_SMALL_NUMBER >= MinimumEventStartInterval,
                    TEXT("Random Event schedule interval is too short. Index=%d, Interval=%.3f, Minimum=%.3f"),
                    Index,
                    Interval,
                    MinimumEventStartInterval
                ))
            {
                return false;
            }
        }

        PreviousAttemptTime = AttemptTime;
    }

    return true;
}

#pragma endregion

#pragma region Scheduling

void UDMRandomEventDirector::HandleEventAttempt(const int32 AttemptIndex)
{
    TryScheduleEvent(AttemptIndex);
}

bool UDMRandomEventDirector::TryScheduleEvent(const int32 AttemptIndex)
{
    ADMMatchGameMode* MatchGameMode = OwnerGameMode.Get();

    ADMMatchGameState* MatchGameState = GetMatchGameState();

    if (!IsValid(MatchGameMode) || !MatchGameMode->HasAuthority() || !IsValid(MatchGameState))
    {
        return false;
    }

    if (AttemptIndex < 0 || AttemptIndex >= ResolvedEventAttemptCount)
    {
        return false;
    }

    if (MatchGameState->GetCurrentPhase() != EDMMatchPhase::Playing)
    {
        UE_LOG(
            LogDMRandomEventDirector,
            Log,
            TEXT("Random Event attempt skipped. Reason=NotPlaying, Attempt=%d"),
            AttemptIndex
        );

        return false;
    }

    /*
     * Warning 상태도 이미 Event 사용 중으로 취급한다.
     */
    if (ActiveEventState.IsScheduledOrActive() || MatchGameState->HasCurrentRandomEvent())
    {
        UE_LOG(
            LogDMRandomEventDirector,
            Log,
            TEXT("Random Event attempt skipped. Reason=EventAlreadyActive, Attempt=%d"),
            AttemptIndex
        );

        return false;
    }

    /*
     * Quest와 Event는 절대 중첩하지 않는다.
     */
    if (MatchGameState->HasCurrentQuest())
    {
        UE_LOG(
            LogDMRandomEventDirector,
            Log,
            TEXT("Random Event attempt skipped. Reason=QuestActive, Attempt=%d"),
            AttemptIndex
        );

        return false;
    }

    if (!AreAllPlayersAlive())
    {
        UE_LOG(
            LogDMRandomEventDirector,
            Log,
            TEXT("Random Event attempt skipped. Reason=NotAllPlayersAlive, Attempt=%d"),
            AttemptIndex
        );

        return false;
    }

    const int32 DefinitionIndex = SelectNextEventDefinitionIndex();

    if (!CachedEventDefinitions.IsValidIndex(DefinitionIndex))
    {
        return false;
    }

    const FDMRandomEventDefinition& Definition = CachedEventDefinitions[DefinitionIndex];

    const double ServerNow = MatchGameState->GetServerWorldTimeSeconds();

    const double PlannedStartServerTime = ServerNow + static_cast<double>(Definition.WarningDuration);

    if (LastStartedEventServerTime > 0.0
        && PlannedStartServerTime - LastStartedEventServerTime + static_cast<double>(KINDA_SMALL_NUMBER)
            < MinimumEventStartInterval)
    {
        UE_LOG(
            LogDMRandomEventDirector,
            Log,
            TEXT("Random Event attempt skipped. Reason=MinimumInterval, Attempt=%d"),
            AttemptIndex
        );

        return false;
    }

    /*
     * Event가 Overtime으로 넘어가지 않게
     * 현재 남은 정규 경기 시간을 확인한다.
     */
    const double RequiredRemainingTime = static_cast<double>(Definition.WarningDuration + Definition.Duration);

    if (MatchGameState->GetRemainingRegularTime() + static_cast<double>(KINDA_SMALL_NUMBER) < RequiredRemainingTime)
    {
        UE_LOG(
            LogDMRandomEventDirector,
            Log,
            TEXT("Random Event attempt skipped. Reason=InsufficientRegularTime, Attempt=%d"),
            AttemptIndex
        );

        return false;
    }

    FDMRandomEventRuntimeState NewEventState;

    NewEventState.EventTag = Definition.EventTag;

    NewEventState.WarningStartServerTime = ServerNow;

    NewEventState.StartServerTime = PlannedStartServerTime;

    NewEventState.EndServerTime = PlannedStartServerTime + static_cast<double>(Definition.Duration);

    ActiveDefinitionIndex = DefinitionIndex;

    ActiveEventState = NewEventState;

    if (!PublishPublicState(ActiveEventState))
    {
        ActiveEventState = FDMRandomEventRuntimeState();

        ActiveDefinitionIndex = INDEX_NONE;

        return false;
    }

    UE_LOG(
        LogDMRandomEventDirector,
        Log,
        TEXT("Random Event warning started. Event=%s, Attempt=%d, Warning=%.3f, Duration=%.3f"),
        *Definition.EventTag.ToString(),
        AttemptIndex,
        Definition.WarningDuration,
        Definition.Duration
    );

    /*
     * WarningDuration 0도 데이터상 허용한다.
     */
    if (Definition.WarningDuration <= KINDA_SMALL_NUMBER)
    {
        HandleWarningFinished();

        return true;
    }

    UWorld* World = MatchGameMode->GetWorld();

    if (!IsValid(World))
    {
        CancelScheduledEvent(TEXT("World unavailable"));

        return false;
    }

    World->GetTimerManager().SetTimer(
        WarningTimerHandle, this, &UDMRandomEventDirector::HandleWarningFinished, Definition.WarningDuration, false
    );

    if (!World->GetTimerManager().IsTimerActive(WarningTimerHandle))
    {
        CancelScheduledEvent(TEXT("Warning timer activation failed"));

        return false;
    }

    return true;
}

int32 UDMRandomEventDirector::SelectNextEventDefinitionIndex() const
{
    if (CachedEventDefinitions.IsEmpty())
    {
        return INDEX_NONE;
    }

    TArray<int32> CandidateIndices;

    CandidateIndices.Reserve(CachedEventDefinitions.Num());

    for (int32 Index = 0; Index < CachedEventDefinitions.Num(); ++Index)
    {
        const FDMRandomEventDefinition& Definition = CachedEventDefinitions[Index];

        if (PreviousEventTag.IsValid() && Definition.EventTag == PreviousEventTag)
        {
            continue;
        }

        CandidateIndices.Add(Index);
    }

    if (CandidateIndices.IsEmpty())
    {
        return INDEX_NONE;
    }

    const int32 CandidateArrayIndex = FMath::RandRange(0, CandidateIndices.Num() - 1);

    return CandidateIndices[CandidateArrayIndex];
}

bool UDMRandomEventDirector::AreAllPlayersAlive() const
{
    const ADMMatchGameState* MatchGameState = GetMatchGameState();

    if (!IsValid(MatchGameState))
    {
        return false;
    }

    const int32 StartingPlayerCount = MatchGameState->GetStartingPlayerCount();

    if (!DMMatch::IsValidPlayerCount(StartingPlayerCount) || MatchGameState->PlayerArray.Num() != StartingPlayerCount)
    {
        return false;
    }

    for (APlayerState* PlayerState : MatchGameState->PlayerArray)
    {
        const ADMMatchPlayerState* MatchPlayerState = Cast<ADMMatchPlayerState>(PlayerState);

        if (!IsValid(MatchPlayerState) || MatchPlayerState->GetLifeState() != EDMPlayerLifeState::Alive)
        {
            return false;
        }
    }

    return true;
}

void UDMRandomEventDirector::ClearEventAttemptTimers()
{
    ADMMatchGameMode* MatchGameMode = OwnerGameMode.Get();

    if (!IsValid(MatchGameMode))
    {
        EventAttemptTimerHandles.Reset();

        return;
    }

    UWorld* World = MatchGameMode->GetWorld();

    if (!IsValid(World))
    {
        EventAttemptTimerHandles.Reset();

        return;
    }

    for (FTimerHandle& TimerHandle : EventAttemptTimerHandles)
    {
        World->GetTimerManager().ClearTimer(TimerHandle);
    }

    EventAttemptTimerHandles.Reset();
}

#pragma endregion

#pragma region Active Event

void UDMRandomEventDirector::HandleWarningFinished()
{
    ADMMatchGameMode* MatchGameMode = OwnerGameMode.Get();

    ADMMatchGameState* MatchGameState = GetMatchGameState();

    if (!IsValid(MatchGameMode) || !MatchGameMode->HasAuthority() || !IsValid(MatchGameState)
        || !ActiveEventState.IsScheduledOrActive() || !CachedEventDefinitions.IsValidIndex(ActiveDefinitionIndex))
    {
        return;
    }

    if (UWorld* World = MatchGameMode->GetWorld())
    {
        World->GetTimerManager().ClearTimer(WarningTimerHandle);
    }

    /*
     * Warning 중 누군가 죽었거나
     * Phase가 변경됐다면 실제 Event는 시작하지 않는다.
     */
    if (MatchGameState->GetCurrentPhase() != EDMMatchPhase::Playing)
    {
        CancelScheduledEvent(TEXT("Match is no longer Playing"));

        return;
    }

    if (MatchGameState->HasCurrentQuest())
    {
        CancelScheduledEvent(TEXT("Quest became active"));

        return;
    }

    if (!AreAllPlayersAlive())
    {
        CancelScheduledEvent(TEXT("Not all players are alive at effect start"));

        return;
    }

    const double ServerNow = MatchGameState->GetServerWorldTimeSeconds();

    if (ServerNow >= ActiveEventState.EndServerTime)
    {
        CancelScheduledEvent(TEXT("Event window already expired"));

        return;
    }

    const FDMRandomEventDefinition& Definition = CachedEventDefinitions[ActiveDefinitionIndex];

    /*
     * 같은 Event 연속 금지의 기준은
     * Warning이 아니라 실제 Event 시작이다.
     */
    PreviousEventTag = Definition.EventTag;

    LastStartedEventServerTime = ActiveEventState.StartServerTime;

    const double RemainingDuration = ActiveEventState.EndServerTime - ServerNow;

    UWorld* World = MatchGameMode->GetWorld();

    if (!IsValid(World) || RemainingDuration <= KINDA_SMALL_NUMBER)
    {
        CancelScheduledEvent(TEXT("No remaining active duration"));

        return;
    }

    /*
     * Event별 Gameplay Effect는
     * Active Window 진입 경로에서 실행한다.
     *
     * Effect 적용 전에 EventEndTimer가 유효해야 하며,
     * Effect 시작 실패 시 공통 Cancel 경로로 정리한다.
     */

    World->GetTimerManager().SetTimer(
        EventEndTimerHandle,
        this,
        &UDMRandomEventDirector::HandleEventFinished,
        static_cast<float>(RemainingDuration),
        false
    );

    if (!World->GetTimerManager().IsTimerActive(EventEndTimerHandle))
    {
        CancelScheduledEvent(TEXT("Event end timer activation failed"));

        return;
    }

    /*
     * InputReverse는 Character의 동시 Status로 관리한다.
     *
     * EventDirector는 반전 bool을 별도로 저장하지 않고,
     * 각 Player Character의 CharacterStateComponent에
     * authoritative Status만 적용한다.
     */
    if (Definition.EventTag == DMGameplayTags::RandomEvent_InputReverse)
    {
        if (!ApplyInputReverseEffect(static_cast<float>(RemainingDuration)))
        {
            CancelScheduledEvent(TEXT("InputReverse effect application failed"));

            return;
        }
    }

    /*
     * Panic은 Server AI Gameplay Effect이므로
     * GameMode의 기존 NPC Reaction Bridge를 통해
     * PopulationDirector에 전달한다.
     *
     * EventDirector는 NPC Registry나
     * 개별 AIController를 직접 참조하지 않는다.
     */
    if (Definition.EventTag == DMGameplayTags::RandomEvent_Panic)
    {
        MatchGameMode->RequestNPCPanic();
    }

    UE_LOG(
        LogDMRandomEventDirector,
        Log,
        TEXT("Random Event active window started. Event=%s, EndServerTime=%.3f"),
        *Definition.EventTag.ToString(),
        ActiveEventState.EndServerTime
    );
}

void UDMRandomEventDirector::HandleEventFinished()
{
    if (!ActiveEventState.IsScheduledOrActive())
    {
        return;
    }

    const FGameplayTag FinishedEventTag = ActiveEventState.EventTag;

    if (FinishedEventTag == DMGameplayTags::RandomEvent_InputReverse)
    {
        RemoveInputReverseEffect();
    }

    /*
     * Event별 Gameplay Effect 종료 처리는
     * Public State를 지우기 전에 이 위치에서 수행한다.
     */

    ClearActiveEventTimers();

    if (OwnerGameMode.IsValid())
    {
        PublishPublicState(FDMRandomEventRuntimeState());
    }

    ActiveEventState = FDMRandomEventRuntimeState();

    ActiveDefinitionIndex = INDEX_NONE;

    UE_LOG(LogDMRandomEventDirector, Log, TEXT("Random Event finished. Event=%s"), *FinishedEventTag.ToString());
}

void UDMRandomEventDirector::CancelScheduledEvent(const TCHAR* Reason)
{
    const FGameplayTag CancelledEventTag = ActiveEventState.EventTag;

    if (CancelledEventTag == DMGameplayTags::RandomEvent_InputReverse)
    {
        RemoveInputReverseEffect();
    }

    ClearActiveEventTimers();

    if (OwnerGameMode.IsValid())
    {
        PublishPublicState(FDMRandomEventRuntimeState());
    }

    ActiveEventState = FDMRandomEventRuntimeState();

    ActiveDefinitionIndex = INDEX_NONE;

    UE_LOG(
        LogDMRandomEventDirector,
        Log,
        TEXT("Random Event cancelled. Event=%s, Reason=%s"),
        *CancelledEventTag.ToString(),
        Reason != nullptr ? Reason : TEXT("Unknown")
    );
}

void UDMRandomEventDirector::ClearActiveEventTimers()
{
    ADMMatchGameMode* MatchGameMode = OwnerGameMode.Get();

    if (!IsValid(MatchGameMode))
    {
        WarningTimerHandle.Invalidate();
        EventEndTimerHandle.Invalidate();

        return;
    }

    UWorld* World = MatchGameMode->GetWorld();

    if (!IsValid(World))
    {
        WarningTimerHandle.Invalidate();
        EventEndTimerHandle.Invalidate();

        return;
    }

    World->GetTimerManager().ClearTimer(WarningTimerHandle);

    World->GetTimerManager().ClearTimer(EventEndTimerHandle);
}

#pragma endregion

#pragma region Input Reverse

bool UDMRandomEventDirector::ApplyInputReverseEffect(const float EffectDuration)
{
    if (EffectDuration <= 0.0f)
    {
        return false;
    }

    ADMMatchGameState* MatchGameState = GetMatchGameState();

    if (!IsValid(MatchGameState))
    {
        return false;
    }

    /*
     * 먼저 모든 대상을 검증한 뒤 적용한다.
     *
     * 일부 Player에게만 적용된 상태에서
     * 중간 실패하는 partial commit을 방지한다.
     */
    TArray<UDMCharacterStateComponent*> TargetStateComponents;

    TargetStateComponents.Reserve(MatchGameState->PlayerArray.Num());

    for (APlayerState* PlayerState : MatchGameState->PlayerArray)
    {
        ADMMatchPlayerState* MatchPlayerState = Cast<ADMMatchPlayerState>(PlayerState);

        if (!IsValid(MatchPlayerState) || MatchPlayerState->GetLifeState() != EDMPlayerLifeState::Alive)
        {
            return false;
        }

        ADMPlayerCharacter* PlayerCharacter = MatchPlayerState->GetPawn<ADMPlayerCharacter>();

        if (!IsValid(PlayerCharacter))
        {
            return false;
        }

        UDMCharacterStateComponent* StateComponent = PlayerCharacter->GetCharacterStateComponent();

        if (!IsValid(StateComponent))
        {
            return false;
        }

        /*
         * 현재 InputReverse의 유일한 적용 원본은
         * Random Event다.
         *
         * 이미 같은 Status가 존재하는 상태에서
         * Timer를 덮어쓰지 않는다.
         */
        if (StateComponent->HasStatus(DMGameplayTags::Status_InputReversed))
        {
            return false;
        }

        TargetStateComponents.Add(StateComponent);
    }

    if (TargetStateComponents.IsEmpty())
    {
        return false;
    }

    TArray<UDMCharacterStateComponent*> AppliedStateComponents;

    AppliedStateComponents.Reserve(TargetStateComponents.Num());

    for (UDMCharacterStateComponent* StateComponent : TargetStateComponents)
    {
        if (!StateComponent->ApplyTimedStatus(DMGameplayTags::Status_InputReversed, EffectDuration))
        {
            /*
             * All-or-nothing.
             *
             * 앞에서 이미 적용된 Player가 있다면
             * 전부 rollback한다.
             */
            for (UDMCharacterStateComponent* AppliedComponent : AppliedStateComponents)
            {
                if (IsValid(AppliedComponent))
                {
                    AppliedComponent->RemoveStatus(DMGameplayTags::Status_InputReversed);
                }
            }

            return false;
        }

        AppliedStateComponents.Add(StateComponent);
    }

    UE_LOG(
        LogDMRandomEventDirector,
        Log,
        TEXT("InputReverse effect applied. Players=%d, Duration=%.3f"),
        AppliedStateComponents.Num(),
        EffectDuration
    );

    return true;
}

void UDMRandomEventDirector::RemoveInputReverseEffect()
{
    ADMMatchGameState* MatchGameState = GetMatchGameState();

    if (!IsValid(MatchGameState))
    {
        return;
    }

    int32 RemovedCount = 0;

    for (ADMMatchPlayerState* MatchPlayerState : DMGameplay::GetMatchPlayerStates(MatchGameState))
    {
        ADMPlayerCharacter* PlayerCharacter = MatchPlayerState->GetPawn<ADMPlayerCharacter>();

        if (!IsValid(PlayerCharacter))
        {
            continue;
        }

        UDMCharacterStateComponent* StateComponent = PlayerCharacter->GetCharacterStateComponent();

        if (!IsValid(StateComponent) || !StateComponent->HasStatus(DMGameplayTags::Status_InputReversed))
        {
            continue;
        }

        if (StateComponent->RemoveStatus(DMGameplayTags::Status_InputReversed))
        {
            ++RemovedCount;
        }
    }

    UE_LOG(LogDMRandomEventDirector, Verbose, TEXT("InputReverse effect removed. Players=%d"), RemovedCount);
}

#pragma endregion

#pragma region Helpers

ADMMatchGameState* UDMRandomEventDirector::GetMatchGameState() const
{
    const ADMMatchGameMode* MatchGameMode = OwnerGameMode.Get();

    if (!IsValid(MatchGameMode))
    {
        return nullptr;
    }

    return MatchGameMode->GetGameState<ADMMatchGameState>();
}

bool UDMRandomEventDirector::PublishPublicState(const FDMRandomEventRuntimeState& NewPublicState) const
{
    return PublicStatePublisher.IsBound() && PublicStatePublisher.Execute(NewPublicState);
}

#pragma endregion