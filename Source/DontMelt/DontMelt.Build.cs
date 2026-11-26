
using UnrealBuildTool;

public class DontMelt : ModuleRules
{
    public DontMelt(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "CoreUObject",
                "Engine",
                "InputCore",
                "EnhancedInput",
                "AIModule",
                "NavigationSystem",
                "NetCore",
                "UMG",
                "SlateCore",
                "GameplayTags",
                "OnlineSubsystem",
                "CoreOnline",
                "Niagara"
            }
        );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "EngineSettings",
                "OnlineSubsystemUtils",
                "Slate"
            }
        );

        // PIE 시작 매개변수와 Editor 설정 접근은 Editor 타깃에만 포함한다.
        if (Target.bBuildEditor)
        {
            PrivateDependencyModuleNames.Add("UnrealEd");
        }

        // Steam 온라인 서브시스템을 동적으로 로드한다.
        DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");

        // Windows에서 Steam Overlay 사용 가능 여부를 확인한다.
        if (Target.Platform == UnrealTargetPlatform.Win64)
        {
            PrivateDependencyModuleNames.Add("Steamworks");
        }
    }
}
