// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;
using System.IO;

public class DiscordRpc : ModuleRules
{
#if WITH_FORWARDED_MODULE_RULES_CTOR
    public DiscordRpc(ReadOnlyTargetRules Target) : base(Target)
#else
    public DiscordRpc(TargetInfo Target)
#endif
    {
        Definitions.Add("DISCORD_DYNAMIC_LIB=1");

        PublicIncludePaths.AddRange(
            new string[] {
                "DiscordRpc/Public"
            }
            );

        PrivateIncludePaths.AddRange(
            new string[] {
                "DiscordRpc/Private"
            }
            );

        PublicDependencyModuleNames.AddRange(
            new string[]
            {
                "Core",
                "DiscordRpcLibrary"
            }
            );

        PrivateDependencyModuleNames.AddRange(
            new string[]
            {
                "CoreUObject",
                "Engine",
                "Slate",
                "SlateCore",
                "Projects"
            }
            );

        DynamicallyLoadedModuleNames.AddRange(
            new string[]
            {
                // ... add any modules that your module loads dynamically here ...
            }
            );

        string BaseDirectory = Path.GetFullPath(Path.Combine(ModuleDirectory, "..", "..", "Source", "ThirdParty", "DiscordRpcLibrary"));
        PublicIncludePaths.Add(Path.Combine(BaseDirectory, "Include"));
    }
}