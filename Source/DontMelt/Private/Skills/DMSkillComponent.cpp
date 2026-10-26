#include "Skills/DMSkillComponent.h"

#include "Core/DMGameplayUtils.h"

#include "Characters/DMPlayerCharacter.h"
#include "Characters/Components/DMCharacterStateComponent.h"
#include "Characters/Components/DMPoseComponent.h"
#include "Core/DMGameplayTags.h"
#include "Data/DMSkillSetDataAsset.h"
#include "Framework/Match/DMMatchGameMode.h"
#include "Framework/Match/DMMatchGameState.h"
#include "Framework/Match/DMMatchPlayerState.h"
#include "Framework/Match/DMMatchPlayerController.h"
#include "Relic/DMRelicManager.h"

#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "GameFramework/GameStateBase.h"
#include "Net/UnrealNetwork.h"
#include "TimerManager.h"

DEFINE_LOG_CATEGORY_STATIC(LogDMSkill, Log, All);

namespace
{
    constexpr float VisionDistortEntryDuration = 0.75f;

    constexpr float VisionDistortExitDuration = 0.50f;
}

#pragma region Lifecycle

UDMSkillComponent::UDMSkillComponent()
{
    PrimaryComponentTick.bCanEverTick = false;

    SetIsReplicatedByDefault(true);
}

void UDMSkillComponent::BeginPlay()
{
    Super::BeginPlay();

    ADMMatchPlayerState* OwnerPlayerState = Cast<ADMMatchPlayerState>(GetOwner());

    if (!IsValid(OwnerPlayerState))
    {
        return;
    }

    /*
     * 중복 Bind 방지.
     */
    OwnerPlayerState->OnLifeStateChanged().RemoveAll(this);

    OwnerPlayerState->OnLifeStateChanged().AddUObject(this, &ThisClass::HandleOwnerLifeStateChanged);
}

void UDMSkillComponent::EndPlay(const EEndPlayReason::Type EndPlayReason)
{
    /*
     * Owner LifeState Delegate 해제.
     */
    if (ADMMatchPlayerState* OwnerPlayerState = Cast<ADMMatchPlayerState>(GetOwner()))
    {
        OwnerPlayerState->OnLifeStateChanged().RemoveAll(this);
    }

    /*
     * Server-only caster-target context와
     * 관련 Timer를 정리한다.
     *
     * Object 종료 중이므로 Client RPC는 보내지 않는다.
     */
    ClearFootstepAmplifyEffect(false);

    SkillReadModelChangedDelegate.Clear();

    Super::EndPlay(EndPlayReason);
}

void UDMSkillComponent::GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const
{
    Super::GetLifetimeReplicatedProps(OutLifetimeProps);

    DOREPLIFETIME_CONDITION(UDMSkillComponent, SkillSetData, COND_OwnerOnly);

    DOREPLIFETIME_CONDITION(UDMSkillComponent, SkillCooldownEntries, COND_OwnerOnly);
}

#pragma endregion

#pragma region Replication Notification

void UDMSkillComponent::OnRep_SkillSetData()
{
    NotifySkillReadModelChanged();
}

void UDMSkillComponent::OnRep_SkillCooldownEntries()
{
    NotifySkillReadModelChanged();
}

void UDMSkillComponent::NotifySkillReadModelChanged()
{
    SkillReadModelChangedDelegate.Broadcast();
}

#pragma endregion

#pragma region Lifecycle Internal

void UDMSkillComponent::HandleOwnerLifeStateChanged(
    ADMMatchPlayerState* ChangedPlayerState, const EDMPlayerLifeState NewLifeState
)
{
    if (!HasServerAuthority())
    {
        return;
    }

    ADMMatchPlayerState* OwnerPlayerState = Cast<ADMMatchPlayerState>(GetOwner());

    if (!IsValid(OwnerPlayerState) || ChangedPlayerState != OwnerPlayerState)
    {
        return;
    }

    /*
     * Death Commit 순간과 실제 Disconnect 순간에만
     * 즉시 Skill transient cleanup을 수행한다.
     *
     * Dead 뒤의 Spectating 전환에서
     * 같은 cleanup RPC를 반복하지 않는다.
     */
    if (NewLifeState != EDMPlayerLifeState::Dead && NewLifeState != EDMPlayerLifeState::Disconnected)
    {
        return;
    }

    CleanupOwnerPawnTransientSkillStatuses();

    if (ADMMatchPlayerController* MatchPlayerController = Cast<ADMMatchPlayerController>(OwnerPlayerState->GetOwner()))
    {
        MatchPlayerController->ClientCleanupSkillPresentationForLifeStateChange();
    }
}

void UDMSkillComponent::CleanupOwnerPawnTransientSkillStatuses()
{
    if (!HasServerAuthority())
    {
        return;
    }

    ADMMatchPlayerState* OwnerPlayerState = Cast<ADMMatchPlayerState>(GetOwner());

    if (!IsValid(OwnerPlayerState))
    {
        return;
    }

    ADMPlayerCharacter* OwnerCharacter = Cast<ADMPlayerCharacter>(OwnerPlayerState->GetPawn());

    if (!IsValid(OwnerCharacter))
    {
        return;
    }

    UDMCharacterStateComponent* StateComponent = OwnerCharacter->GetCharacterStateComponent();

    if (!IsValid(StateComponent))
    {
        return;
    }

    /*
     * 투명 상태로 죽은 Character의 Mesh가
     * Death Presentation에서도 계속 숨겨지는 것을 막는다.
     */
    if (StateComponent->HasStatus(DMGameplayTags::Status_Invisible))
    {
        StateComponent->RemoveStatus(DMGameplayTags::Status_Invisible);
    }

    /*
     * VisionDistort는 Pawn-bound Status이므로
     * Death / Disconnect 이후 새 Pawn으로 승계하지 않는다.
     *
     * Local PostProcess 제거는 PlayerController RPC가 맡는다.
     */
    if (StateComponent->HasStatus(DMGameplayTags::Status_VisionDistorted))
    {
        StateComponent->RemoveStatus(DMGameplayTags::Status_VisionDistorted);
    }
}

void UDMSkillComponent::HandleFootstepAmplifyTargetLifeStateChanged(
    ADMMatchPlayerState* ChangedPlayerState, const EDMPlayerLifeState NewLifeState
)
{
    if (!HasServerAuthority())
    {
        return;
    }

    ADMMatchPlayerState* ActiveTarget = ActiveFootstepAmplifyTarget.Get();

    if (!IsValid(ActiveTarget) || ChangedPlayerState != ActiveTarget)
    {
        return;
    }

    /*
     * Alive가 아니게 되는 최초 순간에
     * FootstepAmplify는 즉시 종료한다.
     */
    if (NewLifeState == EDMPlayerLifeState::Alive)
    {
        return;
    }

    UE_LOG(
        LogDMSkill,
        Log,
        TEXT("[%s] Footstep Amplify ended early because Target became unavailable. Target=%s, LifeState=%s"),
        *GetNameSafe(GetOwner()),
        *GetNameSafe(ChangedPlayerState),
        *UEnum::GetValueAsString(NewLifeState)
    );

    ClearFootstepAmplifyEffect(true);
}

#pragma endregion

#pragma region Lifecycle Cleanup

void UDMSkillComponent::CleanupTransientSkillRuntimeForMatchEnd()
{
    if (!HasServerAuthority())
    {
        return;
    }

    /*
     * Match가 끝났으므로 caster-specific
     * active effect도 종료한다.
     *
     * Local Presentation은 아래 Controller RPC에서
     * 한 번에 정리하므로 여기서는 별도 RPC를 보내지 않는다.
     */
    ClearFootstepAmplifyEffect(false);

    CleanupOwnerPawnTransientSkillStatuses();

    ADMMatchPlayerState* OwnerPlayerState = Cast<ADMMatchPlayerState>(GetOwner());

    if (!IsValid(OwnerPlayerState))
    {
        return;
    }

    if (ADMMatchPlayerController* MatchPlayerController = Cast<ADMMatchPlayerController>(OwnerPlayerState->GetOwner()))
    {
        MatchPlayerController->ClientCleanupAllSkillPresentationForMatchEnd();
    }
}

#pragma endregion

#pragma region Initialization

bool UDMSkillComponent::InitializeSkillSystem(UDMSkillSetDataAsset* InSkillSetData)
{
    // Server에서만 Skill Runtime 상태를 초기화한다.
    if (!HasServerAuthority())
    {
        return false;
    }

    // 사용할 SkillSet이 없으면 초기화할 수 없다.
    if (!ensureMsgf(
            IsValid(InSkillSetData),
            TEXT("[%s] Skill initialization failed. SkillSetData is null."),
            *GetNameSafe(GetOwner())
        ))
    {
        return false;
    }

    // Data Asset 자체의 구성이 올바른지 먼저 검증한다.
    FString DefinitionFailureReason;

    if (!ensureMsgf(
            InSkillSetData->ValidateDefinitionSet(DefinitionFailureReason),
            TEXT("[%s] Skill initialization failed. Reason=%s"),
            *GetNameSafe(GetOwner()),
            *DefinitionFailureReason
        ))
    {
        return false;
    }

    /*
     * 이미 초기화된 경우.
     *
     * 같은 SkillSet으로 다시 호출된 것은
     * 안전하게 성공 처리한다.
     *
     * 다른 SkillSet으로 재초기화해서 기존 cooldown을
     * 초기화하는 것은 허용하지 않는다.
     */
    if (IsValid(SkillSetData.Get()))
    {
        if (SkillSetData.Get() == InSkillSetData)
        {
            return true;
        }

        UE_LOG(
            LogDMSkill,
            Error,
            TEXT("[%s] Skill initialization rejected. SkillSetData was already assigned."),
            *GetNameSafe(GetOwner())
        );

        return false;
    }

    const TArray<FDMSkillDefinition>& Definitions = InSkillSetData->GetSkillDefinitions();

    /*
     * 기존 배열을 직접 수정하지 않고
     * 임시 배열을 완성한 뒤 한 번에 적용한다.
     *
     * 초기화 중간 실패 시 Component가
     * 반쯤 초기화된 상태가 되는 것을 방지한다.
     */
    TArray<FDMSkillCooldownEntry> NewCooldownEntries;

    NewCooldownEntries.Reserve(Definitions.Num());

    for (const FDMSkillDefinition& Definition : Definitions)
    {
        FDMSkillCooldownEntry CooldownEntry;

        CooldownEntry.SkillTag = Definition.SkillTag;

        /*
         * 0.0은 Match 시작 시
         * 모든 Skill이 즉시 사용 가능한 상태를 의미한다.
         *
         * RemainingCooldown을 별도 저장하지 않는다.
         */
        CooldownEntry.EndServerTime = 0.0;

        NewCooldownEntries.Add(CooldownEntry);
    }

    /*
     * 모든 검증과 Entry 생성이 끝난 뒤
     * 실제 Runtime 상태에 반영한다.
     */
    SkillSetData = InSkillSetData;

    SkillCooldownEntries = MoveTemp(NewCooldownEntries);

    /*
     * Listen Server Host의 Local UI는
     * Server에서 OnRep이 발생하지 않으므로
     * Runtime 변경 순간에도 동일한 Read Model Event를 보낸다.
     */
    NotifySkillReadModelChanged();

    /*
     * PlayerState가 Owner인 Replicated Component이므로
     * 초기 상태가 Owning Client에 빠르게 전달되도록 한다.
     */
    if (AActor* OwnerActor = GetOwner())
    {
        OwnerActor->ForceNetUpdate();
    }

    UE_LOG(
        LogDMSkill,
        Log,
        TEXT("[%s] Skill system initialized. SkillCount=%d"),
        *GetNameSafe(GetOwner()),
        SkillCooldownEntries.Num()
    );

    return true;
}

#pragma endregion

#pragma region Cooldown Query

const FDMSkillCooldownEntry* UDMSkillComponent::FindCooldownEntry(const FGameplayTag& SkillTag) const
{
    if (!SkillTag.IsValid())
    {
        return nullptr;
    }

    return SkillCooldownEntries.FindByPredicate(
        [&SkillTag](const FDMSkillCooldownEntry& Entry)
        {
            return Entry.SkillTag == SkillTag;
        }
    );
}

FDMSkillCooldownEntry* UDMSkillComponent::FindCooldownEntryMutable(const FGameplayTag& SkillTag)
{
    if (!SkillTag.IsValid())
    {
        return nullptr;
    }

    return SkillCooldownEntries.FindByPredicate(
        [&SkillTag](const FDMSkillCooldownEntry& Entry)
        {
            return Entry.SkillTag == SkillTag;
        }
    );
}

bool UDMSkillComponent::IsSkillCooldownReady(const FGameplayTag SkillTag) const
{
    const FDMSkillCooldownEntry* CooldownEntry = FindCooldownEntry(SkillTag);

    if (CooldownEntry == nullptr)
    {
        return false;
    }

    return DMGameplay::GetServerTimeSeconds(this) >= CooldownEntry->EndServerTime;
}

float UDMSkillComponent::GetRemainingSkillCooldown(const FGameplayTag SkillTag) const
{
    const FDMSkillCooldownEntry* CooldownEntry = FindCooldownEntry(SkillTag);

    if (CooldownEntry == nullptr)
    {
        return 0.0f;
    }

    const double RemainingTime = CooldownEntry->EndServerTime - DMGameplay::GetServerTimeSeconds(this);

    return static_cast<float>(FMath::Max(0.0, RemainingTime));
}

double UDMSkillComponent::GetSkillCooldownEndServerTime(const FGameplayTag SkillTag) const
{
    const FDMSkillCooldownEntry* CooldownEntry = FindCooldownEntry(SkillTag);

    return CooldownEntry != nullptr ? CooldownEntry->EndServerTime : 0.0;
}

#pragma endregion

#pragma region Targeting

bool UDMSkillComponent::IsTargetEligibleForSkill(const FGameplayTag& SkillTag, ADMMatchPlayerState* Candidate) const
{
    if (!HasServerAuthority())
    {
        return false;
    }

    if (!IsValid(Candidate) || !SkillTag.IsValid())
    {
        return false;
    }

    const ADMMatchPlayerState* OwnerPlayerState = Cast<ADMMatchPlayerState>(GetOwner());

    if (!IsValid(OwnerPlayerState))
    {
        return false;
    }

    /*
     * 자기 자신은 상대 대상 Skill의
     * Target이 될 수 없다.
     */
    if (Candidate == OwnerPlayerState)
    {
        return false;
    }

    /*
     * Alive 이외:
     * Dead / Spectating / Respawning /
     * Disconnected를 모두 제외한다.
     */
    if (Candidate->GetLifeState() != EDMPlayerLifeState::Alive)
    {
        return false;
    }

    /*
     * Leader Targeting에서 RelicManager를
     * 조회하기 위한 정상 BaseSlot이 필요하다.
     */
    if (!Candidate->HasAssignedBaseSlot())
    {
        return false;
    }

    ADMPlayerCharacter* CandidateCharacter = Cast<ADMPlayerCharacter>(Candidate->GetPawn());

    if (!IsValid(CandidateCharacter))
    {
        return false;
    }

    UDMCharacterStateComponent* CandidateStateComponent = CandidateCharacter->GetCharacterStateComponent();

    if (!IsValid(CandidateStateComponent))
    {
        return false;
    }

    /*
     * Spawn Protection 중인 Player는
     * 상대 Skill의 Target이 될 수 없다.
     */
    if (CandidateStateComponent->HasStatus(DMGameplayTags::Status_SpawnProtected))
    {
        return false;
    }

    /*
     * 동일 효과 중복 방지.
     */
    if (IsTargetAlreadyAffectedBySkill(SkillTag, Candidate))
    {
        return false;
    }

    return true;
}

bool UDMSkillComponent::IsTargetAlreadyAffectedBySkill(const FGameplayTag& SkillTag, ADMMatchPlayerState* Candidate)
    const
{
    if (!IsValid(Candidate))
    {
        return false;
    }

    ADMPlayerCharacter* CandidateCharacter = Cast<ADMPlayerCharacter>(Candidate->GetPawn());

    if (!IsValid(CandidateCharacter))
    {
        return false;
    }

    const UDMCharacterStateComponent* CandidateStateComponent = CandidateCharacter->GetCharacterStateComponent();

    if (!IsValid(CandidateStateComponent))
    {
        return false;
    }

    /*
     * Vision Distort는 Target의 범용 Status이므로
     * CharacterStateComponent가 SSOT다.
     */
    if (SkillTag == DMGameplayTags::Skill_VisionDistort)
    {
        return CandidateStateComponent->HasStatus(DMGameplayTags::Status_VisionDistorted);
    }

    if (SkillTag == DMGameplayTags::Skill_FootstepAmplify)
    {
        /*
         * Footstep Amplify는 Target의 범용 Status가 아니다.
         *
         * 이 SkillComponent가 현재 caster를 의미하므로
         * 같은 caster가 같은 target에 이미 효과를
         * 유지 중인지만 검사한다.
         */
        return ActiveFootstepAmplifyTarget.IsValid() && ActiveFootstepAmplifyTarget.Get() == Candidate
            && DMGameplay::GetServerTimeSeconds(this) < FootstepAmplifyEndServerTime;
    }

    return false;
}

void UDMSkillComponent::GatherEligibleTargetsForSkill(
    const FGameplayTag& SkillTag, TArray<ADMMatchPlayerState*>& OutTargets
) const
{
    OutTargets.Reset();

    if (!HasServerAuthority())
    {
        return;
    }

    if (!IsSkillSystemInitialized())
    {
        return;
    }

    const FDMSkillDefinition* Definition = SkillSetData->FindSkillDefinition(SkillTag);

    if (Definition == nullptr || !Definition->bRequiresTargetSelection)
    {
        return;
    }

    const UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return;
    }

    const ADMMatchGameState* MatchGameState = World->GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        return;
    }

    /*
     * 최대 4인 Match이므로 별도 Target Cache를
     * 유지할 필요가 없다.
     */
    OutTargets.Reserve(MatchGameState->PlayerArray.Num());

    for (APlayerState* PlayerState : MatchGameState->PlayerArray)
    {
        ADMMatchPlayerState* Candidate = Cast<ADMMatchPlayerState>(PlayerState);

        /*
         * NPC는 ADMMatchPlayerState가 없으므로
         * 여기서 자연스럽게 제외된다.
         */
        if (!IsTargetEligibleForSkill(SkillTag, Candidate))
        {
            continue;
        }

        OutTargets.Add(Candidate);
    }
}

bool UDMSkillComponent::HasEligibleTargetForSkill(const FGameplayTag& SkillTag) const
{
    TArray<ADMMatchPlayerState*> EligibleTargets;

    GatherEligibleTargetsForSkill(SkillTag, EligibleTargets);

    return !EligibleTargets.IsEmpty();
}

ADMMatchPlayerState*
UDMSkillComponent::ResolveSkillTarget(const FGameplayTag& SkillTag, const EDMSkillTargetMode TargetMode) const
{
    if (!HasServerAuthority())
    {
        return nullptr;
    }

    if (!IsSkillSystemInitialized())
    {
        return nullptr;
    }

    const FDMSkillDefinition* Definition = SkillSetData->FindSkillDefinition(SkillTag);

    if (Definition == nullptr || !Definition->bRequiresTargetSelection)
    {
        return nullptr;
    }

    TArray<ADMMatchPlayerState*> EligibleTargets;

    GatherEligibleTargetsForSkill(SkillTag, EligibleTargets);

    if (EligibleTargets.IsEmpty())
    {
        return nullptr;
    }

    switch (TargetMode)
    {
    case EDMSkillTargetMode::RandomEligible:
    {
        const int32 RandomIndex = FMath::RandRange(0, EligibleTargets.Num() - 1);

        return EligibleTargets[RandomIndex];
    }

    case EDMSkillTargetMode::LeadingEligible:
    {
        const UWorld* World = GetWorld();

        if (World == nullptr)
        {
            return nullptr;
        }

        const ADMMatchGameMode* MatchGameMode = Cast<ADMMatchGameMode>(World->GetAuthGameMode());

        if (!IsValid(MatchGameMode))
        {
            return nullptr;
        }

        const ADMRelicManager* RelicManager = MatchGameMode->GetRelicManager();

        if (!IsValid(RelicManager))
        {
            return nullptr;
        }

        int32 HighestRegisteredRelicCount = INDEX_NONE;

        TArray<ADMMatchPlayerState*> LeadingTargets;

        LeadingTargets.Reserve(EligibleTargets.Num());

        for (ADMMatchPlayerState* Candidate : EligibleTargets)
        {
            if (!IsValid(Candidate))
            {
                continue;
            }

            const int32 RegisteredRelicCount = RelicManager->GetRegisteredRelicCount(Candidate->GetBaseSlot());

            if (RegisteredRelicCount > HighestRegisteredRelicCount)
            {
                HighestRegisteredRelicCount = RegisteredRelicCount;

                LeadingTargets.Reset();

                LeadingTargets.Add(Candidate);

                continue;
            }

            if (RegisteredRelicCount == HighestRegisteredRelicCount)
            {
                LeadingTargets.Add(Candidate);
            }
        }

        if (LeadingTargets.IsEmpty())
        {
            return nullptr;
        }

        /*
         * 등록 Relic 수가 동일하면
         * 해당 공동 1위 중 서버에서 랜덤 선택한다.
         */
        const int32 RandomLeaderIndex = FMath::RandRange(0, LeadingTargets.Num() - 1);

        return LeadingTargets[RandomLeaderIndex];
    }

    default:
        break;
    }

    return nullptr;
}

#pragma endregion

#pragma region Availability

FDMActionAvailabilityResult UDMSkillComponent::GetSkillAvailability(const FGameplayTag& SkillTag) const
{
    /*
     * 실제 Skill 사용 승인은 Server만 한다.
     */
    if (!HasServerAuthority())
    {
        return FDMActionAvailabilityResult{};
    }

    if (!IsSkillSystemInitialized())
    {
        UE_LOG(
            LogDMSkill,
            Error,
            TEXT("[%s] Skill availability requested before initialization."),
            *GetNameSafe(GetOwner())
        );

        return FDMActionAvailabilityResult{};
    }

    const FDMSkillDefinition* Definition = SkillSetData->FindSkillDefinition(SkillTag);

    if (Definition == nullptr)
    {
        UE_LOG(
            LogDMSkill,
            Warning,
            TEXT("[%s] Unknown SkillTag requested. Skill=%s"),
            *GetNameSafe(GetOwner()),
            *SkillTag.ToString()
        );

        return FDMActionAvailabilityResult{};
    }

    const UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return FDMActionAvailabilityResult{};
    }

    const ADMMatchGameState* MatchGameState = World->GetGameState<ADMMatchGameState>();

    if (!IsValid(MatchGameState))
    {
        return FDMActionAvailabilityResult{};
    }

    const EDMMatchPhase CurrentPhase = MatchGameState->GetCurrentPhase();

    if (CurrentPhase != EDMMatchPhase::Playing && CurrentPhase != EDMMatchPhase::Overtime)
    {
        return FDMActionAvailabilityResult ::Blocked(EDMActionBlockReason::MatchPhase);
    }

    ADMMatchPlayerState* MatchPlayerState = Cast<ADMMatchPlayerState>(GetOwner());

    if (!IsValid(MatchPlayerState))
    {
        return FDMActionAvailabilityResult{};
    }

    if (MatchPlayerState->GetLifeState() != EDMPlayerLifeState::Alive)
    {
        return FDMActionAvailabilityResult ::Blocked(EDMActionBlockReason::LifeState);
    }

    const ADMPlayerCharacter* PlayerCharacter = Cast<ADMPlayerCharacter>(MatchPlayerState->GetPawn());

    if (!IsValid(PlayerCharacter))
    {
        /*
         * Alive인데 Pawn이 없다면 정상적인 Skill 사용
         * 상태가 아니므로 fail closed.
         */
        return FDMActionAvailabilityResult ::Blocked(EDMActionBlockReason::LifeState);
    }

    const UDMCharacterStateComponent* StateComponent = PlayerCharacter->GetCharacterStateComponent();

    if (!IsValid(StateComponent))
    {
        return FDMActionAvailabilityResult{};
    }

    if (StateComponent->IsBusy())
    {
        return FDMActionAvailabilityResult ::Blocked(EDMActionBlockReason::Busy);
    }

    if (StateComponent->HasStatus(DMGameplayTags::Status_Stunned)
        || StateComponent->HasStatus(DMGameplayTags::Status_SpawnProtected))
    {
        return FDMActionAvailabilityResult ::Blocked(EDMActionBlockReason::Status);
    }

    /*
     * CharacterState의 현재 정책을 최종적으로 다시 사용한다.
     * 위의 개별 검사는 UI BlockReason 구분을 위한 것이다.
     */
    if (!StateComponent->CanPerformAction(EDMActionRequest::Skill))
    {
        return FDMActionAvailabilityResult ::Blocked(EDMActionBlockReason::Status);
    }

    const UDMPoseComponent* PoseComponent = PlayerCharacter->FindComponentByClass<UDMPoseComponent>();

    if (IsValid(PoseComponent) && !PoseComponent->IsActionAllowedByPose(EDMActionRequest::Skill))
    {
        return FDMActionAvailabilityResult ::Blocked(EDMActionBlockReason::Pose);
    }

    if (!Definition->bCanUseWhileCarryingRelic)
    {
        const ADMMatchGameMode* MatchGameMode = Cast<ADMMatchGameMode>(World->GetAuthGameMode());

        if (!IsValid(MatchGameMode))
        {
            return FDMActionAvailabilityResult{};
        }

        const ADMRelicManager* RelicManager = MatchGameMode->GetRelicManager();

        if (!IsValid(RelicManager))
        {
            return FDMActionAvailabilityResult{};
        }

        if (RelicManager->IsPlayerCarryingRelic(MatchPlayerState))
        {
            return FDMActionAvailabilityResult ::Blocked(EDMActionBlockReason ::CarryingRelic);
        }
    }

    if (!IsSkillCooldownReady(SkillTag))
    {
        return FDMActionAvailabilityResult ::Blocked(EDMActionBlockReason::Cooldown);
    }

    /*
     * 상대 Target을 요구하는 Skill은
     * 최소 한 명의 Server-valid target이 있어야 한다.
     *
     * Invisibility처럼 Self Skill은 이 검사를 하지 않는다.
     */
    if (Definition->bRequiresTargetSelection && !HasEligibleTargetForSkill(SkillTag))
    {
        return FDMActionAvailabilityResult ::Blocked(EDMActionBlockReason::NoTarget);
    }

    return FDMActionAvailabilityResult ::Allowed();
}

#pragma endregion

#pragma region Validation

bool UDMSkillComponent::HasServerAuthority() const
{
    const AActor* OwnerActor = GetOwner();

    return IsValid(OwnerActor) && OwnerActor->HasAuthority();
}

#pragma endregion

#pragma region Cooldown Mutation

bool UDMSkillComponent::StartSkillCooldown(const FGameplayTag& SkillTag)
{
    if (!HasServerAuthority())
    {
        return false;
    }

    if (!IsSkillSystemInitialized())
    {
        return false;
    }

    const FDMSkillDefinition* Definition = SkillSetData->FindSkillDefinition(SkillTag);

    if (Definition == nullptr)
    {
        return false;
    }

    FDMSkillCooldownEntry* CooldownEntry = FindCooldownEntryMutable(SkillTag);

    if (CooldownEntry == nullptr)
    {
        return false;
    }

    /*
     * 같은 Skill cooldown을 중복 소비하지 않는다.
     */
    if (!IsSkillCooldownReady(SkillTag))
    {
        return false;
    }

    CooldownEntry->EndServerTime =
        DMGameplay::GetServerTimeSeconds(this) + static_cast<double>(FMath::Max(0.0f, Definition->Cooldown));

    /*
     * Listen Server Host UI 갱신.
     *
     * Remote Owning Client는 OwnerOnly Replication 후
     * OnRep_SkillCooldownEntries()에서 같은 Event를 받는다.
     */
    NotifySkillReadModelChanged();

    if (AActor* OwnerActor = GetOwner())
    {
        OwnerActor->ForceNetUpdate();
    }

    UE_LOG(
        LogDMSkill,
        Log,
        TEXT("[%s] Skill cooldown started. Skill=%s, EndServerTime=%.3f"),
        *GetNameSafe(GetOwner()),
        *SkillTag.ToString(),
        CooldownEntry->EndServerTime
    );

    return true;
}

#pragma endregion

#pragma region Skill Execution

bool UDMSkillComponent::TryExecuteInvisibility()
{
    if (!HasServerAuthority())
    {
        return false;
    }

    if (!IsSkillSystemInitialized())
    {
        return false;
    }

    const FGameplayTag& SkillTag = DMGameplayTags::Skill_Invisibility;

    /*
     * MatchPhase / LifeState / Busy / Stun /
     * SpawnProtection / Pose / Relic Carry /
     * Cooldown 등을 기존 공통 정책에서 다시 검증한다.
     */
    const FDMActionAvailabilityResult Availability = GetSkillAvailability(SkillTag);

    if (!Availability.bAllowed)
    {
        UE_LOG(
            LogDMSkill,
            Log,
            TEXT("[%s] Invisibility rejected. Reason=%s"),
            *GetNameSafe(GetOwner()),
            *UEnum::GetValueAsString(Availability.BlockReason)
        );

        return false;
    }

    const FDMSkillDefinition* Definition = SkillSetData->FindSkillDefinition(SkillTag);

    if (Definition == nullptr)
    {
        return false;
    }

    /*
     * Invisibility는 Self Skill이어야 한다.
     */
    if (!ensureMsgf(
            !Definition->bRequiresTargetSelection,
            TEXT("[%s] Invisibility configuration invalid. It must not require target selection."),
            *GetNameSafe(GetOwner())
        ))
    {
        return false;
    }

    if (!ensureMsgf(
            Definition->Duration > 0.0f,
            TEXT("[%s] Invisibility configuration invalid. Duration=%.3f"),
            *GetNameSafe(GetOwner()),
            Definition->Duration
        ))
    {
        return false;
    }

    ADMMatchPlayerState* MatchPlayerState = Cast<ADMMatchPlayerState>(GetOwner());

    if (!IsValid(MatchPlayerState))
    {
        return false;
    }

    ADMPlayerCharacter* PlayerCharacter = Cast<ADMPlayerCharacter>(MatchPlayerState->GetPawn());

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
     * Status 적용이 먼저 성공해야 한다.
     *
     * 동일 Status가 이미 있거나 Server 상태가 잘못됐다면
     * 여기서 실패하고 cooldown은 소비하지 않는다.
     */
    if (!StateComponent->ApplyTimedStatus(DMGameplayTags::Status_Invisible, Definition->Duration))
    {
        UE_LOG(LogDMSkill, Log, TEXT("[%s] Invisibility status application failed."), *GetNameSafe(GetOwner()));

        return false;
    }

    /*
     * Gameplay Effect 적용 성공 이후에만
     * cooldown을 commit한다.
     */
    if (!ensureMsgf(
            StartSkillCooldown(SkillTag),
            TEXT("[%s] Invisibility cooldown commit failed. Status was rolled back."),
            *GetNameSafe(GetOwner())
        ))
    {
        /*
         * 이 경로는 정상 구성에서는 발생하면 안 된다.
         *
         * 그래도 Effect만 적용되고 cooldown이 없는
         * 비정상 상태를 남기지 않도록 rollback한다.
         */
        StateComponent->RemoveStatus(DMGameplayTags::Status_Invisible);

        return false;
    }

    UE_LOG(
        LogDMSkill,
        Log,
        TEXT("[%s] Invisibility activated. Duration=%.3f, Cooldown=%.3f"),
        *GetNameSafe(GetOwner()),
        Definition->Duration,
        Definition->Cooldown
    );

    return true;
}

bool UDMSkillComponent::TryExecuteFootstepAmplify(ADMMatchPlayerState* TargetPlayerState)
{
    if (!HasServerAuthority())
    {
        return false;
    }

    if (!IsSkillSystemInitialized() || !IsValid(TargetPlayerState))
    {
        return false;
    }

    const FGameplayTag& SkillTag = DMGameplayTags::Skill_FootstepAmplify;

    /*
     * Caster 자신의 현재 상태 재검증.
     */
    const FDMActionAvailabilityResult Availability = GetSkillAvailability(SkillTag);

    if (!Availability.bAllowed)
    {
        UE_LOG(
            LogDMSkill,
            Log,
            TEXT("[%s] Footstep Amplify rejected. Reason=%s"),
            *GetNameSafe(GetOwner()),
            *UEnum::GetValueAsString(Availability.BlockReason)
        );

        return false;
    }

    /*
     * Target 자체도 실행 직전에 다시 검증.
     *
     * Targeting 진입 이후 Death /
     * SpawnProtection 등의 상태가 바뀔 수 있다.
     */
    if (!IsTargetEligibleForSkill(SkillTag, TargetPlayerState))
    {
        UE_LOG(
            LogDMSkill, Log, TEXT("[%s] Footstep Amplify rejected. Target became invalid."), *GetNameSafe(GetOwner())
        );

        return false;
    }

    const FDMSkillDefinition* Definition = SkillSetData->FindSkillDefinition(SkillTag);

    if (Definition == nullptr || !Definition->bRequiresTargetSelection || Definition->Duration <= 0.0f)
    {
        return false;
    }

    ADMMatchPlayerState* CasterPlayerState = Cast<ADMMatchPlayerState>(GetOwner());

    if (!IsValid(CasterPlayerState))
    {
        return false;
    }

    ADMMatchPlayerController* CasterController = Cast<ADMMatchPlayerController>(CasterPlayerState->GetOwner());

    ADMMatchPlayerController* TargetController = Cast<ADMMatchPlayerController>(TargetPlayerState->GetOwner());

    if (!IsValid(CasterController) || !IsValid(TargetController))
    {
        return false;
    }

    UWorld* World = GetWorld();

    if (World == nullptr)
    {
        return false;
    }

    /*
     * 이전 stale context가 있다면 제거.
     */
    ClearFootstepAmplifyEffect(true);

    ActiveFootstepAmplifyTarget = TargetPlayerState;

    /*
     * Target이 효과 도중 죽거나 Disconnect되면
     * 즉시 caster-specific 효과를 종료한다.
     */
    TargetPlayerState->OnLifeStateChanged().RemoveAll(this);

    TargetPlayerState->OnLifeStateChanged().AddUObject(this, &ThisClass::HandleFootstepAmplifyTargetLifeStateChanged);

    FootstepAmplifyEndServerTime = DMGameplay::GetServerTimeSeconds(this) + static_cast<double>(Definition->Duration);

    /*
     * Caster Client에게만 Target의 Audio ×2를 전달한다.
     */
    CasterController->ClientSetFootstepAmplifyPresentation(TargetPlayerState, true, FootstepAmplifyEndServerTime);

    /*
     * 실제 효과 준비가 끝난 뒤 cooldown commit.
     */
    if (!ensureMsgf(
            StartSkillCooldown(SkillTag),
            TEXT("[%s] Footstep Amplify cooldown commit failed. Effect rolled back."),
            *GetNameSafe(GetOwner())
        ))
    {
        ClearFootstepAmplifyEffect(true);

        return false;
    }

    /*
     * 8초 effect duration.
     */
    World->GetTimerManager().SetTimer(
        FootstepAmplifyDurationTimerHandle, this, &ThisClass::HandleFootstepAmplifyExpired, Definition->Duration, false
    );

    /*
     * 기획의 "2~3초 후 경고"를
     * Server random 2.0~3.0초로 해석한다.
     */
    const float WarningDelay = FMath::FRandRange(2.0f, 3.0f);

    World->GetTimerManager().SetTimer(
        FootstepAmplifyWarningTimerHandle,
        this,
        &ThisClass::HandleFootstepAmplifyWarningDelayElapsed,
        WarningDelay,
        false
    );

    UE_LOG(
        LogDMSkill,
        Log,
        TEXT("[%s] Footstep Amplify activated. Target=%s, Duration=%.3f, Cooldown=%.3f, WarningDelay=%.3f"),
        *GetNameSafe(GetOwner()),
        *GetNameSafe(TargetPlayerState),
        Definition->Duration,
        Definition->Cooldown,
        WarningDelay
    );

    return true;
}

void UDMSkillComponent::HandleFootstepAmplifyExpired()
{
    if (!HasServerAuthority())
    {
        return;
    }

    ClearFootstepAmplifyEffect(true);

    UE_LOG(LogDMSkill, Log, TEXT("[%s] Footstep Amplify expired."), *GetNameSafe(GetOwner()));
}

void UDMSkillComponent::HandleFootstepAmplifyWarningDelayElapsed()
{
    if (!HasServerAuthority())
    {
        return;
    }

    ADMMatchPlayerState* TargetPlayerState = ActiveFootstepAmplifyTarget.Get();

    if (!IsValid(TargetPlayerState))
    {
        return;
    }

    /*
     * 효과가 이미 종료된 경우 늦은 Warning은 보내지 않는다.
     */
    if (DMGameplay::GetServerTimeSeconds(this) >= FootstepAmplifyEndServerTime)
    {
        return;
    }

    ADMMatchPlayerController* TargetController = Cast<ADMMatchPlayerController>(TargetPlayerState->GetOwner());

    if (!IsValid(TargetController))
    {
        return;
    }

    // 피해자에게 효과의 남은 시간을 정확히 계산할 수 있게 한다.
    TargetController->ClientNotifyFootstepAmplifyWarning(FootstepAmplifyEndServerTime);

    UE_LOG(
        LogDMSkill,
        Verbose,
        TEXT("[%s] Footstep Amplify warning sent. Target=%s"),
        *GetNameSafe(GetOwner()),
        *GetNameSafe(TargetPlayerState)
    );
}

void UDMSkillComponent::ClearFootstepAmplifyEffect(const bool bNotifyCasterClient)
{
    UWorld* World = GetWorld();

    if (World != nullptr)
    {
        World->GetTimerManager().ClearTimer(FootstepAmplifyDurationTimerHandle);

        World->GetTimerManager().ClearTimer(FootstepAmplifyWarningTimerHandle);
    }

    ADMMatchPlayerState* CasterPlayerState = Cast<ADMMatchPlayerState>(GetOwner());

    ADMMatchPlayerState* TargetPlayerState = ActiveFootstepAmplifyTarget.Get();

    if (IsValid(TargetPlayerState))
    {
        TargetPlayerState->OnLifeStateChanged().RemoveAll(this);
    }

    if (bNotifyCasterClient && IsValid(CasterPlayerState))
    {
        if (ADMMatchPlayerController* CasterController = Cast<ADMMatchPlayerController>(CasterPlayerState->GetOwner()))
        {
            CasterController->ClientSetFootstepAmplifyPresentation(TargetPlayerState, false, 0.0);
        }
    }

    ActiveFootstepAmplifyTarget.Reset();

    FootstepAmplifyEndServerTime = 0.0;
}

bool UDMSkillComponent::TryExecuteVisionDistort(ADMMatchPlayerState* TargetPlayerState)
{
    if (!HasServerAuthority())
    {
        return false;
    }

    if (!IsSkillSystemInitialized() || !IsValid(TargetPlayerState))
    {
        return false;
    }

    const FGameplayTag& SkillTag = DMGameplayTags::Skill_VisionDistort;

    const FDMActionAvailabilityResult Availability = GetSkillAvailability(SkillTag);

    if (!Availability.bAllowed)
    {
        UE_LOG(
            LogDMSkill,
            Log,
            TEXT("[%s] Vision Distort rejected. Reason=%s"),
            *GetNameSafe(GetOwner()),
            *UEnum::GetValueAsString(Availability.BlockReason)
        );

        return false;
    }

    /*
     * Targeting 진입 이후
     * Target의 상태가 바뀌었을 수 있으므로
     * 실행 직전에 다시 검증한다.
     */
    if (!IsTargetEligibleForSkill(SkillTag, TargetPlayerState))
    {
        UE_LOG(LogDMSkill, Log, TEXT("[%s] Vision Distort rejected. Target became invalid."), *GetNameSafe(GetOwner()));

        return false;
    }

    const FDMSkillDefinition* Definition = SkillSetData->FindSkillDefinition(SkillTag);

    if (Definition == nullptr || !Definition->bRequiresTargetSelection || Definition->Duration <= 0.0f)
    {
        return false;
    }

    ADMPlayerCharacter* TargetCharacter = Cast<ADMPlayerCharacter>(TargetPlayerState->GetPawn());

    if (!IsValid(TargetCharacter))
    {
        return false;
    }

    UDMCharacterStateComponent* TargetStateComponent = TargetCharacter->GetCharacterStateComponent();

    if (!IsValid(TargetStateComponent))
    {
        return false;
    }

    ADMMatchPlayerController* TargetController = Cast<ADMMatchPlayerController>(TargetPlayerState->GetOwner());

    if (!IsValid(TargetController))
    {
        return false;
    }

    const float MainDuration = Definition->Duration;

    const float TotalDuration = VisionDistortEntryDuration + MainDuration + VisionDistortExitDuration;

    const double EffectStartServerTime = DMGameplay::GetServerTimeSeconds(this);

    /*
     * Status가 Gameplay SSOT다.
     *
     * 같은 Vision effect가 이미 적용 중이면
     * ApplyTimedStatus 자체도 실패한다.
     */
    if (!TargetStateComponent->ApplyTimedStatus(DMGameplayTags::Status_VisionDistorted, TotalDuration))
    {
        return false;
    }

    /*
     * Target Client에만 Presentation 명령.
     *
     * Caster 정보는 전달하지 않는다.
     */
    TargetController->ClientBeginVisionDistortPresentation(
        EffectStartServerTime, VisionDistortEntryDuration, MainDuration, VisionDistortExitDuration
    );

    /*
     * 효과 적용 성공 이후 cooldown commit.
     */
    if (!ensureMsgf(
            StartSkillCooldown(SkillTag),
            TEXT("[%s] Vision Distort cooldown commit failed. Effect rolled back."),
            *GetNameSafe(GetOwner())
        ))
    {
        TargetStateComponent->RemoveStatus(DMGameplayTags::Status_VisionDistorted);

        TargetController->ClientClearVisionDistortPresentation();

        return false;
    }

    UE_LOG(
        LogDMSkill,
        Log,
        TEXT("[%s] Vision Distort activated. Target=%s, Entry=%.3f, Main=%.3f, Exit=%.3f, Total=%.3f, Cooldown=%.3f"),
        *GetNameSafe(GetOwner()),
        *GetNameSafe(TargetPlayerState),
        VisionDistortEntryDuration,
        MainDuration,
        VisionDistortExitDuration,
        TotalDuration,
        Definition->Cooldown
    );

    return true;
}

#pragma endregion
