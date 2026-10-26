#include "Characters/Components/DMCombatComponent.h"

#include "Core/DMGameplayUtils.h"

#include "Data/DMCombatRuleDataAsset.h"
#include "Characters/DMPlayerCharacter.h"
#include "Characters/Components/DMCharacterStateComponent.h"
#include "Characters/Components/DMPoseComponent.h"
#include "Characters/DMJellyCharacterBase.h"
#include "Characters/DMNPCCharacter.h"
#include "Components/SkeletalMeshComponent.h"
#include "Core/DMMatchTypes.h"
#include "Core/DMGameplayTags.h"
#include "Framework/Match/DMMatchGameState.h"
#include "Framework/Match/DMMatchPlayerState.h"
#include "Framework/Match/DMMatchGameMode.h"
#include "Animation/AnimInstance.h"
#include "Animation/AnimMontage.h"

#include "CollisionQueryParams.h"
#include "CollisionShape.h"
#include "Engine/OverlapResult.h"
#include "Engine/EngineTypes.h"
#include "Engine/World.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMCombat, Log, All);

namespace
{
    // 서버가 예측 공격을 거절했을 때 로컬 몽타주를 끊어내는 블렌드 아웃 시간.
    constexpr float PredictedAttackRejectBlendOutSeconds = 0.1f;

    // 원격 클라이언트의 서버 시간 추정 오차를 흡수하는 쿨다운 여유(초).
    constexpr double RemoteCooldownToleranceSeconds = 0.1;

    // 서버가 원격 클라이언트의 요청을 검사할 때만 여유를 준다.
    // 호스트 본인과 클라이언트 로컬 검사는 기존과 같다.
    double GetRemoteCooldownTolerance(const UActorComponent* Component)
    {
        const APawn* OwnerPawn = Component != nullptr ? Cast<APawn>(Component->GetOwner()) : nullptr;

        const bool bServerCheckingRemoteClient =
            IsValid(OwnerPawn) && OwnerPawn->HasAuthority() && !OwnerPawn->IsLocallyControlled();

        return bServerCheckingRemoteClient ? RemoteCooldownToleranceSeconds : 0.0;
    }
}
#pragma region Lifecycle

UDMCombatComponent::UDMCombatComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    SetIsReplicatedByDefault(true);
}

void UDMCombatComponent::BeginPlay()
{
    Super::BeginPlay();

    // Unreliable 연출 RPC가 유실되어도 복제된 ActionState로 연출을 맞춘다.
    if (const ADMPlayerCharacter* OwnerCharacter = GetOwner<ADMPlayerCharacter>())
    {
        if (UDMCharacterStateComponent* StateComponent = OwnerCharacter->GetCharacterStateComponent())
        {
            StateComponent->OnReplicatedActionStateChanged().AddUObject(
                this, &ThisClass::HandleReplicatedActionStateChanged
            );
        }
    }
    if (!ensureMsgf(CombatRuleData != nullptr, TEXT("[%s] CombatRuleData is null."), *GetNameSafe(GetOwner())))
    {
        return;
    }

    UE_LOG(
        LogDMCombat,
        Log,
        TEXT("[%s] CombatComponent initialized. AttackCooldown=%.2f, DefenseCooldown=%.2f"),
        *GetNameSafe(GetOwner()),
        CombatRuleData->GetAttackCooldown(),
        CombatRuleData->GetDefenseCooldown()
    );
}

void UDMCombatComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    if (const ADMPlayerCharacter* OwnerCharacter = GetOwner<ADMPlayerCharacter>())
    {
        if (UDMCharacterStateComponent* StateComponent = OwnerCharacter->GetCharacterStateComponent())
        {
            StateComponent->OnReplicatedActionStateChanged().RemoveAll(this);
        }
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DefenseDurationTimerHandle);
    }

    Super::EndPlay(EndPlayReason);
}

#pragma endregion

#pragma region Respawn Protection Query

float UDMCombatComponent::GetSpawnProtectionDuration() const
{
    if (!IsValid(CombatRuleData))
    {
        return 0.0f;
    }

    return CombatRuleData->GetSpawnProtectionDuration();
}

#pragma endregion

#pragma region Attack Request

void UDMCombatComponent::RequestAttack()
{
    const ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());

    if (OwnerCharacter == nullptr || !OwnerCharacter->IsLocallyControlled())
    {
        return;
    }

    const UDMCharacterStateComponent* StateComponent = OwnerCharacter->GetCharacterStateComponent();

    /*
     * Invisibility의 특수 Attack 규칙.
     *
     * 일반 CanRequestAttack()은 Status.Invisible 때문에
     * Attack을 차단한다.
     *
     * 하지만 "Attack 입력 즉시 투명화 해제"를 위해
     * Invisible 상태에서는 일반 Local validation을 우회하고
     * Server에 Attack 의도를 전달한다.
     *
     * 실제 투명 해제와 공격 승인 여부는 Server가 결정한다.
     */
    if (IsValid(StateComponent) && StateComponent->HasStatus(DMGameplayTags::Status_Invisible))
    {
        // 투명화 해제와 공격 승인은 서버가 함께 결정하므로 예측하지 않는다.
        bLastAttackRequestPredicted = false;
        ServerRequestAttack(0);

        return;
    }

    FString FailureReason;

    if (!CanRequestAttack(FailureReason))
    {
        UE_LOG(
            LogDMCombat,
            Verbose,
            TEXT("[%s] Local attack request blocked. Reason=%s"),
            *GetNameSafe(OwnerCharacter),
            *FailureReason
        );

        return;
    }

    /*
     * 원격 소유 클라이언트는 서버 응답을 기다리지 않고 공격 연출을 먼저 재생한다.
     * 리슨 서버 호스트는 서버 처리가 즉시 일어나므로 예측하지 않는다.
     */
    uint32 PredictionId = 0;

    if (!OwnerCharacter->HasAuthority())
    {
        TryStartPredictedAttack(PredictionId);
    }

    bLastAttackRequestPredicted = PredictionId != 0;

    ServerRequestAttack(PredictionId);
}

bool UDMCombatComponent::CanRequestAttack(FString& OutFailureReason) const
{
    OutFailureReason.Reset();

    const ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());

    if (OwnerCharacter == nullptr)
    {
        OutFailureReason = TEXT("InvalidOwner");

        return false;
    }

    if (!OwnerCharacter->IsPlayerControlled())
    {
        OutFailureReason = TEXT("NotPlayerControlled");

        return false;
    }

    if (CombatRuleData == nullptr)
    {
        OutFailureReason = TEXT("MissingCombatRuleData");

        return false;
    }

    if (!OwnerCharacter->CanPlayAttackMontage())
    {
        OutFailureReason = TEXT("AttackMontageUnavailable");

        return false;
    }

    const UWorld* World = GetWorld();

    if (World == nullptr)
    {
        OutFailureReason = TEXT("InvalidWorld");

        return false;
    }

    const ADMMatchGameState* MatchGameState = World->GetGameState<ADMMatchGameState>();

    if (MatchGameState == nullptr)
    {
        OutFailureReason = TEXT("MissingMatchGameState");

        return false;
    }

    const EDMMatchPhase CurrentPhase = MatchGameState->GetCurrentPhase();

    if (CurrentPhase != EDMMatchPhase::Playing && CurrentPhase != EDMMatchPhase::Overtime)
    {
        OutFailureReason = TEXT("InvalidMatchPhase");

        return false;
    }

    const UDMCharacterStateComponent* StateComponent = OwnerCharacter->GetCharacterStateComponent();

    if (StateComponent == nullptr)
    {
        OutFailureReason = TEXT("MissingCharacterState");

        return false;
    }

    if (!StateComponent->CanPerformAction(EDMActionRequest::Attack))
    {
        OutFailureReason = TEXT("CharacterStateBlocked");

        return false;
    }

    const UDMPoseComponent* PoseComponent = OwnerCharacter->FindComponentByClass<UDMPoseComponent>();

    if (PoseComponent != nullptr && !PoseComponent->IsActionAllowedByPose(EDMActionRequest::Attack))
    {
        OutFailureReason = TEXT("PoseBlocked");

        return false;
    }

    if (!IsAttackCooldownReady())
    {
        OutFailureReason = TEXT("AttackCooldown");

        return false;
    }

    return true;
}

void UDMCombatComponent::ServerRequestAttack_Implementation(const uint32 PredictionId)
{
    ADMPlayerCharacter* OwnerCharacter = GetOwner<ADMPlayerCharacter>();

    if (!IsValid(OwnerCharacter) || !OwnerCharacter->HasAuthority())
    {
        return;
    }

    UDMCharacterStateComponent* StateComponent = OwnerCharacter->GetCharacterStateComponent();

    // 거절된 예측 공격은 소유 클라이언트에 알려 로컬 연출을 되돌린다.
    auto RejectPrediction = [this, PredictionId]()
    {
        if (PredictionId != 0)
        {
            ClientRejectPredictedAttack(PredictionId);
        }
    };

    if (!IsValid(StateComponent))
    {
        RejectPrediction();
        return;
    }

    /*
     * Attack 입력 자체가 Invisibility break trigger다.
     *
     * 따라서 기존 Attack cooldown / ActionState /
     * Pose 등의 검증보다 먼저 투명화를 해제한다.
     *
     * 이후 Attack 자체가 실패하더라도
     * 투명화는 이미 해제된 상태를 유지한다.
     */
    if (StateComponent->HasStatus(DMGameplayTags::Status_Invisible))
    {
        if (!StateComponent->RemoveStatus(DMGameplayTags::Status_Invisible))
        {
            UE_LOG(
                LogDMCombat,
                Warning,
                TEXT("[%s] Attack request failed to break Invisibility."),
                *GetNameSafe(OwnerCharacter)
            );

            RejectPrediction();
            return;
        }

        UE_LOG(LogDMCombat, Log, TEXT("[%s] Invisibility broken by Attack input."), *GetNameSafe(OwnerCharacter));
    }

    FString FailureReason;

    if (!CanRequestAttack(FailureReason))
    {
        UE_LOG(
            LogDMCombat,
            Verbose,
            TEXT("[%s] Server attack request rejected. Reason=%s"),
            *GetNameSafe(OwnerCharacter),
            *FailureReason
        );

        RejectPrediction();
        return;
    }

    if (!BeginAttackAction(PredictionId))
    {
        UE_LOG(LogDMCombat, Warning, TEXT("[%s] Failed to begin attack action."), *GetNameSafe(OwnerCharacter));

        RejectPrediction();
        return;
    }

    if (!ConsumeAttackCooldown())
    {
        StateComponent->EndAction(EDMActionState::Attack);

        /*
         * BeginAttackAction에서 이미 시작된
         * 서버/클라이언트 Presentation도 Rollback한다.
         */
        OwnerCharacter->StopAttackMontage();

        MulticastStopCombatPresentation();

        UE_LOG(
            LogDMCombat,
            Warning,
            TEXT("[%s] Attack started but cooldown commit failed. Presentation rolled back."),
            *GetNameSafe(OwnerCharacter)
        );

        RejectPrediction();
        return;
    }

    UE_LOG(
        LogDMCombat,
        Log,
        TEXT("[%s] Attack started. Sequence=%u, CooldownEnd=%.3f"),
        *GetNameSafe(OwnerCharacter),
        AttackSequence,
        AttackCooldownEndServerTime
    );
}

#pragma endregion

#pragma region Replication

void UDMCombatComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION(UDMCombatComponent, AttackCooldownEndServerTime, COND_OwnerOnly);

    DOREPLIFETIME_CONDITION(UDMCombatComponent, DefenseCooldownEndServerTime, COND_OwnerOnly);
}

#pragma endregion

#pragma region Attack Prediction

bool UDMCombatComponent::TryStartPredictedAttack(uint32& OutPredictionId)
{
    OutPredictionId = 0;

    ADMPlayerCharacter* OwnerCharacter = GetOwner<ADMPlayerCharacter>();

    if (OwnerCharacter == nullptr || OwnerCharacter->HasAuthority() || !OwnerCharacter->IsLocallyControlled())
    {
        return false;
    }

    if (OwnerCharacter->PlayAttackMontage() <= 0.0f)
    {
        return false;
    }

    ++LastPredictedAttackId;

    // 0은 "예측 없음"으로 남긴다.
    if (LastPredictedAttackId == 0)
    {
        ++LastPredictedAttackId;
    }

    PendingPredictedAttackId = LastPredictedAttackId;

    PredictedAttackCooldownEndServerTime = DMGameplay::GetServerTimeSeconds(this) + GetAttackCooldownDuration();

    CombatCooldownChangedDelegate.Broadcast();

    OutPredictionId = PendingPredictedAttackId;

    return true;
}

void UDMCombatComponent::ClientRejectPredictedAttack_Implementation(const uint32 PredictionId)
{
    // 이후에 새로 시작한 예측 공격은 건드리지 않는다.
    if (PredictionId == 0 || PredictionId != LastPredictedAttackId)
    {
        return;
    }

    PendingPredictedAttackId = 0;

    PredictedAttackCooldownEndServerTime = 0.0;

    StopPredictedAttackPresentation();

    CombatCooldownChangedDelegate.Broadcast();

    UE_LOG(
        LogDMCombat,
        Verbose,
        TEXT("[%s] Predicted attack rejected by server. PredictionId=%u"),
        *GetNameSafe(GetOwner()),
        PredictionId
    );
}

void UDMCombatComponent::StopPredictedAttackPresentation()
{
    ADMPlayerCharacter* OwnerCharacter = GetOwner<ADMPlayerCharacter>();

    if (OwnerCharacter == nullptr)
    {
        return;
    }

    UAnimMontage* AttackMontage = OwnerCharacter->GetAttackMontage();

    USkeletalMeshComponent* MeshComponent = OwnerCharacter->GetMesh();

    UAnimInstance* AnimInstance = MeshComponent != nullptr ? MeshComponent->GetAnimInstance() : nullptr;

    if (AttackMontage != nullptr && AnimInstance != nullptr)
    {
        AnimInstance->Montage_Stop(PredictedAttackRejectBlendOutSeconds, AttackMontage);
    }
}

#pragma endregion

#pragma region Replicated State Presentation

void UDMCombatComponent::HandleReplicatedActionStateChanged(
    const EDMActionState PreviousActionState, const EDMActionState NewActionState
)
{
    ADMPlayerCharacter* OwnerCharacter = GetOwner<ADMPlayerCharacter>();

    // 서버는 연출을 직접 재생하므로 복구할 필요가 없다.
    if (OwnerCharacter == nullptr || OwnerCharacter->HasAuthority())
    {
        return;
    }

    USkeletalMeshComponent* MeshComponent = OwnerCharacter->GetMesh();

    UAnimInstance* AnimInstance = MeshComponent != nullptr ? MeshComponent->GetAnimInstance() : nullptr;

    if (AnimInstance == nullptr)
    {
        return;
    }

    // 공격 시작 연출이 유실되었거나 아직 도착하지 않았다면 복제된 상태로 시작한다.
    // 소유 클라이언트가 예측 재생한 공격은 이미 보여줬으므로 다시 재생하지 않는다.
    const bool bPredictedByOwner = OwnerCharacter->IsLocallyControlled() && bLastAttackRequestPredicted;

    if (NewActionState == EDMActionState::Attack && PreviousActionState != EDMActionState::Attack && !bPredictedByOwner)
    {
        UAnimMontage* AttackMontage = OwnerCharacter->GetAttackMontage();

        if (AttackMontage != nullptr && !AnimInstance->Montage_IsPlaying(AttackMontage))
        {
            bAttackStartedFromReplicatedState = OwnerCharacter->PlayAttackMontage() > 0.0f;
        }
    }

    // 방어 시작 연출도 같은 방식으로 복구한다.
    if (NewActionState == EDMActionState::Defense && PreviousActionState != EDMActionState::Defense)
    {
        UAnimMontage* DefenseMontage = OwnerCharacter->GetDefenseMontage();

        if (DefenseMontage != nullptr && !AnimInstance->Montage_IsPlaying(DefenseMontage) && IsValid(CombatRuleData))
        {
            bDefenseStartedFromReplicatedState =
                OwnerCharacter->PlayDefenseMontage(CombatRuleData->GetDefenseDuration()) > 0.0f;
        }
    }

    // 액션이 끝나면 안전망 표시를 지워 다음 시작 RPC가 새로 재생하게 한다.
    if (PreviousActionState == EDMActionState::Attack && NewActionState != EDMActionState::Attack)
    {
        bAttackStartedFromReplicatedState = false;
    }

    // 방어 종료 연출이 유실되어도 방어 자세가 남지 않게 한다.
    if (PreviousActionState == EDMActionState::Defense && NewActionState != EDMActionState::Defense)
    {
        bDefenseStartedFromReplicatedState = false;

        OwnerCharacter->StopDefenseMontage();
    }
}

#pragma endregion

#pragma region Cooldown

bool UDMCombatComponent::IsAttackCooldownReady() const
{
    // 서버 값이 복제되기 전에는 소유 클라이언트의 예측 쿨다운도 함께 확인한다.
    return DMGameplay::GetServerTimeSeconds(this) + GetRemoteCooldownTolerance(this)
        >= FMath::Max(AttackCooldownEndServerTime, PredictedAttackCooldownEndServerTime);
}

bool UDMCombatComponent::IsDefenseCooldownReady() const
{
    return DMGameplay::GetServerTimeSeconds(this) + GetRemoteCooldownTolerance(this) >= DefenseCooldownEndServerTime;
}

float UDMCombatComponent::GetRemainingAttackCooldown() const
{
    const double RemainingTime = FMath::Max(AttackCooldownEndServerTime, PredictedAttackCooldownEndServerTime)
        - DMGameplay::GetServerTimeSeconds(this);

    return static_cast<float>(FMath::Max(0.0, RemainingTime));
}

float UDMCombatComponent::GetRemainingDefenseCooldown() const
{
    const double RemainingTime = DefenseCooldownEndServerTime - DMGameplay::GetServerTimeSeconds(this);

    return static_cast<float>(FMath::Max(0.0, RemainingTime));
}

float UDMCombatComponent::GetAttackCooldownDuration() const
{
    return IsValid(CombatRuleData) ? FMath::Max(0.0f, CombatRuleData->GetAttackCooldown()) : 0.0f;
}

float UDMCombatComponent::GetDefenseCooldownDuration() const
{
    return IsValid(CombatRuleData) ? FMath::Max(0.0f, CombatRuleData->GetDefenseCooldown()) : 0.0f;
}

void UDMCombatComponent::OnRep_AttackCooldownEndServerTime()
{
    // 서버가 확정한 쿨다운이 도착하면 로컬 예측값은 더 이상 필요 없다.
    PredictedAttackCooldownEndServerTime = 0.0;

    // 소유 클라이언트가 새 공격 쿨다운을 받았음을 알린다.
    CombatCooldownChangedDelegate.Broadcast();
}

void UDMCombatComponent::OnRep_DefenseCooldownEndServerTime()
{
    // 소유 클라이언트가 새 방어 쿨다운을 받았음을 알린다.
    CombatCooldownChangedDelegate.Broadcast();
}

bool UDMCombatComponent::ConsumeAttackCooldown()
{
    AActor* OwnerActor = GetOwner();

    if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
    {
        return false;
    }

    if (CombatRuleData == nullptr)
    {
        return false;
    }

    if (!IsAttackCooldownReady())
    {
        return false;
    }

    const double CurrentServerTime = DMGameplay::GetServerTimeSeconds(this);

    AttackCooldownEndServerTime =
        CurrentServerTime + static_cast<double>(FMath::Max(0.0f, CombatRuleData->GetAttackCooldown()));

    // Listen Server의 로컬 HUD는 RepNotify를 기다릴 수 없으므로
    // 서버 값 변경 직후 동일한 로컬 알림을 발생시킨다.
    CombatCooldownChangedDelegate.Broadcast();

    OwnerActor->ForceNetUpdate();

    UE_LOG(
        LogDMCombat,
        Verbose,
        TEXT("[%s] Attack cooldown consumed. EndServerTime=%.3f"),
        *GetNameSafe(OwnerActor),
        AttackCooldownEndServerTime
    );

    return true;
}

bool UDMCombatComponent::ConsumeDefenseCooldown()
{
    AActor* OwnerActor = GetOwner();

    if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
    {
        return false;
    }

    if (CombatRuleData == nullptr)
    {
        return false;
    }

    if (!IsDefenseCooldownReady())
    {
        return false;
    }

    const double CurrentServerTime = DMGameplay::GetServerTimeSeconds(this);

    DefenseCooldownEndServerTime =
        CurrentServerTime + static_cast<double>(FMath::Max(0.0f, CombatRuleData->GetDefenseCooldown()));

    // 서버에서 방어 쿨다운이 확정되었음을 로컬 UI에 알린다.
    CombatCooldownChangedDelegate.Broadcast();

    OwnerActor->ForceNetUpdate();

    UE_LOG(
        LogDMCombat,
        Verbose,
        TEXT("[%s] Defense cooldown consumed. EndServerTime=%.3f"),
        *GetNameSafe(OwnerActor),
        DefenseCooldownEndServerTime
    );

    return true;
}

#pragma endregion

#pragma region Attack Execution

bool UDMCombatComponent::BeginAttackAction(const uint32 PredictionId)
{
    ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());

    if (OwnerCharacter == nullptr || !OwnerCharacter->HasAuthority())
    {
        return false;
    }

    UDMCharacterStateComponent* StateComponent = OwnerCharacter->GetCharacterStateComponent();

    if (StateComponent == nullptr)
    {
        return false;
    }

    if (!StateComponent->TryBeginAction(EDMActionState::Attack))
    {
        return false;
    }

    ++AttackSequence;

    // uint32가 한 바퀴 돌아도 0은 "아직 공격 없음" 용도로 남긴다.
    if (AttackSequence == 0)
    {
        ++AttackSequence;
    }

    if (!PlayAttackMontageOnServer())
    {
        StateComponent->EndAction(EDMActionState::Attack);

        return false;
    }

    /*
     * 서버 Montage가 실제로 시작된 직후의
     * 서버 시간을 Presentation 기준 시각으로 사용한다.
     */
    const double AttackStartServerTime = DMGameplay::GetServerTimeSeconds(this);

    MulticastPresentAttack(AttackSequence, AttackStartServerTime, PredictionId);

    return true;
}

bool UDMCombatComponent::PlayAttackMontageOnServer()
{
    ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());

    if (OwnerCharacter == nullptr || !OwnerCharacter->HasAuthority())
    {
        return false;
    }

    const float MontageDuration = OwnerCharacter->PlayAttackMontage();

    if (MontageDuration <= 0.0f)
    {
        UE_LOG(LogDMCombat, Error, TEXT("[%s] Failed to play AttackMontage on server."), *GetNameSafe(OwnerCharacter));

        return false;
    }

    USkeletalMeshComponent* MeshComponent = OwnerCharacter->GetMesh();

    if (MeshComponent == nullptr)
    {
        return false;
    }

    UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance();

    if (AnimInstance == nullptr)
    {
        return false;
    }

    FOnMontageEnded EndDelegate;

    EndDelegate.BindUObject(this, &ThisClass::HandleAttackMontageEnded);

    AnimInstance->Montage_SetEndDelegate(EndDelegate, OwnerCharacter->GetAttackMontage());

    return true;
}

void UDMCombatComponent::MulticastPresentAttack_Implementation(
    const uint32 Sequence, const double AttackStartServerTime, const uint32 PredictionId
)
{
    ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());

    if (OwnerCharacter == nullptr)
    {
        return;
    }

    /*
     * 서버는 Authority Montage를
     * 이미 직접 시작했다.
     */
    if (OwnerCharacter->HasAuthority())
    {
        return;
    }

    // 소유 클라이언트가 이미 예측 재생한 공격이면 다시 재생하지 않는다.
    if (PredictionId != 0 && PredictionId == PendingPredictedAttackId)
    {
        PendingPredictedAttackId = 0;
        return;
    }

    UAnimMontage* AttackMontage = OwnerCharacter->GetAttackMontage();

    if (AttackMontage == nullptr)
    {
        return;
    }

    const float MontageLength = AttackMontage->GetPlayLength();

    if (MontageLength <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    /*
     * RPC가 도착하기까지 지나간 시간을
     * 동기화된 Server World Time으로 계산한다.
     */
    const double ElapsedServerTime = FMath::Max(0.0, DMGameplay::GetServerTimeSeconds(this) - AttackStartServerTime);

    const float Elapsed = static_cast<float>(ElapsedServerTime);

    /*
     * 이미 서버 공격 Montage가 끝났어야 하는
     * 시점이라면 뒤늦은 표현을 시작하지 않는다.
     */
    if (Elapsed >= MontageLength)
    {
        UE_LOG(
            LogDMCombat,
            Verbose,
            TEXT("[%s] Late attack presentation skipped. Sequence=%u, Elapsed=%.3f"),
            *GetNameSafe(OwnerCharacter),
            Sequence,
            Elapsed
        );

        return;
    }

    USkeletalMeshComponent* PresentationMesh = OwnerCharacter->GetMesh();
    UAnimInstance* PresentationAnimInstance =
        PresentationMesh != nullptr ? PresentationMesh->GetAnimInstance() : nullptr;

    // ActionState 복제로 이미 시작된 연출이면 재생 위치만 맞춘다.
    const bool bAlreadyPlaying = bAttackStartedFromReplicatedState && PresentationAnimInstance != nullptr
        && PresentationAnimInstance->Montage_IsPlaying(AttackMontage);

    bAttackStartedFromReplicatedState = false;

    if (!bAlreadyPlaying)
    {
        const float PlayedDuration = OwnerCharacter->PlayAttackMontage();

        if (PlayedDuration <= 0.0f)
        {
            return;
        }
    }

    /*
     * Client는 RPC 수신 시점부터 0초에서
     * 재생하는 대신 이미 지나간 시간만큼
     * Montage 위치를 앞으로 이동한다.
     *
     * Attack은 현재 PlayRate 1.0이므로
     * 경과시간 == Montage Position이다.
     */
    if (Elapsed > KINDA_SMALL_NUMBER)
    {
        USkeletalMeshComponent* MeshComponent = OwnerCharacter->GetMesh();

        if (MeshComponent != nullptr)
        {
            if (UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance())
            {
                AnimInstance->Montage_SetPosition(
                    AttackMontage, FMath::Clamp(Elapsed, 0.0f, MontageLength - KINDA_SMALL_NUMBER)
                );
            }
        }
    }

    UE_LOG(
        LogDMCombat,
        Verbose,
        TEXT("[%s] Attack presentation synchronized. Sequence=%u, Elapsed=%.3f"),
        *GetNameSafe(OwnerCharacter),
        Sequence,
        Elapsed
    );
}

void UDMCombatComponent::HandleAttackMontageEnded(UAnimMontage* Montage, const bool bInterrupted)
{
    ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());

    if (OwnerCharacter == nullptr || !OwnerCharacter->HasAuthority())
    {
        return;
    }

    if (Montage != OwnerCharacter->GetAttackMontage())
    {
        return;
    }

    UDMCharacterStateComponent* StateComponent = OwnerCharacter->GetCharacterStateComponent();

    if (StateComponent != nullptr)
    {
        StateComponent->EndAction(EDMActionState::Attack);
    }

    UE_LOG(
        LogDMCombat,
        Verbose,
        TEXT("[%s] Attack montage ended. Interrupted=%s"),
        *GetNameSafe(OwnerCharacter),
        bInterrupted ? TEXT("true") : TEXT("false")
    );
}

void UDMCombatComponent::HandleAttackHitNotify()
{
    ADMPlayerCharacter* OwnerCharacter = GetOwner<ADMPlayerCharacter>();

    if (OwnerCharacter == nullptr || !OwnerCharacter->HasAuthority())
    {
        return;
    }

    const ADMMatchPlayerState* AttackerPlayerState = OwnerCharacter->GetPlayerState<ADMMatchPlayerState>();

    if (AttackerPlayerState == nullptr || AttackerPlayerState->GetLifeState() != EDMPlayerLifeState::Alive)
    {
        UE_LOG(
            LogDMCombat,
            Verbose,
            TEXT("[%s] Attack hit notify rejected: attacker is no longer Alive. Sequence=%u"),
            *GetNameSafe(OwnerCharacter),
            AttackSequence
        );

        return;
    }

    const UDMCharacterStateComponent* StateComponent = OwnerCharacter->GetCharacterStateComponent();

    /*
     * Notify가 도착한 시점에도
     * 실제로 Attack Action 중인지 다시 검증한다.
     *
     * Montage가 중단되었거나
     * ActionState가 변경된 뒤 들어온 Notify라면
     * 공격 판정을 수행하지 않는다.
     */
    if (StateComponent == nullptr || StateComponent->GetActionState() != EDMActionState::Attack)
    {
        return;
    }

    /*
     * 아직 유효한 공격 Sequence가 아니거나,
     * 현재 공격에서 Hit Notify가 이미 처리됐다면
     * 중복 판정을 막는다.
     */
    if (AttackSequence == 0 || LastResolvedAttackHitSequence == AttackSequence)
    {
        return;
    }

    /*
     * 현재 공격의 Hit Notify는
     * 이 시점부터 처리 완료로 본다.
     *
     * 동일 Montage에 Notify가 실수로
     * 여러 개 들어가더라도
     * 공격 판정은 한 번만 실행된다.
     */
    LastResolvedAttackHitSequence = AttackSequence;

    UE_LOG(
        LogDMCombat,
        Log,
        TEXT("[%s] Attack hit notify accepted. Sequence=%u"),
        *GetNameSafe(OwnerCharacter),
        AttackSequence
    );

    /*
     * 서버 월드 기준으로
     * Range / Forward / LOS를 통과한
     * 모든 Player/NPC 후보를 수집한다.
     */
    TArray<ADMJellyCharacterBase*> ValidTargets;

    GatherValidAttackTargets(ValidTargets);

    TArray<ADMPlayerCharacter*> FatalPlayerTargets;

    TArray<ADMNPCCharacter*> HitNPCTargets;

    /*
     * NPC가 여러 명 맞더라도
     * 오인 공격 스턴은
     * 공격 1회당 한 번만 적용해야 한다.
     *
     */
    bool bHitAnyNPC = false;

    int32 PlayerTargetCount = 0;
    int32 NPCTargetCount = 0;

    for (ADMJellyCharacterBase* Target : ValidTargets)
    {
        if (Target == nullptr)
        {
            continue;
        }

        /*
         * Player Target
         */
        if (ADMPlayerCharacter* PlayerTarget = Cast<ADMPlayerCharacter>(Target))
        {
            ++PlayerTargetCount;

            /*
             * 내부 판정 순서:
             *
             * 1. LifeState == Alive 재검증
             * 2. SpawnProtected 검사
             * 3. Defense 상태 검사
             * 4. RemainingDefenseBlocks 검사
             * 5. Defense Arc 검사
             *
             * 위 조건으로 보호/방어되지 않은 경우에만
             * Fatal Hit Candidate가 된다.
             */
            if (ShouldApplyFatalHitToPlayer(PlayerTarget))
            {
                FatalPlayerTargets.Add(PlayerTarget);

                UE_LOG(
                    LogDMCombat,
                    Log,
                    TEXT("[%s] Fatal player hit candidate. Sequence=%u, Target=%s"),
                    *GetNameSafe(OwnerCharacter),
                    AttackSequence,
                    *GetNameSafe(PlayerTarget)
                );
            }

            continue;
        }

        /*
         * NPC Target
         *
         * NPC는 죽지 않는다.
         *
         * NPC가 한 명 이상 포함됐다는 사실만
         * 현재 Attack 단위로 기록한다.
         *
         */
        if (ADMNPCCharacter* NPCTarget = Cast<ADMNPCCharacter>(Target))
        {
            ++NPCTargetCount;

            bHitAnyNPC = true;

            HitNPCTargets.Add(NPCTarget);

            UE_LOG(
                LogDMCombat,
                Verbose,
                TEXT("[%s] NPC hit candidate. Sequence=%u, Target=%s"),
                *GetNameSafe(OwnerCharacter),
                AttackSequence,
                *GetNameSafe(NPCTarget)
            );

            continue;
        }
    }

    /*
     *
     * 여기서 PlayerTargetCount는
     * SpawnProtected / Defense 여부와 무관하게
     * Range + Forward + LOS를 통과한
     * Player 후보 수다.
     */
    UE_LOG(
        LogDMCombat,
        Log,
        TEXT("[%s] Attack target query completed. Sequence=%u, Total=%d, Players=%d, NPCs=%d"),
        *GetNameSafe(OwnerCharacter),
        AttackSequence,
        ValidTargets.Num(),
        PlayerTargetCount,
        NPCTargetCount
    );

    /*
     * FatalPlayers:
     * 보호/방어되지 않아
     * 실제 사망 처리가 필요한 Player 수.
     *
     * HitAnyNPC:
     * 이번 공격에서 NPC가 하나 이상
     * 유효하게 맞았는지 여부.
     */
    UE_LOG(
        LogDMCombat,
        Log,
        TEXT("[%s] Attack resolution completed. Sequence=%u, FatalPlayers=%d, HitAnyNPC=%s"),
        *GetNameSafe(OwnerCharacter),
        AttackSequence,
        FatalPlayerTargets.Num(),
        bHitAnyNPC ? TEXT("true") : TEXT("false")
    );

    if (bHitAnyNPC)
    {
        ApplyWrongTargetPenalty();

        RequestNPCWrongTargetReaction(HitNPCTargets

        );
    }

    if (FatalPlayerTargets.Num() > 0)
    {
        UWorld* World = GetWorld();

        ADMMatchGameMode* MatchGameMode = World != nullptr ? World->GetAuthGameMode<ADMMatchGameMode>() : nullptr;

        if (!ensureMsgf(
                MatchGameMode != nullptr,
                TEXT("[%s] Fatal hit resolution failed: MatchGameMode is unavailable. Sequence=%u"),
                *GetNameSafe(OwnerCharacter),
                AttackSequence
            ))
        {
            return;
        }

        for (ADMPlayerCharacter* FatalTarget : FatalPlayerTargets)
        {
            if (!IsValid(FatalTarget))
            {
                continue;
            }

            const bool bDeathCommitted = MatchGameMode->TryCommitPlayerDeath(FatalTarget, OwnerCharacter);

            if (bDeathCommitted)
            {
                UE_LOG(
                    LogDMCombat,
                    Log,
                    TEXT("[%s] Death Commit succeeded. Sequence=%u, Target=%s"),
                    *GetNameSafe(OwnerCharacter),
                    AttackSequence,
                    *GetNameSafe(FatalTarget)
                );
            }
            else
            {
                UE_LOG(
                    LogDMCombat,
                    Verbose,
                    TEXT("[%s] Death Commit rejected. Sequence=%u, Target=%s"),
                    *GetNameSafe(OwnerCharacter),
                    AttackSequence,
                    *GetNameSafe(FatalTarget)
                );
            }
        }
    }
}

#pragma endregion

#pragma region Attack Targeting

void UDMCombatComponent::GatherValidAttackTargets(TArray<ADMJellyCharacterBase*>& OutTargets) const
{
    OutTargets.Reset();

    const ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());

    if (OwnerCharacter == nullptr || !OwnerCharacter->HasAuthority() || CombatRuleData == nullptr)
    {
        return;
    }

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return;
    }

    const float AttackRange = CombatRuleData->GetAttackRange();

    if (!ensureMsgf(
            AttackRange > 0.0f,
            TEXT("[%s] Attack target query failed: AttackRange must be greater than 0."),
            *GetNameSafe(OwnerCharacter)
        ))
    {
        return;
    }

    const FVector AttackOrigin = OwnerCharacter->GetActorLocation();

    /*
     * 1차 후보 수집:
     * 공격자를 중심으로 AttackRange 안의
     * Pawn Object만 조회한다.
     *
     * 이후 실제 거리/전방/종류를 다시 검사한다.
     */
    TArray<FOverlapResult> OverlapResults;

    FCollisionObjectQueryParams ObjectQueryParams;

    ObjectQueryParams.AddObjectTypesToQuery(ECC_Pawn);

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DMCombatAttackOverlap), false, OwnerCharacter);

    World->OverlapMultiByObjectType(
        OverlapResults,
        AttackOrigin,
        FQuat::Identity,
        ObjectQueryParams,
        FCollisionShape::MakeSphere(AttackRange),
        QueryParams
    );

    /*
     * 같은 Character의 여러 PrimitiveComponent가
     * Overlap 결과에 들어올 수 있으므로
     * Actor 단위로 중복 제거한다.
     */
    TSet<ADMJellyCharacterBase*> SeenCharacters;

    TArray<ADMJellyCharacterBase*> GeometricCandidates;

    for (const FOverlapResult& OverlapResult : OverlapResults)
    {
        ADMJellyCharacterBase* Candidate = Cast<ADMJellyCharacterBase>(OverlapResult.GetActor());

        if (Candidate == nullptr || Candidate == OwnerCharacter || SeenCharacters.Contains(Candidate))
        {
            continue;
        }

        SeenCharacters.Add(Candidate);

        if (!IsInsideAttackGeometry(Candidate, AttackRange))
        {
            continue;
        }

        GeometricCandidates.Add(Candidate);
    }

    /*
     * Multi Target 공격이므로
     * Character끼리는 LOS 장애물로 취급하지 않는다.
     *
     * 그렇지 않으면 앞의 Player가
     * 뒤의 Player를 가려서
     * "범위 내 모든 유효 대상" 규칙과 충돌한다.
     */
    TArray<AActor*> IgnoredCharacters;

    IgnoredCharacters.Reserve(GeometricCandidates.Num() + 1);

    IgnoredCharacters.Add(const_cast<ADMPlayerCharacter*>(OwnerCharacter));

    for (ADMJellyCharacterBase* Candidate : GeometricCandidates)
    {
        IgnoredCharacters.Add(Candidate);
    }

    for (ADMJellyCharacterBase* Candidate : GeometricCandidates)
    {
        if (!IsEligibleAttackTarget(Candidate))
        {
            continue;
        }

        if (IsWorldGeometryBlockingTarget(Candidate, IgnoredCharacters))
        {
            continue;
        }

        OutTargets.Add(Candidate);
    }
}

bool UDMCombatComponent::IsInsideAttackGeometry(const ADMJellyCharacterBase* Target, const float AttackRange) const
{
    const ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());

    if (OwnerCharacter == nullptr || Target == nullptr || AttackRange <= 0.0f)
    {
        return false;
    }

    const FVector OwnerLocation = OwnerCharacter->GetActorLocation();

    const FVector TargetLocation = Target->GetActorLocation();

    const FVector ToTarget = TargetLocation - OwnerLocation;

    /*
     * Overlap은 Target의 Collision Shape를 기준으로
     * 결과를 줄 수 있으므로,
     * Character 중심 간 실제 거리도 다시 검증한다.
     */
    if (ToTarget.SizeSquared() > FMath::Square(AttackRange))
    {
        return false;
    }

    const FVector Forward2D = OwnerCharacter->GetActorForwardVector().GetSafeNormal2D();

    const FVector ToTarget2D = ToTarget.GetSafeNormal2D();

    if (Forward2D.IsNearlyZero() || ToTarget2D.IsNearlyZero())
    {
        return false;
    }

    const float ForwardDot = FVector::DotProduct(Forward2D, ToTarget2D);

    return ForwardDot > 0.0f;
}

bool UDMCombatComponent::IsEligibleAttackTarget(const ADMJellyCharacterBase* Target) const
{
    if (Target == nullptr)
    {
        return false;
    }

    /*
     * Player
     */
    if (const ADMPlayerCharacter* PlayerTarget = Cast<ADMPlayerCharacter>(Target))
    {
        const ADMMatchPlayerState* MatchPlayerState = Cast<ADMMatchPlayerState>(PlayerTarget->GetPlayerState());

        if (MatchPlayerState == nullptr)
        {
            return false;
        }

        return MatchPlayerState->GetLifeState() == EDMPlayerLifeState::Alive;
    }

    /*
     * NPC
     *
     * NPC의 FakeDeath 역시
     * 실제 NPC로 판정해야 하므로
     * 여기서는 ActionState로 제외하지 않는다.
     */
    if (Cast<ADMNPCCharacter>(Target) != nullptr)
    {
        return true;
    }

    return false;
}

bool UDMCombatComponent::IsWorldGeometryBlockingTarget(
    const ADMJellyCharacterBase* Target, const TArray<AActor*>& IgnoredCharacters
) const
{
    if (Target == nullptr || CombatRuleData == nullptr)
    {
        return true;
    }

    if (!CombatRuleData->IsAttackBlockedByWorldGeometry())
    {
        return false;
    }

    const ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());

    if (OwnerCharacter == nullptr)
    {
        return true;
    }

    const UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return true;
    }

    FCollisionQueryParams QueryParams(SCENE_QUERY_STAT(DMCombatAttackLOS), false, OwnerCharacter);

    QueryParams.AddIgnoredActors(IgnoredCharacters);

    const FVector TraceStart = OwnerCharacter->GetActorLocation();

    const FVector TraceEnd = Target->GetActorLocation();

    return World->LineTraceTestByChannel(TraceStart, TraceEnd, ECC_Visibility, QueryParams);
}

#pragma endregion

#pragma region Defense Request

void UDMCombatComponent::RequestDefense()
{
    const ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());

    if (OwnerCharacter == nullptr || !OwnerCharacter->IsLocallyControlled())
    {
        return;
    }

    FString FailureReason;

    if (!CanRequestDefense(FailureReason))
    {
        UE_LOG(
            LogDMCombat,
            Verbose,
            TEXT("[%s] Local defense request blocked. Reason=%s"),
            *GetNameSafe(OwnerCharacter),
            *FailureReason
        );

        return;
    }

    ServerRequestDefense();
}

bool UDMCombatComponent::CanRequestDefense(FString& OutFailureReason) const
{
    OutFailureReason.Reset();

    const ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());

    if (OwnerCharacter == nullptr)
    {
        OutFailureReason = TEXT("InvalidOwner");

        return false;
    }

    if (!OwnerCharacter->IsPlayerControlled())
    {
        OutFailureReason = TEXT("NotPlayerControlled");

        return false;
    }

    if (CombatRuleData == nullptr)
    {
        OutFailureReason = TEXT("MissingCombatRuleData");

        return false;
    }

    if (CombatRuleData->GetDefenseDuration() <= 0.0f)
    {
        OutFailureReason = TEXT("InvalidDefenseDuration");

        return false;
    }

    if (CombatRuleData->GetDefenseBlockCount() <= 0)
    {
        OutFailureReason = TEXT("InvalidDefenseBlockCount");

        return false;
    }

    if (!OwnerCharacter->CanPlayDefenseMontage())
    {
        OutFailureReason = TEXT("DefenseMontageUnavailable");

        return false;
    }

    const UWorld* World = GetWorld();

    if (World == nullptr)
    {
        OutFailureReason = TEXT("InvalidWorld");

        return false;
    }

    const ADMMatchGameState* MatchGameState = World->GetGameState<ADMMatchGameState>();

    if (MatchGameState == nullptr)
    {
        OutFailureReason = TEXT("MissingMatchGameState");

        return false;
    }

    const EDMMatchPhase CurrentPhase = MatchGameState->GetCurrentPhase();

    if (CurrentPhase != EDMMatchPhase::Playing && CurrentPhase != EDMMatchPhase::Overtime)
    {
        OutFailureReason = TEXT("InvalidMatchPhase");

        return false;
    }

    const UDMCharacterStateComponent* StateComponent = OwnerCharacter->GetCharacterStateComponent();

    if (StateComponent == nullptr)
    {
        OutFailureReason = TEXT("MissingCharacterState");

        return false;
    }

    if (!StateComponent->CanPerformAction(EDMActionRequest::Defense))
    {
        OutFailureReason = TEXT("CharacterStateBlocked");

        return false;
    }

    const UDMPoseComponent* PoseComponent = OwnerCharacter->FindComponentByClass<UDMPoseComponent>();

    if (PoseComponent != nullptr && !PoseComponent->IsActionAllowedByPose(EDMActionRequest::Defense))
    {
        OutFailureReason = TEXT("PoseBlocked");

        return false;
    }

    if (!IsDefenseCooldownReady())
    {
        OutFailureReason = TEXT("DefenseCooldown");

        return false;
    }

    return true;
}

void UDMCombatComponent::ServerRequestDefense_Implementation()
{
    AActor* OwnerActor = GetOwner();

    if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
    {
        return;
    }

    FString FailureReason;

    if (!CanRequestDefense(FailureReason))
    {
        UE_LOG(
            LogDMCombat,
            Verbose,
            TEXT("[%s] Server defense request rejected. Reason=%s"),
            *GetNameSafe(OwnerActor),
            *FailureReason
        );

        return;
    }

    if (!BeginDefenseAction())
    {
        UE_LOG(LogDMCombat, Warning, TEXT("[%s] Failed to begin defense action."), *GetNameSafe(OwnerActor));

        return;
    }

    if (!ConsumeDefenseCooldown())
    {
        EndDefenseAction();

        UE_LOG(
            LogDMCombat, Warning, TEXT("[%s] Defense started but cooldown commit failed."), *GetNameSafe(OwnerActor)
        );

        return;
    }

    UE_LOG(
        LogDMCombat,
        Log,
        TEXT("[%s] Defense started. Duration=%.2f, Blocks=%d, CooldownEnd=%.3f"),
        *GetNameSafe(OwnerActor),
        CombatRuleData->GetDefenseDuration(),
        RemainingDefenseBlocks,
        DefenseCooldownEndServerTime
    );
}

#pragma endregion

#pragma region Defense Execution

bool UDMCombatComponent::BeginDefenseAction()
{
    ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());

    if (OwnerCharacter == nullptr || !OwnerCharacter->HasAuthority() || CombatRuleData == nullptr)
    {
        return false;
    }

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return false;
    }

    const float DefenseDuration = CombatRuleData->GetDefenseDuration();

    const int32 DefenseBlockCount = CombatRuleData->GetDefenseBlockCount();

    if (DefenseDuration <= 0.0f || DefenseBlockCount <= 0)
    {
        return false;
    }

    UDMCharacterStateComponent* StateComponent = OwnerCharacter->GetCharacterStateComponent();

    if (StateComponent == nullptr)
    {
        return false;
    }

    if (!StateComponent->TryBeginAction(EDMActionState::Defense))
    {
        return false;
    }

    RemainingDefenseBlocks = DefenseBlockCount;

    if (!PlayDefenseMontageOnServer(DefenseDuration))
    {
        RemainingDefenseBlocks = 0;

        StateComponent->EndAction(EDMActionState::Defense);

        return false;
    }

    const double DefenseStartServerTime = DMGameplay::GetServerTimeSeconds(this);

    World->GetTimerManager().SetTimer(
        DefenseDurationTimerHandle, this, &ThisClass::HandleDefenseDurationExpired, DefenseDuration, false
    );

    MulticastPresentDefense(DefenseDuration, DefenseStartServerTime);

    return true;
}

bool UDMCombatComponent::PlayDefenseMontageOnServer(const float DefenseDuration)
{
    ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());

    if (OwnerCharacter == nullptr || !OwnerCharacter->HasAuthority())
    {
        return false;
    }

    const float PlayedDuration = OwnerCharacter->PlayDefenseMontage(DefenseDuration);

    return PlayedDuration > 0.0f;
}

void UDMCombatComponent::MulticastPresentDefense_Implementation(
    const float DefenseDuration, const double DefenseStartServerTime
)
{
    ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());

    if (OwnerCharacter == nullptr || OwnerCharacter->HasAuthority())
    {
        return;
    }

    if (DefenseDuration <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    const double ElapsedServerTime = FMath::Max(0.0, DMGameplay::GetServerTimeSeconds(this) - DefenseStartServerTime);

    const float Elapsed = static_cast<float>(ElapsedServerTime);

    /*
     * 서버의 Defense Window가 이미 끝났다면
     * 늦게 도착한 Montage를 재생하지 않는다.
     */
    if (Elapsed >= DefenseDuration)
    {
        UE_LOG(
            LogDMCombat,
            Verbose,
            TEXT("[%s] Late defense presentation skipped. Elapsed=%.3f, Duration=%.3f"),
            *GetNameSafe(OwnerCharacter),
            Elapsed,
            DefenseDuration
        );

        return;
    }

    UAnimMontage* DefenseMontage = OwnerCharacter->GetDefenseMontage();

    if (DefenseMontage == nullptr)
    {
        return;
    }

    const float MontageLength = DefenseMontage->GetPlayLength();

    if (MontageLength <= KINDA_SMALL_NUMBER)
    {
        return;
    }

    USkeletalMeshComponent* PresentationMesh = OwnerCharacter->GetMesh();
    UAnimInstance* PresentationAnimInstance =
        PresentationMesh != nullptr ? PresentationMesh->GetAnimInstance() : nullptr;

    // ActionState 복제로 이미 시작된 연출이면 재생 위치만 맞춘다.
    const bool bAlreadyPlaying = bDefenseStartedFromReplicatedState && PresentationAnimInstance != nullptr
        && PresentationAnimInstance->Montage_IsPlaying(DefenseMontage);

    bDefenseStartedFromReplicatedState = false;

    if (!bAlreadyPlaying)
    {
        const float PlayedDuration = OwnerCharacter->PlayDefenseMontage(DefenseDuration);

        if (PlayedDuration <= 0.0f)
        {
            return;
        }
    }

    /*
     * Defense Montage는 원본 Montage 길이를
     * DefenseDuration에 맞추기 위해
     * PlayRate를 변경하고 있다.
     *
     * 따라서 실제 경과시간을 Montage Timeline
     * Position으로 변환해야 한다.
     */
    const float PlayRate = MontageLength / DefenseDuration;

    const float MontagePosition = Elapsed * PlayRate;

    if (MontagePosition > KINDA_SMALL_NUMBER)
    {
        USkeletalMeshComponent* MeshComponent = OwnerCharacter->GetMesh();

        if (MeshComponent != nullptr)
        {
            if (UAnimInstance* AnimInstance = MeshComponent->GetAnimInstance())
            {
                AnimInstance->Montage_SetPosition(
                    DefenseMontage, FMath::Clamp(MontagePosition, 0.0f, MontageLength - KINDA_SMALL_NUMBER)
                );
            }
        }
    }

    UE_LOG(
        LogDMCombat,
        Verbose,
        TEXT("[%s] Defense presentation synchronized. Elapsed=%.3f, MontagePosition=%.3f"),
        *GetNameSafe(OwnerCharacter),
        Elapsed,
        MontagePosition
    );
}

void UDMCombatComponent::HandleDefenseDurationExpired()
{
    AActor* OwnerActor = GetOwner();

    if (OwnerActor == nullptr || !OwnerActor->HasAuthority())
    {
        return;
    }

    EndDefenseAction();
}

void UDMCombatComponent::EndDefenseAction()
{
    ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());

    if (OwnerCharacter == nullptr || !OwnerCharacter->HasAuthority())
    {
        return;
    }

    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DefenseDurationTimerHandle);
    }

    /*
     * Defense 시간이 끝나면
     * 더 이상 어떤 공격도 막을 수 없다.
     */
    RemainingDefenseBlocks = 0;

    if (UDMCharacterStateComponent* StateComponent = OwnerCharacter->GetCharacterStateComponent())
    {
        StateComponent->EndAction(EDMActionState::Defense);
    }

    OwnerCharacter->StopDefenseMontage();

    MulticastStopDefensePresentation();

    UE_LOG(LogDMCombat, Verbose, TEXT("[%s] Defense ended."), *GetNameSafe(OwnerCharacter));
}

void UDMCombatComponent::MulticastStopDefensePresentation_Implementation()
{
    ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());

    if (OwnerCharacter == nullptr || OwnerCharacter->HasAuthority())
    {
        return;
    }

    OwnerCharacter->StopDefenseMontage();
}

#pragma endregion

#pragma region Death Cleanup

void UDMCombatComponent::HandleOwnerDeath()
{
    ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());

    if (OwnerCharacter == nullptr || !OwnerCharacter->HasAuthority())
    {
        return;
    }

    /*
     * Defense Timer 종료.
     */
    if (UWorld* World = GetWorld())
    {
        World->GetTimerManager().ClearTimer(DefenseDurationTimerHandle);
    }

    /*
     * 현재 Defense는 더 이상
     * 어떤 공격도 막을 수 없다.
     */
    RemainingDefenseBlocks = 0;

    /*
     * 현재 Attack에 뒤늦게 Notify가
     * 들어오더라도 다시 Hit Resolution을
     * 실행하지 않도록 현재 Sequence를
     * 처리 완료 상태로 만든다.
     */
    LastResolvedAttackHitSequence = AttackSequence;

    /*
     * 서버에서 실행 중 Combat Montage 정리.
     */
    OwnerCharacter->StopAttackMontage();

    OwnerCharacter->StopDefenseMontage();

    /*
     * Combat이 소유한 Action만 여기서 정리.
     *
     * Interaction / Skill / FakeDeath 등은
     * GameMode의 공통 Death Cleanup에서
     * 최종 정리한다.
     */
    if (UDMCharacterStateComponent* StateComponent = OwnerCharacter->GetCharacterStateComponent())
    {
        const EDMActionState CurrentAction = StateComponent->GetActionState();

        if (CurrentAction == EDMActionState::Attack)
        {
            StateComponent->EndAction(EDMActionState::Attack);
        }
        else if (CurrentAction == EDMActionState::Defense)
        {
            StateComponent->EndAction(EDMActionState::Defense);
        }
    }

    /*
     * Remote Client의 Combat Montage도 중단.
     */
    MulticastStopCombatPresentation();

    UE_LOG(LogDMCombat, Log, TEXT("[%s] Combat runtime cleaned for death."), *GetNameSafe(OwnerCharacter));
}

void UDMCombatComponent::MulticastStopCombatPresentation_Implementation()
{
    ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());

    if (OwnerCharacter == nullptr || OwnerCharacter->HasAuthority())
    {
        return;
    }

    OwnerCharacter->StopAttackMontage();

    OwnerCharacter->StopDefenseMontage();
}

#pragma endregion

#pragma region Player Hit Resolution

bool UDMCombatComponent::ShouldApplyFatalHitToPlayer(ADMPlayerCharacter* TargetPlayer) const
{
    const ADMPlayerCharacter* Attacker = Cast<ADMPlayerCharacter>(GetOwner());

    if (Attacker == nullptr || !Attacker->HasAuthority() || TargetPlayer == nullptr || TargetPlayer == Attacker)
    {
        return false;
    }

    /*
     * 1. LifeState 재검증
     *
     * Gather 단계 이후에도 실제 결과 적용 직전
     * 서버 원본을 다시 읽는다.
     */
    const ADMMatchPlayerState* TargetPlayerState = Cast<ADMMatchPlayerState>(TargetPlayer->GetPlayerState());

    if (TargetPlayerState == nullptr || TargetPlayerState->GetLifeState() != EDMPlayerLifeState::Alive)
    {
        return false;
    }

    UDMCharacterStateComponent* TargetStateComponent = TargetPlayer->GetCharacterStateComponent();

    if (TargetStateComponent == nullptr)
    {
        return false;
    }

    /*
     * 2. SpawnProtected
     *
     * 공격 자체는 이미 유효하게 실행되었기 때문에
     * 공격자의 쿨다운은 되돌리지 않는다.
     *
     * 대상만 피해 무효.
     */
    if (TargetStateComponent->HasStatus(DMGameplayTags::Status_SpawnProtected))
    {
        UE_LOG(
            LogDMCombat,
            Verbose,
            TEXT("[%s] Player hit ignored by SpawnProtection. Target=%s"),
            *GetNameSafe(Attacker),
            *GetNameSafe(TargetPlayer)
        );

        return false;
    }

    /*
     * 3. Defense
     */
    UDMCombatComponent* TargetCombatComponent = TargetPlayer->FindComponentByClass<UDMCombatComponent>();

    if (TargetCombatComponent != nullptr && TargetCombatComponent->TryConsumeDefenseBlockAgainst(Attacker))
    {
        UE_LOG(
            LogDMCombat,
            Log,
            TEXT("[%s] Attack blocked by defense. Target=%s"),
            *GetNameSafe(Attacker),
            *GetNameSafe(TargetPlayer)
        );

        return false;
    }

    /*
     * LifeState Alive
     * SpawnProtected 아님
     * Defense 실패
     *
     * → Fatal Hit Candidate
     */
    return true;
}

bool UDMCombatComponent::TryConsumeDefenseBlockAgainst(const ADMPlayerCharacter* Attacker)
{
    ADMPlayerCharacter* Defender = Cast<ADMPlayerCharacter>(GetOwner());

    if (Defender == nullptr || !Defender->HasAuthority() || Attacker == nullptr || Attacker == Defender)
    {
        return false;
    }

    UDMCharacterStateComponent* StateComponent = Defender->GetCharacterStateComponent();

    if (StateComponent == nullptr)
    {
        return false;
    }

    /*
     * 방어 중 SSOT.
     */
    if (StateComponent->GetActionState() != EDMActionState::Defense)
    {
        return false;
    }

    /*
     * 첫 유효 공격을 이미 막았다면
     * 남은 Defense 시간 동안에도
     * 추가 공격은 막을 수 없다.
     */
    if (RemainingDefenseBlocks <= 0)
    {
        return false;
    }

    /*
     * 후방/방어각 바깥 공격은
     * Defense 중이어도 막지 못한다.
     */
    if (!IsAttackerInsideDefenseArc(Attacker))
    {
        return false;
    }

    /*
     * 서버 처리 순서상
     * 첫 유효 공격 1회를 소비.
     */
    --RemainingDefenseBlocks;

    UE_LOG(
        LogDMCombat,
        Log,
        TEXT("[%s] Defense block consumed. Attacker=%s, RemainingBlocks=%d"),
        *GetNameSafe(Defender),
        *GetNameSafe(Attacker),
        RemainingDefenseBlocks
    );

    /*
     * 중요:
     * Block을 모두 소비해도
     * EndDefenseAction() 하지 않는다.
     *
     * 1초 Defense 상태/Animation은
     * 그대로 유지한다.
     */
    return true;
}

bool UDMCombatComponent::IsAttackerInsideDefenseArc(const ADMPlayerCharacter* Attacker) const
{
    const ADMPlayerCharacter* Defender = Cast<ADMPlayerCharacter>(GetOwner());

    if (Defender == nullptr || Attacker == nullptr || CombatRuleData == nullptr)
    {
        return false;
    }

    const float DefenseArcAngle = CombatRuleData->GetDefenseArcAngle();

    /*
     * 기획상 방어는 "전방 범위"이며
     * 360도 방어가 아니다.
     *
     * 0 이하 또는 180 초과는
     * 현재 설계에서 잘못된 설정으로 본다.
     */
    if (!ensureMsgf(
            DefenseArcAngle > 0.0f && DefenseArcAngle <= 180.0f,
            TEXT("[%s] Invalid DefenseArcAngle=%.2f. Expected range: 0 < Angle <= 180."),
            *GetNameSafe(Defender),
            DefenseArcAngle
        ))
    {
        return false;
    }

    const FVector DefenderForward = Defender->GetActorForwardVector().GetSafeNormal2D();

    /*
     * 방어자 → 공격자 방향.
     *
     * 공격자가 방어자의 정면 쪽에 있는지 본다.
     */
    const FVector ToAttacker = (Attacker->GetActorLocation() - Defender->GetActorLocation()).GetSafeNormal2D();

    if (DefenderForward.IsNearlyZero() || ToAttacker.IsNearlyZero())
    {
        return false;
    }

    /*
     * DefenseArcAngle은 전체 각도이므로
     * Dot 판정에는 절반을 사용한다.
     *
     * 예:
     * 120° full arc
     * → 좌우 60°
     */
    const float HalfArcRadians = FMath::DegreesToRadians(DefenseArcAngle * 0.5f);

    const float MinimumDot = FMath::Cos(HalfArcRadians);

    const float ActualDot = FVector::DotProduct(DefenderForward, ToAttacker);

    return ActualDot >= MinimumDot;
}

#pragma endregion

#pragma region Wrong Target Penalty

bool UDMCombatComponent::ApplyWrongTargetPenalty()
{
    ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(GetOwner());

    if (OwnerCharacter == nullptr || !OwnerCharacter->HasAuthority() || CombatRuleData == nullptr)
    {
        return false;
    }

    const float StunDuration = CombatRuleData->GetWrongTargetStunDuration();

    if (!ensureMsgf(
            StunDuration > 0.0f,
            TEXT("[%s] Wrong-target stun failed: invalid duration %.2f."),
            *GetNameSafe(OwnerCharacter),
            StunDuration
        ))
    {
        return false;
    }

    UDMCharacterStateComponent* StateComponent = OwnerCharacter->GetCharacterStateComponent();

    if (!ensureMsgf(
            StateComponent != nullptr,
            TEXT("[%s] Wrong-target stun failed: CharacterStateComponent is null."),
            *GetNameSafe(OwnerCharacter)
        ))
    {
        return false;
    }

    if (!StateComponent->ApplyTimedStatus(DMGameplayTags::Status_Stunned, StunDuration))
    {
        UE_LOG(
            LogDMCombat, Verbose, TEXT("[%s] Wrong-target stun was not newly applied."), *GetNameSafe(OwnerCharacter)
        );

        return false;
    }

    UE_LOG(
        LogDMCombat,
        Log,
        TEXT("[%s] Wrong-target stun applied. Duration=%.2f, Sequence=%u"),
        *GetNameSafe(OwnerCharacter),
        StunDuration,
        AttackSequence
    );

    return true;
}

void UDMCombatComponent::RequestNPCWrongTargetReaction(const TArray<ADMNPCCharacter*>& HitNPCTargets) const
{
    if (HitNPCTargets.Num() <= 0)
    {
        return;
    }

    const ADMPlayerCharacter* Attacker = Cast<ADMPlayerCharacter>(GetOwner());

    if (!IsValid(Attacker))
    {
        return;
    }

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return;
    }

    ADMMatchGameMode* MatchGameMode = World->GetAuthGameMode<ADMMatchGameMode>();

    if (!IsValid(MatchGameMode))
    {
        UE_LOG(LogDMCombat, Warning, TEXT("NPC wrong-target reaction request failed: MatchGameMode is unavailable."));

        return;
    }

    /*
     * 주변 NPC 선택의 중심은
     * 실제 Hit NPC 위치를 사용하고,
     *
     * Flee 방향 계산에는
     * 현재 공격자 위치를 ThreatOrigin으로 전달한다.
     */
    MatchGameMode->RequestNPCWrongTargetReaction(Attacker->GetActorLocation(), HitNPCTargets);
}

#pragma endregion
