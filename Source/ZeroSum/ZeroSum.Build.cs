// Copyright Epic Games, Inc. All Rights Reserved.

using UnrealBuildTool;

public class ZeroSum : ModuleRules
{
    public ZeroSum(ReadOnlyTargetRules Target) : base(Target)
    {
        PCHUsage = PCHUsageMode.UseExplicitOrSharedPCHs;
         
        PublicDependencyModuleNames.AddRange(new string[] { "Core", "CoreUObject", "Engine", "InputCore", "EnhancedInput", "OnlineSubsystem", "OnlineSubsystemUtils", "FPSCore" });

        PrivateDependencyModuleNames.AddRange(new string[] { "OnlineSubsystemNull" });
    }
}