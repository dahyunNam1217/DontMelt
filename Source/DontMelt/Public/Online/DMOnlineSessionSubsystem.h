
#pragma once

#include "CoreMinimal.h"
#include "Interfaces/OnlineSessionInterface.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Templates/SharedPointer.h"
#include "TimerManager.h"

#include "DMOnlineSessionSubsystem.generated.h"

class FOnlineSessionSearch;
class FOnlineSessionSettings;

// 비동기 작업 결과와 상세 메시지를 전달한다.
DECLARE_MULTICAST_DELEGATE_TwoParams(FDMOnlineSessionCompleted, bool, const FString&);

/**
 * Steam 온라인 세션의 생성, 검색, 참가, 해제를 담당한다.
 * 게임플레이 Ready와 Match 판정은 관리하지 않는다.
 */
UCLASS()
class DONTMELT_API UDMOnlineSessionSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
#pragma region Lifecycle

    virtual void Initialize(FSubsystemCollectionBase& Collection) override;

    virtual void Deinitialize() override;

#pragma endregion

#pragma region Session Requests

    // 2~4인 Steam 로비 세션 생성을 요청한다.
    bool CreateLobbySession(int32 MaxPlayers);

    // 참가 가능한 Don’t Melt 세션을 검색한다.
    bool FindLobbySessions();

    // 검색 결과의 인덱스를 사용하여 참가를 요청한다.
    bool JoinLobbySession(int32 ResultIndex);

    // 현재 로컬 세션을 비동기로 해제한다.
    bool DestroyLobbySession();

    // 로비의 광고 상태를 Lobby 또는 Starting으로 갱신한다.
    bool UpdateLobbyMatchStarting(bool bStarting);

#pragma endregion

#pragma region Session Queries

    // 작업 중에는 다른 세션 요청을 시작하지 않는다.
    bool IsBusy() const;

    // 이전 Steam 요청의 종료를 확인하지 못하면 새 요청을 차단한다.
    bool IsRecoveryBlocked() const;

    // 필터를 통과한 검색 결과의 개수다.
    int32 GetFoundSessionCount() const;

    // 검색된 세션의 호스트 표시 이름이다.
    FString GetFoundSessionOwnerName(int32 ResultIndex) const;

    // 검색된 세션의 남은 공개 참가 슬롯 수다.
    int32 GetFoundSessionOpenSlots(int32 ResultIndex) const;

    // JoinSession 후 해석한 플랫폼 연결 주소다.
    const FString& GetJoinedConnectString() const;

    // 로컬 GameSession이 남아 있는지 확인한다. 메인 메뉴의 잔여 세션 정리에 사용한다.
    bool HasLocalLobbySession() const;

#pragma endregion

#pragma region Match Admission

    // 서버가 확정한 매치 참가 또는 로비 복귀 명단을 저장한다.
    bool SetApprovedMatchRoster(const TArray<FUniqueNetIdRepl>& PlayerIds);

    // 매치에 도착해야 하는 승인 참가자 수다.
    int32 GetApprovedMatchPlayerCount() const;

    // 유효한 승인 명단이 준비되었는지 검사한다.
    bool HasApprovedMatchRoster() const;

    // 현재 ID가 승인 명단에 포함되어 있는지 검사한다.
    bool IsApprovedMatchPlayer(const FUniqueNetIdRepl& PlayerId) const;

    // 해당 ID가 이번 매치에 이미 입장했는지 검사한다.
    bool HasMatchPlayerArrived(const FUniqueNetIdRepl& PlayerId) const;

    // 서버가 성공적으로 승인한 최초 입장을 기록한다.
    bool MarkMatchPlayerArrived(const FUniqueNetIdRepl& PlayerId);

    // 같은 명단으로 새 매치 맵을 시작할 때 입장 기록만 초기화한다.
    void ResetMatchArrivalHistory();

    // 시작 취소 또는 세션 해제 시 명단과 입장 기록을 모두 지운다.
    void ClearApprovedMatchRoster();

#pragma endregion

#pragma region Completion Events

    FDMOnlineSessionCompleted OnCreateCompleted;
    FDMOnlineSessionCompleted OnFindCompleted;
    FDMOnlineSessionCompleted OnJoinCompleted;
    FDMOnlineSessionCompleted OnDestroyCompleted;
    // 세션 설정 갱신 완료를 알린다.
    FDMOnlineSessionCompleted OnUpdateCompleted;

#pragma endregion

private:
#pragma region Operation State

    // 플랫폼 세션 상태와 별개의 로컬 비동기 작업 상태다.
    enum class EOperation : uint8
    {
        None,
        Create,
        Find,
        Join,
        Destroy,
        Update
    };

    EOperation CurrentOperation = EOperation::None;

    // Steam 완료 콜백은 세션 이름만 전달하므로 같은 이름의 요청을 겹치지 않게 직렬화한다.
    uint64 OperationSequence = 0;
    uint64 ActiveOperationSequence = 0;

    // 시간 초과 뒤에도 기존 Delegate를 유지하여 늦은 완료 결과를 회수한다.
    bool bWaitingForTimedOutCompletion = false;

    // 실패한 Create/Join/Destroy의 잔여 세션을 실제로 삭제하는 동안의 상태다.
    bool bRecoveryDestroyActive = false;
    EOperation RecoveryOrigin = EOperation::None;
    FString RecoveryFailureDetail;

    // 플랫폼 작업의 종료를 확정할 수 없을 때 안전하지 않은 새 세션 요청을 금지한다.
    bool bRecoveryBlocked = false;

    // 현재 진행 중인 요청에서 등록한 Delegate다.
    FDelegateHandle ActiveDelegateHandle;

    // 검색 취소 완료는 원래 Find 완료와 독립적인 Delegate로 도착한다.
    FDelegateHandle CancelFindDelegateHandle;

    // 비동기 설정 갱신이 끝날 때까지 설정 객체를 보관한다.
    TSharedPtr<FOnlineSessionSettings> PendingUpdateSettings;

    // 검색 결과 원본은 별도로 복제하지 않는다.
    TSharedPtr<FOnlineSessionSearch> SessionSearch;

    TArray<int32> VisibleResultIndices;

    // JoinSession 완료 후 원본 검색 결과를 다시 확인하기 위한 인덱스다.
    int32 PendingJoinSourceIndex = INDEX_NONE;

    FString JoinedConnectString;

    // Steam 완료 콜백이 오지 않을 때 작업 상태를 풀어 주는 타이머다.
    FTimerHandle OperationTimeoutHandle;

    // 서버에서 확정한 최초 매치 참가 허용 명단이다.
    TSet<FString> ApprovedMatchPlayerIds;

    // 실제 매치 입장이 이미 처리된 사용자 명단이다.
    TSet<FString> ArrivedMatchPlayerIds;

#pragma endregion

#pragma region Completion Callbacks

    void HandleCreateComplete(FName SessionName, bool bWasSuccessful);

    void HandleFindComplete(bool bWasSuccessful);

    void HandleCancelFindComplete(bool bWasSuccessful);

    void HandleJoinComplete(FName SessionName, EOnJoinSessionCompleteResult::Type Result);

    void HandleDestroyComplete(FName SessionName, bool bWasSuccessful);

    void HandleUpdateComplete(FName SessionName, bool bWasSuccessful);

#pragma endregion

#pragma region Internal Helpers

    // 현재 등록된 세션 완료 Delegate를 제거한다.
    void ClearActiveDelegate();

    void ClearCancelFindDelegate();

    // 작업을 종료하고 대응하는 완료 이벤트를 전달한다.
    void CompleteOperation(bool bWasSuccessful, const FString& Detail);

    // 작업 시작 번호는 진단 전용이며, Steam의 플랫폼 작업을 강제로 취소하지 않는다.
    void BeginOperation(EOperation Operation);

    // 원래 요청이 종료된 후 남은 세션을 실제 Destroy 완료까지 추적한다.
    void BeginRecoveryDestroy(const FString& FailureDetail);

    // 이전 요청의 완료가 영원히 오지 않으면 실패를 통지하고 새 요청을 안전하게 차단한다.
    void HandleRecoveryTimeout();
    void FailClosedRecovery(const FString& Detail);

    // 세션 복구 차단 상태를 클라이언트에 알려 준다.
    bool RejectBlockedOperation(EOperation RequestedOperation);

    // 검색 결과 및 임시 접속 정보를 초기화한다.
    void ResetCachedSessionData();

    // 세션 작업 시작 시 응답 대기 타이머를 시작한다.
    void StartOperationTimeout();

    // 응답 대기 타이머를 해제한다.
    void ClearOperationTimeout();

    // 응답이 오지 않은 작업을 실패로 종료한다.
    void HandleOperationTimeout();

#pragma endregion
};
