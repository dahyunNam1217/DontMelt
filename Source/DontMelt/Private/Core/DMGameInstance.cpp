#include "Core/DMGameInstance.h"

#include "Engine/Engine.h"
#include "Engine/NetDriver.h"
#include "Engine/World.h"
#include "UObject/UObjectGlobals.h"

/*
#include "Engine/Engine.h"
다음 전역 Engine과 Delegate 접근에 필요
GEngine
GEngine->OnNetworkFailure()
#include "Engine/NetDriver.h"
UNetDriver의 완전한 타입 정보와 로그 이름 출력에 사용
#include "Engine/World.h"
다음 검사에 사용
World->GetGameInstance()
World->GetNetMode()
#include "Misc/GuardValue.h"
TGuardValue<bool>에 필요
*/

DEFINE_LOG_CATEGORY_STATIC(
    LogDMGameInstance,
    Log,
    All
);

void UDMGameInstance::Init()
{
    Super::Init();

    if (GEngine == nullptr)
    {
        UE_LOG(
            LogDMGameInstance,
            Error,
            TEXT(
                "Network failure handler registration failed: "
                "GEngine is unavailable."
            )
        );

        return;
    }

    if (NetworkFailureDelegateHandle.IsValid())
    {
        UE_LOG(
            LogDMGameInstance,
            Warning,
            TEXT(
                "Network failure handler registration ignored: "
                "the Delegate is already registered."
            )
        );

        return;
    }

    NetworkFailureDelegateHandle =
        GEngine->OnNetworkFailure().AddUObject(
            this,
            &UDMGameInstance::HandleNetworkFailure
        );

    if (!NetworkFailureDelegateHandle.IsValid())
    {
        UE_LOG(
            LogDMGameInstance,
            Error,
            TEXT(
                "Network failure handler registration failed: "
                "an invalid Delegate Handle was returned."
            )
        );

        return;
    }

    UE_LOG(
        LogDMGameInstance,
        Log,
        TEXT(
            "Network failure handler registered. "
            "GameInstance=%s"
        ),
        *GetNameSafe(this)
    );

    PostLoadMapDelegateHandle =
        FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
            this,
            &UDMGameInstance::HandlePostLoadMap
        );

    if (!PostLoadMapDelegateHandle.IsValid())
    {
        UE_LOG(
            LogDMGameInstance,
            Error,
            TEXT(
                "PostLoadMapWithWorld delegate "
                "registration failed."
            )
        );

        if (
            IsValid(GEngine)
            && NetworkFailureDelegateHandle.IsValid()
            )
        {
            GEngine->OnNetworkFailure().Remove(
                NetworkFailureDelegateHandle
            );

            NetworkFailureDelegateHandle.Reset();
        }

        return;
    }

    UE_LOG(
        LogDMGameInstance,
        Log,
        TEXT(
            "PostLoadMap handler registered. "
            "GameInstance=%s"
        ),
        *GetNameSafe(this)
    );


}

void UDMGameInstance::Shutdown()
{
    if (
        GEngine != nullptr
        && NetworkFailureDelegateHandle.IsValid()
        )
    {
        GEngine->OnNetworkFailure().Remove(
            NetworkFailureDelegateHandle
        );

        NetworkFailureDelegateHandle.Reset();

        UE_LOG(
            LogDMGameInstance,
            Log,
            TEXT(
                "Network failure handler unregistered. "
                "GameInstance=%s"
            ),
            *GetNameSafe(this)
        );
    }

    if (PostLoadMapDelegateHandle.IsValid())
    {
        FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(
            PostLoadMapDelegateHandle
        );

        PostLoadMapDelegateHandle.Reset();

        UE_LOG(
            LogDMGameInstance,
            Log,
            TEXT(
                "PostLoadMap handler unregistered. "
                "GameInstance=%s"
            ),
            *GetNameSafe(this)
        );
    }

    bHandlingNetworkFailure = false;

    Super::Shutdown();
}

void UDMGameInstance::HandleNetworkFailure(
    UWorld* World,
    UNetDriver* NetDriver,
    ENetworkFailure::Type FailureType,
    const FString& ErrorString
)
{
    /*
     * OnNetworkFailure는 GEngine 전역 Delegate다.
     *
     * Run Under One Process PIE에서는
     * 여러 GameInstance가 하나의 GEngine을 공유하므로
     * Failure가 발생한 World의 GameInstance인지
     * 반드시 검사해야 한다.
     */
    if (!IsValid(World))
    {
        return;
    }

    if (World->GetGameInstance() != this)
    {
        return;
    }

    /*
     * 오래된 World에서 발생한 Failure가
     * 현재 GameInstance의 새 World를 이동시키지 않도록 한다.
     */
    if (GetWorld() != World)
    {
        UE_LOG(
            LogDMGameInstance,
            Verbose,
            TEXT(
                "Network failure ignored: "
                "the failure belongs to a stale World. "
                "FailureWorld=%s, CurrentWorld=%s"
            ),
            *GetNameSafe(World),
            *GetNameSafe(GetWorld())
        );

        return;
    }

    /*
     * 이번 단계는 서버를 잃은 Client의 복구만 담당한다.
     *
     * Listen Server 또는 Standalone World에서 발생한
     * Network Failure는 Lobby 복귀 대상으로 처리하지 않는다.
     */
    if (World->GetNetMode() != NM_Client)
    {
        UE_LOG(
            LogDMGameInstance,
            Verbose,
            TEXT(
                "Network failure ignored for non-client World. "
                "World=%s, NetMode=%d, FailureType=%s"
            ),
            *GetNameSafe(World),
            static_cast<int32>(World->GetNetMode()),
            GetNetworkFailureName(FailureType)
        );

        return;
    }

    /*
     * ReturnToMainMenu 처리 중 다른 Network Failure가
     * 중첩 호출되는 것을 막는다.
     */
   

    if (bHandlingNetworkFailure)
    {
        UE_LOG(
            LogDMGameInstance,
            Log,
            TEXT(
                "Duplicate network failure ignored. "
                "FailureType=%s"
            ),
            GetNetworkFailureName(FailureType)
        );

        return;
    }

    bHandlingNetworkFailure = true;

    const bool bEstablishedConnectionEnded =
        FailureType == ENetworkFailure::ConnectionLost
        || FailureType == ENetworkFailure::ConnectionTimeout
        || FailureType == ENetworkFailure::FailureReceived;

    if (bEstablishedConnectionEnded)
    {
        UE_LOG(
            LogDMGameInstance,
            Warning,
            TEXT(
                "Connection to the Listen Server ended. "
                "World=%s, NetDriver=%s, "
                "FailureType=%s, Error=%s"
            ),
            *GetNameSafe(World),
            *GetNameSafe(NetDriver),
            GetNetworkFailureName(FailureType),
            *ErrorString
        );
    }
    else
    {
        UE_LOG(
            LogDMGameInstance,
            Warning,
            TEXT(
                "Client network failure detected. "
                "World=%s, NetDriver=%s, "
                "FailureType=%s, Error=%s"
            ),
            *GetNameSafe(World),
            *GetNameSafe(NetDriver),
            GetNetworkFailureName(FailureType),
            *ErrorString
        );
    }

    UE_LOG(
        LogDMGameInstance,
        Log,
        TEXT(
            "Returning the Client to the default map."
        )
    );

    /*
     * OnlineSession이 있으면 OnlineSession의
     * Disconnect 처리를 사용하고,
     * 없으면 Engine의 Disconnect 처리를 통해
     * Game Default Map으로 복귀한다.
     */
    ReturnToMainMenu();
}

const TCHAR* UDMGameInstance::GetNetworkFailureName(
    ENetworkFailure::Type FailureType
)
{
    switch (FailureType)
    {
    case ENetworkFailure::NetDriverAlreadyExists:
        return TEXT("NetDriverAlreadyExists");

    case ENetworkFailure::NetDriverCreateFailure:
        return TEXT("NetDriverCreateFailure");

    case ENetworkFailure::NetDriverListenFailure:
        return TEXT("NetDriverListenFailure");

    case ENetworkFailure::ConnectionLost:
        return TEXT("ConnectionLost");

    case ENetworkFailure::ConnectionTimeout:
        return TEXT("ConnectionTimeout");

    case ENetworkFailure::FailureReceived:
        return TEXT("FailureReceived");

    case ENetworkFailure::OutdatedClient:
        return TEXT("OutdatedClient");

    case ENetworkFailure::OutdatedServer:
        return TEXT("OutdatedServer");

    case ENetworkFailure::PendingConnectionFailure:
        return TEXT("PendingConnectionFailure");

    case ENetworkFailure::NetGuidMismatch:
        return TEXT("NetGuidMismatch");

    case ENetworkFailure::NetChecksumMismatch:
        return TEXT("NetChecksumMismatch");

    default:
        return TEXT("Unknown");
    }
}

void UDMGameInstance::HandlePostLoadMap(
    UWorld* LoadedWorld
)
{
    if (!bHandlingNetworkFailure)
    {
        return;
    }

    if (!IsValid(LoadedWorld))
    {
        return;
    }

    if (LoadedWorld->GetGameInstance() != this)
    {
        return;
    }

    bHandlingNetworkFailure = false;

    UE_LOG(
        LogDMGameInstance,
        Log,
        TEXT(
            "Network failure recovery completed. "
            "LoadedWorld=%s"
        ),
        *GetNameSafe(LoadedWorld)
    );
}