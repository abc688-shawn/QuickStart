// Copyright Epic Games, Inc. All Rights Reserved.

using System.IO;
using UnrealBuildTool;

public class QuickStart : ModuleRules
{
	public QuickStart(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
	
		PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "Sockets", "Networking", 
            "PhysicsCore", "OpenCV","OpenCVHelper","RenderCore", "RHI" });
        //"OpenCV", "OpenCVHelper",

        PrivateDependencyModuleNames.AddRange(new string[] { "UMG", "Slate", "SlateCore" });

        string OpenCVPath = "C:\\Users\\lhs\\Downloads\\opencv\\build";
        // Add OpenCV include path
        PublicIncludePaths.Add(Path.Combine(OpenCVPath, "include"));

        // Add OpenCV library path using PublicSystemLibraryPaths
        PublicSystemLibraryPaths.Add(Path.Combine(OpenCVPath, "x64/vc16/lib"));

        // Add OpenCV library file
        PublicAdditionalLibraries.Add(Path.Combine(OpenCVPath, "x64/vc16/lib", "opencv_world490.lib"));

        // Uncomment if you are using Slate UI
        // PrivateDependencyModuleNames.AddRange(new string[] { "Slate", "SlateCore" });

        // Uncomment if you are using online features
        // PrivateDependencyModuleNames.Add("OnlineSubsystem");

        // To include OnlineSubsystemSteam, add it to the plugins section in your uproject file with the Enabled attribute set to true
    }
}
