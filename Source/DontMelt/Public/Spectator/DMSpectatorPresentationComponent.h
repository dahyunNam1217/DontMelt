#pragma once

#include "CoreMinimal.h"
#include "Components/ActorComponent.h"
#include "Core/DMMatchTypes.h"
#include "Engine/EngineTypes.h"
#include "Camera/PlayerCameraManager.h"
#include "Spectator/DMSpectatorPresentationTypes.h"
#include "TimerManager.h"
#include "DMSpectatorPresentationComponent.generated.h"

class ACameraActor;
class ADMMatchPlayerController;
class ADMPlayerCharacter;
class ADMRestrictedSpectatorPawn;
class APawn;
class UDMInputConfigDataAsset;
class UDMJellyPhysicsComponent;
class UDMSpectatorCountdownWidget;
class UEnhancedInputLocalPlayerSubsystem;
class UInputComponent;
struct FInputActionValue;

/**
 * 사망 후 30초 로컬 관전 연출.
 *
 * Corpse(0~5초) → KillerView(5~10초) → FreeCamera(10~25초) → Fade(25~30초).
 *
 * ADMMatchPlayerController가 생성자에서 만드는 비복제 컴포넌트이며,
 * 소유 Client에서만 동작한다. 실제 사망·리스폰 판정은 서버가 하고,
 * Controller의 Client RPC가 이 컴포넌트의 시작·종료 함수를 호출한다.
 */
UCLASS(ClassGroup = (DontMelt))
class DONTMELT_API UDMSpectatorPresentationComponent : public UActorComponent
{
    GENERATED_BODY()

public:
    UDMSpectatorPresentationComponent();

#pragma region Sequence

    /** 서버가 보낸 관전 Context를 받고 정보 제한을 켠다. */
    void ReceiveSpectatorContext(const FDMSpectatorContext& SpectatorContext);

    /** 0~5초 Corpse DeathCam을 시작한다. */
    void BeginCorpseSpectatorPhase();

    /** 서버가 리스폰을 확정한 뒤 관전 연출을 새 Pawn으로 넘기고 정리한다. */
    void FinishSequenceForRespawn(ADMPlayerCharacter* NewPlayerCharacter);

    /** 매치 결과 화면 진입 시 관전 타이머와 오버레이를 멈춘다. */
    void StopForMatchResult();

    /** Controller EndPlay에서 로컬 카메라·Pawn·입력을 정리한다. */
    void ShutdownPresentation();

    /** Controller EndPlay에서 UI 구독을 정리한다. */
    void ClearPresentationDelegates();

    /** 리스폰 인계 시 정리할 관전 연출이 남아 있는지. */
    bool HasActiveSpectatorPresentation() const;

    // 결과 화면 진입 여부와 서버에서 받은 종료 상태를 확인한다.
    bool ShouldBlockSpectatorPresentation() const;

#pragma endregion

#pragma region Query

    /**
     * 현재 PlayerCharacter가 사용하는 InputConfig를 받아
     * Spectator 전용 Controller Binding을 한 번 준비한다.
     *
     * Mapping Context를 활성화하지는 않는다.
     */
    void InitializeSpectatorInput(ADMPlayerCharacter* PlayerCharacter);

    bool IsSpectatorInformationRestricted() const
    {
        return bSpectatorInformationRestricted;
    }

    bool IsSpectatorInputModeActive() const
    {
        return bSpectatorInputModeActive;
    }

    EDMSpectatorPhase GetLocalSpectatorPhase() const
    {
        return LocalSpectatorPhase;
    }

    /**
     * 기존 관전 시작 서버 시각을 기준으로
     * 전체 30초 중 남은 시간을 계산한다.
     */
    double GetRemainingSpectatorTimeSeconds() const;

    FDMOnSpectatorInformationRestrictionChanged& OnSpectatorInformationRestrictionChanged()
    {
        return SpectatorInformationRestrictionChangedDelegate;
    }

    FDMOnLocalSpectatorPhaseChanged& OnLocalSpectatorPhaseChanged()
    {
        return LocalSpectatorPhaseChangedDelegate;
    }

#pragma endregion

private:
#pragma region Owner Access

    ADMMatchPlayerController* GetOwnerController() const;

    FDMSpectatorPresentationSettings GetPresentationSettings() const;

    bool IsLocalController() const;

    APawn* GetPawn() const;

    template <class T>
    T* GetPawn() const;

    FRotator GetControlRotation() const;

    AActor* GetViewTarget() const;

    void SetViewTarget(AActor* NewViewTarget) const;

    void SetViewTargetWithBlend(
        AActor* NewViewTarget, float BlendTime, EViewTargetBlendFunction BlendFunc, float BlendExp, bool bLockOutgoing
    ) const;

    APlayerCameraManager* GetOwnerCameraManager() const;

    UInputComponent* GetOwnerInputComponent() const;

    UEnhancedInputLocalPlayerSubsystem* GetEnhancedInputSubsystem() const;

#pragma endregion

#pragma region Presentation Internal

    /**
     * Local Spectator Information Restriction 상태를 변경하고
     * 실제 변경이 있을 때만 Delegate를 Broadcast한다.
     */
    void SetSpectatorInformationRestricted(bool bRestricted);

    /**
     * Server가 전달한 1회성 Spectator Context.
     *
     * Gameplay SSOT가 아니라
     * Local Presentation용 Snapshot이다.
     */
    UPROPERTY(Transient)
    FDMSpectatorContext LocalSpectatorContext;

    /**
     * 관전 중 Local Presentation 정보 은닉 Gate.
     *
     * Gameplay LifeState가 아니며
     * Network Replication하지 않는다.
     */
    bool bSpectatorInformationRestricted = false;

    /**
     * 현재 Local Spectator Presentation 단계.
     *
     * LifeState == Spectating일 때만 의미가 있으며
     * Replication하지 않는다.
     */
    EDMSpectatorPhase LocalSpectatorPhase = EDMSpectatorPhase::Corpse;

    /** 실제 단계 전환이 발생하면 로컬 UI에 알린다. */
    FDMOnLocalSpectatorPhaseChanged LocalSpectatorPhaseChangedDelegate;

    /**
     * 0~5초 Corpse DeathCam의 Local CameraActor.
     *
     * Server Actor가 아니며
     * 다른 Client에 Replication하지 않는다.
     */
    UPROPERTY(Transient)
    TObjectPtr<ACameraActor> LocalDeathCameraActor = nullptr;

    /**
     * Corpse / Killer / FreeCamera / Fade 전환에
     * 재사용할 Local Presentation Timer.
     *
     * 실제 Respawn Timer와는 별개다.
     */
    FTimerHandle LocalSpectatorPhaseTimerHandle;

    /**
     * 5→4→3→2→1 경계 갱신용 Local Timer.
     *
     * 반복 Timer가 아니라
     * 다음 경계까지의 One-shot Timer로 사용한다.
     */
    FTimerHandle LocalSpectatorCountdownTimerHandle;

    void HandleCorpseSpectatorPhaseExpired();

    void CleanupLocalDeathCamera();

    /**
     * 5~10초 Killer Live View 시작.
     */
    void BeginKillerViewSpectatorPhase();

    /**
     * Killer View 5초 종료.
     *
     * FreeCamera 전환으로 연결한다.
     */
    void HandleKillerViewSpectatorPhaseExpired();

    /**
     * 10~25초 Restricted FreeCamera를 시작한다.
     */
    void BeginFreeCameraSpectatorPhase();

    /**
     * 25초에 FreeCamera 입력을 종료한다.
     *
     * Fade Phase로 연결한다.
     */
    void HandleFreeCameraSpectatorPhaseExpired();

    /**
     * 25~30초 Fade + Countdown Phase 시작.
     */
    void BeginFadeSpectatorPhase();

    /**
     * ServerTime 기준 현재 Countdown 숫자를 갱신한다.
     */
    void UpdateFadeSpectatorCountdown();

    /**
     * Spectator 시작 후 30초 도달.
     *
     * 실제 Respawn은 Server GameMode가 담당한다.
     */
    void HandleFadeSpectatorPhaseExpired();

    /**
     * Fade Camera / Countdown Widget / Local Timer 정리.
     */
    void CleanupLocalSpectatorFadePresentation();

    /**
     * Local RestrictedSpectatorPawn을 안전하게 제거한다.
     */
    void CleanupLocalRestrictedSpectatorPawn();

    /**
     * Spectator Sequence 시작 이후
     * 현재까지의 ServerTime 기준 경과시간.
     */
    double GetElapsedSpectatorTime() const;

    /**
     * 현재 관전 중인 Killer의 실제 Death Presentation 발생.
     */
    void HandleObservedKillerDeathPresentationStarted();

    /**
     * Killer Pawn Destroy/Disconnect 처리.
     */
    UFUNCTION()
    void HandleObservedKillerPawnDestroyed(AActor* DestroyedActor);

    /**
     * KillerView가 더 이상 유효하지 않을 때
     * 기존 DeathCam으로 복귀한다.
     */
    void FallbackKillerViewToDeathCamera(const TCHAR* Reason);

    /**
     * Killer Pawn / Physics Delegate Binding 정리.
     */
    void ClearObservedKillerViewBindings();

    /**
     * 현재 KillerView에서 관찰 중인 Pawn.
     *
     * Gameplay 소유권이 아니므로 Weak Reference.
     */
    TWeakObjectPtr<APawn> LocalObservedKillerPawn;

    /**
     * Killer의 실제 Death Presentation Event 구독용.
     */
    TWeakObjectPtr<UDMJellyPhysicsComponent> LocalObservedKillerPhysicsComponent;

    /**
     * 10~25초 FreeCamera 입력 모드 진입.
     *
     * 기존 Gameplay 계열 Context를 제거하고
     * IMC_Spectator만 활성화한다.
     */
    bool EnterSpectatorInputMode();

    /**
     * Spectator Mapping Context만 제거한다.
     *
     * 죽은 Character Gameplay 입력을 다시 켜지는 않는다.
     * Gameplay 복구는 Respawn된 Pawn의
     * PawnClientRestart()가 담당한다.
     */
    void ExitSpectatorInputMode();

    void InputSpectatorMove(const FInputActionValue& InputActionValue);

    void InputSpectatorLook(const FInputActionValue& InputActionValue);

    /**
     * 현재 Character에서 얻어 온 InputConfig의
     * Local runtime reference.
     *
     * 별도 설정 복사본이 아니다.
     */
    UPROPERTY(Transient)
    TObjectPtr<UDMInputConfigDataAsset> LocalInputConfig = nullptr;

    /**
     * Controller InputComponent에
     * IA_Move / IA_Look Binding을 이미 등록했는지.
     */
    bool bSpectatorInputBindingsInitialized = false;

    /**
     * 현재 IMC_Spectator가 실제 활성 상태인지.
     *
     * Local Presentation 상태이며
     * Gameplay LifeState 원본이 아니다.
     */
    bool bSpectatorInputModeActive = false;

    /**
     * Local FreeCamera Pawn.
     *
     * Controller가 Actor 수명을 소유하지 않으므로
     * Weak Reference를 사용한다.
     */
    TWeakObjectPtr<ADMRestrictedSpectatorPawn> LocalRestrictedSpectatorPawn;

    /**
     * 현재 Camera Fade를 이 Spectator Sequence가
     * 활성화했는지 여부.
     *
     * 다른 시스템의 Camera Fade를 무조건 Stop하지 않기 위한
     * Local Presentation flag.
     */
    bool bLocalSpectatorFadeActive = false;

    /**
     * 마지막으로 Widget에 전달한 숫자.
     *
     * 동일 값 중복 업데이트 방지용.
     */
    int32 LocalDisplayedFadeCountdown = INDEX_NONE;

    /**
     * Local Client에만 존재하는 Countdown Widget.
     */
    UPROPERTY(Transient)
    TObjectPtr<UDMSpectatorCountdownWidget> SpectatorCountdownWidget = nullptr;

    FDMOnSpectatorInformationRestrictionChanged SpectatorInformationRestrictionChangedDelegate;

#pragma endregion
};
