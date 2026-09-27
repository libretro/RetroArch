// Copyright (c) 2017-2026 The Khronos Group Inc.
// Copyright (c) 2017-2019, Valve Corporation
// Copyright (c) 2017-2019, LunarG, Inc.

// SPDX-License-Identifier: Apache-2.0 OR MIT

// *********** THIS FILE IS GENERATED - DO NOT EDIT ***********
//     See utility_source_generator.py for modifications
// ************************************************************

// Copyright (c) 2017-2026 The Khronos Group Inc.
// Copyright (c) 2017-2019 Valve Corporation
// Copyright (c) 2017-2019 LunarG, Inc.
//
// SPDX-License-Identifier: Apache-2.0
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
// Author: Mark Young <marky@lunarg.com>
//

#include "xr_generated_dispatch_table.h"



#ifdef __cplusplus
extern "C" { 
#endif
// Helper function to populate an instance dispatch table
void GeneratedXrPopulateDispatchTable(struct XrGeneratedDispatchTable *table,
                                      XrInstance instance,
                                      PFN_xrGetInstanceProcAddr get_inst_proc_addr) {

    // ---- Core 1.0 commands
    table->GetInstanceProcAddr = get_inst_proc_addr;
    (get_inst_proc_addr(instance, "xrCreateInstance", (PFN_xrVoidFunction*)&table->CreateInstance));
    (get_inst_proc_addr(instance, "xrDestroyInstance", (PFN_xrVoidFunction*)&table->DestroyInstance));
    (get_inst_proc_addr(instance, "xrGetInstanceProperties", (PFN_xrVoidFunction*)&table->GetInstanceProperties));
    (get_inst_proc_addr(instance, "xrPollEvent", (PFN_xrVoidFunction*)&table->PollEvent));
    (get_inst_proc_addr(instance, "xrResultToString", (PFN_xrVoidFunction*)&table->ResultToString));
    (get_inst_proc_addr(instance, "xrStructureTypeToString", (PFN_xrVoidFunction*)&table->StructureTypeToString));
    (get_inst_proc_addr(instance, "xrGetSystem", (PFN_xrVoidFunction*)&table->GetSystem));
    (get_inst_proc_addr(instance, "xrGetSystemProperties", (PFN_xrVoidFunction*)&table->GetSystemProperties));
    (get_inst_proc_addr(instance, "xrEnumerateEnvironmentBlendModes", (PFN_xrVoidFunction*)&table->EnumerateEnvironmentBlendModes));
    (get_inst_proc_addr(instance, "xrCreateSession", (PFN_xrVoidFunction*)&table->CreateSession));
    (get_inst_proc_addr(instance, "xrDestroySession", (PFN_xrVoidFunction*)&table->DestroySession));
    (get_inst_proc_addr(instance, "xrEnumerateReferenceSpaces", (PFN_xrVoidFunction*)&table->EnumerateReferenceSpaces));
    (get_inst_proc_addr(instance, "xrCreateReferenceSpace", (PFN_xrVoidFunction*)&table->CreateReferenceSpace));
    (get_inst_proc_addr(instance, "xrGetReferenceSpaceBoundsRect", (PFN_xrVoidFunction*)&table->GetReferenceSpaceBoundsRect));
    (get_inst_proc_addr(instance, "xrCreateActionSpace", (PFN_xrVoidFunction*)&table->CreateActionSpace));
    (get_inst_proc_addr(instance, "xrLocateSpace", (PFN_xrVoidFunction*)&table->LocateSpace));
    (get_inst_proc_addr(instance, "xrDestroySpace", (PFN_xrVoidFunction*)&table->DestroySpace));
    (get_inst_proc_addr(instance, "xrEnumerateViewConfigurations", (PFN_xrVoidFunction*)&table->EnumerateViewConfigurations));
    (get_inst_proc_addr(instance, "xrGetViewConfigurationProperties", (PFN_xrVoidFunction*)&table->GetViewConfigurationProperties));
    (get_inst_proc_addr(instance, "xrEnumerateViewConfigurationViews", (PFN_xrVoidFunction*)&table->EnumerateViewConfigurationViews));
    (get_inst_proc_addr(instance, "xrEnumerateSwapchainFormats", (PFN_xrVoidFunction*)&table->EnumerateSwapchainFormats));
    (get_inst_proc_addr(instance, "xrCreateSwapchain", (PFN_xrVoidFunction*)&table->CreateSwapchain));
    (get_inst_proc_addr(instance, "xrDestroySwapchain", (PFN_xrVoidFunction*)&table->DestroySwapchain));
    (get_inst_proc_addr(instance, "xrEnumerateSwapchainImages", (PFN_xrVoidFunction*)&table->EnumerateSwapchainImages));
    (get_inst_proc_addr(instance, "xrAcquireSwapchainImage", (PFN_xrVoidFunction*)&table->AcquireSwapchainImage));
    (get_inst_proc_addr(instance, "xrWaitSwapchainImage", (PFN_xrVoidFunction*)&table->WaitSwapchainImage));
    (get_inst_proc_addr(instance, "xrReleaseSwapchainImage", (PFN_xrVoidFunction*)&table->ReleaseSwapchainImage));
    (get_inst_proc_addr(instance, "xrBeginSession", (PFN_xrVoidFunction*)&table->BeginSession));
    (get_inst_proc_addr(instance, "xrEndSession", (PFN_xrVoidFunction*)&table->EndSession));
    (get_inst_proc_addr(instance, "xrRequestExitSession", (PFN_xrVoidFunction*)&table->RequestExitSession));
    (get_inst_proc_addr(instance, "xrWaitFrame", (PFN_xrVoidFunction*)&table->WaitFrame));
    (get_inst_proc_addr(instance, "xrBeginFrame", (PFN_xrVoidFunction*)&table->BeginFrame));
    (get_inst_proc_addr(instance, "xrEndFrame", (PFN_xrVoidFunction*)&table->EndFrame));
    (get_inst_proc_addr(instance, "xrLocateViews", (PFN_xrVoidFunction*)&table->LocateViews));
    (get_inst_proc_addr(instance, "xrStringToPath", (PFN_xrVoidFunction*)&table->StringToPath));
    (get_inst_proc_addr(instance, "xrPathToString", (PFN_xrVoidFunction*)&table->PathToString));
    (get_inst_proc_addr(instance, "xrCreateActionSet", (PFN_xrVoidFunction*)&table->CreateActionSet));
    (get_inst_proc_addr(instance, "xrDestroyActionSet", (PFN_xrVoidFunction*)&table->DestroyActionSet));
    (get_inst_proc_addr(instance, "xrCreateAction", (PFN_xrVoidFunction*)&table->CreateAction));
    (get_inst_proc_addr(instance, "xrDestroyAction", (PFN_xrVoidFunction*)&table->DestroyAction));
    (get_inst_proc_addr(instance, "xrSuggestInteractionProfileBindings", (PFN_xrVoidFunction*)&table->SuggestInteractionProfileBindings));
    (get_inst_proc_addr(instance, "xrAttachSessionActionSets", (PFN_xrVoidFunction*)&table->AttachSessionActionSets));
    (get_inst_proc_addr(instance, "xrGetCurrentInteractionProfile", (PFN_xrVoidFunction*)&table->GetCurrentInteractionProfile));
    (get_inst_proc_addr(instance, "xrGetActionStateBoolean", (PFN_xrVoidFunction*)&table->GetActionStateBoolean));
    (get_inst_proc_addr(instance, "xrGetActionStateFloat", (PFN_xrVoidFunction*)&table->GetActionStateFloat));
    (get_inst_proc_addr(instance, "xrGetActionStateVector2f", (PFN_xrVoidFunction*)&table->GetActionStateVector2f));
    (get_inst_proc_addr(instance, "xrGetActionStatePose", (PFN_xrVoidFunction*)&table->GetActionStatePose));
    (get_inst_proc_addr(instance, "xrSyncActions", (PFN_xrVoidFunction*)&table->SyncActions));
    (get_inst_proc_addr(instance, "xrEnumerateBoundSourcesForAction", (PFN_xrVoidFunction*)&table->EnumerateBoundSourcesForAction));
    (get_inst_proc_addr(instance, "xrGetInputSourceLocalizedName", (PFN_xrVoidFunction*)&table->GetInputSourceLocalizedName));
    (get_inst_proc_addr(instance, "xrApplyHapticFeedback", (PFN_xrVoidFunction*)&table->ApplyHapticFeedback));
    (get_inst_proc_addr(instance, "xrStopHapticFeedback", (PFN_xrVoidFunction*)&table->StopHapticFeedback));

    // ---- Core 1.1 commands
    (get_inst_proc_addr(instance, "xrLocateSpaces", (PFN_xrVoidFunction*)&table->LocateSpaces));

    // ---- XR_KHR_android_thread_settings extension commands
#if defined(XR_USE_PLATFORM_ANDROID)
    (get_inst_proc_addr(instance, "xrSetAndroidApplicationThreadKHR", (PFN_xrVoidFunction*)&table->SetAndroidApplicationThreadKHR));
#endif // defined(XR_USE_PLATFORM_ANDROID)

    // ---- XR_KHR_android_surface_swapchain extension commands
#if defined(XR_USE_PLATFORM_ANDROID)
    (get_inst_proc_addr(instance, "xrCreateSwapchainAndroidSurfaceKHR", (PFN_xrVoidFunction*)&table->CreateSwapchainAndroidSurfaceKHR));
#endif // defined(XR_USE_PLATFORM_ANDROID)

    // ---- XR_KHR_opengl_enable extension commands
#if defined(XR_USE_GRAPHICS_API_OPENGL)
    (get_inst_proc_addr(instance, "xrGetOpenGLGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&table->GetOpenGLGraphicsRequirementsKHR));
#endif // defined(XR_USE_GRAPHICS_API_OPENGL)

    // ---- XR_KHR_opengl_es_enable extension commands
#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
    (get_inst_proc_addr(instance, "xrGetOpenGLESGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&table->GetOpenGLESGraphicsRequirementsKHR));
#endif // defined(XR_USE_GRAPHICS_API_OPENGL_ES)

    // ---- XR_KHR_vulkan_enable extension commands
#if defined(XR_USE_GRAPHICS_API_VULKAN)
    (get_inst_proc_addr(instance, "xrGetVulkanInstanceExtensionsKHR", (PFN_xrVoidFunction*)&table->GetVulkanInstanceExtensionsKHR));
#endif // defined(XR_USE_GRAPHICS_API_VULKAN)
#if defined(XR_USE_GRAPHICS_API_VULKAN)
    (get_inst_proc_addr(instance, "xrGetVulkanDeviceExtensionsKHR", (PFN_xrVoidFunction*)&table->GetVulkanDeviceExtensionsKHR));
#endif // defined(XR_USE_GRAPHICS_API_VULKAN)
#if defined(XR_USE_GRAPHICS_API_VULKAN)
    (get_inst_proc_addr(instance, "xrGetVulkanGraphicsDeviceKHR", (PFN_xrVoidFunction*)&table->GetVulkanGraphicsDeviceKHR));
#endif // defined(XR_USE_GRAPHICS_API_VULKAN)
#if defined(XR_USE_GRAPHICS_API_VULKAN)
    (get_inst_proc_addr(instance, "xrGetVulkanGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&table->GetVulkanGraphicsRequirementsKHR));
#endif // defined(XR_USE_GRAPHICS_API_VULKAN)

    // ---- XR_KHR_D3D11_enable extension commands
#if defined(XR_USE_GRAPHICS_API_D3D11)
    (get_inst_proc_addr(instance, "xrGetD3D11GraphicsRequirementsKHR", (PFN_xrVoidFunction*)&table->GetD3D11GraphicsRequirementsKHR));
#endif // defined(XR_USE_GRAPHICS_API_D3D11)

    // ---- XR_KHR_D3D12_enable extension commands
#if defined(XR_USE_GRAPHICS_API_D3D12)
    (get_inst_proc_addr(instance, "xrGetD3D12GraphicsRequirementsKHR", (PFN_xrVoidFunction*)&table->GetD3D12GraphicsRequirementsKHR));
#endif // defined(XR_USE_GRAPHICS_API_D3D12)

    // ---- XR_KHR_metal_enable extension commands
#if defined(XR_USE_GRAPHICS_API_METAL)
    (get_inst_proc_addr(instance, "xrGetMetalGraphicsRequirementsKHR", (PFN_xrVoidFunction*)&table->GetMetalGraphicsRequirementsKHR));
#endif // defined(XR_USE_GRAPHICS_API_METAL)

    // ---- XR_KHR_visibility_mask extension commands
    (get_inst_proc_addr(instance, "xrGetVisibilityMaskKHR", (PFN_xrVoidFunction*)&table->GetVisibilityMaskKHR));

    // ---- XR_KHR_win32_convert_performance_counter_time extension commands
#if defined(XR_USE_PLATFORM_WIN32)
    (get_inst_proc_addr(instance, "xrConvertWin32PerformanceCounterToTimeKHR", (PFN_xrVoidFunction*)&table->ConvertWin32PerformanceCounterToTimeKHR));
#endif // defined(XR_USE_PLATFORM_WIN32)
#if defined(XR_USE_PLATFORM_WIN32)
    (get_inst_proc_addr(instance, "xrConvertTimeToWin32PerformanceCounterKHR", (PFN_xrVoidFunction*)&table->ConvertTimeToWin32PerformanceCounterKHR));
#endif // defined(XR_USE_PLATFORM_WIN32)

    // ---- XR_KHR_convert_timespec_time extension commands
#if defined(XR_USE_TIMESPEC)
    (get_inst_proc_addr(instance, "xrConvertTimespecTimeToTimeKHR", (PFN_xrVoidFunction*)&table->ConvertTimespecTimeToTimeKHR));
#endif // defined(XR_USE_TIMESPEC)
#if defined(XR_USE_TIMESPEC)
    (get_inst_proc_addr(instance, "xrConvertTimeToTimespecTimeKHR", (PFN_xrVoidFunction*)&table->ConvertTimeToTimespecTimeKHR));
#endif // defined(XR_USE_TIMESPEC)

    // ---- XR_KHR_vulkan_enable2 extension commands
#if defined(XR_USE_GRAPHICS_API_VULKAN)
    (get_inst_proc_addr(instance, "xrCreateVulkanInstanceKHR", (PFN_xrVoidFunction*)&table->CreateVulkanInstanceKHR));
#endif // defined(XR_USE_GRAPHICS_API_VULKAN)
#if defined(XR_USE_GRAPHICS_API_VULKAN)
    (get_inst_proc_addr(instance, "xrCreateVulkanDeviceKHR", (PFN_xrVoidFunction*)&table->CreateVulkanDeviceKHR));
#endif // defined(XR_USE_GRAPHICS_API_VULKAN)
#if defined(XR_USE_GRAPHICS_API_VULKAN)
    (get_inst_proc_addr(instance, "xrGetVulkanGraphicsDevice2KHR", (PFN_xrVoidFunction*)&table->GetVulkanGraphicsDevice2KHR));
#endif // defined(XR_USE_GRAPHICS_API_VULKAN)
#if defined(XR_USE_GRAPHICS_API_VULKAN)
    (get_inst_proc_addr(instance, "xrGetVulkanGraphicsRequirements2KHR", (PFN_xrVoidFunction*)&table->GetVulkanGraphicsRequirements2KHR));
#endif // defined(XR_USE_GRAPHICS_API_VULKAN)

    // ---- XR_KHR_extended_struct_name_lengths extension commands
    (get_inst_proc_addr(instance, "xrStructureTypeToString2KHR", (PFN_xrVoidFunction*)&table->StructureTypeToString2KHR));

    // ---- XR_KHR_locate_spaces extension commands
    (get_inst_proc_addr(instance, "xrLocateSpacesKHR", (PFN_xrVoidFunction*)&table->LocateSpacesKHR));

    // ---- XR_KHR_extended_result_name_lengths extension commands
    (get_inst_proc_addr(instance, "xrResultToString2KHR", (PFN_xrVoidFunction*)&table->ResultToString2KHR));

    // ---- XR_EXT_performance_settings extension commands
    (get_inst_proc_addr(instance, "xrPerfSettingsSetPerformanceLevelEXT", (PFN_xrVoidFunction*)&table->PerfSettingsSetPerformanceLevelEXT));

    // ---- XR_EXT_thermal_query extension commands
    (get_inst_proc_addr(instance, "xrThermalGetTemperatureTrendEXT", (PFN_xrVoidFunction*)&table->ThermalGetTemperatureTrendEXT));

    // ---- XR_EXT_debug_utils extension commands
    (get_inst_proc_addr(instance, "xrSetDebugUtilsObjectNameEXT", (PFN_xrVoidFunction*)&table->SetDebugUtilsObjectNameEXT));
    (get_inst_proc_addr(instance, "xrCreateDebugUtilsMessengerEXT", (PFN_xrVoidFunction*)&table->CreateDebugUtilsMessengerEXT));
    (get_inst_proc_addr(instance, "xrDestroyDebugUtilsMessengerEXT", (PFN_xrVoidFunction*)&table->DestroyDebugUtilsMessengerEXT));
    (get_inst_proc_addr(instance, "xrSubmitDebugUtilsMessageEXT", (PFN_xrVoidFunction*)&table->SubmitDebugUtilsMessageEXT));
    (get_inst_proc_addr(instance, "xrSessionBeginDebugUtilsLabelRegionEXT", (PFN_xrVoidFunction*)&table->SessionBeginDebugUtilsLabelRegionEXT));
    (get_inst_proc_addr(instance, "xrSessionEndDebugUtilsLabelRegionEXT", (PFN_xrVoidFunction*)&table->SessionEndDebugUtilsLabelRegionEXT));
    (get_inst_proc_addr(instance, "xrSessionInsertDebugUtilsLabelEXT", (PFN_xrVoidFunction*)&table->SessionInsertDebugUtilsLabelEXT));

    // ---- XR_MSFT_spatial_anchor extension commands
    (get_inst_proc_addr(instance, "xrCreateSpatialAnchorMSFT", (PFN_xrVoidFunction*)&table->CreateSpatialAnchorMSFT));
    (get_inst_proc_addr(instance, "xrCreateSpatialAnchorSpaceMSFT", (PFN_xrVoidFunction*)&table->CreateSpatialAnchorSpaceMSFT));
    (get_inst_proc_addr(instance, "xrDestroySpatialAnchorMSFT", (PFN_xrVoidFunction*)&table->DestroySpatialAnchorMSFT));

    // ---- XR_EXT_conformance_automation extension commands
    (get_inst_proc_addr(instance, "xrSetInputDeviceActiveEXT", (PFN_xrVoidFunction*)&table->SetInputDeviceActiveEXT));
    (get_inst_proc_addr(instance, "xrSetInputDeviceStateBoolEXT", (PFN_xrVoidFunction*)&table->SetInputDeviceStateBoolEXT));
    (get_inst_proc_addr(instance, "xrSetInputDeviceStateFloatEXT", (PFN_xrVoidFunction*)&table->SetInputDeviceStateFloatEXT));
    (get_inst_proc_addr(instance, "xrSetInputDeviceStateVector2fEXT", (PFN_xrVoidFunction*)&table->SetInputDeviceStateVector2fEXT));
    (get_inst_proc_addr(instance, "xrSetInputDeviceLocationEXT", (PFN_xrVoidFunction*)&table->SetInputDeviceLocationEXT));

    // ---- XR_MSFT_spatial_graph_bridge extension commands
    (get_inst_proc_addr(instance, "xrCreateSpatialGraphNodeSpaceMSFT", (PFN_xrVoidFunction*)&table->CreateSpatialGraphNodeSpaceMSFT));
    (get_inst_proc_addr(instance, "xrTryCreateSpatialGraphStaticNodeBindingMSFT", (PFN_xrVoidFunction*)&table->TryCreateSpatialGraphStaticNodeBindingMSFT));
    (get_inst_proc_addr(instance, "xrDestroySpatialGraphNodeBindingMSFT", (PFN_xrVoidFunction*)&table->DestroySpatialGraphNodeBindingMSFT));
    (get_inst_proc_addr(instance, "xrGetSpatialGraphNodeBindingPropertiesMSFT", (PFN_xrVoidFunction*)&table->GetSpatialGraphNodeBindingPropertiesMSFT));

    // ---- XR_EXT_hand_tracking extension commands
    (get_inst_proc_addr(instance, "xrCreateHandTrackerEXT", (PFN_xrVoidFunction*)&table->CreateHandTrackerEXT));
    (get_inst_proc_addr(instance, "xrDestroyHandTrackerEXT", (PFN_xrVoidFunction*)&table->DestroyHandTrackerEXT));
    (get_inst_proc_addr(instance, "xrLocateHandJointsEXT", (PFN_xrVoidFunction*)&table->LocateHandJointsEXT));

    // ---- XR_MSFT_hand_tracking_mesh extension commands
    (get_inst_proc_addr(instance, "xrCreateHandMeshSpaceMSFT", (PFN_xrVoidFunction*)&table->CreateHandMeshSpaceMSFT));
    (get_inst_proc_addr(instance, "xrUpdateHandMeshMSFT", (PFN_xrVoidFunction*)&table->UpdateHandMeshMSFT));

    // ---- XR_MSFT_controller_model extension commands
    (get_inst_proc_addr(instance, "xrGetControllerModelKeyMSFT", (PFN_xrVoidFunction*)&table->GetControllerModelKeyMSFT));
    (get_inst_proc_addr(instance, "xrLoadControllerModelMSFT", (PFN_xrVoidFunction*)&table->LoadControllerModelMSFT));
    (get_inst_proc_addr(instance, "xrGetControllerModelPropertiesMSFT", (PFN_xrVoidFunction*)&table->GetControllerModelPropertiesMSFT));
    (get_inst_proc_addr(instance, "xrGetControllerModelStateMSFT", (PFN_xrVoidFunction*)&table->GetControllerModelStateMSFT));

    // ---- XR_MSFT_perception_anchor_interop extension commands
#if defined(XR_USE_PLATFORM_WIN32)
    (get_inst_proc_addr(instance, "xrCreateSpatialAnchorFromPerceptionAnchorMSFT", (PFN_xrVoidFunction*)&table->CreateSpatialAnchorFromPerceptionAnchorMSFT));
#endif // defined(XR_USE_PLATFORM_WIN32)
#if defined(XR_USE_PLATFORM_WIN32)
    (get_inst_proc_addr(instance, "xrTryGetPerceptionAnchorFromSpatialAnchorMSFT", (PFN_xrVoidFunction*)&table->TryGetPerceptionAnchorFromSpatialAnchorMSFT));
#endif // defined(XR_USE_PLATFORM_WIN32)

    // ---- XR_MSFT_composition_layer_reprojection extension commands
    (get_inst_proc_addr(instance, "xrEnumerateReprojectionModesMSFT", (PFN_xrVoidFunction*)&table->EnumerateReprojectionModesMSFT));

    // ---- XR_FB_swapchain_update_state extension commands
    (get_inst_proc_addr(instance, "xrUpdateSwapchainFB", (PFN_xrVoidFunction*)&table->UpdateSwapchainFB));
    (get_inst_proc_addr(instance, "xrGetSwapchainStateFB", (PFN_xrVoidFunction*)&table->GetSwapchainStateFB));

    // ---- XR_FB_body_tracking extension commands
    (get_inst_proc_addr(instance, "xrCreateBodyTrackerFB", (PFN_xrVoidFunction*)&table->CreateBodyTrackerFB));
    (get_inst_proc_addr(instance, "xrDestroyBodyTrackerFB", (PFN_xrVoidFunction*)&table->DestroyBodyTrackerFB));
    (get_inst_proc_addr(instance, "xrLocateBodyJointsFB", (PFN_xrVoidFunction*)&table->LocateBodyJointsFB));
    (get_inst_proc_addr(instance, "xrGetBodySkeletonFB", (PFN_xrVoidFunction*)&table->GetBodySkeletonFB));

    // ---- XR_MSFT_scene_understanding extension commands
    (get_inst_proc_addr(instance, "xrEnumerateSceneComputeFeaturesMSFT", (PFN_xrVoidFunction*)&table->EnumerateSceneComputeFeaturesMSFT));
    (get_inst_proc_addr(instance, "xrCreateSceneObserverMSFT", (PFN_xrVoidFunction*)&table->CreateSceneObserverMSFT));
    (get_inst_proc_addr(instance, "xrDestroySceneObserverMSFT", (PFN_xrVoidFunction*)&table->DestroySceneObserverMSFT));
    (get_inst_proc_addr(instance, "xrCreateSceneMSFT", (PFN_xrVoidFunction*)&table->CreateSceneMSFT));
    (get_inst_proc_addr(instance, "xrDestroySceneMSFT", (PFN_xrVoidFunction*)&table->DestroySceneMSFT));
    (get_inst_proc_addr(instance, "xrComputeNewSceneMSFT", (PFN_xrVoidFunction*)&table->ComputeNewSceneMSFT));
    (get_inst_proc_addr(instance, "xrGetSceneComputeStateMSFT", (PFN_xrVoidFunction*)&table->GetSceneComputeStateMSFT));
    (get_inst_proc_addr(instance, "xrGetSceneComponentsMSFT", (PFN_xrVoidFunction*)&table->GetSceneComponentsMSFT));
    (get_inst_proc_addr(instance, "xrLocateSceneComponentsMSFT", (PFN_xrVoidFunction*)&table->LocateSceneComponentsMSFT));
    (get_inst_proc_addr(instance, "xrGetSceneMeshBuffersMSFT", (PFN_xrVoidFunction*)&table->GetSceneMeshBuffersMSFT));

    // ---- XR_MSFT_scene_understanding_serialization extension commands
    (get_inst_proc_addr(instance, "xrDeserializeSceneMSFT", (PFN_xrVoidFunction*)&table->DeserializeSceneMSFT));
    (get_inst_proc_addr(instance, "xrGetSerializedSceneFragmentDataMSFT", (PFN_xrVoidFunction*)&table->GetSerializedSceneFragmentDataMSFT));

    // ---- XR_FB_display_refresh_rate extension commands
    (get_inst_proc_addr(instance, "xrEnumerateDisplayRefreshRatesFB", (PFN_xrVoidFunction*)&table->EnumerateDisplayRefreshRatesFB));
    (get_inst_proc_addr(instance, "xrGetDisplayRefreshRateFB", (PFN_xrVoidFunction*)&table->GetDisplayRefreshRateFB));
    (get_inst_proc_addr(instance, "xrRequestDisplayRefreshRateFB", (PFN_xrVoidFunction*)&table->RequestDisplayRefreshRateFB));

    // ---- XR_HTCX_vive_tracker_interaction extension commands
    (get_inst_proc_addr(instance, "xrEnumerateViveTrackerPathsHTCX", (PFN_xrVoidFunction*)&table->EnumerateViveTrackerPathsHTCX));

    // ---- XR_HTC_facial_tracking extension commands
    (get_inst_proc_addr(instance, "xrCreateFacialTrackerHTC", (PFN_xrVoidFunction*)&table->CreateFacialTrackerHTC));
    (get_inst_proc_addr(instance, "xrDestroyFacialTrackerHTC", (PFN_xrVoidFunction*)&table->DestroyFacialTrackerHTC));
    (get_inst_proc_addr(instance, "xrGetFacialExpressionsHTC", (PFN_xrVoidFunction*)&table->GetFacialExpressionsHTC));

    // ---- XR_FB_color_space extension commands
    (get_inst_proc_addr(instance, "xrEnumerateColorSpacesFB", (PFN_xrVoidFunction*)&table->EnumerateColorSpacesFB));
    (get_inst_proc_addr(instance, "xrSetColorSpaceFB", (PFN_xrVoidFunction*)&table->SetColorSpaceFB));

    // ---- XR_FB_hand_tracking_mesh extension commands
    (get_inst_proc_addr(instance, "xrGetHandMeshFB", (PFN_xrVoidFunction*)&table->GetHandMeshFB));

    // ---- XR_FB_spatial_entity extension commands
    (get_inst_proc_addr(instance, "xrCreateSpatialAnchorFB", (PFN_xrVoidFunction*)&table->CreateSpatialAnchorFB));
    (get_inst_proc_addr(instance, "xrGetSpaceUuidFB", (PFN_xrVoidFunction*)&table->GetSpaceUuidFB));
    (get_inst_proc_addr(instance, "xrEnumerateSpaceSupportedComponentsFB", (PFN_xrVoidFunction*)&table->EnumerateSpaceSupportedComponentsFB));
    (get_inst_proc_addr(instance, "xrSetSpaceComponentStatusFB", (PFN_xrVoidFunction*)&table->SetSpaceComponentStatusFB));
    (get_inst_proc_addr(instance, "xrGetSpaceComponentStatusFB", (PFN_xrVoidFunction*)&table->GetSpaceComponentStatusFB));

    // ---- XR_FB_foveation extension commands
    (get_inst_proc_addr(instance, "xrCreateFoveationProfileFB", (PFN_xrVoidFunction*)&table->CreateFoveationProfileFB));
    (get_inst_proc_addr(instance, "xrDestroyFoveationProfileFB", (PFN_xrVoidFunction*)&table->DestroyFoveationProfileFB));

    // ---- XR_FB_keyboard_tracking extension commands
    (get_inst_proc_addr(instance, "xrQuerySystemTrackedKeyboardFB", (PFN_xrVoidFunction*)&table->QuerySystemTrackedKeyboardFB));
    (get_inst_proc_addr(instance, "xrCreateKeyboardSpaceFB", (PFN_xrVoidFunction*)&table->CreateKeyboardSpaceFB));

    // ---- XR_FB_triangle_mesh extension commands
    (get_inst_proc_addr(instance, "xrCreateTriangleMeshFB", (PFN_xrVoidFunction*)&table->CreateTriangleMeshFB));
    (get_inst_proc_addr(instance, "xrDestroyTriangleMeshFB", (PFN_xrVoidFunction*)&table->DestroyTriangleMeshFB));
    (get_inst_proc_addr(instance, "xrTriangleMeshGetVertexBufferFB", (PFN_xrVoidFunction*)&table->TriangleMeshGetVertexBufferFB));
    (get_inst_proc_addr(instance, "xrTriangleMeshGetIndexBufferFB", (PFN_xrVoidFunction*)&table->TriangleMeshGetIndexBufferFB));
    (get_inst_proc_addr(instance, "xrTriangleMeshBeginUpdateFB", (PFN_xrVoidFunction*)&table->TriangleMeshBeginUpdateFB));
    (get_inst_proc_addr(instance, "xrTriangleMeshEndUpdateFB", (PFN_xrVoidFunction*)&table->TriangleMeshEndUpdateFB));
    (get_inst_proc_addr(instance, "xrTriangleMeshBeginVertexBufferUpdateFB", (PFN_xrVoidFunction*)&table->TriangleMeshBeginVertexBufferUpdateFB));
    (get_inst_proc_addr(instance, "xrTriangleMeshEndVertexBufferUpdateFB", (PFN_xrVoidFunction*)&table->TriangleMeshEndVertexBufferUpdateFB));

    // ---- XR_FB_passthrough extension commands
    (get_inst_proc_addr(instance, "xrCreatePassthroughFB", (PFN_xrVoidFunction*)&table->CreatePassthroughFB));
    (get_inst_proc_addr(instance, "xrDestroyPassthroughFB", (PFN_xrVoidFunction*)&table->DestroyPassthroughFB));
    (get_inst_proc_addr(instance, "xrPassthroughStartFB", (PFN_xrVoidFunction*)&table->PassthroughStartFB));
    (get_inst_proc_addr(instance, "xrPassthroughPauseFB", (PFN_xrVoidFunction*)&table->PassthroughPauseFB));
    (get_inst_proc_addr(instance, "xrCreatePassthroughLayerFB", (PFN_xrVoidFunction*)&table->CreatePassthroughLayerFB));
    (get_inst_proc_addr(instance, "xrDestroyPassthroughLayerFB", (PFN_xrVoidFunction*)&table->DestroyPassthroughLayerFB));
    (get_inst_proc_addr(instance, "xrPassthroughLayerPauseFB", (PFN_xrVoidFunction*)&table->PassthroughLayerPauseFB));
    (get_inst_proc_addr(instance, "xrPassthroughLayerResumeFB", (PFN_xrVoidFunction*)&table->PassthroughLayerResumeFB));
    (get_inst_proc_addr(instance, "xrPassthroughLayerSetStyleFB", (PFN_xrVoidFunction*)&table->PassthroughLayerSetStyleFB));
    (get_inst_proc_addr(instance, "xrCreateGeometryInstanceFB", (PFN_xrVoidFunction*)&table->CreateGeometryInstanceFB));
    (get_inst_proc_addr(instance, "xrDestroyGeometryInstanceFB", (PFN_xrVoidFunction*)&table->DestroyGeometryInstanceFB));
    (get_inst_proc_addr(instance, "xrGeometryInstanceSetTransformFB", (PFN_xrVoidFunction*)&table->GeometryInstanceSetTransformFB));

    // ---- XR_FB_render_model extension commands
    (get_inst_proc_addr(instance, "xrEnumerateRenderModelPathsFB", (PFN_xrVoidFunction*)&table->EnumerateRenderModelPathsFB));
    (get_inst_proc_addr(instance, "xrGetRenderModelPropertiesFB", (PFN_xrVoidFunction*)&table->GetRenderModelPropertiesFB));
    (get_inst_proc_addr(instance, "xrLoadRenderModelFB", (PFN_xrVoidFunction*)&table->LoadRenderModelFB));

    // ---- XR_VARJO_environment_depth_estimation extension commands
    (get_inst_proc_addr(instance, "xrSetEnvironmentDepthEstimationVARJO", (PFN_xrVoidFunction*)&table->SetEnvironmentDepthEstimationVARJO));

    // ---- XR_VARJO_marker_tracking extension commands
    (get_inst_proc_addr(instance, "xrSetMarkerTrackingVARJO", (PFN_xrVoidFunction*)&table->SetMarkerTrackingVARJO));
    (get_inst_proc_addr(instance, "xrSetMarkerTrackingTimeoutVARJO", (PFN_xrVoidFunction*)&table->SetMarkerTrackingTimeoutVARJO));
    (get_inst_proc_addr(instance, "xrSetMarkerTrackingPredictionVARJO", (PFN_xrVoidFunction*)&table->SetMarkerTrackingPredictionVARJO));
    (get_inst_proc_addr(instance, "xrGetMarkerSizeVARJO", (PFN_xrVoidFunction*)&table->GetMarkerSizeVARJO));
    (get_inst_proc_addr(instance, "xrCreateMarkerSpaceVARJO", (PFN_xrVoidFunction*)&table->CreateMarkerSpaceVARJO));

    // ---- XR_VARJO_view_offset extension commands
    (get_inst_proc_addr(instance, "xrSetViewOffsetVARJO", (PFN_xrVoidFunction*)&table->SetViewOffsetVARJO));

    // ---- XR_ML_compat extension commands
#if defined(XR_USE_PLATFORM_ML)
    (get_inst_proc_addr(instance, "xrCreateSpaceFromCoordinateFrameUIDML", (PFN_xrVoidFunction*)&table->CreateSpaceFromCoordinateFrameUIDML));
#endif // defined(XR_USE_PLATFORM_ML)

    // ---- XR_ML_marker_understanding extension commands
    (get_inst_proc_addr(instance, "xrCreateMarkerDetectorML", (PFN_xrVoidFunction*)&table->CreateMarkerDetectorML));
    (get_inst_proc_addr(instance, "xrDestroyMarkerDetectorML", (PFN_xrVoidFunction*)&table->DestroyMarkerDetectorML));
    (get_inst_proc_addr(instance, "xrSnapshotMarkerDetectorML", (PFN_xrVoidFunction*)&table->SnapshotMarkerDetectorML));
    (get_inst_proc_addr(instance, "xrGetMarkerDetectorStateML", (PFN_xrVoidFunction*)&table->GetMarkerDetectorStateML));
    (get_inst_proc_addr(instance, "xrGetMarkersML", (PFN_xrVoidFunction*)&table->GetMarkersML));
    (get_inst_proc_addr(instance, "xrGetMarkerReprojectionErrorML", (PFN_xrVoidFunction*)&table->GetMarkerReprojectionErrorML));
    (get_inst_proc_addr(instance, "xrGetMarkerLengthML", (PFN_xrVoidFunction*)&table->GetMarkerLengthML));
    (get_inst_proc_addr(instance, "xrGetMarkerNumberML", (PFN_xrVoidFunction*)&table->GetMarkerNumberML));
    (get_inst_proc_addr(instance, "xrGetMarkerStringML", (PFN_xrVoidFunction*)&table->GetMarkerStringML));
    (get_inst_proc_addr(instance, "xrCreateMarkerSpaceML", (PFN_xrVoidFunction*)&table->CreateMarkerSpaceML));

    // ---- XR_ML_localization_map extension commands
    (get_inst_proc_addr(instance, "xrEnableLocalizationEventsML", (PFN_xrVoidFunction*)&table->EnableLocalizationEventsML));
    (get_inst_proc_addr(instance, "xrQueryLocalizationMapsML", (PFN_xrVoidFunction*)&table->QueryLocalizationMapsML));
    (get_inst_proc_addr(instance, "xrRequestMapLocalizationML", (PFN_xrVoidFunction*)&table->RequestMapLocalizationML));
    (get_inst_proc_addr(instance, "xrImportLocalizationMapML", (PFN_xrVoidFunction*)&table->ImportLocalizationMapML));
    (get_inst_proc_addr(instance, "xrCreateExportedLocalizationMapML", (PFN_xrVoidFunction*)&table->CreateExportedLocalizationMapML));
    (get_inst_proc_addr(instance, "xrDestroyExportedLocalizationMapML", (PFN_xrVoidFunction*)&table->DestroyExportedLocalizationMapML));
    (get_inst_proc_addr(instance, "xrGetExportedLocalizationMapDataML", (PFN_xrVoidFunction*)&table->GetExportedLocalizationMapDataML));

    // ---- XR_ML_spatial_anchors extension commands
    (get_inst_proc_addr(instance, "xrCreateSpatialAnchorsAsyncML", (PFN_xrVoidFunction*)&table->CreateSpatialAnchorsAsyncML));
    (get_inst_proc_addr(instance, "xrCreateSpatialAnchorsCompleteML", (PFN_xrVoidFunction*)&table->CreateSpatialAnchorsCompleteML));
    (get_inst_proc_addr(instance, "xrGetSpatialAnchorStateML", (PFN_xrVoidFunction*)&table->GetSpatialAnchorStateML));

    // ---- XR_ML_spatial_anchors_storage extension commands
    (get_inst_proc_addr(instance, "xrCreateSpatialAnchorsStorageML", (PFN_xrVoidFunction*)&table->CreateSpatialAnchorsStorageML));
    (get_inst_proc_addr(instance, "xrDestroySpatialAnchorsStorageML", (PFN_xrVoidFunction*)&table->DestroySpatialAnchorsStorageML));
    (get_inst_proc_addr(instance, "xrQuerySpatialAnchorsAsyncML", (PFN_xrVoidFunction*)&table->QuerySpatialAnchorsAsyncML));
    (get_inst_proc_addr(instance, "xrQuerySpatialAnchorsCompleteML", (PFN_xrVoidFunction*)&table->QuerySpatialAnchorsCompleteML));
    (get_inst_proc_addr(instance, "xrPublishSpatialAnchorsAsyncML", (PFN_xrVoidFunction*)&table->PublishSpatialAnchorsAsyncML));
    (get_inst_proc_addr(instance, "xrPublishSpatialAnchorsCompleteML", (PFN_xrVoidFunction*)&table->PublishSpatialAnchorsCompleteML));
    (get_inst_proc_addr(instance, "xrDeleteSpatialAnchorsAsyncML", (PFN_xrVoidFunction*)&table->DeleteSpatialAnchorsAsyncML));
    (get_inst_proc_addr(instance, "xrDeleteSpatialAnchorsCompleteML", (PFN_xrVoidFunction*)&table->DeleteSpatialAnchorsCompleteML));
    (get_inst_proc_addr(instance, "xrUpdateSpatialAnchorsExpirationAsyncML", (PFN_xrVoidFunction*)&table->UpdateSpatialAnchorsExpirationAsyncML));
    (get_inst_proc_addr(instance, "xrUpdateSpatialAnchorsExpirationCompleteML", (PFN_xrVoidFunction*)&table->UpdateSpatialAnchorsExpirationCompleteML));

    // ---- XR_MSFT_spatial_anchor_persistence extension commands
    (get_inst_proc_addr(instance, "xrCreateSpatialAnchorStoreConnectionMSFT", (PFN_xrVoidFunction*)&table->CreateSpatialAnchorStoreConnectionMSFT));
    (get_inst_proc_addr(instance, "xrDestroySpatialAnchorStoreConnectionMSFT", (PFN_xrVoidFunction*)&table->DestroySpatialAnchorStoreConnectionMSFT));
    (get_inst_proc_addr(instance, "xrPersistSpatialAnchorMSFT", (PFN_xrVoidFunction*)&table->PersistSpatialAnchorMSFT));
    (get_inst_proc_addr(instance, "xrEnumeratePersistedSpatialAnchorNamesMSFT", (PFN_xrVoidFunction*)&table->EnumeratePersistedSpatialAnchorNamesMSFT));
    (get_inst_proc_addr(instance, "xrCreateSpatialAnchorFromPersistedNameMSFT", (PFN_xrVoidFunction*)&table->CreateSpatialAnchorFromPersistedNameMSFT));
    (get_inst_proc_addr(instance, "xrUnpersistSpatialAnchorMSFT", (PFN_xrVoidFunction*)&table->UnpersistSpatialAnchorMSFT));
    (get_inst_proc_addr(instance, "xrClearSpatialAnchorStoreMSFT", (PFN_xrVoidFunction*)&table->ClearSpatialAnchorStoreMSFT));

    // ---- XR_MSFT_scene_marker extension commands
    (get_inst_proc_addr(instance, "xrGetSceneMarkerRawDataMSFT", (PFN_xrVoidFunction*)&table->GetSceneMarkerRawDataMSFT));
    (get_inst_proc_addr(instance, "xrGetSceneMarkerDecodedStringMSFT", (PFN_xrVoidFunction*)&table->GetSceneMarkerDecodedStringMSFT));

    // ---- XR_FB_spatial_entity_query extension commands
    (get_inst_proc_addr(instance, "xrQuerySpacesFB", (PFN_xrVoidFunction*)&table->QuerySpacesFB));
    (get_inst_proc_addr(instance, "xrRetrieveSpaceQueryResultsFB", (PFN_xrVoidFunction*)&table->RetrieveSpaceQueryResultsFB));

    // ---- XR_FB_spatial_entity_storage extension commands
    (get_inst_proc_addr(instance, "xrSaveSpaceFB", (PFN_xrVoidFunction*)&table->SaveSpaceFB));
    (get_inst_proc_addr(instance, "xrEraseSpaceFB", (PFN_xrVoidFunction*)&table->EraseSpaceFB));

    // ---- XR_OCULUS_audio_device_guid extension commands
#if defined(XR_USE_PLATFORM_WIN32)
    (get_inst_proc_addr(instance, "xrGetAudioOutputDeviceGuidOculus", (PFN_xrVoidFunction*)&table->GetAudioOutputDeviceGuidOculus));
#endif // defined(XR_USE_PLATFORM_WIN32)
#if defined(XR_USE_PLATFORM_WIN32)
    (get_inst_proc_addr(instance, "xrGetAudioInputDeviceGuidOculus", (PFN_xrVoidFunction*)&table->GetAudioInputDeviceGuidOculus));
#endif // defined(XR_USE_PLATFORM_WIN32)

    // ---- XR_FB_spatial_entity_sharing extension commands
    (get_inst_proc_addr(instance, "xrShareSpacesFB", (PFN_xrVoidFunction*)&table->ShareSpacesFB));

    // ---- XR_FB_scene extension commands
    (get_inst_proc_addr(instance, "xrGetSpaceBoundingBox2DFB", (PFN_xrVoidFunction*)&table->GetSpaceBoundingBox2DFB));
    (get_inst_proc_addr(instance, "xrGetSpaceBoundingBox3DFB", (PFN_xrVoidFunction*)&table->GetSpaceBoundingBox3DFB));
    (get_inst_proc_addr(instance, "xrGetSpaceSemanticLabelsFB", (PFN_xrVoidFunction*)&table->GetSpaceSemanticLabelsFB));
    (get_inst_proc_addr(instance, "xrGetSpaceBoundary2DFB", (PFN_xrVoidFunction*)&table->GetSpaceBoundary2DFB));
    (get_inst_proc_addr(instance, "xrGetSpaceRoomLayoutFB", (PFN_xrVoidFunction*)&table->GetSpaceRoomLayoutFB));

    // ---- XR_ALMALENCE_digital_lens_control extension commands
    (get_inst_proc_addr(instance, "xrSetDigitalLensControlALMALENCE", (PFN_xrVoidFunction*)&table->SetDigitalLensControlALMALENCE));

    // ---- XR_FB_scene_capture extension commands
    (get_inst_proc_addr(instance, "xrRequestSceneCaptureFB", (PFN_xrVoidFunction*)&table->RequestSceneCaptureFB));

    // ---- XR_FB_spatial_entity_container extension commands
    (get_inst_proc_addr(instance, "xrGetSpaceContainerFB", (PFN_xrVoidFunction*)&table->GetSpaceContainerFB));

    // ---- XR_META_foveation_eye_tracked extension commands
    (get_inst_proc_addr(instance, "xrGetFoveationEyeTrackedStateMETA", (PFN_xrVoidFunction*)&table->GetFoveationEyeTrackedStateMETA));

    // ---- XR_FB_face_tracking extension commands
    (get_inst_proc_addr(instance, "xrCreateFaceTrackerFB", (PFN_xrVoidFunction*)&table->CreateFaceTrackerFB));
    (get_inst_proc_addr(instance, "xrDestroyFaceTrackerFB", (PFN_xrVoidFunction*)&table->DestroyFaceTrackerFB));
    (get_inst_proc_addr(instance, "xrGetFaceExpressionWeightsFB", (PFN_xrVoidFunction*)&table->GetFaceExpressionWeightsFB));

    // ---- XR_FB_eye_tracking_social extension commands
    (get_inst_proc_addr(instance, "xrCreateEyeTrackerFB", (PFN_xrVoidFunction*)&table->CreateEyeTrackerFB));
    (get_inst_proc_addr(instance, "xrDestroyEyeTrackerFB", (PFN_xrVoidFunction*)&table->DestroyEyeTrackerFB));
    (get_inst_proc_addr(instance, "xrGetEyeGazesFB", (PFN_xrVoidFunction*)&table->GetEyeGazesFB));

    // ---- XR_FB_passthrough_keyboard_hands extension commands
    (get_inst_proc_addr(instance, "xrPassthroughLayerSetKeyboardHandsIntensityFB", (PFN_xrVoidFunction*)&table->PassthroughLayerSetKeyboardHandsIntensityFB));

    // ---- XR_FB_haptic_pcm extension commands
    (get_inst_proc_addr(instance, "xrGetDeviceSampleRateFB", (PFN_xrVoidFunction*)&table->GetDeviceSampleRateFB));

    // ---- XR_META_passthrough_preferences extension commands
    (get_inst_proc_addr(instance, "xrGetPassthroughPreferencesMETA", (PFN_xrVoidFunction*)&table->GetPassthroughPreferencesMETA));

    // ---- XR_META_virtual_keyboard extension commands
    (get_inst_proc_addr(instance, "xrCreateVirtualKeyboardMETA", (PFN_xrVoidFunction*)&table->CreateVirtualKeyboardMETA));
    (get_inst_proc_addr(instance, "xrDestroyVirtualKeyboardMETA", (PFN_xrVoidFunction*)&table->DestroyVirtualKeyboardMETA));
    (get_inst_proc_addr(instance, "xrCreateVirtualKeyboardSpaceMETA", (PFN_xrVoidFunction*)&table->CreateVirtualKeyboardSpaceMETA));
    (get_inst_proc_addr(instance, "xrSuggestVirtualKeyboardLocationMETA", (PFN_xrVoidFunction*)&table->SuggestVirtualKeyboardLocationMETA));
    (get_inst_proc_addr(instance, "xrGetVirtualKeyboardScaleMETA", (PFN_xrVoidFunction*)&table->GetVirtualKeyboardScaleMETA));
    (get_inst_proc_addr(instance, "xrSetVirtualKeyboardModelVisibilityMETA", (PFN_xrVoidFunction*)&table->SetVirtualKeyboardModelVisibilityMETA));
    (get_inst_proc_addr(instance, "xrGetVirtualKeyboardModelAnimationStatesMETA", (PFN_xrVoidFunction*)&table->GetVirtualKeyboardModelAnimationStatesMETA));
    (get_inst_proc_addr(instance, "xrGetVirtualKeyboardDirtyTexturesMETA", (PFN_xrVoidFunction*)&table->GetVirtualKeyboardDirtyTexturesMETA));
    (get_inst_proc_addr(instance, "xrGetVirtualKeyboardTextureDataMETA", (PFN_xrVoidFunction*)&table->GetVirtualKeyboardTextureDataMETA));
    (get_inst_proc_addr(instance, "xrSendVirtualKeyboardInputMETA", (PFN_xrVoidFunction*)&table->SendVirtualKeyboardInputMETA));
    (get_inst_proc_addr(instance, "xrChangeVirtualKeyboardTextContextMETA", (PFN_xrVoidFunction*)&table->ChangeVirtualKeyboardTextContextMETA));

    // ---- XR_OCULUS_external_camera extension commands
    (get_inst_proc_addr(instance, "xrEnumerateExternalCamerasOCULUS", (PFN_xrVoidFunction*)&table->EnumerateExternalCamerasOCULUS));

    // ---- XR_META_performance_metrics extension commands
    (get_inst_proc_addr(instance, "xrEnumeratePerformanceMetricsCounterPathsMETA", (PFN_xrVoidFunction*)&table->EnumeratePerformanceMetricsCounterPathsMETA));
    (get_inst_proc_addr(instance, "xrSetPerformanceMetricsStateMETA", (PFN_xrVoidFunction*)&table->SetPerformanceMetricsStateMETA));
    (get_inst_proc_addr(instance, "xrGetPerformanceMetricsStateMETA", (PFN_xrVoidFunction*)&table->GetPerformanceMetricsStateMETA));
    (get_inst_proc_addr(instance, "xrQueryPerformanceMetricsCounterMETA", (PFN_xrVoidFunction*)&table->QueryPerformanceMetricsCounterMETA));

    // ---- XR_FB_spatial_entity_storage_batch extension commands
    (get_inst_proc_addr(instance, "xrSaveSpaceListFB", (PFN_xrVoidFunction*)&table->SaveSpaceListFB));

    // ---- XR_FB_spatial_entity_user extension commands
    (get_inst_proc_addr(instance, "xrCreateSpaceUserFB", (PFN_xrVoidFunction*)&table->CreateSpaceUserFB));
    (get_inst_proc_addr(instance, "xrGetSpaceUserIdFB", (PFN_xrVoidFunction*)&table->GetSpaceUserIdFB));
    (get_inst_proc_addr(instance, "xrDestroySpaceUserFB", (PFN_xrVoidFunction*)&table->DestroySpaceUserFB));

    // ---- XR_META_spatial_entity_discovery extension commands
    (get_inst_proc_addr(instance, "xrDiscoverSpacesMETA", (PFN_xrVoidFunction*)&table->DiscoverSpacesMETA));
    (get_inst_proc_addr(instance, "xrRetrieveSpaceDiscoveryResultsMETA", (PFN_xrVoidFunction*)&table->RetrieveSpaceDiscoveryResultsMETA));

    // ---- XR_META_recommended_layer_resolution extension commands
    (get_inst_proc_addr(instance, "xrGetRecommendedLayerResolutionMETA", (PFN_xrVoidFunction*)&table->GetRecommendedLayerResolutionMETA));

    // ---- XR_META_spatial_entity_persistence extension commands
    (get_inst_proc_addr(instance, "xrSaveSpacesMETA", (PFN_xrVoidFunction*)&table->SaveSpacesMETA));
    (get_inst_proc_addr(instance, "xrEraseSpacesMETA", (PFN_xrVoidFunction*)&table->EraseSpacesMETA));

    // ---- XR_META_passthrough_color_lut extension commands
    (get_inst_proc_addr(instance, "xrCreatePassthroughColorLutMETA", (PFN_xrVoidFunction*)&table->CreatePassthroughColorLutMETA));
    (get_inst_proc_addr(instance, "xrDestroyPassthroughColorLutMETA", (PFN_xrVoidFunction*)&table->DestroyPassthroughColorLutMETA));
    (get_inst_proc_addr(instance, "xrUpdatePassthroughColorLutMETA", (PFN_xrVoidFunction*)&table->UpdatePassthroughColorLutMETA));

    // ---- XR_META_spatial_entity_mesh extension commands
    (get_inst_proc_addr(instance, "xrGetSpaceTriangleMeshMETA", (PFN_xrVoidFunction*)&table->GetSpaceTriangleMeshMETA));

    // ---- XR_META_body_tracking_calibration extension commands
    (get_inst_proc_addr(instance, "xrSuggestBodyTrackingCalibrationOverrideMETA", (PFN_xrVoidFunction*)&table->SuggestBodyTrackingCalibrationOverrideMETA));
    (get_inst_proc_addr(instance, "xrResetBodyTrackingCalibrationMETA", (PFN_xrVoidFunction*)&table->ResetBodyTrackingCalibrationMETA));

    // ---- XR_META_body_tracking_fidelity extension commands
    (get_inst_proc_addr(instance, "xrRequestBodyTrackingFidelityMETA", (PFN_xrVoidFunction*)&table->RequestBodyTrackingFidelityMETA));

    // ---- XR_FB_face_tracking2 extension commands
    (get_inst_proc_addr(instance, "xrCreateFaceTracker2FB", (PFN_xrVoidFunction*)&table->CreateFaceTracker2FB));
    (get_inst_proc_addr(instance, "xrDestroyFaceTracker2FB", (PFN_xrVoidFunction*)&table->DestroyFaceTracker2FB));
    (get_inst_proc_addr(instance, "xrGetFaceExpressionWeights2FB", (PFN_xrVoidFunction*)&table->GetFaceExpressionWeights2FB));

    // ---- XR_META_spatial_entity_sharing extension commands
    (get_inst_proc_addr(instance, "xrShareSpacesMETA", (PFN_xrVoidFunction*)&table->ShareSpacesMETA));

    // ---- XR_META_environment_depth extension commands
    (get_inst_proc_addr(instance, "xrCreateEnvironmentDepthProviderMETA", (PFN_xrVoidFunction*)&table->CreateEnvironmentDepthProviderMETA));
    (get_inst_proc_addr(instance, "xrDestroyEnvironmentDepthProviderMETA", (PFN_xrVoidFunction*)&table->DestroyEnvironmentDepthProviderMETA));
    (get_inst_proc_addr(instance, "xrStartEnvironmentDepthProviderMETA", (PFN_xrVoidFunction*)&table->StartEnvironmentDepthProviderMETA));
    (get_inst_proc_addr(instance, "xrStopEnvironmentDepthProviderMETA", (PFN_xrVoidFunction*)&table->StopEnvironmentDepthProviderMETA));
    (get_inst_proc_addr(instance, "xrCreateEnvironmentDepthSwapchainMETA", (PFN_xrVoidFunction*)&table->CreateEnvironmentDepthSwapchainMETA));
    (get_inst_proc_addr(instance, "xrDestroyEnvironmentDepthSwapchainMETA", (PFN_xrVoidFunction*)&table->DestroyEnvironmentDepthSwapchainMETA));
    (get_inst_proc_addr(instance, "xrEnumerateEnvironmentDepthSwapchainImagesMETA", (PFN_xrVoidFunction*)&table->EnumerateEnvironmentDepthSwapchainImagesMETA));
    (get_inst_proc_addr(instance, "xrGetEnvironmentDepthSwapchainStateMETA", (PFN_xrVoidFunction*)&table->GetEnvironmentDepthSwapchainStateMETA));
    (get_inst_proc_addr(instance, "xrAcquireEnvironmentDepthImageMETA", (PFN_xrVoidFunction*)&table->AcquireEnvironmentDepthImageMETA));
    (get_inst_proc_addr(instance, "xrSetEnvironmentDepthHandRemovalMETA", (PFN_xrVoidFunction*)&table->SetEnvironmentDepthHandRemovalMETA));

    // ---- XR_EXT_render_model extension commands
    (get_inst_proc_addr(instance, "xrCreateRenderModelEXT", (PFN_xrVoidFunction*)&table->CreateRenderModelEXT));
    (get_inst_proc_addr(instance, "xrDestroyRenderModelEXT", (PFN_xrVoidFunction*)&table->DestroyRenderModelEXT));
    (get_inst_proc_addr(instance, "xrGetRenderModelPropertiesEXT", (PFN_xrVoidFunction*)&table->GetRenderModelPropertiesEXT));
    (get_inst_proc_addr(instance, "xrCreateRenderModelSpaceEXT", (PFN_xrVoidFunction*)&table->CreateRenderModelSpaceEXT));
    (get_inst_proc_addr(instance, "xrCreateRenderModelAssetEXT", (PFN_xrVoidFunction*)&table->CreateRenderModelAssetEXT));
    (get_inst_proc_addr(instance, "xrDestroyRenderModelAssetEXT", (PFN_xrVoidFunction*)&table->DestroyRenderModelAssetEXT));
    (get_inst_proc_addr(instance, "xrGetRenderModelAssetDataEXT", (PFN_xrVoidFunction*)&table->GetRenderModelAssetDataEXT));
    (get_inst_proc_addr(instance, "xrGetRenderModelAssetPropertiesEXT", (PFN_xrVoidFunction*)&table->GetRenderModelAssetPropertiesEXT));
    (get_inst_proc_addr(instance, "xrGetRenderModelStateEXT", (PFN_xrVoidFunction*)&table->GetRenderModelStateEXT));

    // ---- XR_EXT_interaction_render_model extension commands
    (get_inst_proc_addr(instance, "xrEnumerateInteractionRenderModelIdsEXT", (PFN_xrVoidFunction*)&table->EnumerateInteractionRenderModelIdsEXT));
    (get_inst_proc_addr(instance, "xrEnumerateRenderModelSubactionPathsEXT", (PFN_xrVoidFunction*)&table->EnumerateRenderModelSubactionPathsEXT));
    (get_inst_proc_addr(instance, "xrGetRenderModelPoseTopLevelUserPathEXT", (PFN_xrVoidFunction*)&table->GetRenderModelPoseTopLevelUserPathEXT));

    // ---- XR_QCOM_tracking_optimization_settings extension commands
    (get_inst_proc_addr(instance, "xrSetTrackingOptimizationSettingsHintQCOM", (PFN_xrVoidFunction*)&table->SetTrackingOptimizationSettingsHintQCOM));

    // ---- XR_QCOM_hand_tracking_gesture extension commands
    (get_inst_proc_addr(instance, "xrGetHandGestureQCOM", (PFN_xrVoidFunction*)&table->GetHandGestureQCOM));

    // ---- XR_HTC_passthrough extension commands
    (get_inst_proc_addr(instance, "xrCreatePassthroughHTC", (PFN_xrVoidFunction*)&table->CreatePassthroughHTC));
    (get_inst_proc_addr(instance, "xrDestroyPassthroughHTC", (PFN_xrVoidFunction*)&table->DestroyPassthroughHTC));

    // ---- XR_HTC_foveation extension commands
    (get_inst_proc_addr(instance, "xrApplyFoveationHTC", (PFN_xrVoidFunction*)&table->ApplyFoveationHTC));

    // ---- XR_HTC_anchor extension commands
    (get_inst_proc_addr(instance, "xrCreateSpatialAnchorHTC", (PFN_xrVoidFunction*)&table->CreateSpatialAnchorHTC));
    (get_inst_proc_addr(instance, "xrGetSpatialAnchorNameHTC", (PFN_xrVoidFunction*)&table->GetSpatialAnchorNameHTC));

    // ---- XR_HTC_body_tracking extension commands
    (get_inst_proc_addr(instance, "xrCreateBodyTrackerHTC", (PFN_xrVoidFunction*)&table->CreateBodyTrackerHTC));
    (get_inst_proc_addr(instance, "xrDestroyBodyTrackerHTC", (PFN_xrVoidFunction*)&table->DestroyBodyTrackerHTC));
    (get_inst_proc_addr(instance, "xrLocateBodyJointsHTC", (PFN_xrVoidFunction*)&table->LocateBodyJointsHTC));
    (get_inst_proc_addr(instance, "xrGetBodySkeletonHTC", (PFN_xrVoidFunction*)&table->GetBodySkeletonHTC));

    // ---- XR_MNDX_force_feedback_curl extension commands
    (get_inst_proc_addr(instance, "xrApplyForceFeedbackCurlMNDX", (PFN_xrVoidFunction*)&table->ApplyForceFeedbackCurlMNDX));

    // ---- XR_BD_body_tracking extension commands
    (get_inst_proc_addr(instance, "xrCreateBodyTrackerBD", (PFN_xrVoidFunction*)&table->CreateBodyTrackerBD));
    (get_inst_proc_addr(instance, "xrDestroyBodyTrackerBD", (PFN_xrVoidFunction*)&table->DestroyBodyTrackerBD));
    (get_inst_proc_addr(instance, "xrLocateBodyJointsBD", (PFN_xrVoidFunction*)&table->LocateBodyJointsBD));

    // ---- XR_BD_facial_simulation extension commands
    (get_inst_proc_addr(instance, "xrEnumerateFacialSimulationModesBD", (PFN_xrVoidFunction*)&table->EnumerateFacialSimulationModesBD));
    (get_inst_proc_addr(instance, "xrCreateFaceTrackerBD", (PFN_xrVoidFunction*)&table->CreateFaceTrackerBD));
    (get_inst_proc_addr(instance, "xrDestroyFaceTrackerBD", (PFN_xrVoidFunction*)&table->DestroyFaceTrackerBD));
    (get_inst_proc_addr(instance, "xrGetFacialSimulationDataBD", (PFN_xrVoidFunction*)&table->GetFacialSimulationDataBD));
    (get_inst_proc_addr(instance, "xrSetFacialSimulationModeBD", (PFN_xrVoidFunction*)&table->SetFacialSimulationModeBD));
    (get_inst_proc_addr(instance, "xrGetFacialSimulationModeBD", (PFN_xrVoidFunction*)&table->GetFacialSimulationModeBD));

    // ---- XR_BD_spatial_sensing extension commands
    (get_inst_proc_addr(instance, "xrEnumerateSpatialEntityComponentTypesBD", (PFN_xrVoidFunction*)&table->EnumerateSpatialEntityComponentTypesBD));
    (get_inst_proc_addr(instance, "xrGetSpatialEntityUuidBD", (PFN_xrVoidFunction*)&table->GetSpatialEntityUuidBD));
    (get_inst_proc_addr(instance, "xrGetSpatialEntityComponentDataBD", (PFN_xrVoidFunction*)&table->GetSpatialEntityComponentDataBD));
    (get_inst_proc_addr(instance, "xrCreateSenseDataProviderBD", (PFN_xrVoidFunction*)&table->CreateSenseDataProviderBD));
    (get_inst_proc_addr(instance, "xrStartSenseDataProviderAsyncBD", (PFN_xrVoidFunction*)&table->StartSenseDataProviderAsyncBD));
    (get_inst_proc_addr(instance, "xrStartSenseDataProviderCompleteBD", (PFN_xrVoidFunction*)&table->StartSenseDataProviderCompleteBD));
    (get_inst_proc_addr(instance, "xrGetSenseDataProviderStateBD", (PFN_xrVoidFunction*)&table->GetSenseDataProviderStateBD));
    (get_inst_proc_addr(instance, "xrQuerySenseDataAsyncBD", (PFN_xrVoidFunction*)&table->QuerySenseDataAsyncBD));
    (get_inst_proc_addr(instance, "xrQuerySenseDataCompleteBD", (PFN_xrVoidFunction*)&table->QuerySenseDataCompleteBD));
    (get_inst_proc_addr(instance, "xrDestroySenseDataSnapshotBD", (PFN_xrVoidFunction*)&table->DestroySenseDataSnapshotBD));
    (get_inst_proc_addr(instance, "xrGetQueriedSenseDataBD", (PFN_xrVoidFunction*)&table->GetQueriedSenseDataBD));
    (get_inst_proc_addr(instance, "xrStopSenseDataProviderBD", (PFN_xrVoidFunction*)&table->StopSenseDataProviderBD));
    (get_inst_proc_addr(instance, "xrDestroySenseDataProviderBD", (PFN_xrVoidFunction*)&table->DestroySenseDataProviderBD));
    (get_inst_proc_addr(instance, "xrCreateSpatialEntityAnchorBD", (PFN_xrVoidFunction*)&table->CreateSpatialEntityAnchorBD));
    (get_inst_proc_addr(instance, "xrDestroyAnchorBD", (PFN_xrVoidFunction*)&table->DestroyAnchorBD));
    (get_inst_proc_addr(instance, "xrGetAnchorUuidBD", (PFN_xrVoidFunction*)&table->GetAnchorUuidBD));
    (get_inst_proc_addr(instance, "xrCreateAnchorSpaceBD", (PFN_xrVoidFunction*)&table->CreateAnchorSpaceBD));

    // ---- XR_BD_spatial_anchor extension commands
    (get_inst_proc_addr(instance, "xrCreateSpatialAnchorAsyncBD", (PFN_xrVoidFunction*)&table->CreateSpatialAnchorAsyncBD));
    (get_inst_proc_addr(instance, "xrCreateSpatialAnchorCompleteBD", (PFN_xrVoidFunction*)&table->CreateSpatialAnchorCompleteBD));
    (get_inst_proc_addr(instance, "xrPersistSpatialAnchorAsyncBD", (PFN_xrVoidFunction*)&table->PersistSpatialAnchorAsyncBD));
    (get_inst_proc_addr(instance, "xrPersistSpatialAnchorCompleteBD", (PFN_xrVoidFunction*)&table->PersistSpatialAnchorCompleteBD));
    (get_inst_proc_addr(instance, "xrUnpersistSpatialAnchorAsyncBD", (PFN_xrVoidFunction*)&table->UnpersistSpatialAnchorAsyncBD));
    (get_inst_proc_addr(instance, "xrUnpersistSpatialAnchorCompleteBD", (PFN_xrVoidFunction*)&table->UnpersistSpatialAnchorCompleteBD));

    // ---- XR_BD_spatial_anchor_sharing extension commands
    (get_inst_proc_addr(instance, "xrShareSpatialAnchorAsyncBD", (PFN_xrVoidFunction*)&table->ShareSpatialAnchorAsyncBD));
    (get_inst_proc_addr(instance, "xrShareSpatialAnchorCompleteBD", (PFN_xrVoidFunction*)&table->ShareSpatialAnchorCompleteBD));
    (get_inst_proc_addr(instance, "xrDownloadSharedSpatialAnchorAsyncBD", (PFN_xrVoidFunction*)&table->DownloadSharedSpatialAnchorAsyncBD));
    (get_inst_proc_addr(instance, "xrDownloadSharedSpatialAnchorCompleteBD", (PFN_xrVoidFunction*)&table->DownloadSharedSpatialAnchorCompleteBD));

    // ---- XR_BD_spatial_scene extension commands
    (get_inst_proc_addr(instance, "xrCaptureSceneAsyncBD", (PFN_xrVoidFunction*)&table->CaptureSceneAsyncBD));
    (get_inst_proc_addr(instance, "xrCaptureSceneCompleteBD", (PFN_xrVoidFunction*)&table->CaptureSceneCompleteBD));

    // ---- XR_BD_body_tracking_auxiliary_metrics extension commands
    (get_inst_proc_addr(instance, "xrStartBodyTrackingCalibrationAppBD", (PFN_xrVoidFunction*)&table->StartBodyTrackingCalibrationAppBD));
    (get_inst_proc_addr(instance, "xrGetBodyTrackingStateBD", (PFN_xrVoidFunction*)&table->GetBodyTrackingStateBD));

    // ---- XR_BD_spatial_light_estimation extension commands
    (get_inst_proc_addr(instance, "xrEnumerateEnvironmentTextureResolutionsBD", (PFN_xrVoidFunction*)&table->EnumerateEnvironmentTextureResolutionsBD));
    (get_inst_proc_addr(instance, "xrEnumerateEnvironmentTexturePixelFormatsBD", (PFN_xrVoidFunction*)&table->EnumerateEnvironmentTexturePixelFormatsBD));
    (get_inst_proc_addr(instance, "xrEnumerateEnvironmentTextureTransferTypesBD", (PFN_xrVoidFunction*)&table->EnumerateEnvironmentTextureTransferTypesBD));

    // ---- XR_BD_spatial_audio_rendering extension commands
    (get_inst_proc_addr(instance, "xrEnumerateSupportedAudioSampleRateBD", (PFN_xrVoidFunction*)&table->EnumerateSupportedAudioSampleRateBD));
    (get_inst_proc_addr(instance, "xrQueryFramesPerBufferRangeBD", (PFN_xrVoidFunction*)&table->QueryFramesPerBufferRangeBD));
    (get_inst_proc_addr(instance, "xrCreateSpatialAudioRendererBD", (PFN_xrVoidFunction*)&table->CreateSpatialAudioRendererBD));
    (get_inst_proc_addr(instance, "xrDestroySpatialAudioRendererBD", (PFN_xrVoidFunction*)&table->DestroySpatialAudioRendererBD));
    (get_inst_proc_addr(instance, "xrCreateSoundObstacleMaterialBD", (PFN_xrVoidFunction*)&table->CreateSoundObstacleMaterialBD));
    (get_inst_proc_addr(instance, "xrUpdateSoundObstacleMaterialConfigBD", (PFN_xrVoidFunction*)&table->UpdateSoundObstacleMaterialConfigBD));
    (get_inst_proc_addr(instance, "xrDestroySoundObstacleMaterialBD", (PFN_xrVoidFunction*)&table->DestroySoundObstacleMaterialBD));
    (get_inst_proc_addr(instance, "xrCreateSoundObstacleBD", (PFN_xrVoidFunction*)&table->CreateSoundObstacleBD));
    (get_inst_proc_addr(instance, "xrUpdateSoundObstacleConfigBD", (PFN_xrVoidFunction*)&table->UpdateSoundObstacleConfigBD));
    (get_inst_proc_addr(instance, "xrDestroySoundObstacleBD", (PFN_xrVoidFunction*)&table->DestroySoundObstacleBD));
    (get_inst_proc_addr(instance, "xrCreateSoundObjectBD", (PFN_xrVoidFunction*)&table->CreateSoundObjectBD));
    (get_inst_proc_addr(instance, "xrUpdateSoundObjectConfigBD", (PFN_xrVoidFunction*)&table->UpdateSoundObjectConfigBD));
    (get_inst_proc_addr(instance, "xrSubmitSoundObjectBufferBD", (PFN_xrVoidFunction*)&table->SubmitSoundObjectBufferBD));
    (get_inst_proc_addr(instance, "xrDestroySoundObjectBD", (PFN_xrVoidFunction*)&table->DestroySoundObjectBD));
    (get_inst_proc_addr(instance, "xrCreateSoundFieldBD", (PFN_xrVoidFunction*)&table->CreateSoundFieldBD));
    (get_inst_proc_addr(instance, "xrUpdateSoundFieldConfigBD", (PFN_xrVoidFunction*)&table->UpdateSoundFieldConfigBD));
    (get_inst_proc_addr(instance, "xrSubmitSoundFieldBufferBD", (PFN_xrVoidFunction*)&table->SubmitSoundFieldBufferBD));
    (get_inst_proc_addr(instance, "xrDestroySoundFieldBD", (PFN_xrVoidFunction*)&table->DestroySoundFieldBD));
    (get_inst_proc_addr(instance, "xrWaitAudioPeriodBD", (PFN_xrVoidFunction*)&table->WaitAudioPeriodBD));
    (get_inst_proc_addr(instance, "xrEndAudioPeriodBD", (PFN_xrVoidFunction*)&table->EndAudioPeriodBD));

    // ---- XR_EXT_plane_detection extension commands
    (get_inst_proc_addr(instance, "xrCreatePlaneDetectorEXT", (PFN_xrVoidFunction*)&table->CreatePlaneDetectorEXT));
    (get_inst_proc_addr(instance, "xrDestroyPlaneDetectorEXT", (PFN_xrVoidFunction*)&table->DestroyPlaneDetectorEXT));
    (get_inst_proc_addr(instance, "xrBeginPlaneDetectionEXT", (PFN_xrVoidFunction*)&table->BeginPlaneDetectionEXT));
    (get_inst_proc_addr(instance, "xrGetPlaneDetectionStateEXT", (PFN_xrVoidFunction*)&table->GetPlaneDetectionStateEXT));
    (get_inst_proc_addr(instance, "xrGetPlaneDetectionsEXT", (PFN_xrVoidFunction*)&table->GetPlaneDetectionsEXT));
    (get_inst_proc_addr(instance, "xrGetPlanePolygonBufferEXT", (PFN_xrVoidFunction*)&table->GetPlanePolygonBufferEXT));

    // ---- XR_ANDROID_trackables extension commands
    (get_inst_proc_addr(instance, "xrEnumerateSupportedTrackableTypesANDROID", (PFN_xrVoidFunction*)&table->EnumerateSupportedTrackableTypesANDROID));
    (get_inst_proc_addr(instance, "xrEnumerateSupportedAnchorTrackableTypesANDROID", (PFN_xrVoidFunction*)&table->EnumerateSupportedAnchorTrackableTypesANDROID));
    (get_inst_proc_addr(instance, "xrCreateTrackableTrackerANDROID", (PFN_xrVoidFunction*)&table->CreateTrackableTrackerANDROID));
    (get_inst_proc_addr(instance, "xrDestroyTrackableTrackerANDROID", (PFN_xrVoidFunction*)&table->DestroyTrackableTrackerANDROID));
    (get_inst_proc_addr(instance, "xrGetAllTrackablesANDROID", (PFN_xrVoidFunction*)&table->GetAllTrackablesANDROID));
    (get_inst_proc_addr(instance, "xrGetTrackablePlaneANDROID", (PFN_xrVoidFunction*)&table->GetTrackablePlaneANDROID));
    (get_inst_proc_addr(instance, "xrCreateAnchorSpaceANDROID", (PFN_xrVoidFunction*)&table->CreateAnchorSpaceANDROID));

    // ---- XR_ANDROID_eye_tracking extension commands
    (get_inst_proc_addr(instance, "xrCreateEyeTrackerANDROID", (PFN_xrVoidFunction*)&table->CreateEyeTrackerANDROID));
    (get_inst_proc_addr(instance, "xrDestroyEyeTrackerANDROID", (PFN_xrVoidFunction*)&table->DestroyEyeTrackerANDROID));
    (get_inst_proc_addr(instance, "xrGetFineTrackingEyesInfoANDROID", (PFN_xrVoidFunction*)&table->GetFineTrackingEyesInfoANDROID));
    (get_inst_proc_addr(instance, "xrGetCoarseTrackingEyesInfoANDROID", (PFN_xrVoidFunction*)&table->GetCoarseTrackingEyesInfoANDROID));

    // ---- XR_ANDROID_device_anchor_persistence extension commands
    (get_inst_proc_addr(instance, "xrEnumerateSupportedPersistenceAnchorTypesANDROID", (PFN_xrVoidFunction*)&table->EnumerateSupportedPersistenceAnchorTypesANDROID));
    (get_inst_proc_addr(instance, "xrCreateDeviceAnchorPersistenceANDROID", (PFN_xrVoidFunction*)&table->CreateDeviceAnchorPersistenceANDROID));
    (get_inst_proc_addr(instance, "xrDestroyDeviceAnchorPersistenceANDROID", (PFN_xrVoidFunction*)&table->DestroyDeviceAnchorPersistenceANDROID));
    (get_inst_proc_addr(instance, "xrPersistAnchorANDROID", (PFN_xrVoidFunction*)&table->PersistAnchorANDROID));
    (get_inst_proc_addr(instance, "xrGetAnchorPersistStateANDROID", (PFN_xrVoidFunction*)&table->GetAnchorPersistStateANDROID));
    (get_inst_proc_addr(instance, "xrCreatePersistedAnchorSpaceANDROID", (PFN_xrVoidFunction*)&table->CreatePersistedAnchorSpaceANDROID));
    (get_inst_proc_addr(instance, "xrEnumeratePersistedAnchorsANDROID", (PFN_xrVoidFunction*)&table->EnumeratePersistedAnchorsANDROID));
    (get_inst_proc_addr(instance, "xrUnpersistAnchorANDROID", (PFN_xrVoidFunction*)&table->UnpersistAnchorANDROID));

    // ---- XR_ANDROID_face_tracking extension commands
    (get_inst_proc_addr(instance, "xrCreateFaceTrackerANDROID", (PFN_xrVoidFunction*)&table->CreateFaceTrackerANDROID));
    (get_inst_proc_addr(instance, "xrDestroyFaceTrackerANDROID", (PFN_xrVoidFunction*)&table->DestroyFaceTrackerANDROID));
    (get_inst_proc_addr(instance, "xrGetFaceStateANDROID", (PFN_xrVoidFunction*)&table->GetFaceStateANDROID));
    (get_inst_proc_addr(instance, "xrGetFaceCalibrationStateANDROID", (PFN_xrVoidFunction*)&table->GetFaceCalibrationStateANDROID));

    // ---- XR_ANDROID_passthrough_camera_state extension commands
    (get_inst_proc_addr(instance, "xrGetPassthroughCameraStateANDROID", (PFN_xrVoidFunction*)&table->GetPassthroughCameraStateANDROID));

    // ---- XR_ANDROID_composition_layer_passthrough_mesh extension commands
    (get_inst_proc_addr(instance, "xrCreatePassthroughLayerANDROID", (PFN_xrVoidFunction*)&table->CreatePassthroughLayerANDROID));
    (get_inst_proc_addr(instance, "xrDestroyPassthroughLayerANDROID", (PFN_xrVoidFunction*)&table->DestroyPassthroughLayerANDROID));
    (get_inst_proc_addr(instance, "xrSetPassthroughLayerMeshANDROID", (PFN_xrVoidFunction*)&table->SetPassthroughLayerMeshANDROID));

    // ---- XR_ANDROID_raycast extension commands
    (get_inst_proc_addr(instance, "xrEnumerateRaycastSupportedTrackableTypesANDROID", (PFN_xrVoidFunction*)&table->EnumerateRaycastSupportedTrackableTypesANDROID));
    (get_inst_proc_addr(instance, "xrRaycastANDROID", (PFN_xrVoidFunction*)&table->RaycastANDROID));

    // ---- XR_ANDROID_performance_metrics extension commands
    (get_inst_proc_addr(instance, "xrEnumeratePerformanceMetricsCounterPathsANDROID", (PFN_xrVoidFunction*)&table->EnumeratePerformanceMetricsCounterPathsANDROID));
    (get_inst_proc_addr(instance, "xrSetPerformanceMetricsStateANDROID", (PFN_xrVoidFunction*)&table->SetPerformanceMetricsStateANDROID));
    (get_inst_proc_addr(instance, "xrGetPerformanceMetricsStateANDROID", (PFN_xrVoidFunction*)&table->GetPerformanceMetricsStateANDROID));
    (get_inst_proc_addr(instance, "xrQueryPerformanceMetricsCounterANDROID", (PFN_xrVoidFunction*)&table->QueryPerformanceMetricsCounterANDROID));

    // ---- XR_ANDROID_trackables_object extension commands
    (get_inst_proc_addr(instance, "xrGetTrackableObjectANDROID", (PFN_xrVoidFunction*)&table->GetTrackableObjectANDROID));

    // ---- XR_EXT_future extension commands
    (get_inst_proc_addr(instance, "xrPollFutureEXT", (PFN_xrVoidFunction*)&table->PollFutureEXT));
    (get_inst_proc_addr(instance, "xrCancelFutureEXT", (PFN_xrVoidFunction*)&table->CancelFutureEXT));

    // ---- XR_ML_user_calibration extension commands
    (get_inst_proc_addr(instance, "xrEnableUserCalibrationEventsML", (PFN_xrVoidFunction*)&table->EnableUserCalibrationEventsML));

    // ---- XR_ML_system_notifications extension commands
    (get_inst_proc_addr(instance, "xrSetSystemNotificationsML", (PFN_xrVoidFunction*)&table->SetSystemNotificationsML));

    // ---- XR_ML_world_mesh_detection extension commands
    (get_inst_proc_addr(instance, "xrCreateWorldMeshDetectorML", (PFN_xrVoidFunction*)&table->CreateWorldMeshDetectorML));
    (get_inst_proc_addr(instance, "xrDestroyWorldMeshDetectorML", (PFN_xrVoidFunction*)&table->DestroyWorldMeshDetectorML));
    (get_inst_proc_addr(instance, "xrRequestWorldMeshStateAsyncML", (PFN_xrVoidFunction*)&table->RequestWorldMeshStateAsyncML));
    (get_inst_proc_addr(instance, "xrRequestWorldMeshStateCompleteML", (PFN_xrVoidFunction*)&table->RequestWorldMeshStateCompleteML));
    (get_inst_proc_addr(instance, "xrGetWorldMeshBufferRecommendSizeML", (PFN_xrVoidFunction*)&table->GetWorldMeshBufferRecommendSizeML));
    (get_inst_proc_addr(instance, "xrAllocateWorldMeshBufferML", (PFN_xrVoidFunction*)&table->AllocateWorldMeshBufferML));
    (get_inst_proc_addr(instance, "xrFreeWorldMeshBufferML", (PFN_xrVoidFunction*)&table->FreeWorldMeshBufferML));
    (get_inst_proc_addr(instance, "xrRequestWorldMeshAsyncML", (PFN_xrVoidFunction*)&table->RequestWorldMeshAsyncML));
    (get_inst_proc_addr(instance, "xrRequestWorldMeshCompleteML", (PFN_xrVoidFunction*)&table->RequestWorldMeshCompleteML));

    // ---- XR_ML_facial_expression extension commands
    (get_inst_proc_addr(instance, "xrCreateFacialExpressionClientML", (PFN_xrVoidFunction*)&table->CreateFacialExpressionClientML));
    (get_inst_proc_addr(instance, "xrDestroyFacialExpressionClientML", (PFN_xrVoidFunction*)&table->DestroyFacialExpressionClientML));
    (get_inst_proc_addr(instance, "xrGetFacialExpressionBlendShapePropertiesML", (PFN_xrVoidFunction*)&table->GetFacialExpressionBlendShapePropertiesML));

    // ---- XR_META_boundary_visibility extension commands
    (get_inst_proc_addr(instance, "xrRequestBoundaryVisibilityMETA", (PFN_xrVoidFunction*)&table->RequestBoundaryVisibilityMETA));

    // ---- XR_META_simultaneous_hands_and_controllers extension commands
    (get_inst_proc_addr(instance, "xrResumeSimultaneousHandsAndControllersTrackingMETA", (PFN_xrVoidFunction*)&table->ResumeSimultaneousHandsAndControllersTrackingMETA));
    (get_inst_proc_addr(instance, "xrPauseSimultaneousHandsAndControllersTrackingMETA", (PFN_xrVoidFunction*)&table->PauseSimultaneousHandsAndControllersTrackingMETA));

    // ---- XR_META_spatial_entity_room_mesh extension commands
    (get_inst_proc_addr(instance, "xrGetSpaceRoomMeshMETA", (PFN_xrVoidFunction*)&table->GetSpaceRoomMeshMETA));
    (get_inst_proc_addr(instance, "xrGetSpaceRoomMeshFaceIndicesMETA", (PFN_xrVoidFunction*)&table->GetSpaceRoomMeshFaceIndicesMETA));

    // ---- XR_META_colocation_discovery extension commands
    (get_inst_proc_addr(instance, "xrStartColocationDiscoveryMETA", (PFN_xrVoidFunction*)&table->StartColocationDiscoveryMETA));
    (get_inst_proc_addr(instance, "xrStopColocationDiscoveryMETA", (PFN_xrVoidFunction*)&table->StopColocationDiscoveryMETA));
    (get_inst_proc_addr(instance, "xrStartColocationAdvertisementMETA", (PFN_xrVoidFunction*)&table->StartColocationAdvertisementMETA));
    (get_inst_proc_addr(instance, "xrStopColocationAdvertisementMETA", (PFN_xrVoidFunction*)&table->StopColocationAdvertisementMETA));

    // ---- XR_META_environment_raycast extension commands
    (get_inst_proc_addr(instance, "xrCreateEnvironmentRaycasterAsyncMETA", (PFN_xrVoidFunction*)&table->CreateEnvironmentRaycasterAsyncMETA));
    (get_inst_proc_addr(instance, "xrCreateEnvironmentRaycasterCompleteMETA", (PFN_xrVoidFunction*)&table->CreateEnvironmentRaycasterCompleteMETA));
    (get_inst_proc_addr(instance, "xrDestroyEnvironmentRaycasterMETA", (PFN_xrVoidFunction*)&table->DestroyEnvironmentRaycasterMETA));
    (get_inst_proc_addr(instance, "xrPerformEnvironmentRaycastMETA", (PFN_xrVoidFunction*)&table->PerformEnvironmentRaycastMETA));

    // ---- XR_META_tile_properties_hint extension commands
    (get_inst_proc_addr(instance, "xrSetTilePropertiesHintMETA", (PFN_xrVoidFunction*)&table->SetTilePropertiesHintMETA));

    // ---- XR_META_hand_tracking_frequency_hint extension commands
    (get_inst_proc_addr(instance, "xrSetHandTrackingFrequencyHintMETA", (PFN_xrVoidFunction*)&table->SetHandTrackingFrequencyHintMETA));

    // ---- XR_ANDROID_light_estimation extension commands
    (get_inst_proc_addr(instance, "xrCreateLightEstimatorANDROID", (PFN_xrVoidFunction*)&table->CreateLightEstimatorANDROID));
    (get_inst_proc_addr(instance, "xrDestroyLightEstimatorANDROID", (PFN_xrVoidFunction*)&table->DestroyLightEstimatorANDROID));
    (get_inst_proc_addr(instance, "xrGetLightEstimateANDROID", (PFN_xrVoidFunction*)&table->GetLightEstimateANDROID));

    // ---- XR_ANDROID_anchor_sharing_export extension commands
#if defined(XR_USE_PLATFORM_ANDROID)
    (get_inst_proc_addr(instance, "xrShareAnchorANDROID", (PFN_xrVoidFunction*)&table->ShareAnchorANDROID));
#endif // defined(XR_USE_PLATFORM_ANDROID)
#if defined(XR_USE_PLATFORM_ANDROID)
    (get_inst_proc_addr(instance, "xrUnshareAnchorANDROID", (PFN_xrVoidFunction*)&table->UnshareAnchorANDROID));
#endif // defined(XR_USE_PLATFORM_ANDROID)

    // ---- XR_ANDROID_trackables_marker extension commands
    (get_inst_proc_addr(instance, "xrGetTrackableMarkerANDROID", (PFN_xrVoidFunction*)&table->GetTrackableMarkerANDROID));

    // ---- XR_ANDROID_trackables_qr_code extension commands
    (get_inst_proc_addr(instance, "xrGetTrackableQrCodeANDROID", (PFN_xrVoidFunction*)&table->GetTrackableQrCodeANDROID));

    // ---- XR_ANDROID_trackables_image extension commands
    (get_inst_proc_addr(instance, "xrCreateTrackableImageDatabaseAsyncANDROID", (PFN_xrVoidFunction*)&table->CreateTrackableImageDatabaseAsyncANDROID));
    (get_inst_proc_addr(instance, "xrCreateTrackableImageDatabaseCompleteANDROID", (PFN_xrVoidFunction*)&table->CreateTrackableImageDatabaseCompleteANDROID));
    (get_inst_proc_addr(instance, "xrDestroyTrackableImageDatabaseANDROID", (PFN_xrVoidFunction*)&table->DestroyTrackableImageDatabaseANDROID));
    (get_inst_proc_addr(instance, "xrAddTrackableImageDatabaseANDROID", (PFN_xrVoidFunction*)&table->AddTrackableImageDatabaseANDROID));
    (get_inst_proc_addr(instance, "xrRemoveTrackableImageDatabaseANDROID", (PFN_xrVoidFunction*)&table->RemoveTrackableImageDatabaseANDROID));
    (get_inst_proc_addr(instance, "xrGetTrackableImageANDROID", (PFN_xrVoidFunction*)&table->GetTrackableImageANDROID));

    // ---- XR_ANDROID_scene_meshing extension commands
    (get_inst_proc_addr(instance, "xrEnumerateSupportedSemanticLabelSetsANDROID", (PFN_xrVoidFunction*)&table->EnumerateSupportedSemanticLabelSetsANDROID));
    (get_inst_proc_addr(instance, "xrCreateSceneMeshingTrackerANDROID", (PFN_xrVoidFunction*)&table->CreateSceneMeshingTrackerANDROID));
    (get_inst_proc_addr(instance, "xrDestroySceneMeshingTrackerANDROID", (PFN_xrVoidFunction*)&table->DestroySceneMeshingTrackerANDROID));
    (get_inst_proc_addr(instance, "xrCreateSceneMeshSnapshotANDROID", (PFN_xrVoidFunction*)&table->CreateSceneMeshSnapshotANDROID));
    (get_inst_proc_addr(instance, "xrDestroySceneMeshSnapshotANDROID", (PFN_xrVoidFunction*)&table->DestroySceneMeshSnapshotANDROID));
    (get_inst_proc_addr(instance, "xrGetAllSubmeshStatesANDROID", (PFN_xrVoidFunction*)&table->GetAllSubmeshStatesANDROID));
    (get_inst_proc_addr(instance, "xrGetSubmeshDataANDROID", (PFN_xrVoidFunction*)&table->GetSubmeshDataANDROID));

    // ---- XR_EXT_spatial_entity extension commands
    (get_inst_proc_addr(instance, "xrEnumerateSpatialCapabilitiesEXT", (PFN_xrVoidFunction*)&table->EnumerateSpatialCapabilitiesEXT));
    (get_inst_proc_addr(instance, "xrEnumerateSpatialCapabilityComponentTypesEXT", (PFN_xrVoidFunction*)&table->EnumerateSpatialCapabilityComponentTypesEXT));
    (get_inst_proc_addr(instance, "xrEnumerateSpatialCapabilityFeaturesEXT", (PFN_xrVoidFunction*)&table->EnumerateSpatialCapabilityFeaturesEXT));
    (get_inst_proc_addr(instance, "xrCreateSpatialContextAsyncEXT", (PFN_xrVoidFunction*)&table->CreateSpatialContextAsyncEXT));
    (get_inst_proc_addr(instance, "xrCreateSpatialContextCompleteEXT", (PFN_xrVoidFunction*)&table->CreateSpatialContextCompleteEXT));
    (get_inst_proc_addr(instance, "xrDestroySpatialContextEXT", (PFN_xrVoidFunction*)&table->DestroySpatialContextEXT));
    (get_inst_proc_addr(instance, "xrCreateSpatialDiscoverySnapshotAsyncEXT", (PFN_xrVoidFunction*)&table->CreateSpatialDiscoverySnapshotAsyncEXT));
    (get_inst_proc_addr(instance, "xrCreateSpatialDiscoverySnapshotCompleteEXT", (PFN_xrVoidFunction*)&table->CreateSpatialDiscoverySnapshotCompleteEXT));
    (get_inst_proc_addr(instance, "xrQuerySpatialComponentDataEXT", (PFN_xrVoidFunction*)&table->QuerySpatialComponentDataEXT));
    (get_inst_proc_addr(instance, "xrDestroySpatialSnapshotEXT", (PFN_xrVoidFunction*)&table->DestroySpatialSnapshotEXT));
    (get_inst_proc_addr(instance, "xrCreateSpatialEntityFromIdEXT", (PFN_xrVoidFunction*)&table->CreateSpatialEntityFromIdEXT));
    (get_inst_proc_addr(instance, "xrDestroySpatialEntityEXT", (PFN_xrVoidFunction*)&table->DestroySpatialEntityEXT));
    (get_inst_proc_addr(instance, "xrCreateSpatialUpdateSnapshotEXT", (PFN_xrVoidFunction*)&table->CreateSpatialUpdateSnapshotEXT));
    (get_inst_proc_addr(instance, "xrGetSpatialBufferStringEXT", (PFN_xrVoidFunction*)&table->GetSpatialBufferStringEXT));
    (get_inst_proc_addr(instance, "xrGetSpatialBufferUint8EXT", (PFN_xrVoidFunction*)&table->GetSpatialBufferUint8EXT));
    (get_inst_proc_addr(instance, "xrGetSpatialBufferUint16EXT", (PFN_xrVoidFunction*)&table->GetSpatialBufferUint16EXT));
    (get_inst_proc_addr(instance, "xrGetSpatialBufferUint32EXT", (PFN_xrVoidFunction*)&table->GetSpatialBufferUint32EXT));
    (get_inst_proc_addr(instance, "xrGetSpatialBufferFloatEXT", (PFN_xrVoidFunction*)&table->GetSpatialBufferFloatEXT));
    (get_inst_proc_addr(instance, "xrGetSpatialBufferVector2fEXT", (PFN_xrVoidFunction*)&table->GetSpatialBufferVector2fEXT));
    (get_inst_proc_addr(instance, "xrGetSpatialBufferVector3fEXT", (PFN_xrVoidFunction*)&table->GetSpatialBufferVector3fEXT));

    // ---- XR_EXT_stationary_reference_space extension commands
    (get_inst_proc_addr(instance, "xrGetStationaryReferenceSpaceGenerationIdEXT", (PFN_xrVoidFunction*)&table->GetStationaryReferenceSpaceGenerationIdEXT));

    // ---- XR_BD_camera_image extension commands
    (get_inst_proc_addr(instance, "xrEnumerateAvailableCamerasBD", (PFN_xrVoidFunction*)&table->EnumerateAvailableCamerasBD));
    (get_inst_proc_addr(instance, "xrEnumerateCameraPropertyTypesBD", (PFN_xrVoidFunction*)&table->EnumerateCameraPropertyTypesBD));
    (get_inst_proc_addr(instance, "xrGetCameraPropertiesBD", (PFN_xrVoidFunction*)&table->GetCameraPropertiesBD));
    (get_inst_proc_addr(instance, "xrEnumerateCameraCapabilityTypesBD", (PFN_xrVoidFunction*)&table->EnumerateCameraCapabilityTypesBD));
    (get_inst_proc_addr(instance, "xrGetCameraSupportedCapabilitiesBD", (PFN_xrVoidFunction*)&table->GetCameraSupportedCapabilitiesBD));
    (get_inst_proc_addr(instance, "xrCreateCameraDeviceAsyncBD", (PFN_xrVoidFunction*)&table->CreateCameraDeviceAsyncBD));
    (get_inst_proc_addr(instance, "xrCreateCameraDeviceCompleteBD", (PFN_xrVoidFunction*)&table->CreateCameraDeviceCompleteBD));
    (get_inst_proc_addr(instance, "xrDestroyCameraDeviceBD", (PFN_xrVoidFunction*)&table->DestroyCameraDeviceBD));
    (get_inst_proc_addr(instance, "xrCreateCameraCaptureSessionAsyncBD", (PFN_xrVoidFunction*)&table->CreateCameraCaptureSessionAsyncBD));
    (get_inst_proc_addr(instance, "xrCreateCameraCaptureSessionCompleteBD", (PFN_xrVoidFunction*)&table->CreateCameraCaptureSessionCompleteBD));
    (get_inst_proc_addr(instance, "xrDestroyCameraCaptureSessionBD", (PFN_xrVoidFunction*)&table->DestroyCameraCaptureSessionBD));
    (get_inst_proc_addr(instance, "xrBeginCameraCaptureBD", (PFN_xrVoidFunction*)&table->BeginCameraCaptureBD));
    (get_inst_proc_addr(instance, "xrEndCameraCaptureBD", (PFN_xrVoidFunction*)&table->EndCameraCaptureBD));
    (get_inst_proc_addr(instance, "xrAcquireCameraImageBD", (PFN_xrVoidFunction*)&table->AcquireCameraImageBD));
    (get_inst_proc_addr(instance, "xrGetCameraImageDataBD", (PFN_xrVoidFunction*)&table->GetCameraImageDataBD));
    (get_inst_proc_addr(instance, "xrReleaseCameraImageBD", (PFN_xrVoidFunction*)&table->ReleaseCameraImageBD));

    // ---- XR_EXT_spatial_anchor extension commands
    (get_inst_proc_addr(instance, "xrCreateSpatialAnchorEXT", (PFN_xrVoidFunction*)&table->CreateSpatialAnchorEXT));

    // ---- XR_EXT_spatial_persistence extension commands
    (get_inst_proc_addr(instance, "xrEnumerateSpatialPersistenceScopesEXT", (PFN_xrVoidFunction*)&table->EnumerateSpatialPersistenceScopesEXT));
    (get_inst_proc_addr(instance, "xrCreateSpatialPersistenceContextAsyncEXT", (PFN_xrVoidFunction*)&table->CreateSpatialPersistenceContextAsyncEXT));
    (get_inst_proc_addr(instance, "xrCreateSpatialPersistenceContextCompleteEXT", (PFN_xrVoidFunction*)&table->CreateSpatialPersistenceContextCompleteEXT));
    (get_inst_proc_addr(instance, "xrDestroySpatialPersistenceContextEXT", (PFN_xrVoidFunction*)&table->DestroySpatialPersistenceContextEXT));

    // ---- XR_EXT_haptic_parametric extension commands
    (get_inst_proc_addr(instance, "xrHapticParametricGetPropertiesEXT", (PFN_xrVoidFunction*)&table->HapticParametricGetPropertiesEXT));

    // ---- XR_SONY_swapchain_color_space extension commands
    (get_inst_proc_addr(instance, "xrEnumerateColorSpacesSONY", (PFN_xrVoidFunction*)&table->EnumerateColorSpacesSONY));

    // ---- XR_SONY_hdr_metadata extension commands
    (get_inst_proc_addr(instance, "xrSetHdrMetadataSONY", (PFN_xrVoidFunction*)&table->SetHdrMetadataSONY));

    // ---- XR_EXT_spatial_persistence_operations extension commands
    (get_inst_proc_addr(instance, "xrPersistSpatialEntityAsyncEXT", (PFN_xrVoidFunction*)&table->PersistSpatialEntityAsyncEXT));
    (get_inst_proc_addr(instance, "xrPersistSpatialEntityCompleteEXT", (PFN_xrVoidFunction*)&table->PersistSpatialEntityCompleteEXT));
    (get_inst_proc_addr(instance, "xrUnpersistSpatialEntityAsyncEXT", (PFN_xrVoidFunction*)&table->UnpersistSpatialEntityAsyncEXT));
    (get_inst_proc_addr(instance, "xrUnpersistSpatialEntityCompleteEXT", (PFN_xrVoidFunction*)&table->UnpersistSpatialEntityCompleteEXT));

    // ---- XR_EXT_spatial_image_tracking extension commands
    (get_inst_proc_addr(instance, "xrEnumerateSpatialReferenceImageFormatsEXT", (PFN_xrVoidFunction*)&table->EnumerateSpatialReferenceImageFormatsEXT));
    (get_inst_proc_addr(instance, "xrCreateSpatialImageTrackingDatabaseAsyncEXT", (PFN_xrVoidFunction*)&table->CreateSpatialImageTrackingDatabaseAsyncEXT));
    (get_inst_proc_addr(instance, "xrCreateSpatialImageTrackingDatabaseCompleteEXT", (PFN_xrVoidFunction*)&table->CreateSpatialImageTrackingDatabaseCompleteEXT));
    (get_inst_proc_addr(instance, "xrDestroySpatialImageTrackingDatabaseEXT", (PFN_xrVoidFunction*)&table->DestroySpatialImageTrackingDatabaseEXT));

    // ---- XR_ANDROID_spatial_discovery_raycast extension commands
    (get_inst_proc_addr(instance, "xrCreateSpatialRaycastSnapshotANDROID", (PFN_xrVoidFunction*)&table->CreateSpatialRaycastSnapshotANDROID));

    // ---- XR_ANDROID_google_cloud_auth extension commands
    (get_inst_proc_addr(instance, "xrSetGoogleCloudAuthAsyncANDROID", (PFN_xrVoidFunction*)&table->SetGoogleCloudAuthAsyncANDROID));
    (get_inst_proc_addr(instance, "xrSetGoogleCloudAuthCompleteANDROID", (PFN_xrVoidFunction*)&table->SetGoogleCloudAuthCompleteANDROID));

    // ---- XR_ANDROID_geospatial extension commands
    (get_inst_proc_addr(instance, "xrCreateGeospatialTrackerANDROID", (PFN_xrVoidFunction*)&table->CreateGeospatialTrackerANDROID));
    (get_inst_proc_addr(instance, "xrDestroyGeospatialTrackerANDROID", (PFN_xrVoidFunction*)&table->DestroyGeospatialTrackerANDROID));
    (get_inst_proc_addr(instance, "xrLocateGeospatialPoseFromPoseANDROID", (PFN_xrVoidFunction*)&table->LocateGeospatialPoseFromPoseANDROID));
    (get_inst_proc_addr(instance, "xrLocateGeospatialPoseANDROID", (PFN_xrVoidFunction*)&table->LocateGeospatialPoseANDROID));
    (get_inst_proc_addr(instance, "xrCheckVpsAvailabilityAsyncANDROID", (PFN_xrVoidFunction*)&table->CheckVpsAvailabilityAsyncANDROID));
    (get_inst_proc_addr(instance, "xrCheckVpsAvailabilityCompleteANDROID", (PFN_xrVoidFunction*)&table->CheckVpsAvailabilityCompleteANDROID));

    // ---- XR_ANDROID_spatial_entity_bound_anchor extension commands
    (get_inst_proc_addr(instance, "xrEnumerateSpatialAnchorAttachableComponentsANDROID", (PFN_xrVoidFunction*)&table->EnumerateSpatialAnchorAttachableComponentsANDROID));

    // ---- XR_ANDROID_spatial_anchor_space extension commands
    (get_inst_proc_addr(instance, "xrCreateSpatialAnchorSpaceANDROID", (PFN_xrVoidFunction*)&table->CreateSpatialAnchorSpaceANDROID));
    (get_inst_proc_addr(instance, "xrCreateSpatialAnchorSpaceFromIdANDROID", (PFN_xrVoidFunction*)&table->CreateSpatialAnchorSpaceFromIdANDROID));

    // ---- XR_ANDROID_geospatial_anchor extension commands
    (get_inst_proc_addr(instance, "xrCreateGeospatialAnchorANDROID", (PFN_xrVoidFunction*)&table->CreateGeospatialAnchorANDROID));
    (get_inst_proc_addr(instance, "xrCreateSurfaceAnchorAsyncANDROID", (PFN_xrVoidFunction*)&table->CreateSurfaceAnchorAsyncANDROID));
    (get_inst_proc_addr(instance, "xrCreateSurfaceAnchorCompleteANDROID", (PFN_xrVoidFunction*)&table->CreateSurfaceAnchorCompleteANDROID));

    // ---- XR_EXT_spatial_container extension commands
    (get_inst_proc_addr(instance, "xrCreateSpatialContainerEXT", (PFN_xrVoidFunction*)&table->CreateSpatialContainerEXT));
    (get_inst_proc_addr(instance, "xrDestroySpatialContainerEXT", (PFN_xrVoidFunction*)&table->DestroySpatialContainerEXT));
    (get_inst_proc_addr(instance, "xrCreateSpatialContainerSpaceEXT", (PFN_xrVoidFunction*)&table->CreateSpatialContainerSpaceEXT));
    (get_inst_proc_addr(instance, "xrRequestSpatialContainerVisibleEXT", (PFN_xrVoidFunction*)&table->RequestSpatialContainerVisibleEXT));
    (get_inst_proc_addr(instance, "xrRequestSpatialContainerBoundsModeEXT", (PFN_xrVoidFunction*)&table->RequestSpatialContainerBoundsModeEXT));
    (get_inst_proc_addr(instance, "xrGetSpatialContainerBoundsEXT", (PFN_xrVoidFunction*)&table->GetSpatialContainerBoundsEXT));
    (get_inst_proc_addr(instance, "xrGetSpatialContainerStateEXT", (PFN_xrVoidFunction*)&table->GetSpatialContainerStateEXT));
    (get_inst_proc_addr(instance, "xrEnumerateSupportedSpatialContainerGraphicsPresentationsEXT", (PFN_xrVoidFunction*)&table->EnumerateSupportedSpatialContainerGraphicsPresentationsEXT));

    // ---- XR_EXT_spatial_container_self_rendering extension commands
    (get_inst_proc_addr(instance, "xrBeginSpatialContainerRenderingEXT", (PFN_xrVoidFunction*)&table->BeginSpatialContainerRenderingEXT));
    (get_inst_proc_addr(instance, "xrEndSpatialContainerRenderingEXT", (PFN_xrVoidFunction*)&table->EndSpatialContainerRenderingEXT));
    (get_inst_proc_addr(instance, "xrLocateSpatialContainerViewsEXT", (PFN_xrVoidFunction*)&table->LocateSpatialContainerViewsEXT));
}


#ifdef __cplusplus
} // extern "C"
#endif

