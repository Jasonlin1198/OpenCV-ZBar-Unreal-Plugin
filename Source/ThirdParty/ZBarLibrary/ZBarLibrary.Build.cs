// Fill out your copyright notice in the Description page of Project Settings.

using System.IO;
using UnrealBuildTool;

public class ZBarLibrary : ModuleRules
{
	public ZBarLibrary(ReadOnlyTargetRules Target) : base(Target)
	{
		PCHUsage = ModuleRules.PCHUsageMode.UseExplicitOrSharedPCHs;

		// Tell Unreal that this Module only imports Third-Party-Assets
		Type = ModuleType.External;

		LoadZBarLibrary(Target);
	}

	public bool LoadZBarLibrary(ReadOnlyTargetRules Target)
    {
		bool isLibrarySupported = false;
		string platformString = null;

		// Check which platform Unreal is built for
		if (Target.Platform == UnrealTargetPlatform.Win64)
		{
			platformString = "x64";
		}
		else if (Target.Platform == UnrealTargetPlatform.HoloLens)
		{ 
			platformString = "ARM64";
		}

		// Add dependencies if current platform is supported
		if (platformString != null)
        {
			isLibrarySupported = true;

			// Add the static import library 
			PublicAdditionalLibraries.Add(Path.Combine(ModulePath, "lib", "libzbar64-0.lib"));

			// Add include directory path
			PublicIncludePaths.Add(Path.Combine(ModulePath, "include"));

			// Delay-load the DLL, so we can load it from the right place first
			PublicDelayLoadDLLs.Add("libconv.dll");
		}

		return isLibrarySupported;

	}

	// ModuleDirectory points to the directory .uplugin is located
	private string ModulePath
	{
		get { return ModuleDirectory; }
	}

}
