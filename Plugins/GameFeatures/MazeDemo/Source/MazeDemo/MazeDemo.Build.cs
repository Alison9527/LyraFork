// Copyright MazeDemo. All Rights Reserved.

using UnrealBuildTool;

public class MazeDemo : ModuleRules
{
	public MazeDemo(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		IncludeOrderVersion = EngineIncludeOrderVersion.Unreal5_7;

		PublicIncludePaths.AddRange(
			new string[] {
				// ... add public include paths required here ...
			}
			);

		PrivateIncludePaths.AddRange(
			new string[] {
				// ... add other private include paths required here ...
			}
			);

		PublicDependencyModuleNames.AddRange(
			new string[]
			{
				"Core",
			}
			);

		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				//"LyraGame",              // 第二步：接入 PawnData / AbilitySet 时启用
				//"ModularGameplayActors", // 第三步：AModularAIController 时启用
				//"NavigationSystem",      // 第四步：NavMesh 重建时启用
				//"AIModule",              // 第三步：AIController / 行为树时启用
				// ... add private dependencies that you statically link with here ...
			}
			);

		DynamicallyLoadedModuleNames.AddRange(
			new string[]
			{
				// ... add any modules that your module loads dynamically here ...
			}
			);
	}
}
