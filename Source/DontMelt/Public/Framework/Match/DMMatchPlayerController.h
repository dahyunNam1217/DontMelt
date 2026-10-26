#pragma once

#include "CoreMinimal.h"
#include "Core/DMMatchTypes.h"
#include "GameFramework/PlayerController.h"
#include "GameplayTagContainer.h"
#include "Skills/DMSkillTypes.h"
#include "Spectator/DMSpectatorPresentationTypes.h"
#include "DMMatchPlayerController.generated.h"

class ADMRelicChest;
class ADMMatchGameState;
class UNiagaraSystem;
class UNiagaraComponent;
class ADMPlayerCharacter;
class UDMGameRootWidget;
class UDMPreMatchTutorialWidget;
class UDMSpectatorCountdownWidget;
class UDMSkillComponent;
class ADMMatchPlayerState;
class UCameraComponent;
class UMaterialInterface;
class UMaterialInstanceDynamic;
class ACameraActor;
class UDMJellyPhysicsComponent;
class ADMRestrictedSpectatorPawn;
class UDMSpectatorPresentationComponent;
class UDMInputConfigDataAsset;
class UInputAction;
class UInputMappingContext;
class UWorld;
class UEnhancedInputLocalPlayerSubsystem;

struct FInputActionValue;

DECLARE_MULTICAST_DELEGATE_ThreeParams(FDMOnChestInteractionChanged, bool, int32, int32);

DECLARE_MULTICAST_DELEGATE(FDMOnRelicStolenNotification);

/**
 * 발소리 증폭 피해자에게 효과 경고를 전달한다.
 *
 * 인수는 효과가 끝날 예정인 서버 기준 시각이다.
 * 공격자 정보는 포함하지 않는다.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnFootstepAmplifyWarning, double);

DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnVisionDistortPresentationChanged, bool);

/**
 * Local Skill Targeting Input Mode의 시작/종료 Event.
 *
 * bool:
 * true  = Targeting 시작
 * false = Targeting 종료
 *
 * FGameplayTag:
 * 해당 Targeting Skill.
 */
DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnSkillTargetingStateChanged, bool, FGameplayTag);

/**
 * Local Spectator Information Restriction
 * 시작/종료 Event.
 *
 * true:
 * Spectator 중 민감한 Gameplay 정보 표시 금지.
 *
 * false:
 * 정상 Gameplay Presentation 복귀 가능.
 */

/**
 * 로컬 관전 카메라 단계 변경 알림.
 *
 * 서버 Gameplay Phase와 별개이며,
 * UI는 이 알림을 통해 현재 관전 단계를 다시 읽는다.
 */

/**
 * Local Voice Policy가 변경되었음을 알린다.
 *
 * UDMVoiceChatSubsystem이 이 알림을 구독한다.
 */
DECLARE_MULTICAST_DELEGATE_OneParam(FDMOnLocalVoiceModeChanged, EDMVoiceMode);

/**
 * Match에서 Controller 수명에 귀속되는 플레이어 기능의 경계.
 *
 * 현재는 Match 흐름 검증용 개발 콘솔 명령을 제공한다.
 */
UCLASS()
class DONTMELT_API ADMMatchPlayerController : public APlayerController
{
    GENERATED_BODY()

public:
    ADMMatchPlayerController();

protected:
    virtual void SetupInputComponent() override;

    virtual void BeginPlay() override;

    virtual void EndPlay(const EEndPlayReason::Type EndPlayReason) override;

    /**
     * 엔진 기본 구현(ToggleSpeaking)은 GameSession::bRequiresPushToTalk 값으로
     * 네트워크 음성 전송을 강제로 켜고 끄므로 UDMVoiceChatSubsystem의 전송 상태와 어긋난다.
     * 음성 전송 판단은 VoiceChatSubsystem에만 맡긴다.
     */
    virtual void ClientEnableNetworkVoice_Implementation(bool bEnable) override;

public:
    virtual void OnRep_PlayerState() override;

    virtual void OnRep_Pawn() override;

#pragma region Debug Commands

    UFUNCTION(Exec)
    void DMTestFinishMatch(int32 WinningBaseSlot);

    UFUNCTION(Exec)
    void DMTestAbortMatch();

    UFUNCTION(Exec)
    void DMTestStartOvertime();

    UFUNCTION(Exec)
    void DMTestFinishOvertime();

    UFUNCTION(Exec)
    void DMTestNPCPanic();

#pragma endregion

#pragma region Pre-match Tutorial

    // 서버만 매치 준비가 끝난 뒤 각 소유 클라이언트에 표시를 요청한다.
    UFUNCTION(Client, Reliable)
    void ClientShowPreMatchTutorial();

    UFUNCTION(Server, Reliable)
    void ServerFinishPreMatchTutorial();

    UFUNCTION(Server, Reliable)
    void ServerReportPreMatchTutorialDisplay(bool bSucceeded);

    void HandlePreMatchTutorialFinished();

    void ClosePreMatchTutorial();

    bool IsPreMatchTutorialVisible() const;

#pragma endregion

#pragma region UI Data Binding

    void RefreshUIDataSources();

#pragma endregion

#pragma region Gameplay Input

    /** 일반 게임플레이 중 마우스 캡처와 입력 모드를 복구한다. */
    void RestoreGameplayMouseCapture();

#pragma endregion

#pragma region Ingame Menu Public

    bool IsIngameMenuOpen() const
    {
        return bIngameMenuOpen;
    }

    // 로컬 Esc 메뉴를 연다.
    void OpenIngameMenu();

    // bRefreshPresentation=false는 Root 내부 갱신 중 사용한다.
    void CloseIngameMenu(bool bRefreshPresentation = true);

    // 서버 연결을 떠나 메인 메뉴 레벨로 이동한다.
    // 결과 화면에서는 true를 전달하여 메인 메뉴 패널을 표시한다.
    void LeaveMatchToTitle(bool bOpenMainMenuPanel = false);

#pragma endregion

#pragma region Spectator

    /**
     * Server에서 Death / Respawn Lifecycle가 정상적으로 시작된 뒤
     * 사망한 Owner Client에게 한 번 전달되는 관전 시작 Context.
     *
     * 이 RPC 자체는 Gameplay Death를 확정하지 않으며
     * Local Spectator Presentation을 준비하기 위한 진입점이다.
     */
    UFUNCTION(Client, Reliable)
    void ClientBeginSpectatorSequence(FDMSpectatorContext SpectatorContext);

    /**
     * Server Respawn Pipeline이
     *
     * - 새 Pawn Spawn
     * - Possess
     * - SpawnProtection
     * - LifeState Alive
     *
     * 까지 모두 성공한 뒤 Local Client에 전달하는
     * Presentation Handoff 승인.
     */
    UFUNCTION(Client, Reliable)
    void ClientFinalizeSpectatorRespawnHandoff(ADMPlayerCharacter* ExpectedNewPawn);

    // 서버만 소유 Pawn과 LifeState를 검증하여 이전 부활 승인 내용을 재전송한다.
    UFUNCTION(Server, Reliable)
    void ServerRequestRespawnHandoffResync();

    // 서버 승인 RPC와 Pawn 복제의 순서가 뒤바뀌어도 부활을 마무리한다.
    void TryCompletePendingRespawnHandoff();

    // PawnClientRestart가 입력 MappingContext 설정을 마친 뒤 호출한다.
    void NotifyLocalGameplayPawnReady(ADMPlayerCharacter* ReadyPawn);

    /**
     * 현재 PlayerCharacter가 사용하는 InputConfig를 받아
     * Spectator 전용 Controller Binding을 한 번 준비한다.
     *
     * Mapping Context를 활성화하지는 않는다.
     */
    void InitializeSpectatorInput(ADMPlayerCharacter* PlayerCharacter);

    /**
     * 현재 Local Spectator Presentation에서
     * 민감 Gameplay 정보 표시가 제한되어 있는지 반환한다.
     *
     * Gameplay LifeState SSOT가 아니다.
     */
    bool IsSpectatorInformationRestricted() const;

    /**
     * UI / Local Presentation 계층에서
     * Spectator 정보 제한 전환을 구독한다.
     */
    FDMOnSpectatorInformationRestrictionChanged& OnSpectatorInformationRestrictionChanged();

    /** 현재 로컬 관전 카메라의 Presentation 단계. */
    EDMSpectatorPhase GetLocalSpectatorPhase() const;

    /**
     * 기존 관전 시작 서버 시각을 기준으로
     * 전체 30초 중 남은 시간을 계산한다.
     *
     * 이 값은 서버 Respawn 승인 여부를 뜻하지 않는다.
     */
    double GetRemainingSpectatorTimeSeconds() const;

    /** 관전 단계 변경 시 UI에서 구독하는 알림. */
    FDMOnLocalSpectatorPhaseChanged& OnLocalSpectatorPhaseChanged();

    /**
     * 매치가 Finished 또는 Aborted가 되었을 때
     * 기존 관전 타이머와 화면 오버레이를 종료한다.
     *
     * 서버 LifeState나 Respawn 상태는 변경하지 않는다.
     */
    void PrepareLocalPresentationForMatchResult();

    /** 결과 화면 진입 후에는 관전 연출을 다시 시작하지 않는다. */
    bool IsMatchResultPresentationStarted() const
    {
        return bMatchResultPresentationStarted;
    }

    /** BP에 설정된 관전 연출 값을 관전 컴포넌트에 전달한다. */
    FDMSpectatorPresentationSettings GetSpectatorPresentationSettings() const;

#pragma endregion

#pragma region Voice Integration Boundary

    /**
     * Local Gameplay Pawn이 준비된 시점에
     * Voice Policy를 Gameplay 상태로 복구한다.
     *
     * 실제 Voice Backend는 조작하지 않는다.
     */
    void RefreshLocalVoiceModeForGameplayPawn(ADMPlayerCharacter* PlayerCharacter);

    EDMVoiceMode GetLocalVoiceMode() const
    {
        return LocalVoiceMode;
    }

    /**
     * 현재 Local Player가 음성을 송신할 수 있는 정책인지.
     *
     * DeadGlobal도 true다.
     */
    bool IsLocalVoiceTransmissionAllowed() const
    {
        return LocalVoiceMode != EDMVoiceMode::Disabled;
    }

    FDMOnLocalVoiceModeChanged& OnLocalVoiceModeChanged()
    {
        return LocalVoiceModeChangedDelegate;
    }

#pragma endregion

#pragma region Relic HUD

    /**
     * 서버에서 Steal Commit이 발생했음을
     * 각 Client에게 전달한다.
     *
     * 누구인지 / 어느 Base인지 공개하지 않는다.
     */
    UFUNCTION(Client, Reliable)
    void ClientNotifyRelicStolen();

    FDMOnChestInteractionChanged& OnChestInteractionChanged()
    {
        return ChestInteractionChangedDelegate;
    }

    FDMOnRelicStolenNotification& OnRelicStolenNotification()
    {
        return RelicStolenNotificationDelegate;
    }

#pragma endregion

#pragma region Match Result Commands

    UFUNCTION(Server, Reliable)
    void ServerSetRematchReady(bool bNewReady);

    UFUNCTION(Server, Reliable)
    void ServerRequestReturnToLobby();

    UFUNCTION(Server, Reliable)
    void ServerRequestRestartMatch();

public:
    // GameState가 결과 보관을 끝낸 뒤 호출한다.
    void NotifyMatchResultCached(const FGuid& ResultId);

private:
    UFUNCTION(Server, Reliable)
    void ServerAcknowledgeMatchResult(FGuid ResultId);

    // 같은 결과를 중복으로 보내지 않는다.
    FGuid LastAcknowledgedResultId;

#pragma endregion

public:
#pragma region Relic Commands

    /**
     * Local Q 입력 진입점.
     */
    void RequestDropCarriedRelic();

    /**
     * 실제 Drop 요청은 소유 PlayerController의
     * Server RPC를 통해 서버로 전달한다.
     */
    UFUNCTION(Server, Reliable)
    void ServerRequestDropCarriedRelic();

#pragma endregion

#pragma region Skill Commands

    void InputSkillInvisibilityStarted();

    void InputSkillFootstepAmplifyStarted();

    void InputSkillVisionDistortStarted();

    void InputTargetRandomStarted();

    void InputTargetLeaderStarted();

    void InputCancelTargetingStarted();

    /**
     * Local Targeting Input Context를 종료한다.
     *
     * Cancel은 Server RPC를 보내지 않고
     * cooldown도 소비하지 않는다.
     */
    void CancelSkillTargetingLocal();

    bool IsSkillTargetingActive() const
    {
        return PendingTargetSkillTag.IsValid();
    }

    FGameplayTag GetPendingTargetSkillTag() const
    {
        return PendingTargetSkillTag;
    }

    FDMOnSkillTargetingStateChanged& OnSkillTargetingStateChanged()
    {
        return SkillTargetingStateChangedDelegate;
    }

    UFUNCTION(Server, Reliable)
    void ServerRequestUseSelfSkill(FGameplayTag SkillTag);

    UFUNCTION(Server, Reliable)
    void ServerRequestBeginSkillTargeting(FGameplayTag SkillTag);

    UFUNCTION(Client, Reliable)
    void ClientBeginSkillTargeting(FGameplayTag SkillTag);

    UFUNCTION(Server, Reliable)
    void ServerRequestTargetedSkill(FGameplayTag SkillTag, EDMSkillTargetMode TargetMode);

    /**
     * Caster Client에서만 Target Character의
     * Footstep 증폭 Presentation을 켜거나 끈다.
     *
     * Gameplay 상태를 저장하는 RPC가 아니다.
     */
    UFUNCTION(Client, Reliable)
    void ClientSetFootstepAmplifyPresentation(
        ADMMatchPlayerState* TargetPlayerState, bool bActive, double EffectEndServerTime
    );

    /** 피해자에게 경고와 실제 효과의 예정 종료 시각을 전달한다. */
    UFUNCTION(Client, Reliable)
    void ClientNotifyFootstepAmplifyWarning(double EffectEndServerTime);

    /**
     * VisionDistort 대상 Client에만
     * 화면 왜곡 Presentation을 시작한다.
     */
    UFUNCTION(Client, Reliable)
    void ClientBeginVisionDistortPresentation(
        double EffectStartServerTime, float EntryDuration, float MainDuration, float ExitDuration
    );

    /**
     * rollback / lifecycle cleanup용.
     */
    UFUNCTION(Client, Reliable)
    void ClientClearVisionDistortPresentation();

    /**
     * Owner가 Dead / Disconnected가 되었을 때
     * Pawn-bound / input-bound Skill Presentation을 정리한다.
     *
     * Caster가 이미 사용한 FootstepAmplify는
     * 자연 종료까지 유지하므로 여기서는 지우지 않는다.
     */
    UFUNCTION(Client, Reliable)
    void ClientCleanupSkillPresentationForLifeStateChange();

    /**
     * Match Finished / Aborted 전용 전체
     * local Skill Presentation cleanup.
     */
    UFUNCTION(Client, Reliable)
    void ClientCleanupAllSkillPresentationForMatchEnd();

    FDMOnVisionDistortPresentationChanged& OnVisionDistortPresentationChanged()
    {
        return VisionDistortPresentationChangedDelegate;
    }

    bool IsVisionDistortPresentationActive() const
    {
        return bIsLocalVisionDistortPresentationActive;
    }

    FDMOnFootstepAmplifyWarning& OnFootstepAmplifyWarning()
    {
        return FootstepAmplifyWarningDelegate;
    }

#pragma endregion

#pragma region Chest Interaction

    /**
     * 서버 검증이 완료된 Chest를
     * 요청한 Client에서 연다.
     *
     * RegisteredRelicCount는 UI 표시용 임시 Snapshot이며
     * Gameplay 원본 데이터가 아니다.
     */
    UFUNCTION(Client, Reliable)
    void ClientOpenChestInteraction(ADMRelicChest* Chest, int32 BaseSlot, int32 RegisteredRelicCount);

    /**
     * 현재 열려 있는 Chest를 로컬에서 닫는다.
     */
    void CloseChestInteractionLocal();

    /**
     * 현재 Client가 Chest Interaction 상태인지 반환한다.
     */
    bool IsChestInteractionOpen() const
    {
        return OpenedChest.IsValid();
    }

    ADMRelicChest* GetOpenedChest() const
    {
        return OpenedChest.Get();
    }

    int32 GetInspectedBaseSlot() const
    {
        return InspectedBaseSlot;
    }

    int32 GetInspectedRegisteredRelicCount() const
    {
        return InspectedRegisteredRelicCount;
    }

private:
    /**
     * 현재 로컬 Client가 열어 둔 Chest.
     *
     * 서버 Gameplay SSOT가 아니라
     * 로컬 Interaction/UI Context용 임시 상태다.
     */
    TWeakObjectPtr<ADMRelicChest> OpenedChest;

    /**
     * 현재 열어 본 Chest의 BaseSlot.
     *
     * UI/Interaction Context용 임시 Snapshot.
     */
    int32 InspectedBaseSlot = INDEX_NONE;

    /**
     * 서버가 Chest Open 시점에 전달한
     * 등록 유물 개수 Snapshot.
     *
     * Gameplay 판정에 사용하지 않는다.
     */
    int32 InspectedRegisteredRelicCount = 0;

#pragma endregion

#pragma region Spectator Internal

    // 결과 화면에 진입한 이후 과거 관전 연출의 재시작을 막는다.
    // 현재 매치의 로컬 표시 상태이며 복제하지 않는다.
    bool bMatchResultPresentationStarted = false;

    // 결과 화면 진입 여부와 서버에서 받은 종료 상태를 확인한다.
    bool ShouldBlockSpectatorPresentation() const;

    /**
     * Spectator Sequence 시작 직전
     * Local Gameplay/Input Presentation을 정리한다.
     */
    void PrepareLocalGameplayForSpectating();

    /**
     * Server-confirmed Respawn Handoff RPC가 도착한 뒤
     * Local Spectator Presentation을 새 Gameplay Pawn으로 전환한다.
     *
     * 실제 Respawn 권한/판정은 수행하지 않는다.
     */
    void CompleteSpectatorRespawnHandoff(ADMPlayerCharacter* NewPlayerCharacter);

    // 게임플레이 LifeState 원본은 PlayerState이며, 아래 값은 로컬 인계 진행 기록뿐이다.
    bool bRespawnHandoffApproved = false;
    TWeakObjectPtr<ADMPlayerCharacter> LocalPreviousDeadPawn;
    TWeakObjectPtr<ADMPlayerCharacter> ServerApprovedRespawnPawn;
    TWeakObjectPtr<ADMPlayerCharacter> LocalInputReadyPawn;
    TWeakObjectPtr<ADMMatchPlayerState> ObservedRespawnPlayerState;

    // 타이머는 관전 시작부터 최종 완료/오류 복구까지 한 번씩만 관리한다.
    FTimerHandle RespawnHandoffWatchdogHandle;
    FTimerHandle RespawnHandoffResyncHandle;
    int32 RespawnWatchdogExpirations = 0;
    int32 RespawnResyncRequestCount = 0;
    double LastServerRespawnResyncTime = -100.0;

    void BindRespawnLifeStateChanges();
    void UnbindRespawnLifeStateChanges();
    void HandleRespawnLifeStateChanged(ADMMatchPlayerState* ChangedPlayerState, EDMPlayerLifeState NewLifeState);
    void ArmRespawnHandoffWatchdog(float DelaySeconds);
    void HandleRespawnHandoffWatchdog();
    void HandleRespawnHandoffResyncTimer();
    void ClearPendingRespawnHandoff();

    /** 관전 FreeCamera 입력 모드가 활성 상태인지. */
    bool IsSpectatorInputModeActive() const;

    UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const;

    /**
     * 사망 후 30초 로컬 관전 연출 담당.
     *
     * 생성자에서 만드는 비복제 컴포넌트다.
     * 서버 RPC 경계와 관전 전 Gameplay 정리는 Controller에 남는다.
     */
    UPROPERTY(VisibleAnywhere, Category = "DM|Spectator")
    TObjectPtr<UDMSpectatorPresentationComponent> SpectatorPresentation = nullptr;

#pragma endregion

#pragma region Voice Integration Internal

    /**
     * Local Voice Policy를 변경한다.
     *
     * 실제 값이 바뀔 때만 Delegate를 Broadcast한다.
     */
    void SetLocalVoiceMode(EDMVoiceMode NewVoiceMode);

    /**
     * Voice Backend가 소비할
     * 현재 Local Policy Projection.
     *
     * Replication하지 않는다.
     */
    EDMVoiceMode LocalVoiceMode = EDMVoiceMode::Disabled;

    FDMOnLocalVoiceModeChanged LocalVoiceModeChangedDelegate;

#pragma endregion

#pragma region Voice Input

    // 로비와 매치에서 동일한 음성 입력 에셋을 사용한다.
    UPROPERTY(EditDefaultsOnly, Category = "DM|Voice|Input")
    TObjectPtr<UDMInputConfigDataAsset> VoiceInputConfig = nullptr;

    // PTT V 키를 누르면 송신을 시작하고 놓거나 취소하면 중지한다.
    void InputPushToTalkStarted();
    void InputPushToTalkStopped();

    // M 키를 누를 때마다 음성 감지 모드의 음소거를 전환한다.
    void InputToggleMicrophoneMute();

#pragma endregion

#pragma region Spectator Presentation Configuration

    /**
     * 시체 중심점에서 DeathCam까지의 기본 수평 거리.
     *
     * 초기 테스트값이며 플레이테스트로 조정 가능하다.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "DM|Spectator|DeathCam",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0")
    )
    float DeathCamDistance = 350.0f;

    /**
     * 시체 중심보다 DeathCam을 얼마나 위에 둘지 결정한다.
     *
     * 초기 테스트값.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "DM|Spectator|DeathCam",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0")
    )
    float DeathCamHeight = 120.0f;

    /**
     * 기존 Gameplay Camera에서
     * DeathCam으로 넘어가는 Blend 시간.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "DM|Spectator|DeathCam",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0")
    )
    float DeathCamBlendTime = 0.2f;

    /**
     * DeathCam → Killer Live View 전환 Blend.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "DM|Spectator|KillerView",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0")
    )
    float KillerViewBlendTime = 0.25f;

    /**
     * 10~25초 FreeCamera의 최대 이동속도.
     *
     * 현재 Player SprintSpeed C++ 기본값과 같은 900을
     * 초기 테스트값으로 사용한다.
     *
     * Gameplay 이동속도 SSOT가 아니라
     * 독립적인 Spectator Presentation 설정값이다.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "DM|Spectator|FreeCamera",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.0")
    )
    float FreeCameraMaxSpeed = 900.0f;

    /**
     * 25초에 FreeCamera 화면이
     * 완전한 Black으로 전환되는 시간.
     */
    UPROPERTY(
        EditDefaultsOnly,
        BlueprintReadOnly,
        Category = "DM|Spectator|Fade",
        meta = (AllowPrivateAccess = "true", ClampMin = "0.01")
    )
    float SpectatorFadeToBlackDuration = 0.25f;

#pragma endregion

#pragma region Skill Internal

    UDMSkillComponent* GetOwnedSkillComponent() const;

    void CommitPendingTargetSkill(EDMSkillTargetMode TargetMode);

    /**
     * Local Input Context용 임시 상태.
     *
     * 실제 Gameplay Skill 상태나 Target의 원본이 아니다.
     */
    FGameplayTag PendingTargetSkillTag;

    /**
     * Caster Client에만 존재하는 Audio Presentation 대상.
     *
     * Gameplay Target SSOT가 아니다.
     */
    TWeakObjectPtr<ADMMatchPlayerState> LocalFootstepAmplifyTarget;

    /**
     * Local Presentation 종료용 Timer.
     *
     * Server Gameplay Timer와는 별개이며
     * EndServerTime을 기준으로 보정한다.
     */
    FTimerHandle LocalFootstepAmplifyPresentationTimerHandle;

    void ClearLocalFootstepAmplifyPresentation();

    void ClearLocalVisionDistortPresentation();

    TWeakObjectPtr<UCameraComponent> LocalVisionDistortCamera;

    UPROPERTY(Transient)
    TObjectPtr<UMaterialInstanceDynamic> LocalVisionDistortMID = nullptr;

    FTimerHandle LocalVisionDistortPresentationTimerHandle;

    bool bIsLocalVisionDistortPresentationActive = false;

    double LocalVisionDistortEndServerTime = 0.0;

#pragma endregion

#pragma region Skill Presentation Configuration

    /**
     * Target Client의 화면 왜곡에 사용할
     * Post Process Material.
     *
     * Material Domain은 Post Process여야 한다.
     */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|Skill|Vision", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UMaterialInterface> VisionDistortPostProcessMaterial = nullptr;

#pragma endregion

#pragma region Own Chest Marker Presentation

    /** 로컬 플레이어만 자신의 유물상자 위에 표시하는 Niagara. */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|Relic|Own Chest Marker", meta = (AllowPrivateAccess = "true"))
    TObjectPtr<UNiagaraSystem> OwnChestMarkerEffect = nullptr;

    /** 상자 메시 최상단에서 마커 중심까지의 추가 높이(cm). */
    UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category = "DM|Relic|Own Chest Marker", meta = (AllowPrivateAccess = "true", ClampMin = "0.0", UIMin = "0.0"))
    float OwnChestMarkerHeightOffset = 50.0f;

    /** 오직 로컬 실행 환경에 생성한 이펙트이며 복제하지 않는다. */
    UPROPERTY(Transient)
    TObjectPtr<UNiagaraComponent> ActiveOwnChestMarker = nullptr;

    /** 현재 이펙트를 생성한 상자. 슬롯 원본은 PlayerState만 소유한다. */
    TWeakObjectPtr<ADMRelicChest> MarkedOwnChest;

    /** 현재 구독 중인 PlayerState와 GameState. 소유권 데이터의 복사본이 아니다. */
    TWeakObjectPtr<ADMMatchPlayerState> ObservedOwnChestPlayerState;
    TWeakObjectPtr<ADMMatchGameState> ObservedOwnChestGameState;

    void BindOwnChestMarkerSources();
    void UnbindOwnChestMarkerSources();
    void RefreshOwnChestMarker();
    void ClearOwnChestMarker();
    void HandleOwnChestBaseSlotChanged(ADMMatchPlayerState* ChangedPlayerState, int32 NewBaseSlot);
    void HandleOwnChestPhaseChanged(ADMMatchGameState* ChangedGameState, FDMMatchPhaseState NewPhaseState);

#pragma endregion

#pragma region UI Configuration

    /**
     * Match UI 최상위 Root Widget 클래스.
     * BP_DMMatchPlayerController에서 지정한다.
     */
    UPROPERTY(EditDefaultsOnly, Category = "DM|UI")
    TSubclassOf<UDMGameRootWidget> GameRootWidgetClass;

    UPROPERTY(EditDefaultsOnly, Category = "DM|UI|Tutorial")
    TSubclassOf<UDMPreMatchTutorialWidget> PreMatchTutorialWidgetClass;

    UPROPERTY(Transient)
    TObjectPtr<UDMPreMatchTutorialWidget> PreMatchTutorialWidget = nullptr;

    bool bPreMatchTutorialActive = false;

    /**
     * Local Controller가 소유하는
     * 실제 Match UI Root 인스턴스.
     */
    UPROPERTY(Transient)
    TObjectPtr<UDMGameRootWidget> GameRootWidget = nullptr;

    /**
     * 25~30초 Spectator Countdown Presentation Widget.
     *
     * BP_DMMatchPlayerController에서
     * WBP_DMSpectatorCountdown을 지정한다.
     */
    UPROPERTY(EditDefaultsOnly, Category = "DM|UI")
    TSubclassOf<UDMSpectatorCountdownWidget> SpectatorCountdownWidgetClass;

#pragma endregion

#pragma region Ingame Menu Configuration

    // 기존 Character 입력 매핑과 독립적으로 유지한다.
    UPROPERTY(EditDefaultsOnly, Category = "DM|Input|Menu")
    TObjectPtr<UInputAction> MenuAction = nullptr;

    UPROPERTY(EditDefaultsOnly, Category = "DM|Input|Menu")
    TObjectPtr<UInputMappingContext> MenuMappingContext = nullptr;

    // 메인 메뉴로 돌아갈 레벨.
    UPROPERTY(EditDefaultsOnly, Category = "DM|UI|Menu")
    TSoftObjectPtr<UWorld> MainMenuMap;

#pragma endregion

#pragma region Ingame Menu State

    // 로컬 UI 상태이며 복제하지 않는다.
    bool bIngameMenuOpen = false;

    // 결과 화면 입력 모드를 한 번만 적용하기 위한 로컬 상태다.
    bool bResultInputModeActive = false;

    // 로컬 퇴장 요청의 중복 실행을 막는다. 복제하지 않는다.
    bool bLeavingMatchToTitle = false;

    void InputMenuStarted();

#pragma endregion

#pragma region UI Delegates

    FDMOnChestInteractionChanged ChestInteractionChangedDelegate;

    FDMOnRelicStolenNotification RelicStolenNotificationDelegate;

    FDMOnFootstepAmplifyWarning FootstepAmplifyWarningDelegate;

    FDMOnVisionDistortPresentationChanged VisionDistortPresentationChangedDelegate;

    FDMOnSkillTargetingStateChanged SkillTargetingStateChangedDelegate;

#pragma endregion
};