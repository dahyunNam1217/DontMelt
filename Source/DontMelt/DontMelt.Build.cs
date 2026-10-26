
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
                "OnlineSubsystemUtils"
            }
        );

        // Steam 온라인 서브시스템을 동적으로 로드한다.
        DynamicallyLoadedModuleNames.Add("OnlineSubsystemSteam");
    }
}
