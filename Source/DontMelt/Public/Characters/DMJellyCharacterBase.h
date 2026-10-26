#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Character.h"
#include "DMJellyCharacterBase.generated.h"

class UDMCharacterStateComponent;
class UDMCharacterAudioComponent;
class UDMJellyPhysicsComponent;
class UAnimMontage;
class UDMCharacterVFXComponent;

 /**
 * Player와 NPC가 공유하는 Jelly Character 기반 클래스.
 *
 * 공통 CharacterMovement 기반과 공통 Component를 소유하며,
 * Player/NPC에서 공유하는 최소 Gameplay API를 제공한다.
 */
UCLASS(Abstract)
class DONTMELT_API ADMJellyCharacterBase : public ACharacter
{
    GENERATED_BODY()

public:
    ADMJellyCharacterBase(const FObjectInitializer& ObjectInitializer);

#pragma region Movement API

    /**
     * 현재 Sprint 요청을 해제한다.
     *
     * MaxWalkSpeed 값을 직접 변경하지 않고
     * CharacterMovement의 Sprint 의도만 제거한다.
     */
    void ClearSprintRequest();

    float GetWalkSpeed() const;

    float GetSprintSpeed() const;

    /**
     * 현재 Character가 Sprint 가능한지 반환한다.
     *
     * Base/NPC는 기본적으로 허용하며,
     * Player는 Pose 등의 Gameplay 조건을 추가 검사한다.
     */
    virtual bool CanSprint() const;

#pragma endregion

#pragma region Emote API

    /**
     * Player와 NPC가 공유하는 Emote 요청 진입점.
     */
    UFUNCTION(BlueprintCallable, Category = "DontMelt|Emote")
    void RequestEmote(FName EmoteId);

    /**
     * 지속형 감정표현 시작 요청.
     * 서버에서 ActionState=Emote로 전환되며 QuestEvent.Emote.Start가 발생한다.
     * (RequestEmote는 일회성 표현만 재생하고 퀘스트 이벤트를 만들지 않는다.)
     */
    UFUNCTION(BlueprintCallable, Category = "DontMelt|Emote")
    void RequestBeginEmote(FName EmoteId);

    /**
     * 지속형 감정표현 종료 요청.
     * 감정표현 중이 아니면 서버에서 무시된다.
     */
    UFUNCTION(BlueprintCallable, Category = "DontMelt|Emote")
    void RequestEndEmote();

#pragma endregion

#pragma region Component Access

    UDMCharacterStateComponent* GetCharacterStateComponent() const
    {
        return CharacterStateComponent;
    }

    UDMCharacterAudioComponent* GetCharacterAudioComponent() const
    {
        return CharacterAudioComponent;
    }

    UDMCharacterVFXComponent* GetCharacterVFXComponent() const
    {
        return CharacterVFXComponent;
    }

    UDMJellyPhysicsComponent* GetJellyPhysicsComponent() const;

#pragma endregion

#pragma region Combat Animation

    bool CanPlayAttackMontage() const;

    float PlayAttackMontage();

    void StopAttackMontage();

    UAnimMontage* GetAttackMontage() const
    {
        return AttackMontage;
    }

#pragma endregion

protected:
#pragma region Lifecycle

    virtual void PostInitializeComponents() override;

#pragma endregion

#pragma region Movement Internal

    /**
     * CharacterMovement에 Sprint 이동 의도를 전달한다.
     *
     * 실제 네트워크 전달은 CharacterMovement SavedMove가 담당한다.
     */
    void SetSprintRequested(bool bEnableSprint);

#pragma endregion

private:
#pragma region Components

    UPROPERTY(
        VisibleAnywhere, BlueprintReadOnly, Category = "DontMelt|Components", meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UDMCharacterStateComponent> CharacterStateComponent;

    UPROPERTY(
        VisibleAnywhere, BlueprintReadOnly, Category = "DontMelt|Components", meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UDMCharacterAudioComponent> CharacterAudioComponent;

    UPROPERTY(
        VisibleAnywhere, BlueprintReadOnly, Category = "DontMelt|Components", meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UDMJellyPhysicsComponent> JellyPhysicsComponent;

    UPROPERTY(
        VisibleAnywhere, BlueprintReadOnly, Category = "DontMelt|Components", meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UDMCharacterVFXComponent> CharacterVFXComponent;

#pragma endregion

#pragma region Movement Configuration

    /**
     * 기본 지상 이동 최대 속도.
     *
     * 밸런스 설정값이며 현재 이동 상태를 의미하지 않는다.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "DontMelt|Movement",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0")
    )
    float WalkSpeed = 200.0f;

    /**
     * Sprint 요청이 유효할 때 사용할 최대 이동 속도.
     *
     * 밸런스 설정값이며 현재 Sprint 상태를 의미하지 않는다.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "DontMelt|Movement",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0")
    )
    float SprintSpeed = 500.0f;

#pragma endregion

#pragma region Combat Animation Configuration

    /**
     * Player/NPC가 공유할 기본 공격 Montage.
     *
     * Gameplay 판정 원본이 아니라
     * Attack Action의 표현 Asset이다.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "DontMelt|Animation|Combat",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UAnimMontage> AttackMontage = nullptr;

#pragma endregion
};