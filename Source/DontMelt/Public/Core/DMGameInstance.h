#pragma once

#include "CoreMinimal.h"
#include "Engine/GameInstance.h"
#include "Net/Core/Connection/NetEnums.h"
#include "DMGameInstance.generated.h"

class UNetDriver;
class UWorld;

/**
 * 게임 실행 동안 유지되는 전역 실행 흐름 관리자.
 *
 * 현재 단계에서는 Client Network Failure를 감지하고
 * 기본 Lobby 맵으로 복귀시키는 책임만 가진다.
 */
UCLASS()
class DONTMELT_API UDMGameInstance
    : public UGameInstance
{
    GENERATED_BODY()

public:
    virtual void Init() override;

    virtual void Shutdown() override;

private:
    /**
     * 이 GameInstance에 속한 World의
     * Network Failure를 처리한다.
     */
    void HandleNetworkFailure(
        UWorld* World,
        UNetDriver* NetDriver,
        ENetworkFailure::Type FailureType,
        const FString& ErrorString
    );

    /**
     * 로그 출력을 위한 FailureType 이름 변환.
     */
    static const TCHAR* GetNetworkFailureName(
        ENetworkFailure::Type FailureType
    );

    /**
     * Network Failure 복구로 시작된 맵 이동이 완료되면
     * 중복 처리 잠금을 해제한다.
     */
    void HandlePostLoadMap(
        UWorld* LoadedWorld
    );

    /**
     * Engine Network Failure Delegate를
     * 정확히 제거하기 위한 Handle.
     */
    FDelegateHandle NetworkFailureDelegateHandle;

    /**
     * PostLoadMapWithWorld Delegate 해제용 Handle.
     */
    FDelegateHandle PostLoadMapDelegateHandle;

    /**
     * 동일 Network Failure 처리 중 재진입 방지.
     */
    bool bHandlingNetworkFailure = false;
};