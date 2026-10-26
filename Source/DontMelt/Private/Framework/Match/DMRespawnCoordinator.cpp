#include "Framework/Match/DMRespawnCoordinator.h"

#include "Characters/DMPlayerCharacter.h"
#include "Characters/Components/DMCharacterStateComponent.h"
#include "Characters/Components/DMCombatComponent.h"
#include "Core/DMGameplayTags.h"
#include "Core/DMMatchConstants.h"
#include "Data/DMMatchRuleDataAsset.h"
#include "Framework/Match/DMMatchGameMode.h"
#include "Framework/Match/DMMatchGameState.h"
#include "Framework/Match/DMMatchPlayerController.h"
#include "Framework/Match/DMMatchPlayerState.h"
#include "Framework/Match/DMRespawnPoint.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMRespawn, Log, All);

#pragma region Lifecycle

void UDMRespawnCoordinator::Initialize(
    ADMMatchGameMode* InOwnerGameMode, UDMMatchRuleDataAsset* InMatchRuleData, const float InRespawnRetryDelaySeconds
)
{
    OwnerGameMode = InOwnerGameMode;

    MatchRuleData = InMatchRuleData;

    RespawnRetryDelaySeconds = InRespawnRetryDelaySeconds;
}

UWorld* UDMRespawnCoordinator::GetWorld() const
{
    return OwnerGameMode != nullptr ? OwnerGameMode->GetWorld() : nullptr;
}

FTimerManager& UDMRespawnCoordinator::GetWorldTimerManager() const
{
    return OwnerGameMode->GetWorldTimerManager();
}

template <class T>
T* UDMRespawnCoordinator::GetGameState() const
{
    return OwnerGameMode != nullptr ? OwnerGameMode->GetGameState<T>() : nullptr;
}

#pragma endregion

#pragma region Respawn

bool UDMRespawnCoordinator::ValidateRespawnPointsForStartingRoster(FString& OutFailureReason) const
{
    OutFailureReason.Reset();

    const ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        OutFailureReason = TEXT("MatchGameState is unavailable.");

        return false;
    }

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        OutFailureReason = TEXT("World is unavailable.");

        return false;
    }

    for (APlayerState* PlayerState : MatchGameState->PlayerArray)
    {
        const ADMMatchPlayerState* MatchPlayerState = Cast<ADMMatchPlayerState>(PlayerState);

        if (!IsValid(MatchPlayerState) || !MatchPlayerState->HasAssignedBaseSlot())
        {
            OutFailureReason = TEXT("Starting PlayerState has no valid BaseSlot.");

            return false;
        }

        const int32 BaseSlot = MatchPlayerState->GetBaseSlot();

        bool bFoundValidRespawnPoint = false;

        for (TActorIterator<ADMRespawnPoint> Iterator(World); Iterator; ++Iterator)
        {
            const ADMRespawnPoint* RespawnPoint = *Iterator;

            if (!IsValid(RespawnPoint) || !RespawnPoint->IsRespawnEnabled() || RespawnPoint->GetBaseSlot() != BaseSlot
                || RespawnPoint->GetWeight() <= 0.0f)
            {
                continue;
            }

            bFoundValidRespawnPoint = true;

            break;
        }

        if (!bFoundValidRespawnPoint)
        {
            OutFailureReason = FString::Printf(TEXT("No valid RespawnPoint exists for BaseSlot=%d."), BaseSlot);

            return false;
        }
    }

    return true;
}

ADMRespawnPoint* UDMRespawnCoordinator::FindRandomRespawnPointForBaseSlot(const int32 BaseSlot) const
{
    if (!DMMatch::IsValidBaseSlot(BaseSlot))
    {
        return nullptr;
    }

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return nullptr;
    }

    TArray<ADMRespawnPoint*> ValidRespawnPoints;

    float TotalWeight = 0.0f;

    for (TActorIterator<ADMRespawnPoint> Iterator(World); Iterator; ++Iterator)
    {
        ADMRespawnPoint* RespawnPoint = *Iterator;

        if (!IsValid(RespawnPoint) || !RespawnPoint->IsRespawnEnabled() || RespawnPoint->GetBaseSlot() != BaseSlot
            || RespawnPoint->GetWeight() <= 0.0f)
        {
            continue;
        }

        ValidRespawnPoints.Add(RespawnPoint);

        TotalWeight += RespawnPoint->GetWeight();
    }

    if (ValidRespawnPoints.IsEmpty() || TotalWeight <= 0.0f)
    {
        UE_LOG(LogDMRespawn, Error, TEXT("RespawnPoint selection failed: no valid candidate. BaseSlot=%d"), BaseSlot);

        return nullptr;
    }

    float RandomWeight = FMath::FRandRange(0.0f, TotalWeight);

    for (ADMRespawnPoint* RespawnPoint : ValidRespawnPoints)
    {
        RandomWeight -= RespawnPoint->GetWeight();

        if (RandomWeight <= 0.0f)
        {
            return RespawnPoint;
        }
    }

    /*
     * Float 경계 오차용 fallback.
     */
    return ValidRespawnPoints.Last();
}

AController* UDMRespawnCoordinator::FindControllerForPlayerState(const ADMMatchPlayerState* MatchPlayerState) const
{
    if (!IsValid(MatchPlayerState))
    {
        return nullptr;
    }

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return nullptr;
    }

    for (FConstPlayerControllerIterator Iterator = World->GetPlayerControllerIterator(); Iterator; ++Iterator)
    {
        APlayerController* PlayerController = Iterator->Get();

        if (IsValid(PlayerController) && PlayerController->GetPlayerState<ADMMatchPlayerState>() == MatchPlayerState)
        {
            return PlayerController;
        }
    }

    return nullptr;
}

void UDMRespawnCoordinator::ClearRespawnTimer(ADMMatchPlayerState* MatchPlayerState)
{
    if (!IsValid(MatchPlayerState))
    {
        return;
    }

    const TWeakObjectPtr<ADMMatchPlayerState> PlayerStateKey(MatchPlayerState);
    if (FTimerHandle* TimerHandle = RespawnTimerHandles.Find(PlayerStateKey))
    {
        GetWorldTimerManager().ClearTimer(*TimerHandle);
        RespawnTimerHandles.Remove(PlayerStateKey);
    }
    RespawnRetryAttemptCounts.Remove(PlayerStateKey);
}

void UDMRespawnCoordinator::ClearAllRespawnTimers()
{
    for (TPair<TWeakObjectPtr<ADMMatchPlayerState>, FTimerHandle>& TimerPair : RespawnTimerHandles)
    {
        GetWorldTimerManager().ClearTimer(TimerPair.Value);
    }

    RespawnTimerHandles.Empty();
    RespawnRetryAttemptCounts.Empty();
}

bool UDMRespawnCoordinator::StartRespawnLifecycle(ADMMatchPlayerState* MatchPlayerState)
{
    if (!IsValid(MatchPlayerState) || !IsValid(MatchRuleData))
    {
        return false;
    }

    if (MatchPlayerState->GetLifeState() != EDMPlayerLifeState::Dead)
    {
        return false;
    }

    const float RespawnDuration = MatchRuleData->RespawnDuration;

    if (!ensureMsgf(
            RespawnDuration > 0.0f,
            TEXT("Respawn lifecycle failed: RespawnDuration must be positive. Duration=%.3f"),
            RespawnDuration
        ))
    {
        return false;
    }

    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState) || MatchGameState->GetMatchEndReason() != EDMMatchEndReason::None)
    {
        return false;
    }

    const EDMMatchPhase CurrentPhase = MatchGameState->GetCurrentPhase();

    if (CurrentPhase != EDMMatchPhase::Playing && CurrentPhase != EDMMatchPhase::Overtime)
    {
        return false;
    }

    ClearRespawnTimer(MatchPlayerState);

    /*
     * Dead는 사망 Commit 상태.
     *
     * 실제 30초 대기 구간에 들어가면
     * 서버 LifeState는 Spectating으로 전환한다.
     *
     * 구체적인 Corpse/Killer/FreeCamera/Fade는
     * 이후 로컬 Spectator Presentation 단계.
     */
    MatchPlayerState->SetLifeState(EDMPlayerLifeState::Spectating);

    MatchPlayerState->ForceNetUpdate();

    const TWeakObjectPtr<ADMMatchPlayerState> PlayerStateKey(MatchPlayerState);

    FTimerHandle& RespawnTimerHandle = RespawnTimerHandles.FindOrAdd(PlayerStateKey);

    FTimerDelegate RespawnTimerDelegate;

    RespawnTimerDelegate.BindUObject(this, &ThisClass::HandleRespawnTimerExpired, MatchPlayerState);

    GetWorldTimerManager().SetTimer(RespawnTimerHandle, RespawnTimerDelegate, RespawnDuration, false);

    UE_LOG(
        LogDMRespawn,
        Log,
        TEXT("Respawn lifecycle started. PlayerName=%s, BaseSlot=%d, Duration=%.3f, LifeState=Spectating"),
        *MatchPlayerState->GetPlayerName(),
        MatchPlayerState->GetBaseSlot(),
        RespawnDuration
    );

    return true;
}

void UDMRespawnCoordinator::HandleRespawnTimerExpired(ADMMatchPlayerState* MatchPlayerState)
{
    if (!IsValid(MatchPlayerState))
    {
        return;
    }

    const TWeakObjectPtr<ADMMatchPlayerState> PlayerStateKey(MatchPlayerState);

    /*
     * 현재 실행 중인 Timer Handle 기록 제거.
     */
    RespawnTimerHandles.Remove(PlayerStateKey);

    if (MatchPlayerState->GetLifeState() != EDMPlayerLifeState::Spectating)
    {
        UE_LOG(
            LogDMRespawn,
            Verbose,
            TEXT("Respawn skipped: LifeState is no longer Spectating. PlayerName=%s, LifeState=%s"),
            *MatchPlayerState->GetPlayerName(),
            *UEnum::GetValueAsString(MatchPlayerState->GetLifeState())
        );

        return;
    }

    ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState) || MatchGameState->GetMatchEndReason() != EDMMatchEndReason::None
        || MatchGameState->GetCurrentPhase() == EDMMatchPhase::Finished
        || MatchGameState->GetCurrentPhase() == EDMMatchPhase::Aborted)
    {
        UE_LOG(
            LogDMRespawn,
            Verbose,
            TEXT("Respawn cancelled: Match is no longer active. PlayerName=%s"),
            *MatchPlayerState->GetPlayerName()
        );

        return;
    }

    AController* PlayerController = FindControllerForPlayerState(MatchPlayerState);

    if (!IsValid(PlayerController))
    {
        UE_LOG(
            LogDMRespawn,
            Error,
            TEXT("Respawn failed: Controller was not found. PlayerName=%s"),
            *MatchPlayerState->GetPlayerName()
        );

        ScheduleRespawnRetry(MatchPlayerState);
        return;
    }

    const int32 BaseSlot = MatchPlayerState->GetBaseSlot();

    ADMRespawnPoint* RespawnPoint = FindRandomRespawnPointForBaseSlot(BaseSlot);

    if (!IsValid(RespawnPoint))
    {
        UE_LOG(
            LogDMRespawn,
            Error,
            TEXT("Respawn failed: valid RespawnPoint unavailable. PlayerName=%s, BaseSlot=%d"),
            *MatchPlayerState->GetPlayerName(),
            BaseSlot
        );

        // 리스폰 실패로 Spectating에 영구 고립되지 않도록 재시도를 예약한다.
        ScheduleRespawnRetry(MatchPlayerState);

        return;
    }

    /*
     * 아직 실제 Pawn Spawn이 완료되지 않았으므로
     * Alive로 변경하면 안 된다.
     */
    MatchPlayerState->SetLifeState(EDMPlayerLifeState::Respawning);

    MatchPlayerState->ForceNetUpdate();

    APawn* PreviousPawn = PlayerController->GetPawn();

    if (IsValid(PreviousPawn))
    {
        PlayerController->UnPossess();
    }

    /*
     * 기존 초기 Spawn의 ChoosePlayerStart를
     * 통하지 않는다.
     *
     * 이미 서버가 선택한 전용 RespawnPoint를
     * 엔진 Restart Pipeline에 직접 전달한다.
     */
    OwnerGameMode->RestartPlayerAtPlayerStart(PlayerController, RespawnPoint);

    ADMPlayerCharacter* NewPlayerCharacter = PlayerController->GetPawn<ADMPlayerCharacter>();

    const bool bRespawnSucceeded = IsValid(NewPlayerCharacter)
        && NewPlayerCharacter->GetController() == PlayerController
        && NewPlayerCharacter->GetPlayerState<ADMMatchPlayerState>() == MatchPlayerState;

    if (!bRespawnSucceeded)
    {
        UE_LOG(
            LogDMRespawn,
            Error,
            TEXT(
                "Respawn failed: new Pawn spawn or Possess validation failed. PlayerName=%s, BaseSlot=%d, RespawnPoint=%s"
            ),
            *MatchPlayerState->GetPlayerName(),
            BaseSlot,
            *GetNameSafe(RespawnPoint)
        );

        /*
         * 실패 시 Alive로 절대 만들지 않는다.
         */
        MatchPlayerState->SetLifeState(EDMPlayerLifeState::Spectating);

        MatchPlayerState->ForceNetUpdate();

        // Spawn 자체는 성공했지만 Possess/PlayerState 검증이 실패한 새 Pawn을 남기지 않는다.
        if (IsValid(NewPlayerCharacter) && NewPlayerCharacter != PreviousPawn)
        {
            if (PlayerController->GetPawn() == NewPlayerCharacter)
            {
                PlayerController->UnPossess();
            }
            NewPlayerCharacter->Destroy();
        }

        /*
         * 개발 중 Spawn 설정 오류가 발생하더라도
         * 기존 Pawn이 살아 있다면 다시 연결해
         * Controller가 완전히 고아가 되는 것을 막는다.
         */
        if (IsValid(PreviousPawn) && PlayerController->GetPawn() == nullptr)
        {
            PlayerController->Possess(PreviousPawn);
        }

        // 리스폰 실패로 Spectating에 영구 고립되지 않도록 재시도를 예약한다.
        ScheduleRespawnRetry(MatchPlayerState);

        return;
    }

    /*
     * ==================================================
     * Spawn Protection
     * ==================================================
     *
     * Respawn된 Pawn이 실제 Gameplay에 진입하기 전에
     * Status.SpawnProtected를 먼저 적용한다.
     *
     * LifeState는 아직 Respawning 상태다.
     */
    UDMCharacterStateComponent* NewStateComponent = NewPlayerCharacter->GetCharacterStateComponent();

    UDMCombatComponent* NewCombatComponent = NewPlayerCharacter->FindComponentByClass<UDMCombatComponent>();

    float SpawnProtectionDuration = 0.0f;

    if (IsValid(NewCombatComponent))
    {
        SpawnProtectionDuration = NewCombatComponent->GetSpawnProtectionDuration();
    }

    const bool bSpawnProtectionApplied = IsValid(NewStateComponent) && IsValid(NewCombatComponent)
        && SpawnProtectionDuration > 0.0f
        && NewStateComponent->ApplyTimedStatus(DMGameplayTags::Status_SpawnProtected, SpawnProtectionDuration);

    if (!bSpawnProtectionApplied)
    {
        UE_LOG(
            LogDMRespawn,
            Error,
            TEXT(
                "Respawn failed: SpawnProtection could not be applied. PlayerName=%s, StateComponent=%s, CombatComponent=%s, Duration=%.3f"
            ),
            *MatchPlayerState->GetPlayerName(),
            *GetNameSafe(NewStateComponent),
            *GetNameSafe(NewCombatComponent),
            SpawnProtectionDuration
        );

        /*
         * 보호 상태 없이 Alive가 되는 것을
         * 허용하지 않는다.
         */
        if (PlayerController->GetPawn() == NewPlayerCharacter)
        {
            PlayerController->UnPossess();
        }

        NewPlayerCharacter->Destroy();

        MatchPlayerState->SetLifeState(EDMPlayerLifeState::Spectating);

        MatchPlayerState->ForceNetUpdate();

        /*
         * 개발 중 설정 오류라면 기존 시체 Pawn이
         * 아직 존재할 경우 다시 연결한다.
         */
        if (IsValid(PreviousPawn))
        {
            PlayerController->Possess(PreviousPawn);
        }

        // 리스폰 실패로 Spectating에 영구 고립되지 않도록 재시도를 예약한다.
        ScheduleRespawnRetry(MatchPlayerState);

        return;
    }

    UE_LOG(
        LogDMRespawn,
        Log,
        TEXT("SpawnProtection applied. PlayerName=%s, Pawn=%s, Duration=%.3f"),
        *MatchPlayerState->GetPlayerName(),
        *GetNameSafe(NewPlayerCharacter),
        SpawnProtectionDuration
    );

    /*
     * 보호 상태까지 정상적으로 준비된 뒤
     * 이전 Ragdoll/Spectator Pawn을 제거한다.
     */
    if (IsValid(PreviousPawn) && PreviousPawn != NewPlayerCharacter)
    {
        PreviousPawn->Destroy();
    }

    MatchPlayerState->SetLifeState(EDMPlayerLifeState::Alive);
    RespawnRetryAttemptCounts.Remove(PlayerStateKey);

    if (ADMMatchPlayerController* MatchPlayerController = Cast<ADMMatchPlayerController>(PlayerController))
    {
        MatchPlayerController->ClientFinalizeSpectatorRespawnHandoff(NewPlayerCharacter);
    }
    else
    {
        UE_LOG(
            LogDMRespawn,
            Error,
            TEXT("Respawn Presentation Handoff failed: MatchPlayerController is unavailable. PlayerName=%s"),
            *MatchPlayerState->GetPlayerName()
        );
    }

    MatchPlayerState->ForceNetUpdate();

    UE_LOG(
        LogDMRespawn,
        Log,
        TEXT("Player respawn completed. PlayerName=%s, BaseSlot=%d, RespawnPoint=%s, NewPawn=%s, LifeState=Alive"),
        *MatchPlayerState->GetPlayerName(),
        BaseSlot,
        *GetNameSafe(RespawnPoint),
        *GetNameSafe(NewPlayerCharacter)
    );
}

void UDMRespawnCoordinator::ScheduleRespawnRetry(ADMMatchPlayerState* MatchPlayerState)
{
    // 경기가 진행 중이고 Spectating 상태일 때만 재시도한다.
    if (!IsValid(MatchPlayerState) || MatchPlayerState->GetLifeState() != EDMPlayerLifeState::Spectating)
    {
        return;
    }

    const ADMMatchGameState* MatchGameState = GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState) || MatchGameState->GetMatchEndReason() != EDMMatchEndReason::None
        || (MatchGameState->GetCurrentPhase() != EDMMatchPhase::Playing
            && MatchGameState->GetCurrentPhase() != EDMMatchPhase::Overtime))
    {
        return;
    }

    const TWeakObjectPtr<ADMMatchPlayerState> RetryKey(MatchPlayerState);
    int32& AttemptCount = RespawnRetryAttemptCounts.FindOrAdd(RetryKey);
    constexpr int32 MaxRespawnRetryAttempts = 3;

    if (AttemptCount >= MaxRespawnRetryAttempts)
    {
        UE_LOG(
            LogDMRespawn, Error,
            TEXT("Respawn failed permanently: retry budget exhausted. Player=%s, Attempts=%d"),
            *MatchPlayerState->GetPlayerName(), AttemptCount
        );

        // 복구 불가 상황을 Spectating 상태로 숨기지 않는다.
        if (IsValid(OwnerGameMode))
        {
            OwnerGameMode->TryAbortMatch(EDMMatchEndReason::InternalError);
        }
        return;
    }
    ++AttemptCount;

    FTimerHandle& RetryTimerHandle =
        RespawnTimerHandles.FindOrAdd(RetryKey);

    FTimerDelegate RetryTimerDelegate;

    RetryTimerDelegate.BindUObject(this, &ThisClass::HandleRespawnTimerExpired, MatchPlayerState);

    GetWorldTimerManager().SetTimer(RetryTimerHandle, RetryTimerDelegate, FMath::Max(0.05f, RespawnRetryDelaySeconds), false);

    if (!GetWorldTimerManager().IsTimerActive(RetryTimerHandle))
    {
        UE_LOG(LogDMRespawn, Error, TEXT("Respawn retry timer could not be scheduled."));
        if (IsValid(OwnerGameMode))
        {
            OwnerGameMode->TryAbortMatch(EDMMatchEndReason::InternalError);
        }
        return;
    }

    UE_LOG(
        LogDMRespawn,
        Warning,
        TEXT("Scheduled respawn retry. PlayerName=%s, Delay=%.2f"),
        *MatchPlayerState->GetPlayerName(),
        RespawnRetryDelaySeconds
    );
}

#pragma endregion
