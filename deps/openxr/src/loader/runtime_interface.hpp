// Copyright (c) 2017-2026 The Khronos Group Inc.
// Copyright (c) 2017-2019 Valve Corporation
// Copyright (c) 2017-2019 LunarG, Inc.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Initial Author: Mark Young <marky@lunarg.com>
//

#pragma once

#include "loader_platform.hpp"

#include <openxr/openxr.h>

#include <string>
#include <vector>
#include <unordered_map>
#include <memory>

#include "loader_lockfree.h"

namespace Json {
class Value;
}

class RuntimeManifestFile;
struct XrGeneratedDispatchTableCore;

class RuntimeInterface {
   public:
    virtual ~RuntimeInterface();

    // Helper functions for loading and unloading the runtime (but only when necessary).
    // The runtime is published whole once built.  Only the holder of the
    // instance claim (ActiveLoaderInstance::Claim) unloads it;
    // a call that uses the runtime without that claim brackets the use with
    // BeginUse/EndUse and takes the runtime LoadRuntime hands back, which an
    // unload then retires rather than frees until no such call is running.
    static XrResult LoadRuntime(const std::string& openxr_command);
    static XrResult LoadRuntime(const std::string& openxr_command, RuntimeInterface** runtime);
    static void UnloadRuntime(const std::string& openxr_command);
    static RuntimeInterface& GetRuntime();
    static void BeginUse();
    static void EndUse();
    static XrResult GetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function);

    // Get the direct dispatch table to this runtime, without API layers or loader terminators.
    static const XrGeneratedDispatchTableCore* GetDispatchTable(XrInstance instance);
    static const XrGeneratedDispatchTableCore* GetDebugUtilsMessengerDispatchTable(XrDebugUtilsMessengerEXT messenger);

    void GetInstanceExtensionProperties(std::vector<XrExtensionProperties>& extension_properties);
    bool SupportsExtension(const std::string& extension_name);
    XrResult CreateInstance(const XrInstanceCreateInfo* info, XrInstance* instance);
    XrResult DestroyInstance(XrInstance instance);
    bool TrackDebugMessenger(XrInstance instance, XrDebugUtilsMessengerEXT messenger);
    void ForgetDebugMessenger(XrDebugUtilsMessengerEXT messenger);

    // No default construction
    RuntimeInterface() = delete;

    // Non-copyable
    RuntimeInterface(const RuntimeInterface&) = delete;
    RuntimeInterface& operator=(const RuntimeInterface&) = delete;

   private:
    RuntimeInterface(LoaderPlatformLibraryHandle runtime_library, PFN_xrGetInstanceProcAddr get_instance_proc_addr);
    void SetSupportedExtensions(std::vector<std::string>& supported_extensions);
    static XrResult TryLoadingSingleRuntime(const std::string& openxr_command, std::unique_ptr<RuntimeManifestFile>& manifest_file,
                                           RuntimeInterface** runtime);
    void PublishMessenger(XrDebugUtilsMessengerEXT messenger, XrInstance instance, bool track);

    LoaderPlatformLibraryHandle _runtime_library;
    PFN_xrGetInstanceProcAddr _get_instance_proc_addr;
    // The loader supports exactly one live runtime XrInstance, so the
    // dispatch table lives in a single slot (a heap RuntimeDispatchSlot,
    // defined in runtime_interface.cpp): CreateInstance publishes it with a
    // release-store and GetDispatchTable acquire-loads it.
    retro_atomic_ptr_t _dispatch_slot;
    // Messenger to instance, a published table: lookups walk it with no
    // lock, tracking replaces it by compare-exchange, and a replaced table
    // waits on the retire stack until the runtime goes.
    retro_atomic_ptr_t _messengers;
    mpsc_stack_t _retired_messengers;
    std::vector<std::string> _supported_extensions;
};
