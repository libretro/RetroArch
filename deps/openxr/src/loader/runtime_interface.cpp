// Copyright (c) 2017-2026 The Khronos Group Inc.
// Copyright (c) 2017-2019 Valve Corporation
// Copyright (c) 2017-2019 LunarG, Inc.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Initial Author: Mark Young <marky@lunarg.com>
//

#include "runtime_interface.hpp"

#include <openxr/openxr.h>
#include <openxr/openxr_loader_negotiation.h>

#include "manifest_file.hpp"
#include "loader_init_data.hpp"
#include "loader_logger.hpp"
#include "loader_platform.hpp"
#include "loader_properties.hpp"
#include "xr_generated_dispatch_table_core.h"

// One live runtime XrInstance and its dispatch table; published as a whole
// through RuntimeInterface::_dispatch_slot.
struct RuntimeDispatchSlot {
    XrInstance instance;
    XrGeneratedDispatchTableCore table;
};

/* One messenger table entry. */
typedef struct loader_messenger_entry {
    XrDebugUtilsMessengerEXT messenger;
    XrInstance instance;
} loader_messenger_entry_t;

/* An unloaded runtime, waiting until no call can still hold it. */
typedef struct loader_runtime_retired {
    mpsc_stack_node_t node;
    RuntimeInterface* runtime;
} loader_runtime_retired_t;

/* The loaded runtime; calls using a runtime outside the instance claim;
 * unloaded runtimes not yet freed. */
static retro_atomic_ptr_t loader_runtime;
static retro_atomic_int_t loader_runtime_users;
static mpsc_stack_t loader_runtime_retired_stack;

/* Frees the retired runtimes once no call outside the instance claim is
 * running. The caller published its retirement before this: a call that
 * begins after the fence finds the runtime gone and loads its own, and one
 * that began before it is counted here, so the runtime it holds stays. */
static void loader_runtime_free_retired(void) {
    mpsc_stack_node_t* node;
    retro_atomic_thread_fence_seq_cst();
    if (retro_atomic_load_acquire_int(&loader_runtime_users) != 0) {
        return;
    }
    node = mpsc_stack_drain(&loader_runtime_retired_stack);
    while (node) {
        loader_runtime_retired_t* retired = (loader_runtime_retired_t*)node;
        node = node->next;
        delete retired->runtime;
        free(retired);
    }
}

/* At exit no call is running: the loaded runtime goes too. */
struct LoaderRuntimeExit {
    ~LoaderRuntimeExit() {
        delete (RuntimeInterface*)retro_atomic_exchange_ptr(&loader_runtime, NULL);
        loader_runtime_free_retired();
    }
};
static LoaderRuntimeExit loader_runtime_exit;

#include <cstring>
#include <memory>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

#if defined(XR_USE_PLATFORM_ANDROID) && defined(XR_HAS_REQUIRED_PLATFORM_LOADER_INIT_STRUCT)
#include "loader_json.hpp"

XrResult GetPlatformRuntimeVirtualManifest(Json::Value& out_manifest) {
    auto& initData = LoaderInitData::instance();
    if (!initData.initialized()) {
        return XR_ERROR_INITIALIZATION_FAILED;
    }
    jobject context = reinterpret_cast<jobject>(initData.getPlatformData().applicationContext);
    if (context == nullptr) {
        return XR_ERROR_INITIALIZATION_FAILED;
    }
    Json::Value virtualManifest;
    if (0 != openxr_android::getActiveRuntimeVirtualManifest(context, virtualManifest)) {
        return XR_ERROR_INITIALIZATION_FAILED;
    }
    out_manifest = virtualManifest;
    return XR_SUCCESS;
}
#endif  // defined(XR_USE_PLATFORM_ANDROID) && defined(XR_HAS_REQUIRED_PLATFORM_LOADER_INIT_STRUCT)

XrResult RuntimeInterface::TryLoadingSingleRuntime(const std::string& openxr_command,
                                                   std::unique_ptr<RuntimeManifestFile>& manifest_file, RuntimeInterface** runtime) {
    LoaderPlatformLibraryHandle runtime_library = LoaderPlatformLibraryOpen(manifest_file->LibraryPath());
    if (nullptr == runtime_library) {
        std::string library_message = LoaderPlatformLibraryOpenError(manifest_file->LibraryPath());
        std::string warning_message = "RuntimeInterface::LoadRuntime skipping manifest file ";
        warning_message += manifest_file->Filename();
        warning_message += ", failed to load with message \"";
        warning_message += library_message;
        warning_message += "\"";
        LoaderLogger::LogErrorMessage(openxr_command, warning_message);
        return XR_ERROR_FILE_ACCESS_ERROR;
    }

#if defined(XR_HAS_REQUIRED_PLATFORM_LOADER_INIT_STRUCT)
    if (!LoaderInitData::instance().initialized()) {
        LoaderLogger::LogErrorMessage(openxr_command, "RuntimeInterface::LoadRuntime skipping manifest file " +
                                                          manifest_file->Filename() +
                                                          " because xrInitializeLoaderKHR was not yet called.");

        LoaderPlatformLibraryClose(runtime_library);
        return XR_ERROR_VALIDATION_FAILURE;
    }
#endif  // defined(XR_HAS_REQUIRED_PLATFORM_LOADER_INIT_STRUCT)

    bool forwardedInitLoader = false;
    if (LoaderInitData::instance().initialized() && LoaderInitData::instance().getPlatformParam() != nullptr) {
        // If we have xrInitializeLoaderKHR exposed as an export, forward call to it.
        const auto function_name = manifest_file->GetFunctionName("xrInitializeLoaderKHR");
        auto initLoader =
            reinterpret_cast<PFN_xrInitializeLoaderKHR>(LoaderPlatformLibraryGetProcAddr(runtime_library, function_name));

        if (initLoader != nullptr) {
            // we found the entry point one way or another.
            LoaderLogger::LogInfoMessage(openxr_command,
                                         "RuntimeInterface::LoadRuntime forwarding xrInitializeLoaderKHR call to runtime before "
                                         "calling xrNegotiateLoaderRuntimeInterface.");
            XrResult res = initLoader(LoaderInitData::instance().getPlatformParam());
            if (!XR_SUCCEEDED(res)) {
                LoaderLogger::LogErrorMessage(openxr_command,
                                              "RuntimeInterface::LoadRuntime forwarded call to xrInitializeLoaderKHR failed.");

                LoaderPlatformLibraryClose(runtime_library);
                return res;
            }
            forwardedInitLoader = true;
        }
    }

    // Get and settle on an runtime interface version (using any provided name if required).
    std::string function_name = manifest_file->GetFunctionName("xrNegotiateLoaderRuntimeInterface");
    auto negotiate =
        reinterpret_cast<PFN_xrNegotiateLoaderRuntimeInterface>(LoaderPlatformLibraryGetProcAddr(runtime_library, function_name));

    // Loader info for negotiation
    XrNegotiateLoaderInfo loader_info = {};
    loader_info.structType = XR_LOADER_INTERFACE_STRUCT_LOADER_INFO;
    loader_info.structVersion = XR_LOADER_INFO_STRUCT_VERSION;
    loader_info.structSize = sizeof(XrNegotiateLoaderInfo);
    loader_info.minInterfaceVersion = 1;
    loader_info.maxInterfaceVersion = XR_CURRENT_LOADER_RUNTIME_VERSION;
    loader_info.minApiVersion = XR_MAKE_VERSION(1, 0, 0);
    loader_info.maxApiVersion = XR_MAKE_VERSION(1, 0x3ff, 0xfff);  // Maximum allowed version for this major version.

    // Set up the runtime return structure
    XrNegotiateRuntimeRequest runtime_info = {};
    runtime_info.structType = XR_LOADER_INTERFACE_STRUCT_RUNTIME_REQUEST;
    runtime_info.structVersion = XR_RUNTIME_INFO_STRUCT_VERSION;
    runtime_info.structSize = sizeof(XrNegotiateRuntimeRequest);

    // Skip calling the negotiate function and fail if the function pointer
    // could not get loaded
    XrResult res = XR_ERROR_RUNTIME_FAILURE;
    if (nullptr != negotiate) {
        res = negotiate(&loader_info, &runtime_info);
    } else {
        std::string error_message = "RuntimeInterface::LoadRuntime failed to find negotiate function ";
        error_message += function_name;
        LoaderLogger::LogErrorMessage(openxr_command, error_message);
    }
    // If we supposedly succeeded, but got a nullptr for GetInstanceProcAddr
    // then something still went wrong, so return with an error.
    if (XR_SUCCEEDED(res)) {
        uint32_t runtime_major = XR_VERSION_MAJOR(runtime_info.runtimeApiVersion);
        uint32_t runtime_minor = XR_VERSION_MINOR(runtime_info.runtimeApiVersion);
        uint32_t loader_major = XR_VERSION_MAJOR(XR_CURRENT_API_VERSION);
        if (nullptr == runtime_info.getInstanceProcAddr) {
            std::string error_message = "RuntimeInterface::LoadRuntime skipping manifest file ";
            error_message += manifest_file->Filename();
            error_message += ", negotiation succeeded but returned NULL getInstanceProcAddr";
            LoaderLogger::LogErrorMessage(openxr_command, error_message);
            res = XR_ERROR_FILE_CONTENTS_INVALID;
        } else if (0 >= runtime_info.runtimeInterfaceVersion ||
                   XR_CURRENT_LOADER_RUNTIME_VERSION < runtime_info.runtimeInterfaceVersion) {
            std::string error_message = "RuntimeInterface::LoadRuntime skipping manifest file ";
            error_message += manifest_file->Filename();
            error_message += ", negotiation succeeded but returned invalid interface version";
            LoaderLogger::LogErrorMessage(openxr_command, error_message);
            res = XR_ERROR_FILE_CONTENTS_INVALID;
        } else if (runtime_major != loader_major || (runtime_major == 0 && runtime_minor == 0)) {
            std::string error_message = "RuntimeInterface::LoadRuntime skipping manifest file ";
            error_message += manifest_file->Filename();
            error_message += ", OpenXR version returned not compatible with this loader";
            LoaderLogger::LogErrorMessage(openxr_command, error_message);
            res = XR_ERROR_FILE_CONTENTS_INVALID;
        }
    }

    if (XR_SUCCEEDED(res) && !forwardedInitLoader && LoaderInitData::instance().getPlatformParam() != nullptr) {
        // Forward initialize loader call, where possible and if we did not do so before.
        PFN_xrVoidFunction initializeVoid = nullptr;
        PFN_xrInitializeLoaderKHR initialize = nullptr;

        // xrInitializeLoaderKHR was not exposed as an export, so now we may try asking with xrGetInstanceProcAddr
        if (XR_SUCCEEDED(runtime_info.getInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR", &initializeVoid))) {
            if (initializeVoid == nullptr) {
                LoaderLogger::LogErrorMessage(openxr_command,
                                              "RuntimeInterface::LoadRuntime got success from xrGetInstanceProcAddr "
                                              "for xrInitializeLoaderKHR, but output a null pointer.");
                res = XR_ERROR_RUNTIME_FAILURE;
            } else {
                initialize = reinterpret_cast<PFN_xrInitializeLoaderKHR>(initializeVoid);
            }
        }
        if (initialize != nullptr) {
            // we found the entry point one way or another.
            LoaderLogger::LogInfoMessage(openxr_command,
                                         "RuntimeInterface::LoadRuntime forwarding xrInitializeLoaderKHR call to runtime after "
                                         "calling xrNegotiateLoaderRuntimeInterface.");
            res = initialize(LoaderInitData::instance().getPlatformParam());
            if (!XR_SUCCEEDED(res)) {
                LoaderLogger::LogErrorMessage(openxr_command,
                                              "RuntimeInterface::LoadRuntime forwarded call to xrInitializeLoaderKHR failed.");
            }
        }
    }

    if (XR_FAILED(res)) {
        std::string warning_message = "RuntimeInterface::LoadRuntime skipping manifest file ";
        warning_message += manifest_file->Filename();
        warning_message += ", negotiation failed with error ";
        warning_message += std::to_string(res);
        LoaderLogger::LogErrorMessage(openxr_command, warning_message);
        LoaderPlatformLibraryClose(runtime_library);
        return res;
    }

    std::string info_message = "RuntimeInterface::LoadRuntime succeeded loading runtime defined in manifest file ";
    info_message += manifest_file->Filename();
    info_message += " using interface version ";
    info_message += std::to_string(runtime_info.runtimeInterfaceVersion);
    info_message += " and OpenXR API version ";
    info_message += std::to_string(XR_VERSION_MAJOR(runtime_info.runtimeApiVersion));
    info_message += ".";
    info_message += std::to_string(XR_VERSION_MINOR(runtime_info.runtimeApiVersion));
    LoaderLogger::LogInfoMessage(openxr_command, info_message);

    // Build this runtime whole, with the list of extensions it supports for
    // easy filtering after the xrCreateInstance call, then publish it.
    RuntimeInterface* candidate = new RuntimeInterface(runtime_library, runtime_info.getInstanceProcAddr);
    std::vector<std::string> supported_extensions;
    std::vector<XrExtensionProperties> extension_properties;
    candidate->GetInstanceExtensionProperties(extension_properties);
    supported_extensions.reserve(extension_properties.size());
    for (const auto& ext_prop : extension_properties) {
        supported_extensions.emplace_back(ext_prop.extensionName);
    }
    candidate->SetSupportedExtensions(supported_extensions);

    /* A call that loaded one first wins; this one was never seen. */
    for (;;) {
        RuntimeInterface* published;
        if (retro_atomic_cas_ptr(&loader_runtime, NULL, candidate)) {
            *runtime = candidate;
            break;
        }
        if ((published = (RuntimeInterface*)retro_atomic_load_acquire_ptr(&loader_runtime))) {
            delete candidate;
            *runtime = published;
            break;
        }
    }

    return XR_SUCCESS;
}

XrResult RuntimeInterface::LoadRuntime(const std::string& openxr_command) {
    RuntimeInterface* runtime;
    return LoadRuntime(openxr_command, &runtime);
}

XrResult RuntimeInterface::LoadRuntime(const std::string& openxr_command, RuntimeInterface** runtime) {
    // If something's already loaded, we're done here.
    if ((*runtime = (RuntimeInterface*)retro_atomic_load_acquire_ptr(&loader_runtime))) {
        return XR_SUCCESS;
    }

#if defined(XR_HAS_REQUIRED_PLATFORM_LOADER_INIT_STRUCT)
    if (!LoaderInitData::instance().initialized()) {
        LoaderLogger::LogErrorMessage(
            openxr_command, "RuntimeInterface::LoadRuntime cannot run because xrInitializeLoaderKHR was not successfully called.");
        return XR_ERROR_INITIALIZATION_FAILED;
    }
#endif  // XR_HAS_REQUIRED_PLATFORM_LOADER_INIT_STRUCT

    std::vector<std::unique_ptr<RuntimeManifestFile>> runtime_manifest_files = {};

    // Find the available runtimes which we may need to report information for.
    XrResult last_error = RuntimeManifestFile::FindManifestFiles(openxr_command, runtime_manifest_files);
    if (XR_FAILED(last_error)) {
        LoaderLogger::LogErrorMessage(openxr_command, "RuntimeInterface::LoadRuntimes - unknown error");
    } else {
        last_error = XR_ERROR_RUNTIME_UNAVAILABLE;
        for (std::unique_ptr<RuntimeManifestFile>& manifest_file : runtime_manifest_files) {
            last_error = RuntimeInterface::TryLoadingSingleRuntime(openxr_command, manifest_file, runtime);
            if (XR_SUCCEEDED(last_error)) {
                break;
            }
        }
    }

    // Unsuccessful in loading any runtime, throw the runtime unavailable message.
    if (XR_FAILED(last_error)) {
        LoaderLogger::LogErrorMessage(openxr_command, "RuntimeInterface::LoadRuntimes - failed to load a runtime");
        last_error = XR_ERROR_RUNTIME_UNAVAILABLE;
    }

    return last_error;
}

void RuntimeInterface::UnloadRuntime(const std::string& openxr_command) {
    loader_runtime_retired_t* retired;
    RuntimeInterface* runtime = (RuntimeInterface*)retro_atomic_exchange_ptr(&loader_runtime, NULL);
    if (!runtime) {
        return;
    }
    LoaderLogger::LogInfoMessage(openxr_command, "RuntimeInterface::UnloadRuntime - Unloading RuntimeInterface");
    /* Out of memory, the runtime stays loaded rather than freed under a call. */
    if ((retired = (loader_runtime_retired_t*)malloc(sizeof(*retired)))) {
        retired->runtime = runtime;
        mpsc_stack_push(&loader_runtime_retired_stack, &retired->node);
    }
    loader_runtime_free_retired();
}

RuntimeInterface& RuntimeInterface::GetRuntime() { return *(RuntimeInterface*)retro_atomic_load_acquire_ptr(&loader_runtime); }

void RuntimeInterface::BeginUse() {
    retro_atomic_fetch_add_int(&loader_runtime_users, 1);
    retro_atomic_thread_fence_seq_cst();
}

void RuntimeInterface::EndUse() { retro_atomic_fetch_sub_int(&loader_runtime_users, 1); }

XrResult RuntimeInterface::GetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function) {
    return GetRuntime()._get_instance_proc_addr(instance, name, function);
}

const XrGeneratedDispatchTableCore* RuntimeInterface::GetDispatchTable(XrInstance instance) {
    const RuntimeDispatchSlot* slot =
        static_cast<const RuntimeDispatchSlot*>(retro_atomic_load_acquire_ptr(&GetRuntime()._dispatch_slot));
    if (slot && slot->instance == instance) {
        return &slot->table;
    }
    return nullptr;
}

const XrGeneratedDispatchTableCore* RuntimeInterface::GetDebugUtilsMessengerDispatchTable(XrDebugUtilsMessengerEXT messenger) {
    size_t i;
    XrInstance runtime_instance = XR_NULL_HANDLE;
    const loader_snapshot_t* table = (const loader_snapshot_t*)retro_atomic_load_acquire_ptr(&GetRuntime()._messengers);
    if (table) {
        const loader_messenger_entry_t* entries = LOADER_SNAPSHOT_ITEMS(table, const loader_messenger_entry_t);
        for (i = 0; i < table->count; i++) {
            if (entries[i].messenger == messenger) {
                runtime_instance = entries[i].instance;
                break;
            }
        }
    }
    return GetDispatchTable(runtime_instance);
}

RuntimeInterface::RuntimeInterface(LoaderPlatformLibraryHandle runtime_library, PFN_xrGetInstanceProcAddr get_instance_proc_addr)
    : _runtime_library(runtime_library), _get_instance_proc_addr(get_instance_proc_addr) {
    retro_atomic_ptr_init(&_dispatch_slot, NULL);
    retro_atomic_ptr_init(&_messengers, NULL);
    mpsc_stack_init(&_retired_messengers);
}

RuntimeInterface::~RuntimeInterface() {
    delete (RuntimeDispatchSlot*)retro_atomic_exchange_ptr(&_dispatch_slot, NULL);
    free(retro_atomic_exchange_ptr(&_messengers, NULL));
    loader_snapshot_free_retired(&_retired_messengers);
    LoaderPlatformLibraryClose(_runtime_library);
}

void RuntimeInterface::GetInstanceExtensionProperties(std::vector<XrExtensionProperties>& extension_properties) {
    std::vector<XrExtensionProperties> runtime_extension_properties;
    PFN_xrEnumerateInstanceExtensionProperties rt_xrEnumerateInstanceExtensionProperties;
    _get_instance_proc_addr(XR_NULL_HANDLE, "xrEnumerateInstanceExtensionProperties",
                            reinterpret_cast<PFN_xrVoidFunction*>(&rt_xrEnumerateInstanceExtensionProperties));
    uint32_t count = 0;
    uint32_t count_output = 0;
    // Get the count from the runtime
    rt_xrEnumerateInstanceExtensionProperties(nullptr, count, &count_output, nullptr);
    if (count_output > 0) {
        XrExtensionProperties example_properties{};
        example_properties.type = XR_TYPE_EXTENSION_PROPERTIES;
        runtime_extension_properties.resize(count_output, example_properties);
        count = count_output;
        rt_xrEnumerateInstanceExtensionProperties(nullptr, count, &count_output, runtime_extension_properties.data());
    }
    size_t ext_count = runtime_extension_properties.size();
    size_t props_count = extension_properties.size();
    for (size_t ext = 0; ext < ext_count; ++ext) {
        bool found = false;
        for (size_t prop = 0; prop < props_count; ++prop) {
            // If we find it, then make sure the spec version matches that of the runtime instead of the
            // layer.
            if (strcmp(extension_properties[prop].extensionName, runtime_extension_properties[ext].extensionName) == 0) {
                // Make sure the spec version used is the runtime's
                extension_properties[prop].extensionVersion = runtime_extension_properties[ext].extensionVersion;
                found = true;
                break;
            }
        }
        if (!found) {
            extension_properties.push_back(runtime_extension_properties[ext]);
        }
    }
}

XrResult RuntimeInterface::CreateInstance(const XrInstanceCreateInfo* info, XrInstance* instance) {
    XrResult res = XR_SUCCESS;
    bool create_succeeded = false;
    PFN_xrCreateInstance rt_xrCreateInstance;
    _get_instance_proc_addr(XR_NULL_HANDLE, "xrCreateInstance", reinterpret_cast<PFN_xrVoidFunction*>(&rt_xrCreateInstance));
    res = rt_xrCreateInstance(info, instance);
    if (XR_SUCCEEDED(res)) {
        create_succeeded = true;
        RuntimeDispatchSlot* slot = new RuntimeDispatchSlot();
        slot->instance = *instance;
        GeneratedXrPopulateDispatchTableCore(&slot->table, *instance, _get_instance_proc_addr);
        // The loader enforces a single live instance, so the previous slot is
        // always null here; swap just in case and drop any stale one.
        RuntimeDispatchSlot* previous =
            static_cast<RuntimeDispatchSlot*>(retro_atomic_exchange_ptr(&_dispatch_slot, slot));
        delete previous;
    }

    // If the failure occurred during the populate, clean up the instance we had picked up from the runtime
    if (XR_FAILED(res) && create_succeeded) {
        PFN_xrDestroyInstance rt_xrDestroyInstance;
        _get_instance_proc_addr(*instance, "xrDestroyInstance", reinterpret_cast<PFN_xrVoidFunction*>(&rt_xrDestroyInstance));
        rt_xrDestroyInstance(*instance);
        *instance = XR_NULL_HANDLE;
    }

    return res;
}

XrResult RuntimeInterface::DestroyInstance(XrInstance instance) {
    if (XR_NULL_HANDLE != instance) {
        // Unpublish the dispatch table for this instance first, then delete
        // it only after the runtime's xrDestroyInstance returns; destroying
        // an instance is externally synchronized against its other uses, so
        // no lookup can still be handing the table out at that point.
        RuntimeDispatchSlot* slot =
            static_cast<RuntimeDispatchSlot*>(retro_atomic_exchange_ptr(&_dispatch_slot, NULL));
        if (slot && slot->instance != instance) {
            // Not ours (should never happen): put it back.
            retro_atomic_store_release_ptr(&_dispatch_slot, slot);
            slot = NULL;
        }
        // Now delete the instance
        PFN_xrDestroyInstance rt_xrDestroyInstance;
        _get_instance_proc_addr(instance, "xrDestroyInstance", reinterpret_cast<PFN_xrVoidFunction*>(&rt_xrDestroyInstance));
        rt_xrDestroyInstance(instance);
        delete slot;
    }
    return XR_SUCCESS;
}

// The current table less messenger, plus messenger -> instance when
// tracking, published in one compare-exchange.
void RuntimeInterface::PublishMessenger(XrDebugUtilsMessengerEXT messenger, XrInstance instance, bool track) {
    for (;;) {
        size_t i;
        size_t count = 0;
        size_t found = 0;
        const loader_messenger_entry_t* entries = NULL;
        loader_messenger_entry_t* next_entries;
        loader_snapshot_t* next;
        loader_snapshot_t* current = (loader_snapshot_t*)retro_atomic_load_acquire_ptr(&_messengers);

        if (current) {
            count = current->count;
            entries = LOADER_SNAPSHOT_ITEMS(current, const loader_messenger_entry_t);
        }
        for (i = 0; i < count; i++) {
            if (entries[i].messenger == messenger) {
                found++;
            }
        }
        if (!track && !found) {
            return;
        }
        if (!(next = loader_snapshot_new(count - found + (track ? 1 : 0), sizeof(loader_messenger_entry_t), 0))) {
            return;
        }
        next_entries = LOADER_SNAPSHOT_ITEMS(next, loader_messenger_entry_t);
        for (i = 0; i < count; i++) {
            if (entries[i].messenger != messenger) {
                *next_entries++ = entries[i];
            }
        }
        if (track) {
            next_entries->messenger = messenger;
            next_entries->instance = instance;
        }

        if (retro_atomic_cas_ptr(&_messengers, current, next)) {
            if (current) {
                mpsc_stack_push(&_retired_messengers, &current->retire.node);
            }
            return;
        }
        free(next);
    }
}

bool RuntimeInterface::TrackDebugMessenger(XrInstance instance, XrDebugUtilsMessengerEXT messenger) {
    PublishMessenger(messenger, instance, true);
    return true;
}

void RuntimeInterface::ForgetDebugMessenger(XrDebugUtilsMessengerEXT messenger) {
    if (XR_NULL_HANDLE != messenger) {
        PublishMessenger(messenger, XR_NULL_HANDLE, false);
    }
}

void RuntimeInterface::SetSupportedExtensions(std::vector<std::string>& supported_extensions) {
    _supported_extensions = supported_extensions;
}

bool RuntimeInterface::SupportsExtension(const std::string& extension_name) {
    bool found_prop = false;
    for (const std::string& supported_extension : _supported_extensions) {
        if (supported_extension == extension_name) {
            found_prop = true;
            break;
        }
    }
    return found_prop;
}
