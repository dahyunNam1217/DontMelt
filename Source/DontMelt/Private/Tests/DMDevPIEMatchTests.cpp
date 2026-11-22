#include "Development/DMDevPIEMatch.h"
#include "Misc/AutomationTest.h"

#if WITH_EDITOR && WITH_DEV_AUTOMATION_TESTS && !UE_BUILD_SHIPPING
#include "Engine/World.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "Settings/LevelEditorPlaySettings.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FDMDevPIEMatchIsolationTest,
    "DontMelt.Development.PIE.Isolation", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FDMDevPIEMatchIsolationTest::RunTest(const FString& Parameters)
{
    // 실제 PIE 실행이 아닌 정책 검사다. 게임 초기화·복제 통과 근거로 사용하지 않는다.
    UPackage* Package = CreatePackage(TEXT("/Game/_DontMelt/Maps/Test/UEDPIE_99_MatchMap_T"));
    UWorld* World = NewObject<UWorld>(Package);
    World->WorldType = EWorldType::PIE;
    World->SetPlayInEditorInitialNetMode(NM_ListenServer);
    const FString Options = TEXT("?DMDevPIE=1?ExpectedPlayers=2");
    TestTrue(TEXT("Explicit two-player Listen PIE is eligible"), DMDevPIEMatch::IsEnabled(World, Options));
    TestFalse(TEXT("No explicit switch"), DMDevPIEMatch::IsEnabled(World, TEXT("?ExpectedPlayers=2")));
    TestFalse(TEXT("Disabled switch"), DMDevPIEMatch::IsEnabled(World, TEXT("?DMDevPIE=0?ExpectedPlayers=2")));
    TestFalse(TEXT("Missing expected count"), DMDevPIEMatch::IsEnabled(World, TEXT("?DMDevPIE=1")));
    TestFalse(TEXT("Three-player not supported yet"), DMDevPIEMatch::IsEnabled(World, TEXT("?DMDevPIE=1?ExpectedPlayers=3")));
    World->WorldType = EWorldType::Game;
    TestFalse(TEXT("Ordinary game cannot activate"), DMDevPIEMatch::IsEnabled(World, Options));
    World->WorldType = EWorldType::Editor;
    TestFalse(TEXT("Editor world cannot activate"), DMDevPIEMatch::IsEnabled(World, Options));
    World->WorldType = EWorldType::PIE;
    for (const ENetMode Mode : {NM_Standalone, NM_Client, NM_DedicatedServer})
    {
        World->SetPlayInEditorInitialNetMode(Mode);
        TestFalse(TEXT("Only Listen Server may activate"), DMDevPIEMatch::IsEnabled(World, Options));
    }
    World->SetPlayInEditorInitialNetMode(NM_ListenServer);
    TestFalse(TEXT("Null world"), DMDevPIEMatch::IsEnabled(nullptr, Options));
    TestTrue(TEXT("Engine loopback"), DMDevPIEMatch::IsLoopbackAddress(TEXT("127.0.0.1:17777")));
    TestFalse(TEXT("External address rejected"), DMDevPIEMatch::IsLoopbackAddress(TEXT("192.168.0.2:17777")));
    TestFalse(TEXT("Loopback prefix spoof rejected"), DMDevPIEMatch::IsLoopbackAddress(TEXT("127.0.0.10:17777")));

    ULevelEditorPlaySettings* Settings = NewObject<ULevelEditorPlaySettings>();
    Settings->SetRunUnderOneProcess(true);
    Settings->SetPlayNumberOfClients(2);
    // private 설정은 테스트 전용 객체의 반사 속성으로만 주입한다. 저장·기본 객체 수정은 하지 않는다.
    const FStrProperty* OptionsProperty = FindFProperty<FStrProperty>(
        ULevelEditorPlaySettings::StaticClass(), TEXT("AdditionalServerGameOptions"));
    if (!TestNotNull(TEXT("Editor server options property exists"), OptionsProperty))
    {
        World->MarkAsGarbage();
        return false;
    }
    OptionsProperty->SetPropertyValue_InContainer(Settings, Options);
    FGameInstancePIEParameters In;
    In.EditorPlaySettings = Settings;
    In.NetMode = PIE_ListenServer;
    In.bRunAsDedicated = false;
    In.bSimulateInEditor = false;
    In.bStartInSpectatorMode = false;
    FGameInstancePIEParameters Out;
    FString Error;
    TestTrue(TEXT("Supported start parameters accepted"), DMDevPIEMatch::BuildStartParameters(World, In, Out, Error));
    TestTrue(TEXT("Server options copied into per-world URL"), Out.OverrideMapURL.EndsWith(Options));
    TestTrue(TEXT("Original parameters unchanged"), In.OverrideMapURL.IsEmpty());
    Settings->SetRunUnderOneProcess(false);
    TestFalse(TEXT("Separate process rejected"), DMDevPIEMatch::BuildStartParameters(World, In, Out, Error));
    Settings->SetRunUnderOneProcess(true);
    Settings->SetPlayNumberOfClients(3);
    TestFalse(TEXT("Three-player editor setting rejected"), DMDevPIEMatch::BuildStartParameters(World, In, Out, Error));
    Settings->SetPlayNumberOfClients(2);
    In.bRunAsDedicated = true;
    TestFalse(TEXT("Dedicated rejected"), DMDevPIEMatch::BuildStartParameters(World, In, Out, Error));
    In.bRunAsDedicated = false;
    In.NetMode = PIE_Client;
    TestTrue(TEXT("Client uses unchanged engine path"), DMDevPIEMatch::BuildStartParameters(World, In, Out, Error));
    TestTrue(TEXT("Client receives no server URL options"), Out.OverrideMapURL.IsEmpty());
    World->MarkAsGarbage();
    return true;
}
#endif
