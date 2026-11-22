#include "Development/DMDevPIEMatch.h"

#include "Engine/World.h"
#include "Kismet/GameplayStatics.h"

#if WITH_EDITOR && !UE_BUILD_SHIPPING
#include "Settings/LevelEditorPlaySettings.h"
#include "Editor/EditorEngine.h"
#include "Engine/Engine.h"
#endif

namespace DMDevPIEMatch
{
    bool IsEnabled(const UWorld* World, const FString& Options)
    {
#if WITH_EDITOR && !UE_BUILD_SHIPPING
        return World != nullptr && World->WorldType == EWorldType::PIE
            && World->GetNetMode() == NM_ListenServer
            && UWorld::RemovePIEPrefix(World->GetPackage()->GetName()) == TEXT("/Game/_DontMelt/Maps/Test/MatchMap_T")
            && UGameplayStatics::ParseOption(Options, TEXT("DMDevPIE")) == TEXT("1")
            && UGameplayStatics::ParseOption(Options, TEXT("ExpectedPlayers")) == TEXT("2");
#else
        return false;
#endif
    }

    bool IsLoopbackAddress(const FString& Address)
    {
        // 최초 지원 대상은 엔진이 사용하는 IPv4 루프백 접속으로 제한한다.
        return Address == TEXT("127.0.0.1") || Address.StartsWith(TEXT("127.0.0.1:"));
    }

#if WITH_EDITOR && !UE_BUILD_SHIPPING
    bool BuildStartParameters(const UWorld* World, const FGameInstancePIEParameters& InParams,
        FGameInstancePIEParameters& OutParams, FString& OutError)
    {
        OutParams = InParams;
        if (World == nullptr || World->WorldType != EWorldType::PIE || InParams.EditorPlaySettings == nullptr)
        {
            return true;
        }

        FString Options;
        InParams.EditorPlaySettings->GetAdditionalServerGameOptions(Options);
        if (!UGameplayStatics::HasOption(Options, TEXT("DMDevPIE")))
        {
            return true;
        }
        // 클라이언트는 엔진의 정상 접속 경로를 그대로 사용한다.
        if (InParams.NetMode == PIE_Client)
        {
            return true;
        }

        bool bOneProcess = false;
        int32 Players = 0;
        InParams.EditorPlaySettings->GetRunUnderOneProcess(bOneProcess);
        InParams.EditorPlaySettings->GetPlayNumberOfClients(Players);
        if (InParams.NetMode != PIE_ListenServer || InParams.bRunAsDedicated
            || InParams.bSimulateInEditor || InParams.bStartInSpectatorMode || !bOneProcess || Players != 2
            || !IsEnabled(World, Options))
        {
            OutError = TEXT("DMDevPIE requires MatchMap_T, Listen Server, two players, one process, DMDevPIE=1 and ExpectedPlayers=2.");
            return false;
        }

        UEditorEngine* Editor = Cast<UEditorEngine>(GEngine);
        if (Editor == nullptr || !Editor->UserEditedPlayWorldURL.IsEmpty())
        {
            OutError = TEXT("DMDevPIE requires the normal PIE URL path; clear the custom Play World URL first.");
            return false;
        }

        // UE5.5.4의 단일 프로세스 경로는 Server Game Options를 URL에 붙이지 않는다.
        // 월드별 시작 매개변수만 복사하며 전역 Editor 설정과 에셋은 변경하지 않는다.
        if (OutParams.OverrideMapURL.IsEmpty())
        {
            OutParams.OverrideMapURL = Editor->BuildPlayWorldURL(*World->GetPackage()->GetName(), false);
        }
        OutParams.OverrideMapURL += Options;
        return true;
    }
#endif
}
