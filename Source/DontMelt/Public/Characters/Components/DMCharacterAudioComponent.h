#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "DMCharacterAudioComponent.generated.h"

class USoundBase;
class USoundAttenuation;

/**
 * Jelly Character 공통 Audio Presentation Component.
 *
 * Gameplay 상태를 소유하지 않는다.
 *
 * Footstep / Combat Notify 및 기존 상태·사망 연출 이벤트를 받아
 * 각 실행 환경에서 3D 사운드를 재생한다.
 */
UCLASS(ClassGroup = (DontMelt), meta = (BlueprintSpawnableComponent))
class DONTMELT_API UDMCharacterAudioComponent : public UActorComponent
{
    GENERATED_BODY()

public:
#pragma region Lifecycle

    UDMCharacterAudioComponent();

#pragma endregion

#pragma region Footstep

    /**
     * Footstep AnimNotify 진입점.
     *
     * Server Gameplay Event가 아니라
     * 각 Client의 Animation Presentation Event다.
     */
    void HandleFootstepNotify();

    /**
     * Invisibility Presentation 상태를
     * Audio Presentation에 반영한다.
     *
     * Gameplay SSOT가 아니며 Replicate하지 않는다.
     */
    void SetInvisibilityPresentationActive(bool bActive)
    {
        bInvisibilityPresentationActive = bActive;
    }

    /**
     * 현재 Local Listener가 이 Character의
     * FootstepAmplify 효과를 듣고 있는지 설정한다.
     *
     * Local Presentation 상태이며 Replicate하지 않는다.
     */
    void SetFootstepAmplifyListenerPresentationActive(bool bActive)
    {
        bFootstepAmplifyListenerPresentationActive = bActive;
    }

    float GetResolvedFootstepVolumeMultiplier() const;

#pragma endregion

#pragma region Combat Audio

    /** 공격 몽타주 Notify에서 공격 동작음을 재생한다. */
    void PlayAttackSound();

    /** 방어 몽타주 Notify에서 방어 동작음을 재생한다. */
    void PlayDefenseSound();

#pragma endregion

#pragma region Reaction Audio

    /** 실제 스턴 상태가 새로 시작됐을 때만 호출한다. */
    void PlayStunSound();

#pragma endregion

protected:
#pragma region Lifecycle Overrides

    virtual void BeginPlay() override;
    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

#pragma endregion

private:
#pragma region Footstep Internal

    bool CanPlayFootstepLocally() const;

#pragma endregion

#pragma region Combat Audio Internal

    /** 공격·방어 사운드의 공통 위치 기반 재생 처리. */
    void PlayCombatSound(USoundBase* Sound) const;

#pragma endregion

#pragma region Reaction Audio Internal

    /** 기존 Emote Multicast에서 Scream 요청을 받은 로컬 실행 지점. */
    UFUNCTION()
    void HandleEmotePresentationRequested(FName EmoteId);

    /** 실제 사망 래그돌 시작 이벤트를 받은 로컬 실행 지점. */
    void HandleDeathPresentationStarted();

    /** 상태·사망·비명 공통 3D 사운드 재생 처리. */
    void PlayReactionSound(USoundBase* Sound) const;

#pragma endregion

#pragma region Footstep Configuration

    /**
     * SoundWave / SoundCue / MetaSound Source 등
     * USoundBase 파생 Asset을 연결한다.
     */
    UPROPERTY(
        EditDefaultsOnly, BlueprintReadOnly, Category = "DontMelt|Audio|Footstep", meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<USoundBase> FootstepSound = nullptr;

    /**
     * 평상시 Footstep 기준 Volume.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "DontMelt|Audio|Footstep",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0")
    )
    float BaseFootstepVolumeMultiplier = 1.0f;

    /**
     * Status.Invisible 활성 상태의
     * Footstep Presentation 배율.
     *
     * 현재 확정 기획값 = 1.5배.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "DontMelt|Audio|Footstep",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0")
    )
    float InvisibilityFootstepVolumeMultiplier = 1.5f;

    /**
     * FootstepAmplify를 사용한 Listener에게만
     * 적용되는 Footstep Volume 배율.
     *
     * 현재 확정 기획값 = 2.0.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "DontMelt|Audio|Footstep",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0")
    )
    float FootstepAmplifyListenerVolumeMultiplier = 2.0f;

    /**
     * Footstep용 3D Attenuation.
     *
     * nullptr이면 Sound Asset 자체의
     * Attenuation 설정을 사용한다.
     */
    UPROPERTY(
        EditDefaultsOnly, BlueprintReadOnly, Category = "DontMelt|Audio|Footstep", meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<USoundAttenuation> FootstepAttenuation = nullptr;

#pragma endregion

#pragma region Combat Audio Configuration

    /** 공격 동작 시 재생할 3D 사운드 에셋. */
    UPROPERTY(EditDefaultsOnly, Category = "DontMelt|Audio|Combat")
    TObjectPtr<USoundBase> AttackSound = nullptr;

    /** 방어 동작 시 재생할 3D 사운드 에셋. */
    UPROPERTY(EditDefaultsOnly, Category = "DontMelt|Audio|Combat")
    TObjectPtr<USoundBase> DefenseSound = nullptr;

    /** 공격·방어 공용 거리 감쇠 설정. 미설정 시 Sound Asset 설정을 사용한다. */
    UPROPERTY(EditDefaultsOnly, Category = "DontMelt|Audio|Combat")
    TObjectPtr<USoundAttenuation> CombatAttenuation = nullptr;

#pragma endregion

#pragma region Reaction Audio Configuration

    /** NPC의 Scream Emote가 실제 전달됐을 때 재생한다. */
    UPROPERTY(EditDefaultsOnly, Category = "DontMelt|Audio|Reaction")
    TObjectPtr<USoundBase> NPCScreamSound = nullptr;

    /** 플레이어 사망을 목격한 NPC의 Laugh Emote에서 재생한다. */
    UPROPERTY(EditDefaultsOnly, Category = "DontMelt|Audio|Reaction")
    TObjectPtr<USoundBase> NPCMockLaughSound = nullptr;

    /** 플레이어 스턴 시작 시 한 번 재생한다. */
    UPROPERTY(EditDefaultsOnly, Category = "DontMelt|Audio|Reaction")
    TObjectPtr<USoundBase> StunSound = nullptr;

    /** 실제 플레이어 사망 연출 시작 시 한 번 재생한다. */
    UPROPERTY(EditDefaultsOnly, Category = "DontMelt|Audio|Reaction")
    TObjectPtr<USoundBase> DeathSound = nullptr;

    /** 미설정 시 기존 CombatAttenuation을 재사용한다. */
    UPROPERTY(EditDefaultsOnly, Category = "DontMelt|Audio|Reaction")
    TObjectPtr<USoundAttenuation> ReactionAttenuation = nullptr;

#pragma endregion

#pragma region Presentation State

    /**
     * Local Presentation projection.
     *
     * Gameplay SSOT가 아니며
     * Network Replication 대상도 아니다.
     */
    bool bInvisibilityPresentationActive = false;

    /**
     * 이 Character의 발소리가 현재 Local Listener에게만
     * 증폭되어야 하는지 나타내는 Presentation projection.
     *
     * Gameplay SSOT가 아니며 Replicate하지 않는다.
     */
    bool bFootstepAmplifyListenerPresentationActive = false;

#pragma endregion
};