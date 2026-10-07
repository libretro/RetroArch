// Copyright (c) 2017-2026 The Khronos Group Inc.
// Copyright (c) 2017-2019 Valve Corporation
// Copyright (c) 2017-2019 LunarG, Inc.
//
// SPDX-License-Identifier: Apache-2.0 OR MIT
//
// Initial Author: Mark Young <marky@lunarg.com>
//

#include "loader_logger.hpp"
#include "runtime_interface.hpp"
#include "loader_instance.hpp"
#include "loader_init_data.hpp"
#ifdef XR_USE_PLATFORM_ANDROID
#include <jni/rjni.h>
#endif
#include "loader_properties.hpp"

XrResult LoaderInitData::initialize(const XrLoaderInitInfoBaseHeaderKHR* info) {
    // We iterate the chain per struct type, so we only pick the first of each type in the chain.

    XrResult result = initializeProperties(info);
    if (result != XR_SUCCESS) {
        return result;
    }

#if defined(XR_HAS_REQUIRED_PLATFORM_LOADER_INIT_STRUCT)
    result = initializePlatform(info);
    if (result != XR_SUCCESS) {
        return result;
    }
#endif  // defined(XR_HAS_REQUIRED_PLATFORM_LOADER_INIT_STRUCT)

    _initialized = true;
    return XR_SUCCESS;
}

XrResult LoaderInitData::initializeProperties(const XrLoaderInitInfoBaseHeaderKHR* info) {
    while (info != nullptr) {
        if (info->type == XR_TYPE_LOADER_INIT_INFO_PROPERTIES_EXT) {
            const auto* propertyInfo = reinterpret_cast<XrLoaderInitInfoPropertiesEXT const*>(info);

            // Validate the inputs first.
            for (uint32_t i = 0; i < propertyInfo->propertyValueCount; i++) {
                if (propertyInfo->propertyValues[i].name == nullptr) {
                    return XR_ERROR_VALIDATION_FAILURE;
                }
                if (propertyInfo->propertyValues[i].value == nullptr) {
                    return XR_ERROR_VALIDATION_FAILURE;
                }
                if (propertyInfo->propertyValues[i].name[0] == '\0') {
                    return XR_ERROR_VALIDATION_FAILURE;
                }
            }

            // Inject provided properties into the loader property store.
            LoaderProperty::ClearOverrides();
            for (uint32_t i = 0; i < propertyInfo->propertyValueCount; i++) {
                LoaderProperty::SetOverride(propertyInfo->propertyValues[i].name, propertyInfo->propertyValues[i].value);
            }
            // Take only the first such struct.
            return XR_SUCCESS;
        }
        info = reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(info->next);
    }

    // fine if we don't find this.
    return XR_SUCCESS;
}

#if defined(XR_USE_PLATFORM_ANDROID) && defined(XR_HAS_REQUIRED_PLATFORM_LOADER_INIT_STRUCT)
XrResult LoaderInitData::initializePlatform(const XrLoaderInitInfoBaseHeaderKHR* info) {
    // Check and copy the Android-specific init data.
    while (info != nullptr) {
        if (info->type == XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR) {
            auto cast_info = reinterpret_cast<XrLoaderInitInfoAndroidKHR const*>(info);

            if (cast_info->applicationVM == nullptr) {
                return XR_ERROR_VALIDATION_FAILURE;
            }
            if (cast_info->applicationContext == nullptr) {
                return XR_ERROR_VALIDATION_FAILURE;
            }

            // Copy and store the JVM pointer and Android Context, ensuring the JVM is initialised.
            _platform_info = *cast_info;
            _platform_info.next = nullptr;  // Not safe to store next pointer since the memory may not exist later.

            // Raw JNI through rjni; one-shot lookups, so no caches needed.
            rjni_set_vm(static_cast<JavaVM*>(_platform_info.applicationVM));
            JNIEnv* env = rjni_env();
            jobject context = static_cast<jobject>(_platform_info.applicationContext);
            if (env == nullptr) {
                return XR_ERROR_INITIALIZATION_FAILED;
            }

            jclass context_class = env->GetObjectClass(context);
            jmethodID get_assets = env->GetMethodID(context_class, "getAssets", "()Landroid/content/res/AssetManager;");
            jmethodID get_app_info =
                env->GetMethodID(context_class, "getApplicationInfo", "()Landroid/content/pm/ApplicationInfo;");
            if (rjni_exception_clear(env) || get_assets == nullptr || get_app_info == nullptr) {
                env->DeleteLocalRef(context_class);
                return XR_ERROR_INITIALIZATION_FAILED;
            }

            // Retrieve a reference to the Android AssetManager.
            jobject asset_manager = env->CallObjectMethod(context, get_assets);
            if (rjni_exception_clear(env) || asset_manager == nullptr) {
                env->DeleteLocalRef(context_class);
                return XR_ERROR_INITIALIZATION_FAILED;
            }
            _android_asset_manager = AAssetManager_fromJava(env, asset_manager);
            env->DeleteLocalRef(asset_manager);

            // Retrieve the path to the native libraries.
            jobject app_info = env->CallObjectMethod(context, get_app_info);
            env->DeleteLocalRef(context_class);
            if (rjni_exception_clear(env) || app_info == nullptr) {
                return XR_ERROR_INITIALIZATION_FAILED;
            }
            jclass app_info_class = env->GetObjectClass(app_info);
            jfieldID native_dir_field = env->GetFieldID(app_info_class, "nativeLibraryDir", "Ljava/lang/String;");
            env->DeleteLocalRef(app_info_class);
            if (rjni_exception_clear(env) || native_dir_field == nullptr) {
                env->DeleteLocalRef(app_info);
                return XR_ERROR_INITIALIZATION_FAILED;
            }
            jstring native_dir = static_cast<jstring>(env->GetObjectField(app_info, native_dir_field));
            env->DeleteLocalRef(app_info);
            if (rjni_exception_clear(env) || native_dir == nullptr) {
                return XR_ERROR_INITIALIZATION_FAILED;
            }
            if (const char* chars = env->GetStringUTFChars(native_dir, nullptr)) {
                _android_native_library_path = chars;
                env->ReleaseStringUTFChars(native_dir, chars);
            }
            env->DeleteLocalRef(native_dir);

            // Take only the first such struct.
            return XR_SUCCESS;
        }
        info = reinterpret_cast<const XrLoaderInitInfoBaseHeaderKHR*>(info->next);
    }

    // We didn't find one.
    return XR_ERROR_VALIDATION_FAILURE;
}
#endif  // defined(XR_USE_PLATFORM_ANDROID) && defined(XR_HAS_REQUIRED_PLATFORM_LOADER_INIT_STRUCT)

XrResult InitializeLoaderInitData(const XrLoaderInitInfoBaseHeaderKHR* loaderInitInfo) {
    if (!ActiveLoaderInstance::IsAvailable()) {
        LoaderLogger::LogVerboseMessage("InitializeLoaderInitData", "Unloading any previously loaded runtime");
        // This will not shutdown the runtime, only unload the library.
        RuntimeInterface::UnloadRuntime("InitializeLoaderInitData");
    } else {
        LoaderLogger::LogErrorMessage("InitializeLoaderInitData",
                                      "An active instance currently exists while trying to reinitialize the loader");
        return XR_ERROR_INITIALIZATION_FAILED;
    }
    return LoaderInitData::instance().initialize(loaderInitInfo);
}

#if defined(XR_USE_PLATFORM_ANDROID) && defined(XR_HAS_REQUIRED_PLATFORM_LOADER_INIT_STRUCT)
std::string GetAndroidNativeLibraryDir() { return LoaderInitData::instance()._android_native_library_path; }

void* GetAndroidAssetManager() { return LoaderInitData::instance()._android_asset_manager; }
#endif  // defined(XR_USE_PLATFORM_ANDROID) && defined(XR_HAS_REQUIRED_PLATFORM_LOADER_INIT_STRUCT)
