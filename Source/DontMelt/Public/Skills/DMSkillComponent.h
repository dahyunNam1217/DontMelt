#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Data/DMSkillSetDataAsset.h"
#include "Skills/DMSkillTypes.h"
#include "Core/DMMatchTypes.h"
#include "DMSkillComponent.generated.h"

class ADMMatchPlayerState;

/**
 * Skill UI가 원본 데이터를 다시 읽어야 할 때 발생한다.
 *
 * Delegate 자체는 Gameplay 데이터를 소유하지 않는다.
 */
DECLARE_MULTICAST_DELEGATE(FDMOnSkillReadModelChanged);

/**
 * Player별 Skill Runtime 상태의 SSOT.
 *
 * MatchPlayerState가 소유하므로
 * Character 사망/Respawn과 관계없이 유지된다.
 *
 * 책임:
 * - Skill Definition Set 참조
 * - Skill별 Cooldown EndServerTime 보관
 * - Server-authoritative 대상 선택 정책
 * - Skill 실행과 Skill별 active effect context 관리
 *
 * 책임지지 않는 것:
 * - Character Status 자체의 저장
 * - 로컬 VFX / Sound의 Gameplay 원본
 */
UCLASS(ClassGroup = (DontMelt))
class DONTMELT_API UDMSkillComponent : public UActorComponent
{
    GENERATED_BODY()

public:
#pragma region Lifecycle

    UDMSkillComponent();

    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

#pragma region Initialization

    /**
     * Match에서 사용할 SkillSet을
     * Server가 한 번 초기화한다.
     *
     * 모든 Skill cooldown은
     * EndServerTime = 0.0으로 시작한다.
     *
     * 동일 Component를 다른 SkillSet으로
     * 다시 초기화하지 않는다.
     */
    bool InitializeSkillSystem(UDMSkillSetDataAsset* InSkillSetData);

#pragma endregion

#pragma region Query

    UFUNCTION(BlueprintPure, Category = "DM|Skill")
    bool IsSkillSystemInitialized() const
    {
        return IsValid(SkillSetData.Get()) && !SkillCooldownEntries.IsEmpty();
    }

    UFUNCTION(BlueprintPure, Category = "DM|Skill")
    UDMSkillSetDataAsset* GetSkillSetData() const
    {
        return SkillSetData;
    }

    const TArray<FDMSkillCooldownEntry>& GetSkillCooldownEntries() const
    {
        return SkillCooldownEntries;
    }

    /**
     * Skill Definition 또는 Cooldown Runtime 값이
     * 변경되었음을 Local UI에 알린다.
     *
     * UI는 이 Delegate의 payload를 저장하는 대신
     * 현재 Component 값을 다시 읽는다.
     */
    FDMOnSkillReadModelChanged& OnSkillReadModelChanged()
    {
        return SkillReadModelChangedDelegate;
    }

    /**
     * 해당 Skill의 cooldown이 끝났는지 반환한다.
     *
     * Client에서 UI 예측용으로 읽을 수 있지만
     * 실제 사용 승인은 Server가 다시 판정한다.
     */
    UFUNCTION(BlueprintPure, Category = "DM|Skill|Cooldown")
    bool IsSkillCooldownReady(FGameplayTag SkillTag) const;

    /**
     * 현재 ServerWorldTime 기준 남은 cooldown.
     *
     * Remaining 값을 별도로 저장하지 않는다.
     */
    UFUNCTION(BlueprintPure, Category = "DM|Skill|Cooldown")
    float GetRemainingSkillCooldown(FGameplayTag SkillTag) const;

    UFUNCTION(BlueprintPure, Category = "DM|Skill|Cooldown")
    double GetSkillCooldownEndServerTime(FGameplayTag SkillTag) const;

    /**
     * 실제 Skill 사용 직전
     * Server-authoritative 공통 사용 가능 여부를 판정한다.
     *
     * Target Skill은 유효 대상 존재 여부까지 확인한다.
     */
    FDMActionAvailabilityResult GetSkillAvailability(const FGameplayTag& SkillTag) const;

    /**
     * 성공적으로 실행된 Skill의 cooldown을 시작한다.
     *
     * Skill 효과가 실패했는데 cooldown부터 소비하지 않도록
     * 실제 실행 성공 이후 호출하는 API다.
     */
    bool StartSkillCooldown(const FGameplayTag& SkillTag);

#pragma endregion

#pragma region Skill Execution

    /**
     * Invisibility Skill을 Server authoritative하게 실행한다.
     *
     * 성공 순서:
     * 1. 공통 Availability 검증
     * 2. Status.Invisible 적용
     * 3. Cooldown 시작
     *
     * Status 적용 실패 시 Cooldown을 소비하지 않는다.
     */
    bool TryExecuteInvisibility();

    /**
     * FootstepAmplify를 Server authoritative하게 실행한다.
     *
     * Target은 이미 Server Target Policy로 선정된
     * MatchPlayerState여야 한다.
     */
    bool TryExecuteFootstepAmplify(ADMMatchPlayerState* TargetPlayerState);

    /**
     * VisionDistort를 Server authoritative하게 실행한다.
     */
    bool TryExecuteVisionDistort(ADMMatchPlayerState* TargetPlayerState);

#pragma endregion

#pragma region Lifecycle Cleanup

    /**
     * Finished / Aborted Match에서
     * 일시적인 Skill Runtime만 정리한다.
     *
     * Cooldown EndServerTime은 초기화하지 않는다.
     */
    void CleanupTransientSkillRuntimeForMatchEnd();

#pragma endregion

#pragma region Targeting

    /**
     * 해당 Target Skill에 현재 유효한 상대가
     * 하나 이상 존재하는지 Server에서 검사한다.
     *
     * 대상 이름/위치/목록을 Client에 전달하지 않는다.
     */
    bool HasEligibleTargetForSkill(const FGameplayTag& SkillTag) const;

    /**
     * 지정된 TargetMode에 따라
     * Server가 최종 Target PlayerState를 선택한다.
     *
     * RandomEligible:
     * 유효 상대 전체 중 랜덤.
     *
     * LeadingEligible:
     * 유효 상대 중 RegisteredRelicCount가 가장 높은
     * Player를 선택하며 동률은 랜덤 처리한다.
     */
    ADMMatchPlayerState* ResolveSkillTarget(const FGameplayTag& SkillTag, EDMSkillTargetMode TargetMode) const;

#pragma endregion

private:
#pragma region Validation

    bool HasServerAuthority() const;

    const FDMSkillCooldownEntry* FindCooldownEntry(const FGameplayTag& SkillTag) const;

    FDMSkillCooldownEntry* FindCooldownEntryMutable(const FGameplayTag& SkillTag);

#pragma endregion

#pragma region Replication Notification

    UFUNCTION()
    void OnRep_SkillSetData();

    UFUNCTION()
    void OnRep_SkillCooldownEntries();

    void NotifySkillReadModelChanged();

#pragma endregion

#pragma region Lifecycle Internal

    void HandleOwnerLifeStateChanged(ADMMatchPlayerState* ChangedPlayerState, EDMPlayerLifeState NewLifeState);

    void HandleFootstepAmplifyTargetLifeStateChanged(
        ADMMatchPlayerState* ChangedPlayerState, EDMPlayerLifeState NewLifeState
    );

    /**
     * Owner Pawn에 붙어 있는
     * Skill 전용 transient Status만 제거한다.
     */
    void CleanupOwnerPawnTransientSkillStatuses();

#pragma endregion

#pragma region Targeting Internal

    /**
     * 현재 Skill에 대해 Target이 될 수 있는
     * 모든 MatchPlayerState를 Server에서 수집한다.
     */
    void GatherEligibleTargetsForSkill(const FGameplayTag& SkillTag, TArray<ADMMatchPlayerState*>& OutTargets) const;

    /**
     * Candidate 한 명이 해당 Skill의
     * 대상 조건을 만족하는지 검사한다.
     */
    bool IsTargetEligibleForSkill(const FGameplayTag& SkillTag, ADMMatchPlayerState* Candidate) const;

    /**
     * 동일 효과가 이미 적용 중인지 검사한다.
     *
     * VisionDistort:
     * Target의 Status.VisionDistorted를 검사한다.
     *
     * FootstepAmplify:
     * 현재 Caster SkillComponent가 동일 Target에
     * active context를 유지 중인지 검사한다.
     */
    bool IsTargetAlreadyAffectedBySkill(const FGameplayTag& SkillTag, ADMMatchPlayerState* Candidate) const;

#pragma endregion

#pragma region Footstep Amplify Internal

    void HandleFootstepAmplifyExpired();

    void HandleFootstepAmplifyWarningDelayElapsed();

    void ClearFootstepAmplifyEffect(bool bNotifyCasterClient);

#pragma endregion

#pragma region Runtime State

    /**
     * 현재 Match에서 사용할 Skill Definition Set.
     *
     * Server가 초기화하고 Owning Client에만 전달한다.
     * Data Asset 자체는 Runtime 상태 원본이 아니다.
     */
    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_SkillSetData,
        BlueprintReadOnly,
        Category = "DM|Skill",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UDMSkillSetDataAsset> SkillSetData = nullptr;

    /**
     * Skill cooldown의 유일한 Runtime 원본.
     *
     * RemainingCooldown은 저장하지 않는다.
     *
     * Remaining =
     * EndServerTime - ServerWorldTime
     */
    UPROPERTY(
        VisibleInstanceOnly,
        ReplicatedUsing = OnRep_SkillCooldownEntries,
        BlueprintReadOnly,
        Category = "DM|Skill|Cooldown",
        meta = (AllowPrivateAccess = "true")
    )
    TArray<FDMSkillCooldownEntry> SkillCooldownEntries;

    /**
     * FootstepAmplify의 현재 caster-target 관계.
     *
     * 이 SkillComponent 자체가 Caster에게 속하므로
     * Target만 저장하면 관계가 완성된다.
     *
     * Server-only runtime context이며 Replicate하지 않는다.
     */
    TWeakObjectPtr<ADMMatchPlayerState> ActiveFootstepAmplifyTarget;

    /**
     * Server 기준 FootstepAmplify 종료 시각.
     *
     * Client Audio Presentation 동기화에도 사용한다.
     */
    double FootstepAmplifyEndServerTime = 0.0;

    /**
     * 실제 Gameplay effect duration timer.
     */
    FTimerHandle FootstepAmplifyDurationTimerHandle;

    /**
     * Target warning 전달 timer.
     */
    FTimerHandle FootstepAmplifyWarningTimerHandle;

#pragma endregion

#pragma region Delegates

    FDMOnSkillReadModelChanged SkillReadModelChangedDelegate;

#pragma endregion
};