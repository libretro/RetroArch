// Copyright 1998-2017 Epic Games, Inc. All Rights Reserved.

#include "DiscordRpcPrivatePCH.h"
#include "IPluginManager.h"
#include "ModuleManager.h"

#define LOCTEXT_NAMESPACE "FDiscordRpcModule"

void FDiscordRpcModule::StartupModule()
{
#if !PLATFORM_LINUX
#if defined(DISCORD_DYNAMIC_LIB)
    // Get the base directory of this plugin
    FString BaseDir = IPluginManager::Get().FindPlugin("DiscordRpc")->GetBaseDir();
    const FString SDKDir =
      FPaths::Combine(*BaseDir, TEXT("Source"), TEXT("ThirdParty"), TEXT("DiscordRpcLibrary"));
#if PLATFORM_WINDOWS
    const FString LibName = TEXT("discord-rpc");
    const FString LibDir = FPaths::Combine(*SDKDir, TEXT("Win64"));
    if (!LoadDependency(LibDir, LibName, DiscordRpcLibraryHandle)) {
        FMessageDialog::Open(
          EAppMsgType::Ok,
          LOCTEXT(LOCTEXT_NAMESPACE,
                  "Failed to load DiscordRpc plugin. Plug-in will not be functional."));
        FreeDependency(DiscordRpcLibraryHandle);
    }
#elif PLATFORM_MAC
    const FString LibName = TEXT("libdiscord-rpc");
    const FString LibDir = FPaths::Combine(*SDKDir, TEXT("Mac"));
    if (!LoadDependency(LibDir, LibName, DiscordRpcLibraryHandle)) {
        FMessageDialog::Open(
          EAppMsgType::Ok,
          LOCTEXT(LOCTEXT_NAMESPACE,
                  "Failed to load DiscordRpc plugin. Plug-in will not be functional."));
        FreeDependency(DiscordRpcLibraryHandle);
    }
#endif
#endif
#endif
}

void FDiscordRpcModule::ShutdownModule()
{
    // Free the dll handle
#if !PLATFORM_LINUX
#if defined(DISCORD_DYNAMIC_LIB)
    FreeDependency(DiscordRpcLibraryHandle);
#endif
#endif
}

bool FDiscordRpcModule::LoadDependency(const FString& Dir, const FString& Name, void*& Handle)
{
    FString Lib = Name + TEXT(".") + FPlatformProcess::GetModuleExtension();
    FString Path = Dir.IsEmpty() ? *Lib : FPaths::Combine(*Dir, *Lib);

    Handle = FPlatformProcess::GetDllHandle(*Path);

    if (Handle == nullptr) {
        return false;
    }

    return true;
}

void FDiscordRpcModule::FreeDependency(void*& Handle)
{
    if (Handle != nullptr) {
        FPlatformProcess::FreeDllHandle(Handle);
        Handle = nullptr;
    }
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FDiscordRpcModule, DiscordRpc)