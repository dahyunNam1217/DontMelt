#pragma once

#include "CoreMinimal.h"
#include "Characters/DMCharacterTypes.h"
#include "Components/ActorComponent.h"
#include "TimerManager.h"
#include "DMCombatComponent.generated.h"

class UDMCombatRuleDataAsset;
class UAnimMontage;
class ADMJellyCharacterBase;
class ADMPlayerCharacter;
class ADMNPCCharacter;

/** 공격·방어 쿨다운 원본 값이 변경되었을 때 발생하는 로컬 알림 */
DECLARE_MULTICAST_DELEGATE(FDMOnCombatCooldownChanged);

/**
 * 플레이어의 공격과 방어 판정을 담당할 컴포넌트.
 */
UCLASS(ClassGroup = (DontMelt), meta = (BlueprintSpawnableComponent))
class DONTMELT_API UDMCombatComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDMCombatComponent();

#pragma region Cooldown Query

    bool IsAttackCooldownReady() const;

    bool IsDefenseCooldownReady() const;

    float GetRemainingAttackCooldown() const;

    float GetRemainingDefenseCooldown() const;

    /** 설정된 공격 쿨다운의 전체 길이를 조회한다. */
    float GetAttackCooldownDuration() const;

    /** 설정된 방어 쿨다운의 전체 길이를 조회한다. */
    float GetDefenseCooldownDuration() const;

    double GetAttackCooldownEndServerTime() const
    {
        return AttackCooldownEndServerTime;
    }

    double GetDefenseCooldownEndServerTime() const
    {
        return DefenseCooldownEndServerTime;
    }

    /** UI 등에서 기존 쿨다운 변경 알림을 구독한다. */
    FDMOnCombatCooldownChanged& OnCombatCooldownChanged()
    {
        return CombatCooldownChangedDelegate;
    }

#pragma endregion

#pragma region Respawn Protection Query

    /**
     * Combat Rule Data에 정의된
     * Spawn Protection 지속시간을 반환한다.
     *
     * Rule Data가 없으면 0을 반환한다.
     */
    float GetSpawnProtectionDuration() const;

#pragma endregion

#pragma region Attack API

    /**
     * Local Player의 공격 입력 진입점.
     *
     * Client는 공격 의도만 서버에 전달한다.
     * 대상이나 HitResult를 전달하지 않는다.
     */
    void RequestAttack();

#pragma endregion

#pragma region Defense API

    /**
     * Local Player의 방어 입력 진입점.
     *
     * Client는 방어 의도만 서버에 전달한다.
     */
    void RequestDefense();

#pragma endregion

#pragma region Attack Notify

    /**
     * Attack Montage의 Hit Notify가 호출한다.
     *
     * 서버에서만 실제 처리된다.
     */
    void HandleAttackHitNotify();

#pragma endregion

#pragma region Death API

    /**
     * Owner Player의 Death가 Commit된 뒤
     * 진행 중 Combat Runtime과 표현을 정리한다.
     *
     * Cooldown 자체는 초기화하지 않는다.
     */
    void HandleOwnerDeath();

#pragma endregion

protected:
#pragma region Lifecycle

    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

private:
#pragma region Configuration

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|Combat|Rules", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UDMCombatRuleDataAsset> CombatRuleData = nullptr;

#pragma endregion

#pragma region Runtime State

    /**
     * 공격 쿨다운이 종료되는 서버 시간.
     *
     * 0.0이면 쿨다운이 없는 상태다.
     */
    UPROPERTY(
        ReplicatedUsing = OnRep_AttackCooldownEndServerTime,
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "DM|Combat|Cooldown",
        meta = (AllowPrivateAccess = "true")
    )
    double AttackCooldownEndServerTime = 0.0;

    UPROPERTY(
        ReplicatedUsing = OnRep_DefenseCooldownEndServerTime,
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "DM|Combat|Cooldown",
        meta = (AllowPrivateAccess = "true")
    )
    double DefenseCooldownEndServerTime = 0.0;

    /** 복제된 쿨다운 값의 변경을 로컬 구독자에게 알린다. */
    FDMOnCombatCooldownChanged CombatCooldownChangedDelegate;

    /**
     * 서버에서 승인된 Attack의 순번.
     *
     * 0은 아직 공격 없음.
     */
    UPROPERTY(VisibleInstanceOnly, Category = "DM|Combat|Attack")
    uint32 AttackSequence = 0;

    /**
     * 현재까지 Hit Notify를 처리한
     * 마지막 Attack Sequence.
     *
     * 동일 Attack에서 Notify가 중복 실행되어도
     * 판정은 한 번만 진행한다.
     */
    uint32 LastResolvedAttackHitSequence = 0;

    /**
     * 현재 Defense 사용에서
     * 아직 막을 수 있는 공격 횟수.
     *
     * 서버 전용 Runtime 값.
     *
     * Defense가 아니면 0.
     */
    int32 RemainingDefenseBlocks = 0;

    /**
     * Defense Action의 서버 지속시간 Timer.
     */
    FTimerHandle DefenseDurationTimerHandle;

    /**
     * 소유 클라이언트가 마지막으로 발급한 공격 예측 번호.
     *
     * 0은 "예측 없음"이다. 소유 클라이언트에서만 사용한다.
     */
    uint32 LastPredictedAttackId = 0;

    /**
     * 서버 승인 전에 로컬에서 먼저 재생한 공격의 예측 번호.
     *
     * 승인된 Multicast가 도착하면 같은 공격을 다시 재생하지 않도록 비운다.
     */
    uint32 PendingPredictedAttackId = 0;

    /**
     * 서버 쿨다운이 복제되기 전의 연속 입력을 막는 로컬 예측 쿨다운 종료 시각.
     *
     * 서버 값이 복제되거나 예측이 거절되면 0으로 되돌린다.
     */
    double PredictedAttackCooldownEndServerTime = 0.0;

    /**
     * 소유 클라이언트의 마지막 공격 요청이 예측 재생되었는지 여부.
     *
     * 예측한 공격은 이미 재생했으므로 ActionState 안전망이 다시 재생하지 않는다.
     */
    bool bLastAttackRequestPredicted = false;

    /**
     * ActionState 안전망이 시작 RPC보다 먼저 재생한 연출.
     *
     * 뒤늦게 도착한 시작 RPC는 이 경우에만 재생 위치만 맞추고,
     * 그 외에는 이전 공격이 재생 중이어도 새로 재생한다.
     */
    bool bAttackStartedFromReplicatedState = false;

    bool bDefenseStartedFromReplicatedState = false;

#pragma endregion

#pragma region Attack Server Request

    /**
     * PredictionId가 0이 아니면 클라이언트가 공격 연출을 먼저 재생한 요청이다.
     * 서버가 거절하면 같은 번호로 ClientRejectPredictedAttack을 보낸다.
     */
    UFUNCTION(Server, Reliable)
    void ServerRequestAttack(uint32 PredictionId);

    bool CanRequestAttack(FString& OutFailureReason) const;

#pragma endregion

#pragma region Attack Prediction

    /**
     * 소유 클라이언트에서 공격 몽타주와 예측 쿨다운을 먼저 적용한다.
     *
     * 판정은 하지 않는다. Hit Notify는 서버에서만 처리된다.
     */
    bool TryStartPredictedAttack(uint32& OutPredictionId);

    /** 서버가 예측한 공격을 거절했을 때 로컬 연출과 예측 쿨다운을 되돌린다. */
    UFUNCTION(Client, Reliable)
    void ClientRejectPredictedAttack(uint32 PredictionId);

    /** 예측 재생한 공격 몽타주를 짧게 블렌드 아웃한다. */
    void StopPredictedAttackPresentation();

#pragma endregion

#pragma region Cooldown

    bool ConsumeAttackCooldown();

    bool ConsumeDefenseCooldown();

    UFUNCTION()
    void OnRep_AttackCooldownEndServerTime();

    UFUNCTION()
    void OnRep_DefenseCooldownEndServerTime();

#pragma endregion

#pragma region Attack Execution

    bool BeginAttackAction(uint32 PredictionId);

    bool PlayAttackMontageOnServer();

    void HandleAttackMontageEnded(UAnimMontage* Montage, bool bInterrupted);

    /**
     * 공격 시작 연출. 일시적인 연출이므로 Unreliable로 보낸다.
     *
     * 유실되어도 복제된 ActionState(OnRep)로 연출을 복구한다.
     */
    UFUNCTION(NetMulticast, Unreliable)
    void MulticastPresentAttack(uint32 Sequence, double AttackStartServerTime, uint32 PredictionId);

#pragma endregion

#pragma region Attack Targeting

    void GatherValidAttackTargets(TArray<ADMJellyCharacterBase*>& OutTargets) const;

    bool IsInsideAttackGeometry(const ADMJellyCharacterBase* Target, float AttackRange) const;

    bool IsEligibleAttackTarget(const ADMJellyCharacterBase* Target) const;

    bool
    IsWorldGeometryBlockingTarget(const ADMJellyCharacterBase* Target, const TArray<AActor*>& IgnoredCharacters) const;

#pragma endregion

#pragma region Defense Server Request

    UFUNCTION(Server, Reliable)
    void ServerRequestDefense();

    bool CanRequestDefense(FString& OutFailureReason) const;

#pragma endregion

#pragma region Defense Execution

    bool BeginDefenseAction();

    void EndDefenseAction();

    void HandleDefenseDurationExpired();

    bool PlayDefenseMontageOnServer(float DefenseDuration);

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastPresentDefense(float DefenseDuration, double DefenseStartServerTime);

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastStopDefensePresentation();

#pragma endregion

#pragma region Death Presentation Cleanup

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastStopCombatPresentation();

#pragma endregion

#pragma region Replicated State Presentation

    /**
     * Unreliable 연출 RPC가 유실된 경우를 위한 안전망.
     *
     * 복제된 ActionState 변화를 보고, 재생되지 않은 공격·방어 연출을 시작하거나
     * 남아 있는 방어 연출을 정리한다. 서버에서는 호출되지 않는다.
     */
    void HandleReplicatedActionStateChanged(EDMActionState PreviousActionState, EDMActionState NewActionState);

#pragma endregion

#pragma region Player Hit Resolution

    bool ShouldApplyFatalHitToPlayer(ADMPlayerCharacter* TargetPlayer) const;

    bool TryConsumeDefenseBlockAgainst(const ADMPlayerCharacter* Attacker);

    bool IsAttackerInsideDefenseArc(const ADMPlayerCharacter* Attacker) const;

#pragma endregion

#pragma region Wrong Target Penalty

    bool ApplyWrongTargetPenalty();

    void RequestNPCWrongTargetReaction(const TArray<ADMNPCCharacter*>& HitNPCTargets) const;

#pragma endregion
};