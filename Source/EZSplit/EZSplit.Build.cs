// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class EZSplit : ModuleRules
{
	public EZSplit(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;
		
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
				"CoreUObject",
				"Engine",
				"InputCore",
				"UnrealEd",
                "Slate",
                "SlateCore",
                "ToolMenus", // Add this for ToolMenus functionality
				"LevelEditor", // For extending the Level Editor menus
                "AssetRegistry", // For managing assets
				"GeometryCore", // Mesh/geometry operations
				"MeshDescription", // Mesh description API
				"StaticMeshDescription", // For static mesh manipulation
				"MeshMergeUtilities" // For Merginf Static Mesh
            }
			);
			
		
		PrivateDependencyModuleNames.AddRange(
			new string[]
			{
				"CoreUObject",
				"Engine",
				"Slate",
				"SlateCore",
                "EditorStyle",
				"LevelEditor",
				"AssetTools"
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
