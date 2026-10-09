// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class SoulsNew : ModuleRules
{
	public SoulsNew(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;

		PublicDependencyModuleNames.AddRange(new string[] {
			"Core",
			"CoreUObject",
			"Engine",
			"GameplayTags",
			"InputCore",
			"EnhancedInput",
			"AIModule",
			"StateTreeModule",
			"GameplayStateTreeModule",
			"UMG",
			"Slate"
		});

		PrivateDependencyModuleNames.AddRange(new string[] { });

		PublicIncludePaths.AddRange(new string[] {
			"SoulsNew",
			"SoulsNew/Variant_Platforming",
			"SoulsNew/Variant_Platforming/Animation",
			"SoulsNew/Variant_Combat",
			"SoulsNew/Variant_Combat/AI",
			"SoulsNew/Variant_Combat/Animation",
			"SoulsNew/Variant_Combat/Gameplay",
			"SoulsNew/Variant_Combat/Interfaces",
			"SoulsNew/Variant_Combat/UI",
			"SoulsNew/Variant_SideScrolling",
			"SoulsNew/Variant_SideScrolling/AI",
			"SoulsNew/Variant_SideScrolling/Gameplay",
			"SoulsNew/Variant_SideScrolling/Interfaces",
			"SoulsNew/Variant_SideScrolling/UI"
		});

		// Uncomment if you are using Slate UI
		// PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

		// Uncomment if you are using online features
		// PrivateDependencyModuleNames.Add("OnlineSubsystem");

		// To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
	}
}
