#pragma once

#include "CoreMinimal.h"
#include "Engine/DataAsset.h"
#include "DMInputConfigDataAsset.generated.h"

class UInputAction;
class UInputMappingContext;

/**
 * Don't Melt의 Enhanced Input Asset 참조를 관리하는 Data Asset.
 *
 * 실제 구현된 입력 Asset만 보유한다.
 */
UCLASS(BlueprintType)
class DONTMELT_API UDMInputConfigDataAsset : public UDataAsset
{
    GENERATED_BODY()

public:
    // Mapping Contexts
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Contexts")
    TObjectPtr<UInputMappingContext> GameplayMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Contexts")
    TObjectPtr<UInputMappingContext> PoseMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Contexts")
    TObjectPtr<UInputMappingContext> InteractionMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Contexts")
    TObjectPtr<UInputMappingContext> SkillTargetingMappingContext;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Contexts")
    TObjectPtr<UInputMappingContext> SpectatorMappingContext;

    // 로비·매치·관전 중 공통으로 유지할 음성 입력이다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Contexts")
    TObjectPtr<UInputMappingContext> VoiceMappingContext;

    // Camera
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> ViewAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> LookAction;

    // Movement
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> MoveAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> JumpAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> SprintAction;

    // Combat
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> AttackAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> DefenseAction;

    // Skill

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> SkillInvisibilityAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> SkillFootstepAmplifyAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> SkillVisionDistortAction;

    // Skill Targeting

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> TargetRandomAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> TargetLeaderAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> CancelTargetingAction;

    // Interaction
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> InteractAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> DropRelicAction;

    // Pose
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> PoseAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> ExitPoseAction;

    // Emote
    // 누르고 있는 동안 지속형 감정표현을 유지한다(감정표현 퀘스트 진행 입력).
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> EmoteAction;

    // Interaction

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> ConfirmInteractionAction;

    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> CloseInteractionAction;

    // 마이크 송신 의도를 전달하는 로컬 입력이다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> PushToTalkAction;

    // 음성 감지 모드에서 마이크 음소거를 전환한다.
    UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = "DontMelt|Input|Actions")
    TObjectPtr<UInputAction> ToggleMicrophoneMuteAction;
};
