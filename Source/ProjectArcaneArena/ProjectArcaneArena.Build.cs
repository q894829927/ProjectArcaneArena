using UnrealBuildTool;

public class ProjectArcaneArena : ModuleRules
{
	public ProjectArcaneArena(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"InputCore",
			"EnhancedInput",
			"GameplayAbilities",
			"GameplayTags",
			"GameplayTasks",
			"AIModule",
			"UMG",
			"MassEntity"
		});

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Niagara",
			"NavigationSystem"
		});
	}
}
