using UnrealBuildTool;

public class ProjectArcaneArenaEditor : ModuleRules
{
	public ProjectArcaneArenaEditor(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PrivateDependencyModuleNames.AddRange(new string[]
		{
			"Core",
			"CoreUObject",
			"Engine",
			"UnrealEd",
			"GameplayAbilities",
			"GameplayTags",
			"Niagara",
			"NiagaraEditor"
		});
	}
}
