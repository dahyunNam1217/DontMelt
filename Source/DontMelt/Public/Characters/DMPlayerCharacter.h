#pragma once

#include "CoreMinimal.h"
#include "Characters/DMJellyCharacterBase.h"
#include "Characters/DMCharacterTypes.h"
#include "GameplayTagContainer.h"
#include "TimerManager.h"
#include "UObject/SoftObjectPath.h"
#include "DMPlayerCharacter.generated.h"

class UCameraComponent;
class USpringArmComponent;

class UDMCombatComponent;
class UDMInteractionComponent;
class UDMPoseComponent;

class UDMInputConfigDataAsset;
class UEnhancedInputLocalPlayerSubsystem;
class UEnhancedInputComponent;

class UAnimMontage;
class UAudioComponent;
class USoundBase;
class USoundAttenuation;

struct FInputActionValue;
struct FDMQuestActionEvent;

DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnQuestActionEvent, const FDMQuestActionEvent&);

/** 로컬 Pose 입력 모드의 활성화·비활성화를 알린다. */
DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnLocalPoseInputModeChanged, bool);

/**
 * 실제 플레이어가 조종하는 Jelly Character.
 *
 * 플레이어 입력, 카메라, 로컬 Input Context 전환을 담당하고
 * Gameplay 기능은 각 전용 Component에 전달한다.
 */
UCLASS()
class DONTMELT_API ADMPlayerCharacter : public ADMJellyCharacterBase
{
    GENERATED_BODY()

public:
    ADMPlayerCharacter(const FObjectInitializer& ObjectInitializer);

#pragma region Quest Event Bridge Public

    FDMOnQuestActionEvent& OnQuestActionEvent()
    {
        return QuestActionEventDelegate;
    }

#pragma endregion

#pragma region Quest Failure Presentation Public

    /**
     * 퀘스트 미수행 사이렌의 공개 상태를 변경한다.
     *
     * Server only.
     * 실제 소리는 Listen Server Host는 즉시, Client는 OnRep에서 재생한다.
     */
    void SetQuestSirenActive(bool bActive);

    bool IsQuestSirenActive() const
    {
        return bQuestSirenActive;
    }

    virtual void GetLifetimeReplicatedProps(TArray<FLifetimeProperty>& OutLifetimeProps) const override;

#pragma endregion

#pragma region Movement Query

    virtual bool CanSprint() const override;

#pragma endregion

#pragma region Camera Policy

    /**
     * Camera View와 Pose Lock 상태를 함께 고려해
     * Character 회전 정책을 다시 적용한다.
     *
     * PoseComponent에서 Pose Lock 상태 변경 후 호출한다.
     */
    void RefreshRotationPolicy();

#pragma endregion

#pragma region Input Configuration Query

    /**
     * 현재 Character가 사용하는 공통 Input Config.
     *
     * Spectator 입력에서도 동일 IA_Move / IA_Look을
     * 재사용하기 위해 MatchPlayerController가 읽는다.
     *
     * Config 소유권은 여전히 Character 설정에 있다.
     */
    UDMInputConfigDataAsset* GetInputConfigDataAsset() const
    {
        return InputConfig;
    }

#pragma endregion

#pragma region Pose UI Query

    /** 현재 로컬 Enhanced Input이 자세 모드인지 조회한다. */
    bool IsPoseInputModeActive() const
    {
        return bIsPoseInputMode;
    }

    /** 로컬 자세 입력 모드 변경 알림 */
    FDMOnLocalPoseInputModeChanged& OnLocalPoseInputModeChanged()
    {
        return LocalPoseInputModeChangedDelegate;
    }

#pragma endregion

#pragma region Spectator Input Lifecycle

    /**
     * Spectator Sequence 시작 시
     * Local Gameplay 계열 Input Context를 전부 제거한다.
     *
     * Server Gameplay 상태를 변경하지 않는다.
     * IMC_Spectator도 여기서는 추가하지 않는다.
     */
    void PrepareLocalInputForSpectating();

#pragma endregion

#pragma region Interaction Input Mode

    void EnterChestInteractionInputMode();

    void ExitChestInteractionInputMode();

#pragma endregion

#pragma region Skill Targeting Input Mode

    bool EnterSkillTargetingInputMode();

    void ExitSkillTargetingInputMode();

#pragma endregion

#pragma region Defense Animation

    bool CanPlayDefenseMontage() const;

    float PlayDefenseMontage(float DesiredDuration);

    void StopDefenseMontage();

    UAnimMontage* GetDefenseMontage() const
    {
        return DefenseMontage;
    }

#pragma endregion

protected:
#pragma region Lifecycle

    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    virtual void OnJumped_Implementation() override;

    virtual void PawnClientRestart() override;

    virtual void SetupPlayerInputComponent(UInputComponent* PlayerInputComponent) override;

#pragma endregion

private:
#pragma region Movement Input

    void InputMove(const FInputActionValue& InputActionValue);

    void InputJumpStarted();

    void InputJumpCompleted();

    void InputSprintStarted();

    void InputSprintCompleted();

#pragma endregion

#pragma region Quest Event Bridge Internal

    void HandleAuthoritativeSprintStateChanged(bool bIsSprinting);

    void HandlePoseTypeChanged(EDMPoseType OldPose, EDMPoseType NewPose);

    void HandleAuthoritativeEmoteStateChanged(bool bStarted, FName EmoteId);

    void BroadcastQuestActionEvent(
        const FGameplayTag& EventTag, EDMPoseType PoseType = EDMPoseType::Default, FName EmoteId = NAME_None
    );

#pragma endregion

#pragma region Quest Failure Presentation Internal

    UFUNCTION()
    void OnRep_QuestSirenActive();

    // 현재 bQuestSirenActive 값에 맞춰 로컬 AudioComponent를 재생하거나 정지한다.
    void RefreshQuestSirenPresentation();

#pragma endregion

#pragma region Combat Input

    void InputAttackStarted();

    void InputDefenseStarted();

#pragma endregion

#pragma region Interaction Input

    void InputInteractStarted();

    void InputInteractCompleted();

    void InputConfirmInteractionStarted();

    void InputConfirmInteractionCompleted();

    void InputCloseInteractionStarted();

    void InputDropRelicStarted();

#pragma endregion

#pragma region Camera

    void InputViewStarted();

    void InputLook(const FInputActionValue& InputActionValue);

    void ApplyLocalCameraView();

    void ApplyViewRotationPolicy();

    UFUNCTION(Server, Reliable)
    void ServerSetFirstPersonView(bool bEnableFirstPerson);

#pragma endregion

#pragma region Pose

    void InputPoseStarted();

    void InputExitPoseStarted();

    bool SetPoseInputMode(bool bEnablePoseMode);

#pragma endregion

#pragma region Emote Input

    // IA_Emote를 누르면 지속형 감정표현을 시작한다.
    void InputEmoteStarted();

    // IA_Emote를 떼거나 취소되면 감정표현을 종료한다.
    void InputEmoteCompleted();

#pragma endregion

#pragma region Input Binding

    void BindMovementInput(UEnhancedInputComponent* EnhancedInputComponent);

    void BindCombatInput(UEnhancedInputComponent* EnhancedInputComponent);

    void BindInteractionInput(UEnhancedInputComponent* EnhancedInputComponent);

    void BindPoseAndEmoteInput(UEnhancedInputComponent* EnhancedInputComponent);

    void BindSkillInput(UEnhancedInputComponent* EnhancedInputComponent);

#pragma endregion

#pragma region Internal Helpers

    UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const;

#pragma endregion

#pragma region Components

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DontMelt|Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<USpringArmComponent> CameraBoom;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "DontMelt|Camera", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UCameraComponent> FollowCamera;

    UPROPERTY(
        VisibleAnywhere, BlueprintReadOnly, Category = "DontMelt|Components", meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UDMCombatComponent> CombatComponent;

    UPROPERTY(
        VisibleAnywhere, BlueprintReadOnly, Category = "DontMelt|Components", meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UDMInteractionComponent> InteractionComponent;

    UPROPERTY(
        VisibleAnywhere, BlueprintReadOnly, Category = "DontMelt|Components", meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UDMPoseComponent> PoseComponent;

#pragma endregion

#pragma region Configuration

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DontMelt|Input", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UDMInputConfigDataAsset> InputConfig;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "DontMelt|Animation|Combat",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UAnimMontage> DefenseMontage = nullptr;

    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "DontMelt|Camera",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0")
    )
    float ThirdPersonArmLength = 300.0f;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DontMelt|Camera", meta = (AllowPrivateAccess = "true"))
    FVector ThirdPersonTargetOffset = FVector::ZeroVector;

    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DontMelt|Camera", meta = (AllowPrivateAccess = "true"))
    FVector ThirdPersonSocketOffset = FVector::ZeroVector;

    /**
     * IA_Emote 입력 시 사용할 감정표현 ID.
     * 감정표현 휠 도입 전 기본값이며, AnimBP Emote 연출에 존재하는 ID로 지정해야 한다.
     * (퀘스트 판정은 EmoteId를 구분하지 않는다.)
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DontMelt|Emote", meta = (AllowPrivateAccess = "true"))
    FName DefaultEmoteId = TEXT("Scream");

#pragma endregion

#pragma region Runtime State

    // 로컬 Input Context 상태이며 Gameplay Pose 상태의 원본이 아니다.
    bool bIsPoseInputMode = false;

    // 로컬 감정표현 입력 유지 상태. Gameplay Emote 상태의 원본은 StateComponent다.
    bool bIsEmoteInputHeld = false;

    // 기본 시점은 1인칭이며 IA_View 입력으로 1인칭/3인칭을 전환한다.
    bool bIsFirstPersonView = true;

    bool bIsInteractionInputMode = false;

    // Local Enhanced Input Context 상태.
    // Gameplay Skill 상태의 원본이 아니다.
    bool bIsSkillTargetingInputMode = false;

    /**
     * 퀘스트 시간 초과 후 완료 전까지 켜지는 공개 상태.
     *
     * 주변 플레이어도 들어야 하므로 모든 연결에 복제한다.
     * 개인 진행 수치는 복제하지 않는다.
     */
    UPROPERTY(ReplicatedUsing = OnRep_QuestSirenActive)
    bool bQuestSirenActive = false;

    /**
     * 사이렌 전용 AudioComponent.
     *
     * 생성자에서 한 번만 만들고 재생과 정지만 반복한다.
     */
    UPROPERTY(
        VisibleAnywhere, BlueprintReadOnly, Category = "DontMelt|Audio|QuestSiren", meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<UAudioComponent> QuestSirenAudioComponent = nullptr;

    /**
     * 루프 재생되는 사이렌 사운드.
     *
     * BP_DMPlayerCharacter에서 지정한다.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "DontMelt|Audio|QuestSiren",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<USoundBase> QuestSirenSound = nullptr;

    /**
     * 사이렌 거리 감쇠 설정.
     *
     * nullptr이면 사운드 에셋 자체의 Attenuation을 사용한다.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "DontMelt|Audio|QuestSiren",
        meta = (AllowPrivateAccess = "true")
    )
    TObjectPtr<USoundAttenuation> QuestSirenAttenuation = nullptr;

#pragma endregion

#pragma region Delegates

    FDMOnQuestActionEvent QuestActionEventDelegate;

    /** UI를 위한 로컬 입력 모드 알림 */
    FDMOnLocalPoseInputModeChanged LocalPoseInputModeChangedDelegate;

#pragma endregion
};