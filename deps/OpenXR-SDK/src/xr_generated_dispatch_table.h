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

#pragma once

#include "xr_dependencies.h"
#include <openxr/openxr.h>
#include <openxr/openxr_platform.h>


#ifdef __cplusplus
extern "C" { 
#endif
// Generated dispatch table
struct XrGeneratedDispatchTable {

    // ---- Core 1.0 commands
    PFN_xrGetInstanceProcAddr GetInstanceProcAddr;
    PFN_xrEnumerateApiLayerProperties EnumerateApiLayerProperties;
    PFN_xrEnumerateInstanceExtensionProperties EnumerateInstanceExtensionProperties;
    PFN_xrCreateInstance CreateInstance;
    PFN_xrDestroyInstance DestroyInstance;
    PFN_xrGetInstanceProperties GetInstanceProperties;
    PFN_xrPollEvent PollEvent;
    PFN_xrResultToString ResultToString;
    PFN_xrStructureTypeToString StructureTypeToString;
    PFN_xrGetSystem GetSystem;
    PFN_xrGetSystemProperties GetSystemProperties;
    PFN_xrEnumerateEnvironmentBlendModes EnumerateEnvironmentBlendModes;
    PFN_xrCreateSession CreateSession;
    PFN_xrDestroySession DestroySession;
    PFN_xrEnumerateReferenceSpaces EnumerateReferenceSpaces;
    PFN_xrCreateReferenceSpace CreateReferenceSpace;
    PFN_xrGetReferenceSpaceBoundsRect GetReferenceSpaceBoundsRect;
    PFN_xrCreateActionSpace CreateActionSpace;
    PFN_xrLocateSpace LocateSpace;
    PFN_xrDestroySpace DestroySpace;
    PFN_xrEnumerateViewConfigurations EnumerateViewConfigurations;
    PFN_xrGetViewConfigurationProperties GetViewConfigurationProperties;
    PFN_xrEnumerateViewConfigurationViews EnumerateViewConfigurationViews;
    PFN_xrEnumerateSwapchainFormats EnumerateSwapchainFormats;
    PFN_xrCreateSwapchain CreateSwapchain;
    PFN_xrDestroySwapchain DestroySwapchain;
    PFN_xrEnumerateSwapchainImages EnumerateSwapchainImages;
    PFN_xrAcquireSwapchainImage AcquireSwapchainImage;
    PFN_xrWaitSwapchainImage WaitSwapchainImage;
    PFN_xrReleaseSwapchainImage ReleaseSwapchainImage;
    PFN_xrBeginSession BeginSession;
    PFN_xrEndSession EndSession;
    PFN_xrRequestExitSession RequestExitSession;
    PFN_xrWaitFrame WaitFrame;
    PFN_xrBeginFrame BeginFrame;
    PFN_xrEndFrame EndFrame;
    PFN_xrLocateViews LocateViews;
    PFN_xrStringToPath StringToPath;
    PFN_xrPathToString PathToString;
    PFN_xrCreateActionSet CreateActionSet;
    PFN_xrDestroyActionSet DestroyActionSet;
    PFN_xrCreateAction CreateAction;
    PFN_xrDestroyAction DestroyAction;
    PFN_xrSuggestInteractionProfileBindings SuggestInteractionProfileBindings;
    PFN_xrAttachSessionActionSets AttachSessionActionSets;
    PFN_xrGetCurrentInteractionProfile GetCurrentInteractionProfile;
    PFN_xrGetActionStateBoolean GetActionStateBoolean;
    PFN_xrGetActionStateFloat GetActionStateFloat;
    PFN_xrGetActionStateVector2f GetActionStateVector2f;
    PFN_xrGetActionStatePose GetActionStatePose;
    PFN_xrSyncActions SyncActions;
    PFN_xrEnumerateBoundSourcesForAction EnumerateBoundSourcesForAction;
    PFN_xrGetInputSourceLocalizedName GetInputSourceLocalizedName;
    PFN_xrApplyHapticFeedback ApplyHapticFeedback;
    PFN_xrStopHapticFeedback StopHapticFeedback;

    // ---- Core 1.1 commands
    PFN_xrLocateSpaces LocateSpaces;

    // ---- XR_KHR_android_thread_settings extension commands
#if defined(XR_USE_PLATFORM_ANDROID)
    PFN_xrSetAndroidApplicationThreadKHR SetAndroidApplicationThreadKHR;
#endif // defined(XR_USE_PLATFORM_ANDROID)

    // ---- XR_KHR_android_surface_swapchain extension commands
#if defined(XR_USE_PLATFORM_ANDROID)
    PFN_xrCreateSwapchainAndroidSurfaceKHR CreateSwapchainAndroidSurfaceKHR;
#endif // defined(XR_USE_PLATFORM_ANDROID)

    // ---- XR_KHR_opengl_enable extension commands
#if defined(XR_USE_GRAPHICS_API_OPENGL)
    PFN_xrGetOpenGLGraphicsRequirementsKHR GetOpenGLGraphicsRequirementsKHR;
#endif // defined(XR_USE_GRAPHICS_API_OPENGL)

    // ---- XR_KHR_opengl_es_enable extension commands
#if defined(XR_USE_GRAPHICS_API_OPENGL_ES)
    PFN_xrGetOpenGLESGraphicsRequirementsKHR GetOpenGLESGraphicsRequirementsKHR;
#endif // defined(XR_USE_GRAPHICS_API_OPENGL_ES)

    // ---- XR_KHR_vulkan_enable extension commands
#if defined(XR_USE_GRAPHICS_API_VULKAN)
    PFN_xrGetVulkanInstanceExtensionsKHR GetVulkanInstanceExtensionsKHR;
#endif // defined(XR_USE_GRAPHICS_API_VULKAN)
#if defined(XR_USE_GRAPHICS_API_VULKAN)
    PFN_xrGetVulkanDeviceExtensionsKHR GetVulkanDeviceExtensionsKHR;
#endif // defined(XR_USE_GRAPHICS_API_VULKAN)
#if defined(XR_USE_GRAPHICS_API_VULKAN)
    PFN_xrGetVulkanGraphicsDeviceKHR GetVulkanGraphicsDeviceKHR;
#endif // defined(XR_USE_GRAPHICS_API_VULKAN)
#if defined(XR_USE_GRAPHICS_API_VULKAN)
    PFN_xrGetVulkanGraphicsRequirementsKHR GetVulkanGraphicsRequirementsKHR;
#endif // defined(XR_USE_GRAPHICS_API_VULKAN)

    // ---- XR_KHR_D3D11_enable extension commands
#if defined(XR_USE_GRAPHICS_API_D3D11)
    PFN_xrGetD3D11GraphicsRequirementsKHR GetD3D11GraphicsRequirementsKHR;
#endif // defined(XR_USE_GRAPHICS_API_D3D11)

    // ---- XR_KHR_D3D12_enable extension commands
#if defined(XR_USE_GRAPHICS_API_D3D12)
    PFN_xrGetD3D12GraphicsRequirementsKHR GetD3D12GraphicsRequirementsKHR;
#endif // defined(XR_USE_GRAPHICS_API_D3D12)

    // ---- XR_KHR_metal_enable extension commands
#if defined(XR_USE_GRAPHICS_API_METAL)
    PFN_xrGetMetalGraphicsRequirementsKHR GetMetalGraphicsRequirementsKHR;
#endif // defined(XR_USE_GRAPHICS_API_METAL)

    // ---- XR_KHR_visibility_mask extension commands
    PFN_xrGetVisibilityMaskKHR GetVisibilityMaskKHR;

    // ---- XR_KHR_win32_convert_performance_counter_time extension commands
#if defined(XR_USE_PLATFORM_WIN32)
    PFN_xrConvertWin32PerformanceCounterToTimeKHR ConvertWin32PerformanceCounterToTimeKHR;
#endif // defined(XR_USE_PLATFORM_WIN32)
#if defined(XR_USE_PLATFORM_WIN32)
    PFN_xrConvertTimeToWin32PerformanceCounterKHR ConvertTimeToWin32PerformanceCounterKHR;
#endif // defined(XR_USE_PLATFORM_WIN32)

    // ---- XR_KHR_convert_timespec_time extension commands
#if defined(XR_USE_TIMESPEC)
    PFN_xrConvertTimespecTimeToTimeKHR ConvertTimespecTimeToTimeKHR;
#endif // defined(XR_USE_TIMESPEC)
#if defined(XR_USE_TIMESPEC)
    PFN_xrConvertTimeToTimespecTimeKHR ConvertTimeToTimespecTimeKHR;
#endif // defined(XR_USE_TIMESPEC)

    // ---- XR_KHR_loader_init extension commands
    PFN_xrInitializeLoaderKHR InitializeLoaderKHR;

    // ---- XR_KHR_vulkan_enable2 extension commands
#if defined(XR_USE_GRAPHICS_API_VULKAN)
    PFN_xrCreateVulkanInstanceKHR CreateVulkanInstanceKHR;
#endif // defined(XR_USE_GRAPHICS_API_VULKAN)
#if defined(XR_USE_GRAPHICS_API_VULKAN)
    PFN_xrCreateVulkanDeviceKHR CreateVulkanDeviceKHR;
#endif // defined(XR_USE_GRAPHICS_API_VULKAN)
#if defined(XR_USE_GRAPHICS_API_VULKAN)
    PFN_xrGetVulkanGraphicsDevice2KHR GetVulkanGraphicsDevice2KHR;
#endif // defined(XR_USE_GRAPHICS_API_VULKAN)
#if defined(XR_USE_GRAPHICS_API_VULKAN)
    PFN_xrGetVulkanGraphicsRequirements2KHR GetVulkanGraphicsRequirements2KHR;
#endif // defined(XR_USE_GRAPHICS_API_VULKAN)

    // ---- XR_KHR_extended_struct_name_lengths extension commands
    PFN_xrStructureTypeToString2KHR StructureTypeToString2KHR;

    // ---- XR_KHR_locate_spaces extension commands
    PFN_xrLocateSpacesKHR LocateSpacesKHR;

    // ---- XR_KHR_extended_result_name_lengths extension commands
    PFN_xrResultToString2KHR ResultToString2KHR;

    // ---- XR_EXT_performance_settings extension commands
    PFN_xrPerfSettingsSetPerformanceLevelEXT PerfSettingsSetPerformanceLevelEXT;

    // ---- XR_EXT_thermal_query extension commands
    PFN_xrThermalGetTemperatureTrendEXT ThermalGetTemperatureTrendEXT;

    // ---- XR_EXT_debug_utils extension commands
    PFN_xrSetDebugUtilsObjectNameEXT SetDebugUtilsObjectNameEXT;
    PFN_xrCreateDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT;
    PFN_xrDestroyDebugUtilsMessengerEXT DestroyDebugUtilsMessengerEXT;
    PFN_xrSubmitDebugUtilsMessageEXT SubmitDebugUtilsMessageEXT;
    PFN_xrSessionBeginDebugUtilsLabelRegionEXT SessionBeginDebugUtilsLabelRegionEXT;
    PFN_xrSessionEndDebugUtilsLabelRegionEXT SessionEndDebugUtilsLabelRegionEXT;
    PFN_xrSessionInsertDebugUtilsLabelEXT SessionInsertDebugUtilsLabelEXT;

    // ---- XR_MSFT_spatial_anchor extension commands
    PFN_xrCreateSpatialAnchorMSFT CreateSpatialAnchorMSFT;
    PFN_xrCreateSpatialAnchorSpaceMSFT CreateSpatialAnchorSpaceMSFT;
    PFN_xrDestroySpatialAnchorMSFT DestroySpatialAnchorMSFT;

    // ---- XR_EXT_conformance_automation extension commands
    PFN_xrSetInputDeviceActiveEXT SetInputDeviceActiveEXT;
    PFN_xrSetInputDeviceStateBoolEXT SetInputDeviceStateBoolEXT;
    PFN_xrSetInputDeviceStateFloatEXT SetInputDeviceStateFloatEXT;
    PFN_xrSetInputDeviceStateVector2fEXT SetInputDeviceStateVector2fEXT;
    PFN_xrSetInputDeviceLocationEXT SetInputDeviceLocationEXT;

    // ---- XR_MSFT_spatial_graph_bridge extension commands
    PFN_xrCreateSpatialGraphNodeSpaceMSFT CreateSpatialGraphNodeSpaceMSFT;
    PFN_xrTryCreateSpatialGraphStaticNodeBindingMSFT TryCreateSpatialGraphStaticNodeBindingMSFT;
    PFN_xrDestroySpatialGraphNodeBindingMSFT DestroySpatialGraphNodeBindingMSFT;
    PFN_xrGetSpatialGraphNodeBindingPropertiesMSFT GetSpatialGraphNodeBindingPropertiesMSFT;

    // ---- XR_EXT_hand_tracking extension commands
    PFN_xrCreateHandTrackerEXT CreateHandTrackerEXT;
    PFN_xrDestroyHandTrackerEXT DestroyHandTrackerEXT;
    PFN_xrLocateHandJointsEXT LocateHandJointsEXT;

    // ---- XR_MSFT_hand_tracking_mesh extension commands
    PFN_xrCreateHandMeshSpaceMSFT CreateHandMeshSpaceMSFT;
    PFN_xrUpdateHandMeshMSFT UpdateHandMeshMSFT;

    // ---- XR_MSFT_controller_model extension commands
    PFN_xrGetControllerModelKeyMSFT GetControllerModelKeyMSFT;
    PFN_xrLoadControllerModelMSFT LoadControllerModelMSFT;
    PFN_xrGetControllerModelPropertiesMSFT GetControllerModelPropertiesMSFT;
    PFN_xrGetControllerModelStateMSFT GetControllerModelStateMSFT;

    // ---- XR_MSFT_perception_anchor_interop extension commands
#if defined(XR_USE_PLATFORM_WIN32)
    PFN_xrCreateSpatialAnchorFromPerceptionAnchorMSFT CreateSpatialAnchorFromPerceptionAnchorMSFT;
#endif // defined(XR_USE_PLATFORM_WIN32)
#if defined(XR_USE_PLATFORM_WIN32)
    PFN_xrTryGetPerceptionAnchorFromSpatialAnchorMSFT TryGetPerceptionAnchorFromSpatialAnchorMSFT;
#endif // defined(XR_USE_PLATFORM_WIN32)

    // ---- XR_MSFT_composition_layer_reprojection extension commands
    PFN_xrEnumerateReprojectionModesMSFT EnumerateReprojectionModesMSFT;

    // ---- XR_FB_swapchain_update_state extension commands
    PFN_xrUpdateSwapchainFB UpdateSwapchainFB;
    PFN_xrGetSwapchainStateFB GetSwapchainStateFB;

    // ---- XR_FB_body_tracking extension commands
    PFN_xrCreateBodyTrackerFB CreateBodyTrackerFB;
    PFN_xrDestroyBodyTrackerFB DestroyBodyTrackerFB;
    PFN_xrLocateBodyJointsFB LocateBodyJointsFB;
    PFN_xrGetBodySkeletonFB GetBodySkeletonFB;

    // ---- XR_MSFT_scene_understanding extension commands
    PFN_xrEnumerateSceneComputeFeaturesMSFT EnumerateSceneComputeFeaturesMSFT;
    PFN_xrCreateSceneObserverMSFT CreateSceneObserverMSFT;
    PFN_xrDestroySceneObserverMSFT DestroySceneObserverMSFT;
    PFN_xrCreateSceneMSFT CreateSceneMSFT;
    PFN_xrDestroySceneMSFT DestroySceneMSFT;
    PFN_xrComputeNewSceneMSFT ComputeNewSceneMSFT;
    PFN_xrGetSceneComputeStateMSFT GetSceneComputeStateMSFT;
    PFN_xrGetSceneComponentsMSFT GetSceneComponentsMSFT;
    PFN_xrLocateSceneComponentsMSFT LocateSceneComponentsMSFT;
    PFN_xrGetSceneMeshBuffersMSFT GetSceneMeshBuffersMSFT;

    // ---- XR_MSFT_scene_understanding_serialization extension commands
    PFN_xrDeserializeSceneMSFT DeserializeSceneMSFT;
    PFN_xrGetSerializedSceneFragmentDataMSFT GetSerializedSceneFragmentDataMSFT;

    // ---- XR_FB_display_refresh_rate extension commands
    PFN_xrEnumerateDisplayRefreshRatesFB EnumerateDisplayRefreshRatesFB;
    PFN_xrGetDisplayRefreshRateFB GetDisplayRefreshRateFB;
    PFN_xrRequestDisplayRefreshRateFB RequestDisplayRefreshRateFB;

    // ---- XR_HTCX_vive_tracker_interaction extension commands
    PFN_xrEnumerateViveTrackerPathsHTCX EnumerateViveTrackerPathsHTCX;

    // ---- XR_HTC_facial_tracking extension commands
    PFN_xrCreateFacialTrackerHTC CreateFacialTrackerHTC;
    PFN_xrDestroyFacialTrackerHTC DestroyFacialTrackerHTC;
    PFN_xrGetFacialExpressionsHTC GetFacialExpressionsHTC;

    // ---- XR_FB_color_space extension commands
    PFN_xrEnumerateColorSpacesFB EnumerateColorSpacesFB;
    PFN_xrSetColorSpaceFB SetColorSpaceFB;

    // ---- XR_FB_hand_tracking_mesh extension commands
    PFN_xrGetHandMeshFB GetHandMeshFB;

    // ---- XR_FB_spatial_entity extension commands
    PFN_xrCreateSpatialAnchorFB CreateSpatialAnchorFB;
    PFN_xrGetSpaceUuidFB GetSpaceUuidFB;
    PFN_xrEnumerateSpaceSupportedComponentsFB EnumerateSpaceSupportedComponentsFB;
    PFN_xrSetSpaceComponentStatusFB SetSpaceComponentStatusFB;
    PFN_xrGetSpaceComponentStatusFB GetSpaceComponentStatusFB;

    // ---- XR_FB_foveation extension commands
    PFN_xrCreateFoveationProfileFB CreateFoveationProfileFB;
    PFN_xrDestroyFoveationProfileFB DestroyFoveationProfileFB;

    // ---- XR_FB_keyboard_tracking extension commands
    PFN_xrQuerySystemTrackedKeyboardFB QuerySystemTrackedKeyboardFB;
    PFN_xrCreateKeyboardSpaceFB CreateKeyboardSpaceFB;

    // ---- XR_FB_triangle_mesh extension commands
    PFN_xrCreateTriangleMeshFB CreateTriangleMeshFB;
    PFN_xrDestroyTriangleMeshFB DestroyTriangleMeshFB;
    PFN_xrTriangleMeshGetVertexBufferFB TriangleMeshGetVertexBufferFB;
    PFN_xrTriangleMeshGetIndexBufferFB TriangleMeshGetIndexBufferFB;
    PFN_xrTriangleMeshBeginUpdateFB TriangleMeshBeginUpdateFB;
    PFN_xrTriangleMeshEndUpdateFB TriangleMeshEndUpdateFB;
    PFN_xrTriangleMeshBeginVertexBufferUpdateFB TriangleMeshBeginVertexBufferUpdateFB;
    PFN_xrTriangleMeshEndVertexBufferUpdateFB TriangleMeshEndVertexBufferUpdateFB;

    // ---- XR_FB_passthrough extension commands
    PFN_xrCreatePassthroughFB CreatePassthroughFB;
    PFN_xrDestroyPassthroughFB DestroyPassthroughFB;
    PFN_xrPassthroughStartFB PassthroughStartFB;
    PFN_xrPassthroughPauseFB PassthroughPauseFB;
    PFN_xrCreatePassthroughLayerFB CreatePassthroughLayerFB;
    PFN_xrDestroyPassthroughLayerFB DestroyPassthroughLayerFB;
    PFN_xrPassthroughLayerPauseFB PassthroughLayerPauseFB;
    PFN_xrPassthroughLayerResumeFB PassthroughLayerResumeFB;
    PFN_xrPassthroughLayerSetStyleFB PassthroughLayerSetStyleFB;
    PFN_xrCreateGeometryInstanceFB CreateGeometryInstanceFB;
    PFN_xrDestroyGeometryInstanceFB DestroyGeometryInstanceFB;
    PFN_xrGeometryInstanceSetTransformFB GeometryInstanceSetTransformFB;

    // ---- XR_FB_render_model extension commands
    PFN_xrEnumerateRenderModelPathsFB EnumerateRenderModelPathsFB;
    PFN_xrGetRenderModelPropertiesFB GetRenderModelPropertiesFB;
    PFN_xrLoadRenderModelFB LoadRenderModelFB;

    // ---- XR_VARJO_environment_depth_estimation extension commands
    PFN_xrSetEnvironmentDepthEstimationVARJO SetEnvironmentDepthEstimationVARJO;

    // ---- XR_VARJO_marker_tracking extension commands
    PFN_xrSetMarkerTrackingVARJO SetMarkerTrackingVARJO;
    PFN_xrSetMarkerTrackingTimeoutVARJO SetMarkerTrackingTimeoutVARJO;
    PFN_xrSetMarkerTrackingPredictionVARJO SetMarkerTrackingPredictionVARJO;
    PFN_xrGetMarkerSizeVARJO GetMarkerSizeVARJO;
    PFN_xrCreateMarkerSpaceVARJO CreateMarkerSpaceVARJO;

    // ---- XR_VARJO_view_offset extension commands
    PFN_xrSetViewOffsetVARJO SetViewOffsetVARJO;

    // ---- XR_ML_compat extension commands
#if defined(XR_USE_PLATFORM_ML)
    PFN_xrCreateSpaceFromCoordinateFrameUIDML CreateSpaceFromCoordinateFrameUIDML;
#endif // defined(XR_USE_PLATFORM_ML)

    // ---- XR_ML_marker_understanding extension commands
    PFN_xrCreateMarkerDetectorML CreateMarkerDetectorML;
    PFN_xrDestroyMarkerDetectorML DestroyMarkerDetectorML;
    PFN_xrSnapshotMarkerDetectorML SnapshotMarkerDetectorML;
    PFN_xrGetMarkerDetectorStateML GetMarkerDetectorStateML;
    PFN_xrGetMarkersML GetMarkersML;
    PFN_xrGetMarkerReprojectionErrorML GetMarkerReprojectionErrorML;
    PFN_xrGetMarkerLengthML GetMarkerLengthML;
    PFN_xrGetMarkerNumberML GetMarkerNumberML;
    PFN_xrGetMarkerStringML GetMarkerStringML;
    PFN_xrCreateMarkerSpaceML CreateMarkerSpaceML;

    // ---- XR_ML_localization_map extension commands
    PFN_xrEnableLocalizationEventsML EnableLocalizationEventsML;
    PFN_xrQueryLocalizationMapsML QueryLocalizationMapsML;
    PFN_xrRequestMapLocalizationML RequestMapLocalizationML;
    PFN_xrImportLocalizationMapML ImportLocalizationMapML;
    PFN_xrCreateExportedLocalizationMapML CreateExportedLocalizationMapML;
    PFN_xrDestroyExportedLocalizationMapML DestroyExportedLocalizationMapML;
    PFN_xrGetExportedLocalizationMapDataML GetExportedLocalizationMapDataML;

    // ---- XR_ML_spatial_anchors extension commands
    PFN_xrCreateSpatialAnchorsAsyncML CreateSpatialAnchorsAsyncML;
    PFN_xrCreateSpatialAnchorsCompleteML CreateSpatialAnchorsCompleteML;
    PFN_xrGetSpatialAnchorStateML GetSpatialAnchorStateML;

    // ---- XR_ML_spatial_anchors_storage extension commands
    PFN_xrCreateSpatialAnchorsStorageML CreateSpatialAnchorsStorageML;
    PFN_xrDestroySpatialAnchorsStorageML DestroySpatialAnchorsStorageML;
    PFN_xrQuerySpatialAnchorsAsyncML QuerySpatialAnchorsAsyncML;
    PFN_xrQuerySpatialAnchorsCompleteML QuerySpatialAnchorsCompleteML;
    PFN_xrPublishSpatialAnchorsAsyncML PublishSpatialAnchorsAsyncML;
    PFN_xrPublishSpatialAnchorsCompleteML PublishSpatialAnchorsCompleteML;
    PFN_xrDeleteSpatialAnchorsAsyncML DeleteSpatialAnchorsAsyncML;
    PFN_xrDeleteSpatialAnchorsCompleteML DeleteSpatialAnchorsCompleteML;
    PFN_xrUpdateSpatialAnchorsExpirationAsyncML UpdateSpatialAnchorsExpirationAsyncML;
    PFN_xrUpdateSpatialAnchorsExpirationCompleteML UpdateSpatialAnchorsExpirationCompleteML;

    // ---- XR_MSFT_spatial_anchor_persistence extension commands
    PFN_xrCreateSpatialAnchorStoreConnectionMSFT CreateSpatialAnchorStoreConnectionMSFT;
    PFN_xrDestroySpatialAnchorStoreConnectionMSFT DestroySpatialAnchorStoreConnectionMSFT;
    PFN_xrPersistSpatialAnchorMSFT PersistSpatialAnchorMSFT;
    PFN_xrEnumeratePersistedSpatialAnchorNamesMSFT EnumeratePersistedSpatialAnchorNamesMSFT;
    PFN_xrCreateSpatialAnchorFromPersistedNameMSFT CreateSpatialAnchorFromPersistedNameMSFT;
    PFN_xrUnpersistSpatialAnchorMSFT UnpersistSpatialAnchorMSFT;
    PFN_xrClearSpatialAnchorStoreMSFT ClearSpatialAnchorStoreMSFT;

    // ---- XR_MSFT_scene_marker extension commands
    PFN_xrGetSceneMarkerRawDataMSFT GetSceneMarkerRawDataMSFT;
    PFN_xrGetSceneMarkerDecodedStringMSFT GetSceneMarkerDecodedStringMSFT;

    // ---- XR_FB_spatial_entity_query extension commands
    PFN_xrQuerySpacesFB QuerySpacesFB;
    PFN_xrRetrieveSpaceQueryResultsFB RetrieveSpaceQueryResultsFB;

    // ---- XR_FB_spatial_entity_storage extension commands
    PFN_xrSaveSpaceFB SaveSpaceFB;
    PFN_xrEraseSpaceFB EraseSpaceFB;

    // ---- XR_OCULUS_audio_device_guid extension commands
#if defined(XR_USE_PLATFORM_WIN32)
    PFN_xrGetAudioOutputDeviceGuidOculus GetAudioOutputDeviceGuidOculus;
#endif // defined(XR_USE_PLATFORM_WIN32)
#if defined(XR_USE_PLATFORM_WIN32)
    PFN_xrGetAudioInputDeviceGuidOculus GetAudioInputDeviceGuidOculus;
#endif // defined(XR_USE_PLATFORM_WIN32)

    // ---- XR_FB_spatial_entity_sharing extension commands
    PFN_xrShareSpacesFB ShareSpacesFB;

    // ---- XR_FB_scene extension commands
    PFN_xrGetSpaceBoundingBox2DFB GetSpaceBoundingBox2DFB;
    PFN_xrGetSpaceBoundingBox3DFB GetSpaceBoundingBox3DFB;
    PFN_xrGetSpaceSemanticLabelsFB GetSpaceSemanticLabelsFB;
    PFN_xrGetSpaceBoundary2DFB GetSpaceBoundary2DFB;
    PFN_xrGetSpaceRoomLayoutFB GetSpaceRoomLayoutFB;

    // ---- XR_ALMALENCE_digital_lens_control extension commands
    PFN_xrSetDigitalLensControlALMALENCE SetDigitalLensControlALMALENCE;

    // ---- XR_FB_scene_capture extension commands
    PFN_xrRequestSceneCaptureFB RequestSceneCaptureFB;

    // ---- XR_FB_spatial_entity_container extension commands
    PFN_xrGetSpaceContainerFB GetSpaceContainerFB;

    // ---- XR_META_foveation_eye_tracked extension commands
    PFN_xrGetFoveationEyeTrackedStateMETA GetFoveationEyeTrackedStateMETA;

    // ---- XR_FB_face_tracking extension commands
    PFN_xrCreateFaceTrackerFB CreateFaceTrackerFB;
    PFN_xrDestroyFaceTrackerFB DestroyFaceTrackerFB;
    PFN_xrGetFaceExpressionWeightsFB GetFaceExpressionWeightsFB;

    // ---- XR_FB_eye_tracking_social extension commands
    PFN_xrCreateEyeTrackerFB CreateEyeTrackerFB;
    PFN_xrDestroyEyeTrackerFB DestroyEyeTrackerFB;
    PFN_xrGetEyeGazesFB GetEyeGazesFB;

    // ---- XR_FB_passthrough_keyboard_hands extension commands
    PFN_xrPassthroughLayerSetKeyboardHandsIntensityFB PassthroughLayerSetKeyboardHandsIntensityFB;

    // ---- XR_FB_haptic_pcm extension commands
    PFN_xrGetDeviceSampleRateFB GetDeviceSampleRateFB;

    // ---- XR_META_passthrough_preferences extension commands
    PFN_xrGetPassthroughPreferencesMETA GetPassthroughPreferencesMETA;

    // ---- XR_META_virtual_keyboard extension commands
    PFN_xrCreateVirtualKeyboardMETA CreateVirtualKeyboardMETA;
    PFN_xrDestroyVirtualKeyboardMETA DestroyVirtualKeyboardMETA;
    PFN_xrCreateVirtualKeyboardSpaceMETA CreateVirtualKeyboardSpaceMETA;
    PFN_xrSuggestVirtualKeyboardLocationMETA SuggestVirtualKeyboardLocationMETA;
    PFN_xrGetVirtualKeyboardScaleMETA GetVirtualKeyboardScaleMETA;
    PFN_xrSetVirtualKeyboardModelVisibilityMETA SetVirtualKeyboardModelVisibilityMETA;
    PFN_xrGetVirtualKeyboardModelAnimationStatesMETA GetVirtualKeyboardModelAnimationStatesMETA;
    PFN_xrGetVirtualKeyboardDirtyTexturesMETA GetVirtualKeyboardDirtyTexturesMETA;
    PFN_xrGetVirtualKeyboardTextureDataMETA GetVirtualKeyboardTextureDataMETA;
    PFN_xrSendVirtualKeyboardInputMETA SendVirtualKeyboardInputMETA;
    PFN_xrChangeVirtualKeyboardTextContextMETA ChangeVirtualKeyboardTextContextMETA;

    // ---- XR_OCULUS_external_camera extension commands
    PFN_xrEnumerateExternalCamerasOCULUS EnumerateExternalCamerasOCULUS;

    // ---- XR_META_performance_metrics extension commands
    PFN_xrEnumeratePerformanceMetricsCounterPathsMETA EnumeratePerformanceMetricsCounterPathsMETA;
    PFN_xrSetPerformanceMetricsStateMETA SetPerformanceMetricsStateMETA;
    PFN_xrGetPerformanceMetricsStateMETA GetPerformanceMetricsStateMETA;
    PFN_xrQueryPerformanceMetricsCounterMETA QueryPerformanceMetricsCounterMETA;

    // ---- XR_FB_spatial_entity_storage_batch extension commands
    PFN_xrSaveSpaceListFB SaveSpaceListFB;

    // ---- XR_FB_spatial_entity_user extension commands
    PFN_xrCreateSpaceUserFB CreateSpaceUserFB;
    PFN_xrGetSpaceUserIdFB GetSpaceUserIdFB;
    PFN_xrDestroySpaceUserFB DestroySpaceUserFB;

    // ---- XR_META_spatial_entity_discovery extension commands
    PFN_xrDiscoverSpacesMETA DiscoverSpacesMETA;
    PFN_xrRetrieveSpaceDiscoveryResultsMETA RetrieveSpaceDiscoveryResultsMETA;

    // ---- XR_META_recommended_layer_resolution extension commands
    PFN_xrGetRecommendedLayerResolutionMETA GetRecommendedLayerResolutionMETA;

    // ---- XR_META_spatial_entity_persistence extension commands
    PFN_xrSaveSpacesMETA SaveSpacesMETA;
    PFN_xrEraseSpacesMETA EraseSpacesMETA;

    // ---- XR_META_passthrough_color_lut extension commands
    PFN_xrCreatePassthroughColorLutMETA CreatePassthroughColorLutMETA;
    PFN_xrDestroyPassthroughColorLutMETA DestroyPassthroughColorLutMETA;
    PFN_xrUpdatePassthroughColorLutMETA UpdatePassthroughColorLutMETA;

    // ---- XR_META_spatial_entity_mesh extension commands
    PFN_xrGetSpaceTriangleMeshMETA GetSpaceTriangleMeshMETA;

    // ---- XR_META_body_tracking_calibration extension commands
    PFN_xrSuggestBodyTrackingCalibrationOverrideMETA SuggestBodyTrackingCalibrationOverrideMETA;
    PFN_xrResetBodyTrackingCalibrationMETA ResetBodyTrackingCalibrationMETA;

    // ---- XR_META_body_tracking_fidelity extension commands
    PFN_xrRequestBodyTrackingFidelityMETA RequestBodyTrackingFidelityMETA;

    // ---- XR_FB_face_tracking2 extension commands
    PFN_xrCreateFaceTracker2FB CreateFaceTracker2FB;
    PFN_xrDestroyFaceTracker2FB DestroyFaceTracker2FB;
    PFN_xrGetFaceExpressionWeights2FB GetFaceExpressionWeights2FB;

    // ---- XR_META_spatial_entity_sharing extension commands
    PFN_xrShareSpacesMETA ShareSpacesMETA;

    // ---- XR_META_environment_depth extension commands
    PFN_xrCreateEnvironmentDepthProviderMETA CreateEnvironmentDepthProviderMETA;
    PFN_xrDestroyEnvironmentDepthProviderMETA DestroyEnvironmentDepthProviderMETA;
    PFN_xrStartEnvironmentDepthProviderMETA StartEnvironmentDepthProviderMETA;
    PFN_xrStopEnvironmentDepthProviderMETA StopEnvironmentDepthProviderMETA;
    PFN_xrCreateEnvironmentDepthSwapchainMETA CreateEnvironmentDepthSwapchainMETA;
    PFN_xrDestroyEnvironmentDepthSwapchainMETA DestroyEnvironmentDepthSwapchainMETA;
    PFN_xrEnumerateEnvironmentDepthSwapchainImagesMETA EnumerateEnvironmentDepthSwapchainImagesMETA;
    PFN_xrGetEnvironmentDepthSwapchainStateMETA GetEnvironmentDepthSwapchainStateMETA;
    PFN_xrAcquireEnvironmentDepthImageMETA AcquireEnvironmentDepthImageMETA;
    PFN_xrSetEnvironmentDepthHandRemovalMETA SetEnvironmentDepthHandRemovalMETA;

    // ---- XR_EXT_render_model extension commands
    PFN_xrCreateRenderModelEXT CreateRenderModelEXT;
    PFN_xrDestroyRenderModelEXT DestroyRenderModelEXT;
    PFN_xrGetRenderModelPropertiesEXT GetRenderModelPropertiesEXT;
    PFN_xrCreateRenderModelSpaceEXT CreateRenderModelSpaceEXT;
    PFN_xrCreateRenderModelAssetEXT CreateRenderModelAssetEXT;
    PFN_xrDestroyRenderModelAssetEXT DestroyRenderModelAssetEXT;
    PFN_xrGetRenderModelAssetDataEXT GetRenderModelAssetDataEXT;
    PFN_xrGetRenderModelAssetPropertiesEXT GetRenderModelAssetPropertiesEXT;
    PFN_xrGetRenderModelStateEXT GetRenderModelStateEXT;

    // ---- XR_EXT_interaction_render_model extension commands
    PFN_xrEnumerateInteractionRenderModelIdsEXT EnumerateInteractionRenderModelIdsEXT;
    PFN_xrEnumerateRenderModelSubactionPathsEXT EnumerateRenderModelSubactionPathsEXT;
    PFN_xrGetRenderModelPoseTopLevelUserPathEXT GetRenderModelPoseTopLevelUserPathEXT;

    // ---- XR_QCOM_tracking_optimization_settings extension commands
    PFN_xrSetTrackingOptimizationSettingsHintQCOM SetTrackingOptimizationSettingsHintQCOM;

    // ---- XR_QCOM_hand_tracking_gesture extension commands
    PFN_xrGetHandGestureQCOM GetHandGestureQCOM;

    // ---- XR_HTC_passthrough extension commands
    PFN_xrCreatePassthroughHTC CreatePassthroughHTC;
    PFN_xrDestroyPassthroughHTC DestroyPassthroughHTC;

    // ---- XR_HTC_foveation extension commands
    PFN_xrApplyFoveationHTC ApplyFoveationHTC;

    // ---- XR_HTC_anchor extension commands
    PFN_xrCreateSpatialAnchorHTC CreateSpatialAnchorHTC;
    PFN_xrGetSpatialAnchorNameHTC GetSpatialAnchorNameHTC;

    // ---- XR_HTC_body_tracking extension commands
    PFN_xrCreateBodyTrackerHTC CreateBodyTrackerHTC;
    PFN_xrDestroyBodyTrackerHTC DestroyBodyTrackerHTC;
    PFN_xrLocateBodyJointsHTC LocateBodyJointsHTC;
    PFN_xrGetBodySkeletonHTC GetBodySkeletonHTC;

    // ---- XR_MNDX_force_feedback_curl extension commands
    PFN_xrApplyForceFeedbackCurlMNDX ApplyForceFeedbackCurlMNDX;

    // ---- XR_BD_body_tracking extension commands
    PFN_xrCreateBodyTrackerBD CreateBodyTrackerBD;
    PFN_xrDestroyBodyTrackerBD DestroyBodyTrackerBD;
    PFN_xrLocateBodyJointsBD LocateBodyJointsBD;

    // ---- XR_BD_facial_simulation extension commands
    PFN_xrEnumerateFacialSimulationModesBD EnumerateFacialSimulationModesBD;
    PFN_xrCreateFaceTrackerBD CreateFaceTrackerBD;
    PFN_xrDestroyFaceTrackerBD DestroyFaceTrackerBD;
    PFN_xrGetFacialSimulationDataBD GetFacialSimulationDataBD;
    PFN_xrSetFacialSimulationModeBD SetFacialSimulationModeBD;
    PFN_xrGetFacialSimulationModeBD GetFacialSimulationModeBD;

    // ---- XR_BD_spatial_sensing extension commands
    PFN_xrEnumerateSpatialEntityComponentTypesBD EnumerateSpatialEntityComponentTypesBD;
    PFN_xrGetSpatialEntityUuidBD GetSpatialEntityUuidBD;
    PFN_xrGetSpatialEntityComponentDataBD GetSpatialEntityComponentDataBD;
    PFN_xrCreateSenseDataProviderBD CreateSenseDataProviderBD;
    PFN_xrStartSenseDataProviderAsyncBD StartSenseDataProviderAsyncBD;
    PFN_xrStartSenseDataProviderCompleteBD StartSenseDataProviderCompleteBD;
    PFN_xrGetSenseDataProviderStateBD GetSenseDataProviderStateBD;
    PFN_xrQuerySenseDataAsyncBD QuerySenseDataAsyncBD;
    PFN_xrQuerySenseDataCompleteBD QuerySenseDataCompleteBD;
    PFN_xrDestroySenseDataSnapshotBD DestroySenseDataSnapshotBD;
    PFN_xrGetQueriedSenseDataBD GetQueriedSenseDataBD;
    PFN_xrStopSenseDataProviderBD StopSenseDataProviderBD;
    PFN_xrDestroySenseDataProviderBD DestroySenseDataProviderBD;
    PFN_xrCreateSpatialEntityAnchorBD CreateSpatialEntityAnchorBD;
    PFN_xrDestroyAnchorBD DestroyAnchorBD;
    PFN_xrGetAnchorUuidBD GetAnchorUuidBD;
    PFN_xrCreateAnchorSpaceBD CreateAnchorSpaceBD;

    // ---- XR_BD_spatial_anchor extension commands
    PFN_xrCreateSpatialAnchorAsyncBD CreateSpatialAnchorAsyncBD;
    PFN_xrCreateSpatialAnchorCompleteBD CreateSpatialAnchorCompleteBD;
    PFN_xrPersistSpatialAnchorAsyncBD PersistSpatialAnchorAsyncBD;
    PFN_xrPersistSpatialAnchorCompleteBD PersistSpatialAnchorCompleteBD;
    PFN_xrUnpersistSpatialAnchorAsyncBD UnpersistSpatialAnchorAsyncBD;
    PFN_xrUnpersistSpatialAnchorCompleteBD UnpersistSpatialAnchorCompleteBD;

    // ---- XR_BD_spatial_anchor_sharing extension commands
    PFN_xrShareSpatialAnchorAsyncBD ShareSpatialAnchorAsyncBD;
    PFN_xrShareSpatialAnchorCompleteBD ShareSpatialAnchorCompleteBD;
    PFN_xrDownloadSharedSpatialAnchorAsyncBD DownloadSharedSpatialAnchorAsyncBD;
    PFN_xrDownloadSharedSpatialAnchorCompleteBD DownloadSharedSpatialAnchorCompleteBD;

    // ---- XR_BD_spatial_scene extension commands
    PFN_xrCaptureSceneAsyncBD CaptureSceneAsyncBD;
    PFN_xrCaptureSceneCompleteBD CaptureSceneCompleteBD;

    // ---- XR_BD_body_tracking_auxiliary_metrics extension commands
    PFN_xrStartBodyTrackingCalibrationAppBD StartBodyTrackingCalibrationAppBD;
    PFN_xrGetBodyTrackingStateBD GetBodyTrackingStateBD;

    // ---- XR_BD_spatial_light_estimation extension commands
    PFN_xrEnumerateEnvironmentTextureResolutionsBD EnumerateEnvironmentTextureResolutionsBD;
    PFN_xrEnumerateEnvironmentTexturePixelFormatsBD EnumerateEnvironmentTexturePixelFormatsBD;
    PFN_xrEnumerateEnvironmentTextureTransferTypesBD EnumerateEnvironmentTextureTransferTypesBD;

    // ---- XR_BD_spatial_audio_rendering extension commands
    PFN_xrEnumerateSupportedAudioSampleRateBD EnumerateSupportedAudioSampleRateBD;
    PFN_xrQueryFramesPerBufferRangeBD QueryFramesPerBufferRangeBD;
    PFN_xrCreateSpatialAudioRendererBD CreateSpatialAudioRendererBD;
    PFN_xrDestroySpatialAudioRendererBD DestroySpatialAudioRendererBD;
    PFN_xrCreateSoundObstacleMaterialBD CreateSoundObstacleMaterialBD;
    PFN_xrUpdateSoundObstacleMaterialConfigBD UpdateSoundObstacleMaterialConfigBD;
    PFN_xrDestroySoundObstacleMaterialBD DestroySoundObstacleMaterialBD;
    PFN_xrCreateSoundObstacleBD CreateSoundObstacleBD;
    PFN_xrUpdateSoundObstacleConfigBD UpdateSoundObstacleConfigBD;
    PFN_xrDestroySoundObstacleBD DestroySoundObstacleBD;
    PFN_xrCreateSoundObjectBD CreateSoundObjectBD;
    PFN_xrUpdateSoundObjectConfigBD UpdateSoundObjectConfigBD;
    PFN_xrSubmitSoundObjectBufferBD SubmitSoundObjectBufferBD;
    PFN_xrDestroySoundObjectBD DestroySoundObjectBD;
    PFN_xrCreateSoundFieldBD CreateSoundFieldBD;
    PFN_xrUpdateSoundFieldConfigBD UpdateSoundFieldConfigBD;
    PFN_xrSubmitSoundFieldBufferBD SubmitSoundFieldBufferBD;
    PFN_xrDestroySoundFieldBD DestroySoundFieldBD;
    PFN_xrWaitAudioPeriodBD WaitAudioPeriodBD;
    PFN_xrEndAudioPeriodBD EndAudioPeriodBD;

    // ---- XR_EXT_plane_detection extension commands
    PFN_xrCreatePlaneDetectorEXT CreatePlaneDetectorEXT;
    PFN_xrDestroyPlaneDetectorEXT DestroyPlaneDetectorEXT;
    PFN_xrBeginPlaneDetectionEXT BeginPlaneDetectionEXT;
    PFN_xrGetPlaneDetectionStateEXT GetPlaneDetectionStateEXT;
    PFN_xrGetPlaneDetectionsEXT GetPlaneDetectionsEXT;
    PFN_xrGetPlanePolygonBufferEXT GetPlanePolygonBufferEXT;

    // ---- XR_ANDROID_trackables extension commands
    PFN_xrEnumerateSupportedTrackableTypesANDROID EnumerateSupportedTrackableTypesANDROID;
    PFN_xrEnumerateSupportedAnchorTrackableTypesANDROID EnumerateSupportedAnchorTrackableTypesANDROID;
    PFN_xrCreateTrackableTrackerANDROID CreateTrackableTrackerANDROID;
    PFN_xrDestroyTrackableTrackerANDROID DestroyTrackableTrackerANDROID;
    PFN_xrGetAllTrackablesANDROID GetAllTrackablesANDROID;
    PFN_xrGetTrackablePlaneANDROID GetTrackablePlaneANDROID;
    PFN_xrCreateAnchorSpaceANDROID CreateAnchorSpaceANDROID;

    // ---- XR_ANDROID_eye_tracking extension commands
    PFN_xrCreateEyeTrackerANDROID CreateEyeTrackerANDROID;
    PFN_xrDestroyEyeTrackerANDROID DestroyEyeTrackerANDROID;
    PFN_xrGetFineTrackingEyesInfoANDROID GetFineTrackingEyesInfoANDROID;
    PFN_xrGetCoarseTrackingEyesInfoANDROID GetCoarseTrackingEyesInfoANDROID;

    // ---- XR_ANDROID_device_anchor_persistence extension commands
    PFN_xrEnumerateSupportedPersistenceAnchorTypesANDROID EnumerateSupportedPersistenceAnchorTypesANDROID;
    PFN_xrCreateDeviceAnchorPersistenceANDROID CreateDeviceAnchorPersistenceANDROID;
    PFN_xrDestroyDeviceAnchorPersistenceANDROID DestroyDeviceAnchorPersistenceANDROID;
    PFN_xrPersistAnchorANDROID PersistAnchorANDROID;
    PFN_xrGetAnchorPersistStateANDROID GetAnchorPersistStateANDROID;
    PFN_xrCreatePersistedAnchorSpaceANDROID CreatePersistedAnchorSpaceANDROID;
    PFN_xrEnumeratePersistedAnchorsANDROID EnumeratePersistedAnchorsANDROID;
    PFN_xrUnpersistAnchorANDROID UnpersistAnchorANDROID;

    // ---- XR_ANDROID_face_tracking extension commands
    PFN_xrCreateFaceTrackerANDROID CreateFaceTrackerANDROID;
    PFN_xrDestroyFaceTrackerANDROID DestroyFaceTrackerANDROID;
    PFN_xrGetFaceStateANDROID GetFaceStateANDROID;
    PFN_xrGetFaceCalibrationStateANDROID GetFaceCalibrationStateANDROID;

    // ---- XR_ANDROID_passthrough_camera_state extension commands
    PFN_xrGetPassthroughCameraStateANDROID GetPassthroughCameraStateANDROID;

    // ---- XR_ANDROID_composition_layer_passthrough_mesh extension commands
    PFN_xrCreatePassthroughLayerANDROID CreatePassthroughLayerANDROID;
    PFN_xrDestroyPassthroughLayerANDROID DestroyPassthroughLayerANDROID;
    PFN_xrSetPassthroughLayerMeshANDROID SetPassthroughLayerMeshANDROID;

    // ---- XR_ANDROID_raycast extension commands
    PFN_xrEnumerateRaycastSupportedTrackableTypesANDROID EnumerateRaycastSupportedTrackableTypesANDROID;
    PFN_xrRaycastANDROID RaycastANDROID;

    // ---- XR_ANDROID_performance_metrics extension commands
    PFN_xrEnumeratePerformanceMetricsCounterPathsANDROID EnumeratePerformanceMetricsCounterPathsANDROID;
    PFN_xrSetPerformanceMetricsStateANDROID SetPerformanceMetricsStateANDROID;
    PFN_xrGetPerformanceMetricsStateANDROID GetPerformanceMetricsStateANDROID;
    PFN_xrQueryPerformanceMetricsCounterANDROID QueryPerformanceMetricsCounterANDROID;

    // ---- XR_ANDROID_trackables_object extension commands
    PFN_xrGetTrackableObjectANDROID GetTrackableObjectANDROID;

    // ---- XR_EXT_future extension commands
    PFN_xrPollFutureEXT PollFutureEXT;
    PFN_xrCancelFutureEXT CancelFutureEXT;

    // ---- XR_ML_user_calibration extension commands
    PFN_xrEnableUserCalibrationEventsML EnableUserCalibrationEventsML;

    // ---- XR_ML_system_notifications extension commands
    PFN_xrSetSystemNotificationsML SetSystemNotificationsML;

    // ---- XR_ML_world_mesh_detection extension commands
    PFN_xrCreateWorldMeshDetectorML CreateWorldMeshDetectorML;
    PFN_xrDestroyWorldMeshDetectorML DestroyWorldMeshDetectorML;
    PFN_xrRequestWorldMeshStateAsyncML RequestWorldMeshStateAsyncML;
    PFN_xrRequestWorldMeshStateCompleteML RequestWorldMeshStateCompleteML;
    PFN_xrGetWorldMeshBufferRecommendSizeML GetWorldMeshBufferRecommendSizeML;
    PFN_xrAllocateWorldMeshBufferML AllocateWorldMeshBufferML;
    PFN_xrFreeWorldMeshBufferML FreeWorldMeshBufferML;
    PFN_xrRequestWorldMeshAsyncML RequestWorldMeshAsyncML;
    PFN_xrRequestWorldMeshCompleteML RequestWorldMeshCompleteML;

    // ---- XR_ML_facial_expression extension commands
    PFN_xrCreateFacialExpressionClientML CreateFacialExpressionClientML;
    PFN_xrDestroyFacialExpressionClientML DestroyFacialExpressionClientML;
    PFN_xrGetFacialExpressionBlendShapePropertiesML GetFacialExpressionBlendShapePropertiesML;

    // ---- XR_META_boundary_visibility extension commands
    PFN_xrRequestBoundaryVisibilityMETA RequestBoundaryVisibilityMETA;

    // ---- XR_META_simultaneous_hands_and_controllers extension commands
    PFN_xrResumeSimultaneousHandsAndControllersTrackingMETA ResumeSimultaneousHandsAndControllersTrackingMETA;
    PFN_xrPauseSimultaneousHandsAndControllersTrackingMETA PauseSimultaneousHandsAndControllersTrackingMETA;

    // ---- XR_META_spatial_entity_room_mesh extension commands
    PFN_xrGetSpaceRoomMeshMETA GetSpaceRoomMeshMETA;
    PFN_xrGetSpaceRoomMeshFaceIndicesMETA GetSpaceRoomMeshFaceIndicesMETA;

    // ---- XR_META_colocation_discovery extension commands
    PFN_xrStartColocationDiscoveryMETA StartColocationDiscoveryMETA;
    PFN_xrStopColocationDiscoveryMETA StopColocationDiscoveryMETA;
    PFN_xrStartColocationAdvertisementMETA StartColocationAdvertisementMETA;
    PFN_xrStopColocationAdvertisementMETA StopColocationAdvertisementMETA;

    // ---- XR_META_environment_raycast extension commands
    PFN_xrCreateEnvironmentRaycasterAsyncMETA CreateEnvironmentRaycasterAsyncMETA;
    PFN_xrCreateEnvironmentRaycasterCompleteMETA CreateEnvironmentRaycasterCompleteMETA;
    PFN_xrDestroyEnvironmentRaycasterMETA DestroyEnvironmentRaycasterMETA;
    PFN_xrPerformEnvironmentRaycastMETA PerformEnvironmentRaycastMETA;

    // ---- XR_META_tile_properties_hint extension commands
    PFN_xrSetTilePropertiesHintMETA SetTilePropertiesHintMETA;

    // ---- XR_META_hand_tracking_frequency_hint extension commands
    PFN_xrSetHandTrackingFrequencyHintMETA SetHandTrackingFrequencyHintMETA;

    // ---- XR_ANDROID_light_estimation extension commands
    PFN_xrCreateLightEstimatorANDROID CreateLightEstimatorANDROID;
    PFN_xrDestroyLightEstimatorANDROID DestroyLightEstimatorANDROID;
    PFN_xrGetLightEstimateANDROID GetLightEstimateANDROID;

    // ---- XR_ANDROID_anchor_sharing_export extension commands
#if defined(XR_USE_PLATFORM_ANDROID)
    PFN_xrShareAnchorANDROID ShareAnchorANDROID;
#endif // defined(XR_USE_PLATFORM_ANDROID)
#if defined(XR_USE_PLATFORM_ANDROID)
    PFN_xrUnshareAnchorANDROID UnshareAnchorANDROID;
#endif // defined(XR_USE_PLATFORM_ANDROID)

    // ---- XR_ANDROID_trackables_marker extension commands
    PFN_xrGetTrackableMarkerANDROID GetTrackableMarkerANDROID;

    // ---- XR_ANDROID_trackables_qr_code extension commands
    PFN_xrGetTrackableQrCodeANDROID GetTrackableQrCodeANDROID;

    // ---- XR_ANDROID_trackables_image extension commands
    PFN_xrCreateTrackableImageDatabaseAsyncANDROID CreateTrackableImageDatabaseAsyncANDROID;
    PFN_xrCreateTrackableImageDatabaseCompleteANDROID CreateTrackableImageDatabaseCompleteANDROID;
    PFN_xrDestroyTrackableImageDatabaseANDROID DestroyTrackableImageDatabaseANDROID;
    PFN_xrAddTrackableImageDatabaseANDROID AddTrackableImageDatabaseANDROID;
    PFN_xrRemoveTrackableImageDatabaseANDROID RemoveTrackableImageDatabaseANDROID;
    PFN_xrGetTrackableImageANDROID GetTrackableImageANDROID;

    // ---- XR_ANDROID_scene_meshing extension commands
    PFN_xrEnumerateSupportedSemanticLabelSetsANDROID EnumerateSupportedSemanticLabelSetsANDROID;
    PFN_xrCreateSceneMeshingTrackerANDROID CreateSceneMeshingTrackerANDROID;
    PFN_xrDestroySceneMeshingTrackerANDROID DestroySceneMeshingTrackerANDROID;
    PFN_xrCreateSceneMeshSnapshotANDROID CreateSceneMeshSnapshotANDROID;
    PFN_xrDestroySceneMeshSnapshotANDROID DestroySceneMeshSnapshotANDROID;
    PFN_xrGetAllSubmeshStatesANDROID GetAllSubmeshStatesANDROID;
    PFN_xrGetSubmeshDataANDROID GetSubmeshDataANDROID;

    // ---- XR_EXT_spatial_entity extension commands
    PFN_xrEnumerateSpatialCapabilitiesEXT EnumerateSpatialCapabilitiesEXT;
    PFN_xrEnumerateSpatialCapabilityComponentTypesEXT EnumerateSpatialCapabilityComponentTypesEXT;
    PFN_xrEnumerateSpatialCapabilityFeaturesEXT EnumerateSpatialCapabilityFeaturesEXT;
    PFN_xrCreateSpatialContextAsyncEXT CreateSpatialContextAsyncEXT;
    PFN_xrCreateSpatialContextCompleteEXT CreateSpatialContextCompleteEXT;
    PFN_xrDestroySpatialContextEXT DestroySpatialContextEXT;
    PFN_xrCreateSpatialDiscoverySnapshotAsyncEXT CreateSpatialDiscoverySnapshotAsyncEXT;
    PFN_xrCreateSpatialDiscoverySnapshotCompleteEXT CreateSpatialDiscoverySnapshotCompleteEXT;
    PFN_xrQuerySpatialComponentDataEXT QuerySpatialComponentDataEXT;
    PFN_xrDestroySpatialSnapshotEXT DestroySpatialSnapshotEXT;
    PFN_xrCreateSpatialEntityFromIdEXT CreateSpatialEntityFromIdEXT;
    PFN_xrDestroySpatialEntityEXT DestroySpatialEntityEXT;
    PFN_xrCreateSpatialUpdateSnapshotEXT CreateSpatialUpdateSnapshotEXT;
    PFN_xrGetSpatialBufferStringEXT GetSpatialBufferStringEXT;
    PFN_xrGetSpatialBufferUint8EXT GetSpatialBufferUint8EXT;
    PFN_xrGetSpatialBufferUint16EXT GetSpatialBufferUint16EXT;
    PFN_xrGetSpatialBufferUint32EXT GetSpatialBufferUint32EXT;
    PFN_xrGetSpatialBufferFloatEXT GetSpatialBufferFloatEXT;
    PFN_xrGetSpatialBufferVector2fEXT GetSpatialBufferVector2fEXT;
    PFN_xrGetSpatialBufferVector3fEXT GetSpatialBufferVector3fEXT;

    // ---- XR_EXT_stationary_reference_space extension commands
    PFN_xrGetStationaryReferenceSpaceGenerationIdEXT GetStationaryReferenceSpaceGenerationIdEXT;

    // ---- XR_BD_camera_image extension commands
    PFN_xrEnumerateAvailableCamerasBD EnumerateAvailableCamerasBD;
    PFN_xrEnumerateCameraPropertyTypesBD EnumerateCameraPropertyTypesBD;
    PFN_xrGetCameraPropertiesBD GetCameraPropertiesBD;
    PFN_xrEnumerateCameraCapabilityTypesBD EnumerateCameraCapabilityTypesBD;
    PFN_xrGetCameraSupportedCapabilitiesBD GetCameraSupportedCapabilitiesBD;
    PFN_xrCreateCameraDeviceAsyncBD CreateCameraDeviceAsyncBD;
    PFN_xrCreateCameraDeviceCompleteBD CreateCameraDeviceCompleteBD;
    PFN_xrDestroyCameraDeviceBD DestroyCameraDeviceBD;
    PFN_xrCreateCameraCaptureSessionAsyncBD CreateCameraCaptureSessionAsyncBD;
    PFN_xrCreateCameraCaptureSessionCompleteBD CreateCameraCaptureSessionCompleteBD;
    PFN_xrDestroyCameraCaptureSessionBD DestroyCameraCaptureSessionBD;
    PFN_xrBeginCameraCaptureBD BeginCameraCaptureBD;
    PFN_xrEndCameraCaptureBD EndCameraCaptureBD;
    PFN_xrAcquireCameraImageBD AcquireCameraImageBD;
    PFN_xrGetCameraImageDataBD GetCameraImageDataBD;
    PFN_xrReleaseCameraImageBD ReleaseCameraImageBD;

    // ---- XR_EXT_spatial_anchor extension commands
    PFN_xrCreateSpatialAnchorEXT CreateSpatialAnchorEXT;

    // ---- XR_EXT_spatial_persistence extension commands
    PFN_xrEnumerateSpatialPersistenceScopesEXT EnumerateSpatialPersistenceScopesEXT;
    PFN_xrCreateSpatialPersistenceContextAsyncEXT CreateSpatialPersistenceContextAsyncEXT;
    PFN_xrCreateSpatialPersistenceContextCompleteEXT CreateSpatialPersistenceContextCompleteEXT;
    PFN_xrDestroySpatialPersistenceContextEXT DestroySpatialPersistenceContextEXT;

    // ---- XR_EXT_haptic_parametric extension commands
    PFN_xrHapticParametricGetPropertiesEXT HapticParametricGetPropertiesEXT;

    // ---- XR_SONY_swapchain_color_space extension commands
    PFN_xrEnumerateColorSpacesSONY EnumerateColorSpacesSONY;

    // ---- XR_SONY_hdr_metadata extension commands
    PFN_xrSetHdrMetadataSONY SetHdrMetadataSONY;

    // ---- XR_EXT_spatial_persistence_operations extension commands
    PFN_xrPersistSpatialEntityAsyncEXT PersistSpatialEntityAsyncEXT;
    PFN_xrPersistSpatialEntityCompleteEXT PersistSpatialEntityCompleteEXT;
    PFN_xrUnpersistSpatialEntityAsyncEXT UnpersistSpatialEntityAsyncEXT;
    PFN_xrUnpersistSpatialEntityCompleteEXT UnpersistSpatialEntityCompleteEXT;

    // ---- XR_EXT_spatial_image_tracking extension commands
    PFN_xrEnumerateSpatialReferenceImageFormatsEXT EnumerateSpatialReferenceImageFormatsEXT;
    PFN_xrCreateSpatialImageTrackingDatabaseAsyncEXT CreateSpatialImageTrackingDatabaseAsyncEXT;
    PFN_xrCreateSpatialImageTrackingDatabaseCompleteEXT CreateSpatialImageTrackingDatabaseCompleteEXT;
    PFN_xrDestroySpatialImageTrackingDatabaseEXT DestroySpatialImageTrackingDatabaseEXT;

    // ---- XR_ANDROID_spatial_discovery_raycast extension commands
    PFN_xrCreateSpatialRaycastSnapshotANDROID CreateSpatialRaycastSnapshotANDROID;

    // ---- XR_ANDROID_google_cloud_auth extension commands
    PFN_xrSetGoogleCloudAuthAsyncANDROID SetGoogleCloudAuthAsyncANDROID;
    PFN_xrSetGoogleCloudAuthCompleteANDROID SetGoogleCloudAuthCompleteANDROID;

    // ---- XR_ANDROID_geospatial extension commands
    PFN_xrCreateGeospatialTrackerANDROID CreateGeospatialTrackerANDROID;
    PFN_xrDestroyGeospatialTrackerANDROID DestroyGeospatialTrackerANDROID;
    PFN_xrLocateGeospatialPoseFromPoseANDROID LocateGeospatialPoseFromPoseANDROID;
    PFN_xrLocateGeospatialPoseANDROID LocateGeospatialPoseANDROID;
    PFN_xrCheckVpsAvailabilityAsyncANDROID CheckVpsAvailabilityAsyncANDROID;
    PFN_xrCheckVpsAvailabilityCompleteANDROID CheckVpsAvailabilityCompleteANDROID;

    // ---- XR_ANDROID_spatial_entity_bound_anchor extension commands
    PFN_xrEnumerateSpatialAnchorAttachableComponentsANDROID EnumerateSpatialAnchorAttachableComponentsANDROID;

    // ---- XR_ANDROID_spatial_anchor_space extension commands
    PFN_xrCreateSpatialAnchorSpaceANDROID CreateSpatialAnchorSpaceANDROID;
    PFN_xrCreateSpatialAnchorSpaceFromIdANDROID CreateSpatialAnchorSpaceFromIdANDROID;

    // ---- XR_ANDROID_geospatial_anchor extension commands
    PFN_xrCreateGeospatialAnchorANDROID CreateGeospatialAnchorANDROID;
    PFN_xrCreateSurfaceAnchorAsyncANDROID CreateSurfaceAnchorAsyncANDROID;
    PFN_xrCreateSurfaceAnchorCompleteANDROID CreateSurfaceAnchorCompleteANDROID;

    // ---- XR_EXT_spatial_container extension commands
    PFN_xrCreateSpatialContainerEXT CreateSpatialContainerEXT;
    PFN_xrDestroySpatialContainerEXT DestroySpatialContainerEXT;
    PFN_xrCreateSpatialContainerSpaceEXT CreateSpatialContainerSpaceEXT;
    PFN_xrRequestSpatialContainerVisibleEXT RequestSpatialContainerVisibleEXT;
    PFN_xrRequestSpatialContainerBoundsModeEXT RequestSpatialContainerBoundsModeEXT;
    PFN_xrGetSpatialContainerBoundsEXT GetSpatialContainerBoundsEXT;
    PFN_xrGetSpatialContainerStateEXT GetSpatialContainerStateEXT;
    PFN_xrEnumerateSupportedSpatialContainerGraphicsPresentationsEXT EnumerateSupportedSpatialContainerGraphicsPresentationsEXT;

    // ---- XR_EXT_spatial_container_self_rendering extension commands
    PFN_xrBeginSpatialContainerRenderingEXT BeginSpatialContainerRenderingEXT;
    PFN_xrEndSpatialContainerRenderingEXT EndSpatialContainerRenderingEXT;
    PFN_xrLocateSpatialContainerViewsEXT LocateSpatialContainerViewsEXT;
};


// Prototype for dispatch table helper function
void GeneratedXrPopulateDispatchTable(struct XrGeneratedDispatchTable *table,
                                      XrInstance instance,
                                      PFN_xrGetInstanceProcAddr get_inst_proc_addr);

#ifdef __cplusplus
} // extern "C"
#endif

