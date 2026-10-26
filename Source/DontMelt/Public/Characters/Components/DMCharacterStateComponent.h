#pragma once

#include "CoreMinimal.h"
#include "Characters/DMCharacterTypes.h"
#include "Components/ActorComponent.h"
#include "GameplayTagContainer.h"
#include "DMCharacterStateComponent.generated.h"

class UMaterialInterface;

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDMEmotePresentationRequested, FName, EmoteId);

DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FDMEmotePresentationEnded, FName, EmoteId);

DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnAuthoritativeEmoteStateChanged, bool, FName);

/** 소유 클라이언트의 상태이상 표시값이 변경되었음을 알린다. */
DECLARE_MULTICAST_DELEGATE(FDMOnCharacterStatusChanged);

/** 클라이언트에서 복제된 ActionState가 바뀌었음을 알린다. (이전 값, 새 값) */
DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnReplicatedActionStateChanged, EDMActionState, EDMActionState);

/**
 * 투명화 HUD에 필요한 시간 정보.
 * Gameplay 상태의 원본이 아니며 소유 클라이언트의 표시용 데이터다.
 */
USTRUCT()
struct FDMInvisibleTiming
{
    GENERATED_BODY()

    UPROPERTY()
    double StartServerTime = 0.0;

    UPROPERTY()
    double EndServerTime = 0.0;
};

/**
 * Player와 NPC가 공유하는 주요 행동 상태의 SSOT.
 *
 * Interaction / Combat / Skill 등이 같은 ActionState를 공유한다.
 */
UCLASS(ClassGroup = (DontMelt), meta = (BlueprintSpawnableComponent))
class DONTMELT_API UDMCharacterStateComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDMCharacterStateComponent();

#pragma region Action State

    UFUNCTION(BlueprintPure, Category = "DM|Character|Action")
    EDMActionState GetActionState() const
    {
        return ActionState;
    }

    UFUNCTION(BlueprintPure, Category = "DM|Character|Action")
    bool IsBusy() const
    {
        return ActionState != EDMActionState::None;
    }

    /**
     * 현재 ActionState 기준으로
     * 요청 행동을 수행할 수 있는지 검사한다.
     *
     * 다른 주요 Action이 진행 중이면
     * 새 Gameplay Action을 허용하지 않는다.
     */
    bool CanPerformAction(EDMActionRequest ActionRequest) const;

    /**
     * 서버에서만 호출한다.
     */
    bool TryBeginAction(EDMActionState NewActionState);

    /**
     * 서버에서만 호출한다.
     */
    void EndAction(EDMActionState ExpectedActionState);

#pragma endregion

#pragma region Status

    bool HasStatus(const FGameplayTag& StatusTag) const;

    /** 투명화 효과의 서버 기준 시작·종료 시각을 반환한다. */
    FDMInvisibleTiming GetInvisibleTiming() const
    {
        return InvisibleTiming;
    }

    /**
     * 모든 관찰자에게 전달되는 스턴 표현 상태를 반환한다.
     *
     * 실제 Gameplay 검증에는 사용하지 않고,
     * 애니메이션 등 시각 표현에만 사용한다.
     */
    bool IsStunPresentationActive() const
    {
        return bStunPresentationActive;
    }

    /** 현재 상태이상 변경을 UI에서 구독한다. */
    FDMOnCharacterStatusChanged& OnStatusChanged()
    {
        return StatusChangedDelegate;
    }

    /**
     * 서버 권한으로 시간 제한 Status를 적용한다.
     *
     * 이미 활성화된 동일 Status는 중첩하지 않는다.
     */
    bool ApplyTimedStatus(const FGameplayTag& StatusTag, float Duration);

    /**
     * 서버 권한으로 Status를 제거한다.
     */
    bool RemoveStatus(const FGameplayTag& StatusTag);

#pragma endregion

#pragma region Emote

    UFUNCTION(Server, Reliable)
    void RequestEmote(FName EmoteId);

    /**
     * Starts a persistent player emote action.
     *
     * Unlike RequestEmote(), this owns EDMActionState::Emote
     * until RequestEndEmote() or another authoritative cleanup.
     */
    UFUNCTION(Server, Reliable)
    void RequestBeginEmote(FName EmoteId);

    UFUNCTION(Server, Reliable)
    void RequestEndEmote();

    FDMOnAuthoritativeEmoteStateChanged& OnAuthoritativeEmoteStateChanged()
    {
        return AuthoritativeEmoteStateChangedDelegate;
    }

    /** 클라이언트에서 ActionState 복제를 받을 때 발생한다. 서버에서는 발생하지 않는다. */
    FDMOnReplicatedActionStateChanged& OnReplicatedActionStateChanged()
    {
        return ReplicatedActionStateChangedDelegate;
    }

    UPROPERTY(BlueprintAssignable, Category = "DontMelt|Emote")
    FDMEmotePresentationRequested OnEmotePresentationRequested;

    UPROPERTY(BlueprintAssignable, Category = "DontMelt|Emote")
    FDMEmotePresentationEnded OnEmotePresentationEnded;

#pragma endregion

protected:
#pragma region Lifecycle

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#pragma endregion

private:
#pragma region Emote Internal

    bool CanRequestEmote(FName EmoteId) const;

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastPresentEmote(FName EmoteId);

    UFUNCTION(NetMulticast, Unreliable)
    void MulticastEndEmote(FName EmoteId);

    /**
     * 복제된 ActionState 변경 처리.
     *
     * 이모트 종료 RPC가 유실되어도 Emote 상태가 끝나면 연출을 정리하고,
     * 다른 컴포넌트(공격·방어 연출)에 변경을 전달한다.
     */
    UFUNCTION()
    void OnRep_ActionState(EDMActionState PreviousActionState);

#pragma endregion

#pragma region Status Internal

    void HandleStatusExpired(FGameplayTag StatusTag);

    /** 소유 클라이언트가 상태이상 복제를 받으면 UI에 알린다. */
    UFUNCTION()
    void OnRep_ActiveStatusTags();

    /**
     * Gameplay Status 변경을 Presentation에 전달한다.
     *
     * Status 원본을 새로 저장하지 않는다.
     */
    void HandleStatusPresentationChanged(const FGameplayTag& StatusTag);

    /** 스턴 표현값을 받은 클라이언트에서 로컬 Niagara를 갱신한다. */
    UFUNCTION()
    void OnRep_StunPresentationActive(bool bPreviousActive);

    /** 현재 복제된 스턴 표현값을 VFXComponent에 적용한다. */
    void ApplyStunPresentation();

    /**
     * 현재 클라이언트의 소유권에 따라 투명화 머티리얼을 적용한다.
     *
     * 투명화 해제 시 원본 머티리얼을 복원하며,
     * 기존 발소리 표현 상태도 함께 갱신한다.
     */
    void ApplyInvisibilityPresentation(bool bInvisible);

    /** 투명화 시간 정보 복제 시 HUD 갱신을 요청한다. */
    UFUNCTION()
    void OnRep_InvisibleTiming();

    /** 늦게 관련성에 들어온 Client도 현재 투명화 연출을 적용한다. */
    UFUNCTION()
    void OnRep_InvisibilityPresentationActive();

#pragma endregion

#pragma region Validation

    bool IsLifeStateAllowed() const;

    bool IsStatusAllowedForAction(EDMActionRequest ActionRequest) const;

#pragma endregion

#pragma region Runtime State

    /**
     * 현재 Character의 주요 상호 배타 Action.
     */
    UPROPERTY(
        ReplicatedUsing = OnRep_ActionState,
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "DM|Character|Action",
        meta = (AllowPrivateAccess = "true")
    )
    EDMActionState ActionState = EDMActionState::None;

    /**
     * 클라이언트에서 마지막으로 시작한 이모트 연출.
     *
     * 종료 RPC가 유실되었을 때 어떤 이모트를 끝낼지 알기 위한 로컬 표현 상태다.
     */
    FName LastPresentedEmoteId = NAME_None;

    /**
     * Server-only context for a persistent Emote action.
     *
     * ActionState says whether an Emote action is active.
     * This value identifies which Emote must be ended.
     */
    FName ActiveEmoteId = NAME_None;

    /**
     * 현재 Character에 적용된 동시 Status의 SSOT.
     *
     * LifeState / ActionState를 이 Container에 중복 저장하지 않는다.
     */

    UPROPERTY(
        ReplicatedUsing = OnRep_ActiveStatusTags,
        VisibleInstanceOnly,
        BlueprintReadOnly,
        Category = "DM|Character|Status",
        meta = (AllowPrivateAccess = "true")
    )
    FGameplayTagContainer ActiveStatusTags;

    /**
     * 투명화 남은 시간 표시를 위한 OwnerOnly 복제 데이터.
     * 효과 유효 여부는 ActiveStatusTags가 결정한다.
     */
    UPROPERTY(ReplicatedUsing = OnRep_InvisibleTiming, Transient)
    FDMInvisibleTiming InvisibleTiming;

    /**
     * Status.Stunned의 관찰자용 표현값.
     *
     * 서버의 ActiveStatusTags에서 파생하며,
     * Gameplay 판정에 사용하지 않는다.
     *
     * Player/NPC의 관련된 모든 Client에게 복제한다.
     */

    UPROPERTY(
        ReplicatedUsing = OnRep_StunPresentationActive,
        Transient,
        VisibleInstanceOnly,
        Category = "DM|Character|Presentation"
    )
    bool bStunPresentationActive = false;

    /**
     * Status.Invisible의 관찰자용 표현값.
     *
     * 투명화 시작 순간 관련성 밖에 있던 Client나
     * 관련성에 다시 들어와 캐릭터가 새로 생성된 Client도
     * 이 값으로 같은 연출을 복구한다. Gameplay 판정에 사용하지 않는다.
     */
    UPROPERTY(
        ReplicatedUsing = OnRep_InvisibilityPresentationActive,
        Transient,
        VisibleInstanceOnly,
        Category = "DM|Character|Presentation"
    )
    bool bInvisibilityPresentationActive = false;

    /**
     * 서버 전용 Status 수명 관리.
     *
     * 실제 Status 원본이 아니라
     * ActiveStatusTags 제거 시점만 관리한다.
     */
    TMap<FGameplayTag, FTimerHandle> StatusExpiryTimers;

#pragma endregion

#pragma region Invisibility Material

    /**
     * 스킬 사용자의 로컬 화면에 적용할 머티리얼.
     * Gameplay 상태나 네트워크 복제 대상이 아니다.
     */
    UPROPERTY(EditDefaultsOnly, Category = "DM|Character|Presentation|Invisibility")
    TObjectPtr<UMaterialInterface> OwnerInvisibilityMaterial = nullptr;

    /**
     * 다른 플레이어 화면에 적용할 투명화 머티리얼.
     */
    UPROPERTY(EditDefaultsOnly, Category = "DM|Character|Presentation|Invisibility")
    TObjectPtr<UMaterialInterface> ObserverInvisibilityMaterial = nullptr;

    /**
     * 투명화 직전에 적용되어 있던 머티리얼.
     * 각 클라이언트에만 보관하고 복제하지 않는다.
     */
    UPROPERTY(Transient)
    TArray<TObjectPtr<UMaterialInterface>> OriginalMeshMaterials;

#pragma endregion

#pragma region Delegates

    FDMOnAuthoritativeEmoteStateChanged AuthoritativeEmoteStateChangedDelegate;

    /** 상태이상의 실제 소유자는 ActiveStatusTags이다. */
    FDMOnCharacterStatusChanged StatusChangedDelegate;

    FDMOnReplicatedActionStateChanged ReplicatedActionStateChangedDelegate;

#pragma endregion
};