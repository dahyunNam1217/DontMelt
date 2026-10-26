#pragma once

#include "CoreMinimal.h"
#include "TimerManager.h"
#include "UObject/Object.h"
#include "DMRespawnCoordinator.generated.h"

class ADMMatchGameMode;
class ADMMatchPlayerState;
class ADMRespawnPoint;
class AController;
class UDMMatchRuleDataAsset;

/**
 * 사망 후 리스폰 수명주기의 서버 전용 담당자.
 *
 * ADMMatchGameMode가 Outer이자 소유자이며 Client에는 존재하지 않는다.
 * 사망 확정과 승패 판정은 GameMode에 남고, 이 객체는 그 이후의
 * 30초 대기 타이머, RespawnPoint 선택, 새 Pawn 생성과 실패 시 롤백·재시도만 맡는다.
 *
 * 생존 상태의 원본은 계속 ADMMatchPlayerState의 LifeState다.
 */
UCLASS()
class DONTMELT_API UDMRespawnCoordinator : public UObject
{
    GENERATED_BODY()

public:
    void Initialize(
        ADMMatchGameMode* InOwnerGameMode, UDMMatchRuleDataAsset* InMatchRuleData, float InRespawnRetryDelaySeconds
    );

    virtual UWorld* GetWorld() const override;

#pragma region Respawn Point

    /**
     * 현재 Starting Roster의 모든 Base에
     * 사용할 수 있는 RespawnPoint가 있는지 검사한다.
     */
    bool ValidateRespawnPointsForStartingRoster(FString& OutFailureReason) const;

#pragma endregion

#pragma region Respawn Lifecycle

    /**
     * Death Commit 후 30초 Respawn Lifecycle을 시작한다.
     */
    bool StartRespawnLifecycle(ADMMatchPlayerState* MatchPlayerState);

    /**
     * Respawn 실패 시 짧은 지연 후 HandleRespawnTimerExpired를 다시 예약한다.
     * Spectating 상태로 영구 고립되는 것을 막는다.
     */
    void ScheduleRespawnRetry(ADMMatchPlayerState* MatchPlayerState);

    void ClearRespawnTimer(ADMMatchPlayerState* MatchPlayerState);

    void ClearAllRespawnTimers();

#pragma endregion

private:
#pragma region Internal

    /**
     * Player별 Respawn Timer 만료 처리.
     */
    void HandleRespawnTimerExpired(ADMMatchPlayerState* MatchPlayerState);

    /**
     * BaseSlot에 대응하는 Enabled RespawnPoint 중
     * Weight를 반영하여 하나를 선택한다.
     */
    ADMRespawnPoint* FindRandomRespawnPointForBaseSlot(int32 BaseSlot) const;

    /**
     * PlayerState를 소유한 현재 Controller를 찾는다.
     */
    AController* FindControllerForPlayerState(const ADMMatchPlayerState* MatchPlayerState) const;

    /** 소유 GameMode와 같은 TimerManager·GameState를 사용한다. */
    FTimerManager& GetWorldTimerManager() const;

    template <class T>
    T* GetGameState() const;

#pragma endregion

#pragma region State

    UPROPERTY(Transient)
    TObjectPtr<ADMMatchGameMode> OwnerGameMode = nullptr;

    UPROPERTY(Transient)
    TObjectPtr<UDMMatchRuleDataAsset> MatchRuleData = nullptr;

    /** Respawn 실패 시 재시도 간격(초). GameMode의 설정값을 받는다. */
    float RespawnRetryDelaySeconds = 1.0f;

    /** Player별 30초 Respawn Timer와 재시도 Timer. */
    TMap<TWeakObjectPtr<ADMMatchPlayerState>, FTimerHandle> RespawnTimerHandles;

    // 동일한 사망 이벤트에서 스폰 오류가 영구 반복되지 않도록 제한한다.
    TMap<TWeakObjectPtr<ADMMatchPlayerState>, int32> RespawnRetryAttemptCounts;

#pragma endregion
};
