# Changelog for OpenXR-SDK-Source and OpenXR-SDK Repo

<!--
Copyright (c) 2019-2026 The Khronos Group Inc.

SPDX-License-Identifier: CC-BY-4.0
-->

Update log for the OpenXR-SDK-Source and OpenXR-SDK repo on GitHub. Updates are
in reverse chronological order starting with the latest public release.

Note that only changes relating to the loader and some of the build changes will
affect the OpenXR-SDK repository. Changes mentioned in this changelog related to
hello_xr, API layers, and the loader tests do *not* apply to the OpenXR-SDK
repository.

This summarizes the periodic public updates, not individual commits. Updates
on GitHub are generally done as single large patches at the release point,
collecting together the resolution of many Khronos internal issues,
along with any public pull requests that have been accepted.
In this repository in particular, since it is primarily software,
pull requests may be integrated as they are accepted even between periodic updates.

## OpenXR SDK 1.1.63 (2026-09-01)

This release primarily contains a large number of new extensions, including
four new ratified extensions, and support for those through the generated parts
of API layers in the SDK. SDK-specific changes include updates to the
`list_json` tool.

- SDK
  - Improvement: Loader: Remove duplicate null check for `outgoing_count` in
    `GetApiLayerProperties`.
    ([internal MR 4256](https://gitlab.khronos.org/openxr/openxr/merge_requests/4256))
  - Improvement: Minor usability improvements to "list_json" tool.
    ([internal MR 4410](https://gitlab.khronos.org/openxr/openxr/merge_requests/4410),
    [internal MR 3289](https://gitlab.khronos.org/openxr/openxr/merge_requests/3289))
  - Improvement: Allow `runClangTidy.sh` to run with clang-tidy 18.
    ([internal MR 4433](https://gitlab.khronos.org/openxr/openxr/merge_requests/4433))
  - Improvement: Allow `runClangTidy.sh` to run with `-modernize-use-scoped-lock`
    enabled.
    ([internal MR 4435](https://gitlab.khronos.org/openxr/openxr/merge_requests/4435))
- Registry
  - New ratified Khronos extension: `XR_KHR_extended_result_name_lengths`
    ([internal MR 3964](https://gitlab.khronos.org/openxr/openxr/merge_requests/3964))
  - New ratified multi-vendor extensions: `XR_EXT_spatial_container`,
    `XR_EXT_spatial_container_self_rendering`
    ([internal MR 3712](https://gitlab.khronos.org/openxr/openxr/merge_requests/3712),
    [internal MR 4472](https://gitlab.khronos.org/openxr/openxr/merge_requests/4472))
  - New ratified multi-vendor extension: `XR_EXT_spatial_image_tracking`
    ([internal MR 3805](https://gitlab.khronos.org/openxr/openxr/merge_requests/3805))
  - New vendor extension: `XR_BD_spatial_light_estimation`
    ([internal MR 3814](https://gitlab.khronos.org/openxr/openxr/merge_requests/3814))
  - New vendor extension: `XR_BD_camera_image`
    ([internal MR 4089](https://gitlab.khronos.org/openxr/openxr/merge_requests/4089))
  - New vendor extension: `XR_SONY_hdr_metadata`
    ([internal MR 4172](https://gitlab.khronos.org/openxr/openxr/merge_requests/4172))
  - Change: Add new semantic definitions including keyboard, mouse, and laptop in
    `XrSemanticLabelBD`.
    ([internal MR 4221](https://gitlab.khronos.org/openxr/openxr/merge_requests/4221))
  - Chore: Reserve extension numbers.
    ([internal MR 4420](https://gitlab.khronos.org/openxr/openxr/merge_requests/4420),
    [internal MR 4421](https://gitlab.khronos.org/openxr/openxr/merge_requests/4421))
  - Fix: Missing comments for `XrResult` error codes added by
    `XR_ANDROID_google_cloud_auth`, `XR_ANDROID_geospatial`, and
    `XR_ANDROID_geospatial_anchor`.
    ([internal MR 4438](https://gitlab.khronos.org/openxr/openxr/merge_requests/4438))
  - Fix: Always output forward slashes in relative paths when merging registry.
    ([internal MR 4451](https://gitlab.khronos.org/openxr/openxr/merge_requests/4451))

## OpenXR SDK 1.1.62 (2026-07-31)

This release includes a new vendor extension, an important fix to several vendor
extension added in the previous release, a loader design doc clarification, and
some software fixes and cleanup. For extension authors, there are workflow
improvements: extensions may now be developed in their own "fragment" XML file
and referenced from `xr.fragmented.xml`. Provided tooling processes this file to
generate the all-inclusive `xr.xml` previously edited directly. This reduces the
merge conflicts seen during extension development and makes review easier,
without requiring changes to tools expecting the conventional combined XML file.

- SDK
  - Fix: Only include EGL headers in `xr_dependencies.h` when using EGL or GLES.
    ([internal MR 4387](https://gitlab.khronos.org/openxr/openxr/merge_requests/4387))
  - Improvement: Add `src/common/platform_exports.h` to de-duplicate platform
    specific export flags.
    ([internal MR 4323](https://gitlab.khronos.org/openxr/openxr/merge_requests/4323))
  - Improvement: Add additional description of Android API layer support.
    ([internal MR 4373](https://gitlab.khronos.org/openxr/openxr/merge_requests/4373))
  - Improvement: Deduplicate `.def`, `.expsym`, and `.map` files used for API
    layers in the SDK.
    ([internal MR 4389](https://gitlab.khronos.org/openxr/openxr/merge_requests/4389),
    [internal MR 3289](https://gitlab.khronos.org/openxr/openxr/merge_requests/3289))
- Registry
  - Change: XML registry `xr.xml` is now checked in after being constructed from
    `xr.fragmented.xml` and multiple per-extension XML file fragments, for reduced
    conflicts during development. The schema of `xr.xml` has not changed. Be sure
    to run `specification/scripts/merge-registry.sh` after editing
    `xr.fragmented.xml` or one of the fragments, as most tooling still uses the
    merged XML registry.
    ([internal MR 4161](https://gitlab.khronos.org/openxr/openxr/merge_requests/4161))
  - Chore: Register `DXR` author ID for The DisplayXR Project.
    ([OpenXR-Docs PR 201](https://github.com/KhronosGroup/OpenXR-Docs/pull/201))
  - Fix: Resolve inconsistency between extension numbers for META vendor extensions
    (`XR_META_hand_tracking_wide_motion_mode2`,
    `XR_META_hand_tracking_frequency_hint`,
    `XR_META_hand_tracking_unextrapolated_poses`).
    ([internal MR 4380](https://gitlab.khronos.org/openxr/openxr/merge_requests/4380))
  - Fix: Add `XR_ERROR_LIMIT_REACHED` as an valid return for
    `xrAcquireEnvironmentDepthImageMETA` matching API description.
    ([internal MR 4414](https://gitlab.khronos.org/openxr/openxr/merge_requests/4414))
  - Improvement: Move `XR_ANDROID_trackables` to extension fragment XML file.
    ([internal MR 4161](https://gitlab.khronos.org/openxr/openxr/merge_requests/4161))
  - Improvement: Add explicit type includes for types that are implicitly included
    to improve generated documentation.
    ([internal MR 4406](https://gitlab.khronos.org/openxr/openxr/merge_requests/4406),
    [internal MR 4413](https://gitlab.khronos.org/openxr/openxr/merge_requests/4413))
  - New vendor extension: `XR_SONY_swapchain_color_space`
    ([internal MR 4171](https://gitlab.khronos.org/openxr/openxr/merge_requests/4171))

## OpenXR SDK 1.1.61 (2026-07-02)

This release marks the initial publication of the development tool API layers
for easier usage by developers for Android/AOSP-based systems. A small addition
to the reflection headers is also included, along with support for new and
revised vendor extensions.

- SDK
  - Addition: Build AAR files of development API layers and publish to Maven
    Central on release. A line like
    `implementation 'org.khronos.openxr:apilayer_core_validation:1.1.61'`
    (or `apilayer_api_dump` or `apilayer_best_practices_validation` equivalently)
    will bundle a layer with your application and enable it automatically as an implicit layer.
    ([internal MR 4136](https://gitlab.khronos.org/openxr/openxr/merge_requests/4136))
  - Loader: Fix: Remove redundant null checks in `initializePlatform`.
    ([internal MR 4261](https://gitlab.khronos.org/openxr/openxr/merge_requests/4261))
- Registry
  - Fix: Typos in enum value names for `XrTrackingOptimizationSettingsHintQCOM`.
    Original names left as aliases for source compatibility.
    ([internal MR 4186](https://gitlab.khronos.org/openxr/openxr/merge_requests/4186))
  - Improvement: `XR_LIST_FUNCTIONS_` macros in the reflection header are now
    generated for all features, even those lacking functions.
    ([internal MR 4329](https://gitlab.khronos.org/openxr/openxr/merge_requests/4329))
  - New vendor extension: `XR_ANDROID_spatial_discovery_bounds`
    ([internal MR 3520](https://gitlab.khronos.org/openxr/openxr/merge_requests/3520))
  - New vendor extension: `XR_BD_body_tracking_auxiliary_metrics`
    ([internal MR 3756](https://gitlab.khronos.org/openxr/openxr/merge_requests/3756))
  - New vendor extension: `XR_BD_dynamic_object_tracking`
    ([internal MR 3944](https://gitlab.khronos.org/openxr/openxr/merge_requests/3944))
  - New vendor extension: `XR_BD_dynamic_object_keyboard`
    ([internal MR 3944](https://gitlab.khronos.org/openxr/openxr/merge_requests/3944))
  - New vendor extension: `XR_BD_dynamic_object_mouse`
    ([internal MR 3944](https://gitlab.khronos.org/openxr/openxr/merge_requests/3944))
  - New vendor extension: `XR_META_hand_tracking_frequency_hint`
    ([internal MR 4110](https://gitlab.khronos.org/openxr/openxr/merge_requests/4110))
  - New vendor extension: `XR_META_hand_tracking_unextrapolated_poses`
    ([internal MR 4113](https://gitlab.khronos.org/openxr/openxr/merge_requests/4113))
  - New vendor extension: `XR_META_hand_tracking_wide_motion_mode2`
    ([internal MR 4114](https://gitlab.khronos.org/openxr/openxr/merge_requests/4114))
  - New vendor extension: `XR_ANDROID_google_cloud_auth`
    ([internal MR 4200](https://gitlab.khronos.org/openxr/openxr/merge_requests/4200))
  - New vendor extension: `XR_ANDROID_geospatial`
    ([internal MR 4201](https://gitlab.khronos.org/openxr/openxr/merge_requests/4201))
  - New vendor extension: `XR_ANDROID_geospatial_anchor`
    ([internal MR 4214](https://gitlab.khronos.org/openxr/openxr/merge_requests/4214))
  - Schematron: Add exception for the comments of a behavior-only
    `XrSpatialComponentTypeEXT`.
    ([internal MR 4353](https://gitlab.khronos.org/openxr/openxr/merge_requests/4353))

## OpenXR SDK 1.1.60 (2026-05-20)

This release contains support for a new multi-vendor extension, as well as many
fixes and cleanups to the layers and loader.

- SDK
  - API Dump Layer: Fix: Prevent runtime failure when encountering an unknown
    XrStruct.
    ([internal MR 4301](https://gitlab.khronos.org/openxr/openxr/merge_requests/4301))
  - API Dump Layer: Fix: Prevent crashes when using extension function pointers
    without the extension enabled.
    ([internal MR 4301](https://gitlab.khronos.org/openxr/openxr/merge_requests/4301),
    [internal issue 2524](https://gitlab.khronos.org/openxr/openxr/issues/2524))
  - API Dump Layer: Fix: Data race due to map access outside the mutex lock in both
    create and destroy paths for handle dispatch maps.
    ([internal MR 4301](https://gitlab.khronos.org/openxr/openxr/merge_requests/4301))
  - API Dump Layer: Fix: Generated code attempting to read uninitialized `char*`
    buffers.
    ([internal MR 4301](https://gitlab.khronos.org/openxr/openxr/merge_requests/4301))
  - Best Practices Layer: Fix: Resolve invalid reference.
    ([internal MR 4273](https://gitlab.khronos.org/openxr/openxr/merge_requests/4273),
    [internal issue 2683](https://gitlab.khronos.org/openxr/openxr/issues/2683),
    [OpenXR-SDK-Source issue 588](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/588))
  - Improvement: Loader: Minor code cleanups without functional change.
    ([internal MR 4264](https://gitlab.khronos.org/openxr/openxr/merge_requests/4264))
  - Improvement: Enable `-Wc++20-extensions`.
    ([internal MR 4265](https://gitlab.khronos.org/openxr/openxr/merge_requests/4265))
  - Improvement: Enable sanitizers for all CMake projects.
    ([internal MR 4296](https://gitlab.khronos.org/openxr/openxr/merge_requests/4296),
    [internal MR 2743](https://gitlab.khronos.org/openxr/openxr/merge_requests/2743))
  - Improvement: Add CMake test to run the CTS with the
    `XR_APILAYER_LUNARG_api_dump` layer and test runtime (when layer and CTS are in
    a single unified repo).
    ([internal MR 4301](https://gitlab.khronos.org/openxr/openxr/merge_requests/4301))
  - Improvement: Add CMake test to run the CTS with the
    `XR_APILAYER_LUNARG_core_validation` layer and test runtime (when layer and CTS
    are in a single unified repo).
    ([internal MR 4306](https://gitlab.khronos.org/openxr/openxr/merge_requests/4306))
  - Loader: Improvement: Refactor `LoaderXrGetInstanceProcAddr` to avoid repeated
    `strcmp` on names and a hypothetical null dereference.
    ([internal MR 3132](https://gitlab.khronos.org/openxr/openxr/merge_requests/3132))
  - Loader: Improvement: Several code cleanups and optimizations.
    ([internal MR 4254](https://gitlab.khronos.org/openxr/openxr/merge_requests/4254),
    [internal MR 4255](https://gitlab.khronos.org/openxr/openxr/merge_requests/4255),
    [internal MR 4257](https://gitlab.khronos.org/openxr/openxr/merge_requests/4257),
    [internal MR 4263](https://gitlab.khronos.org/openxr/openxr/merge_requests/4263))
  - Loader: Fix: Use `strtoul` instead of `atoi`/`strtol` for unsigned integer
    parsing in manifest files.
    ([internal MR 4258](https://gitlab.khronos.org/openxr/openxr/merge_requests/4258))
  - Loader: Fix: Fix wrong VUID in empty application name error message.
    ([internal MR 4262](https://gitlab.khronos.org/openxr/openxr/merge_requests/4262))
  - Loader: Fix: `stack-use-after-scope` problem in `manifest_file.cpp`. (included
    in 1.1.59.1 hotfix as well)
    ([internal MR 4295](https://gitlab.khronos.org/openxr/openxr/merge_requests/4295),
    [internal MR 2743](https://gitlab.khronos.org/openxr/openxr/merge_requests/2743))
  - Validation Layer: Fix: Verify downstream extension function support before
    returning layer wrapper.
    ([internal MR 4306](https://gitlab.khronos.org/openxr/openxr/merge_requests/4306))
  - Validation Layer: Fix: Return `xrCreateApiLayerInstance` errors immediately.
    ([internal MR 4306](https://gitlab.khronos.org/openxr/openxr/merge_requests/4306))
  - Validation Layer: Fix: Return warning rather than error for unknown enum values
    so the runtime can return the correct error code.
    ([internal MR 4306](https://gitlab.khronos.org/openxr/openxr/merge_requests/4306))
  - Validation Layer: Fix: Skip enum validation for
    `xrResultToString`/`xrStructureTypeToString` since these functions accept any
    value by design.
    ([internal MR 4306](https://gitlab.khronos.org/openxr/openxr/merge_requests/4306))
  - Validation Layer: Fix: Skip `strlen()` on non-const output char arrays to avoid
    undefined behavior on uninitialized buffers.
    ([internal MR 4306](https://gitlab.khronos.org/openxr/openxr/merge_requests/4306))
  - Validation Layer: Fix: Destroy implicitly-created debug utils messengers during
    instance teardown to prevent handle leaks.
    ([internal MR 4306](https://gitlab.khronos.org/openxr/openxr/merge_requests/4306))
- Registry
  - New multi-vendor extension: `XR_EXT_haptic_parametric`
    ([internal MR 3307](https://gitlab.khronos.org/openxr/openxr/merge_requests/3307))
  - Change: Deprecate HoloLens 2 related extensions.
    ([internal MR 4133](https://gitlab.khronos.org/openxr/openxr/merge_requests/4133))
  - Change: Mark `XR_ANDROID_trackables` and its child extensions as deprecated by
    various `XR_EXT` and `XR_ANDROID` spatial entity extensions.
    ([internal MR 4228](https://gitlab.khronos.org/openxr/openxr/merge_requests/4228))
  - Fix: Add comment for enum value `XR_SPACE_COMPONENT_TYPE_ROOM_MESH_META` in
    `xr.xml`.
    ([internal MR 4292](https://gitlab.khronos.org/openxr/openxr/merge_requests/4292),
    [internal issue 2740](https://gitlab.khronos.org/openxr/openxr/issues/2740))
  - Fix: Add comment for result value
    `XR_BOUNDARY_VISIBILITY_SUPPRESSION_NOT_ALLOWED_META` in `xr.xml`.
    ([internal MR 4293](https://gitlab.khronos.org/openxr/openxr/merge_requests/4293),
    [internal issue 2739](https://gitlab.khronos.org/openxr/openxr/issues/2739))
  - Fix: Mark `XrSessionCreateInfoOverlayEXTX::createFlags` as optional.
    ([internal MR 4302](https://gitlab.khronos.org/openxr/openxr/merge_requests/4302))
  - Fix: Update `XrFoveationEyeTrackedProfileCreateInfoMETA` to mark `flags` as
    optional
    ([internal MR 4317](https://gitlab.khronos.org/openxr/openxr/merge_requests/4317),
    [internal MR 4317](https://gitlab.khronos.org/openxr/openxr/merge_requests/4317))
  - Improvement: Add schematron validation for `optional` required for empty bit
    flags.
    ([internal MR 4317](https://gitlab.khronos.org/openxr/openxr/merge_requests/4317),
    [internal issue 2745](https://gitlab.khronos.org/openxr/openxr/issues/2745))
  - Scripts: Handle deprecation expressions referring to multiple extensions.
    ([internal MR 4228](https://gitlab.khronos.org/openxr/openxr/merge_requests/4228))

## OpenXR SDK 1.1.59 (2026-04-30)

This release includes several fixes and cleanups in the loader and various API
layers. There is also an additional multi-vendor extension and vendor extensions
added to the registry with this release.

- SDK
  - Best Practices Layer: Fix: Addresses race conditions
    ([internal MR 4208](https://gitlab.khronos.org/openxr/openxr/merge_requests/4208),
    [internal issue 2676](https://gitlab.khronos.org/openxr/openxr/issues/2676),
    [OpenXR-SDK-Source issue 581](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/581))
  - Loader: Improvement: Code cleanup and robustness improvements.
    ([internal MR 3475](https://gitlab.khronos.org/openxr/openxr/merge_requests/3475),
    [OpenXR-SDK-Source PR 413](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/413))
  - Loader: Improvement: Use `std::make_unique` instead of raw `new` where
    constructors are public.
    ([internal MR 4252](https://gitlab.khronos.org/openxr/openxr/merge_requests/4252))
  - Test Runtime: Fix: Address missing directory error when building on macOS.
    ([internal MR 4269](https://gitlab.khronos.org/openxr/openxr/merge_requests/4269))
  - Validation Layer: Fix: Cast flags to `uint64_t` before printing as hexadecimal.
    ([internal MR 4225](https://gitlab.khronos.org/openxr/openxr/merge_requests/4225))
- Registry
  - Chore: Reserve extension numbers and adjust previous reservations.
    ([internal MR 4266](https://gitlab.khronos.org/openxr/openxr/merge_requests/4266),
    [internal MR 4276](https://gitlab.khronos.org/openxr/openxr/merge_requests/4276),
    [internal MR 4282](https://gitlab.khronos.org/openxr/openxr/merge_requests/4282),
    [internal MR 4253](https://gitlab.khronos.org/openxr/openxr/merge_requests/4253))
  - Improvement: Enforce that essentially all functions must be able to return
    `XR_ERROR_RUNTIME_FAILURE`.
    ([internal MR 3611](https://gitlab.khronos.org/openxr/openxr/merge_requests/3611),
    [internal issue 2345](https://gitlab.khronos.org/openxr/openxr/issues/2345))
  - New multi-vendor extension: `XR_EXT_stationary_reference_space`
    ([internal MR 3021](https://gitlab.khronos.org/openxr/openxr/merge_requests/3021))
  - New vendor extension: `XR_META_boundary_visibility`
    ([internal MR 3249](https://gitlab.khronos.org/openxr/openxr/merge_requests/3249))
  - New vendor extension: `XR_META_face_tracking_visemes`
    ([internal MR 3424](https://gitlab.khronos.org/openxr/openxr/merge_requests/3424))
  - New vendor extension: `XR_META_spatial_entity_room_mesh`
    ([internal MR 3530](https://gitlab.khronos.org/openxr/openxr/merge_requests/3530))
  - New vendor extension: `XR_ANDROID_eye_tracking`
    ([internal MR 3773](https://gitlab.khronos.org/openxr/openxr/merge_requests/3773))
  - New vendor extension: `XR_ANDROID_recommended_resolution`
    ([internal MR 3840](https://gitlab.khronos.org/openxr/openxr/merge_requests/3840))
  - New vendor extension: `XR_ANDROID_light_estimation`.
    ([internal MR 3879](https://gitlab.khronos.org/openxr/openxr/merge_requests/3879))
  - Schematron: Improvement: Add validation for ordering of structs in `xr.xml`.
    ([internal MR 4251](https://gitlab.khronos.org/openxr/openxr/merge_requests/4251))
  - Schematron: Bump schXslt Schematron checker version and fix error in wrapper
    script.
    ([internal MR 4288](https://gitlab.khronos.org/openxr/openxr/merge_requests/4288))
  - Update vendor specification: Revision 5 of `XR_FB_passthrough`
    ([internal MR 3862](https://gitlab.khronos.org/openxr/openxr/merge_requests/3862))

## OpenXR SDK 1.1.58 (2026-03-31)

This release features several fixes to the SDK as well as tooling improvements.
Among the several added extensions, the most notable are the two new ratified
multi-vendor extensions, `XR_EXT_view_configuration_views_change` and
`XR_EXT_interaction_profile_battery_state_display`.

- SDK
  - API Dump Layer: Fix: crash when encountering `XR_TYPE_UNKNOWN` in `next` chain
    ([internal MR 4090](https://gitlab.khronos.org/openxr/openxr/merge_requests/4090))
  - Improvement: Enable `-Werror=missing-prototypes` more widely.
    ([internal MR 4120](https://gitlab.khronos.org/openxr/openxr/merge_requests/4120))
  - Improvement: Search using QUIET mode for sanitizers, as they are a developer
    feature.
    ([OpenXR-SDK-Source PR 577](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/577))
  - hello_xr: Improvement: Simplify options parsing
    ([internal MR 4155](https://gitlab.khronos.org/openxr/openxr/merge_requests/4155),
    [internal MR 3369](https://gitlab.khronos.org/openxr/openxr/merge_requests/3369))
  - hello_xr: Fix: Remove error condition check if `hello_xr` is run with
    `XR_VIEW_CONFIGURATION_TYPE_PRIMARY_MONO`.
    ([internal MR 4197](https://gitlab.khronos.org/openxr/openxr/merge_requests/4197),
    [internal issue 2679](https://gitlab.khronos.org/openxr/openxr/issues/2679))
  - hello_xr: Fix: crash with D3D11 when the runtime does not support an
    appropriate depth format.
    ([internal MR 4199](https://gitlab.khronos.org/openxr/openxr/merge_requests/4199),
    [internal issue 2689](https://gitlab.khronos.org/openxr/openxr/issues/2689),
    [OpenXR-SDK-Source issue 589](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/589))
  - hello_xr: Fix: Missed parts of backporting changes from CTS for depth image
    support.
    ([OpenXR-SDK-Source PR 587](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/587),
    [internal issue 2780](https://gitlab.khronos.org/openxr/openxr/issues/2780))
  - hello_xr: Fix: Correct type enum error in D3D12 code.
    ([OpenXR-SDK-Source PR 587](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/587),
    [internal issue 2780](https://gitlab.khronos.org/openxr/openxr/issues/2780))
  - hello_xr: Improvement: Remove now-unused code in D3D11, OpenGL, and OpenGL ES
    backends.
    ([OpenXR-SDK-Source PR 587](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/587),
    [internal issue 2780](https://gitlab.khronos.org/openxr/openxr/issues/2780))
  - loader_test: Fix: Android compatibility
    ([internal MR 4090](https://gitlab.khronos.org/openxr/openxr/merge_requests/4090))
  - loader_test: Improvement: run `loader_test` through CTest on Linux
    ([internal MR 4090](https://gitlab.khronos.org/openxr/openxr/merge_requests/4090))
  - test_runtime: Improvement: Simplify session state implementation in
    `runtime_test.cpp`
    ([internal MR 4159](https://gitlab.khronos.org/openxr/openxr/merge_requests/4159))
  - test_runtime: Fix: Update for marking of `XR_FB_touch_controller_proximity` as
    obsoleted by OpenXR 1.1.
    ([internal MR 4191](https://gitlab.khronos.org/openxr/openxr/merge_requests/4191),
    [internal issue 2599](https://gitlab.khronos.org/openxr/openxr/issues/2599))
  - test_runtime: Improvement: Fix quaternion validation percentage in
    `runtime_test.cpp`
    ([internal MR 4192](https://gitlab.khronos.org/openxr/openxr/merge_requests/4192),
    [internal MR 4159](https://gitlab.khronos.org/openxr/openxr/merge_requests/4159))
- Registry
  - Change: Mark `XR_FB_space_warp` as deprecated by `XR_EXT_frame_synthesis`,
    which provides the same functionality with a slightly modified interface.
    ([internal MR 4173](https://gitlab.khronos.org/openxr/openxr/merge_requests/4173),
    [internal issue 2469](https://gitlab.khronos.org/openxr/openxr/issues/2469))
  - Chore: Reserve extension numbers.
    ([internal MR 4204](https://gitlab.khronos.org/openxr/openxr/merge_requests/4204))
  - Fix: Mark `XR_FB_touch_controller_proximity` as obsoleted by OpenXR 1.1.
    ([internal MR 4191](https://gitlab.khronos.org/openxr/openxr/merge_requests/4191),
    [internal issue 2599](https://gitlab.khronos.org/openxr/openxr/issues/2599))
  - Improvement: Add command return code checks in `xml_consistency.py` for
    specific spatial entity input types.
    ([internal MR 4151](https://gitlab.khronos.org/openxr/openxr/merge_requests/4151))
  - New ratified multi-vendor extension: `XR_EXT_view_configuration_views_change`
    ([internal MR 711](https://gitlab.khronos.org/openxr/openxr/merge_requests/711),
    [internal issue 619](https://gitlab.khronos.org/openxr/openxr/issues/619))
  - New ratified multi-vendor extension:
    `XR_EXT_interaction_profile_battery_state_display`.
    ([internal MR 3528](https://gitlab.khronos.org/openxr/openxr/merge_requests/3528),
    [internal issue 1339](https://gitlab.khronos.org/openxr/openxr/issues/1339),
    [internal issue 1905](https://gitlab.khronos.org/openxr/openxr/issues/1905))
  - New vendor extension: `XR_QCOM_hand_tracking_gesture`
    ([internal MR 2572](https://gitlab.khronos.org/openxr/openxr/merge_requests/2572))
  - New vendor extension: `XR_META_body_tracking_fidelity`
    ([internal MR 2962](https://gitlab.khronos.org/openxr/openxr/merge_requests/2962))
  - New vendor extension: `XR_META_spatial_entity_semantic_label`
    ([internal MR 3529](https://gitlab.khronos.org/openxr/openxr/merge_requests/3529))
  - New vendor extension: `XR_ANDROID_performance_metrics`
    ([internal MR 3607](https://gitlab.khronos.org/openxr/openxr/merge_requests/3607))
  - New vendor extension: `XR_ANDROID_mouse_interaction`
    ([internal MR 3608](https://gitlab.khronos.org/openxr/openxr/merge_requests/3608))
  - New vendor extension: `XR_ANDROID_unbounded_reference_space`
    ([internal MR 3624](https://gitlab.khronos.org/openxr/openxr/merge_requests/3624))
  - New vendor extension: `XR_ANDROID_composition_layer_passthrough_mesh`
    ([internal MR 3625](https://gitlab.khronos.org/openxr/openxr/merge_requests/3625))
  - New vendor extension: `XR_META_environment_raycast`
    ([internal MR 3758](https://gitlab.khronos.org/openxr/openxr/merge_requests/3758))
  - New vendor extension: `XR_ANDROID_trackables_qr_code`
    ([internal MR 3772](https://gitlab.khronos.org/openxr/openxr/merge_requests/3772))
  - New vendor extension: `XR_BD_spatial_audio_rendering`
    ([internal MR 3794](https://gitlab.khronos.org/openxr/openxr/merge_requests/3794))
  - New vendor extension: `XR_ANDROID_scene_meshing`
    ([internal MR 3872](https://gitlab.khronos.org/openxr/openxr/merge_requests/3872))
  - New vendor extension: `XR_META_tile_properties_hint`
    ([internal MR 3885](https://gitlab.khronos.org/openxr/openxr/merge_requests/3885))
  - New vendor extension: `XR_ANDROID_spatial_object_tracking`
    ([internal MR 3992](https://gitlab.khronos.org/openxr/openxr/merge_requests/3992))
  - New vendor extension: `XR_ANDROID_spatial_discovery_raycast`
    ([internal MR 3993](https://gitlab.khronos.org/openxr/openxr/merge_requests/3993))
  - New vendor extension: `XR_ANDROID_spatial_entity_bound_anchor`
    ([internal MR 3994](https://gitlab.khronos.org/openxr/openxr/merge_requests/3994))
  - New vendor extension: `XR_ANDROID_spatial_component_subsumed_by`
    ([internal MR 4125](https://gitlab.khronos.org/openxr/openxr/merge_requests/4125))
  - New vendor extension: `XR_ANDROID_spatial_anchor_space`
    ([internal MR 4132](https://gitlab.khronos.org/openxr/openxr/merge_requests/4132))
  - Schematron: Add schematron validation for prefix `XrCompositionLayer` for
    structures which extend `XrCompositionLayerBaseHeader`.
    ([internal MR 4131](https://gitlab.khronos.org/openxr/openxr/merge_requests/4131))

## OpenXR SDK 1.1.57 (2026-02-12)

This release primarily features several fixes to the SDK as well as tooling
improvements.

Note that specification releases 1.1.55 and 1.1.56 were skipped to keep up with
a monthly increment for the patch version component.

- SDK
  - Fix: Do not attempt to install manpages on Android, and ensure all directory
    prefixes are populated, fixing CMake install.
    ([OpenXR-SDK-Source PR 573](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/573),
    [OpenXR-SDK-Source issue 568](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/568),
    [internal issue 2645](https://gitlab.khronos.org/openxr/openxr/issues/2645))
  - Loader: Reduce severity of Android loader "broker not found" messages from
    error to warning.
    ([internal MR 4134](https://gitlab.khronos.org/openxr/openxr/merge_requests/4134))
  - Validation Layer: Fix core validation layer validation of `xrCreateSession` to
    support `XR_TYPE_GRAPHICS_BINDING_METAL_KHR` and
    `XR_TYPE_GRAPHICS_BINDING_EGL_MNDX`.
    ([internal MR 4119](https://gitlab.khronos.org/openxr/openxr/merge_requests/4119),
    [internal issue 2646](https://gitlab.khronos.org/openxr/openxr/issues/2646))
  - hello_xr: Deduplicate code in internal "gfxwrapper" library used in hello_xr
    (among other uses) for OpenGL/OpenGL ES. Add an explicit always-EGL path.
    ([internal MR 2942](https://gitlab.khronos.org/openxr/openxr/merge_requests/2942),
    [internal issue 1911](https://gitlab.khronos.org/openxr/openxr/issues/1911))
  - hello_xr: Fix out of bounds read when none of the required depth formats is
    supported.
    ([internal MR 4145](https://gitlab.khronos.org/openxr/openxr/merge_requests/4145),
    [OpenXR-SDK-Source issue 578](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/578),
    [internal issue 2663](https://gitlab.khronos.org/openxr/openxr/issues/2663),
    [internal MR 4023](https://gitlab.khronos.org/openxr/openxr/merge_requests/4023))
  - loader_test: Update Catch2 test library from 3.7.1 to 3.11.0.
    ([internal MR 4006](https://gitlab.khronos.org/openxr/openxr/merge_requests/4006),
    [internal issue 2108](https://gitlab.khronos.org/openxr/openxr/issues/2108))
- Registry
  - Change: Update tooling to properly support extending allowable return codes
    from extensions.
    ([internal MR 4009](https://gitlab.khronos.org/openxr/openxr/merge_requests/4009),
    [internal issue 2494](https://gitlab.khronos.org/openxr/openxr/issues/2494))
  - Chore: Reorder function return codes to alphabetic (with `XR_SUCCESS` pulled to
    the front).
    ([internal MR 3979](https://gitlab.khronos.org/openxr/openxr/merge_requests/3979))
  - Chore: Reserve extensions.
    ([internal MR 4157](https://gitlab.khronos.org/openxr/openxr/merge_requests/4157),
    [internal MR 4162](https://gitlab.khronos.org/openxr/openxr/merge_requests/4162))
  - Fix: `XR_KHR_extended_struct_name_lengths` string length can be up to 256
    bytes. (The prose has been modified to match the XML specification.)
    ([internal MR 4126](https://gitlab.khronos.org/openxr/openxr/merge_requests/4126),
    [internal issue 2654](https://gitlab.khronos.org/openxr/openxr/issues/2654))
  - New vendor extension: `XR_ANDROID_trackables_image`.
    ([internal MR 4069](https://gitlab.khronos.org/openxr/openxr/merge_requests/4069))
  - Schematron: Add validation for return code ordering.
    ([internal MR 3979](https://gitlab.khronos.org/openxr/openxr/merge_requests/3979))
  - Schematron: Add validation for semantic label enums defined for the spatial
    entity framework.
    ([internal MR 4081](https://gitlab.khronos.org/openxr/openxr/merge_requests/4081))
  - Update: Mark `XR_EXT_plane_detection` as deprecated by
    `XR_EXT_spatial_plane_tracking`.
    ([internal MR 4057](https://gitlab.khronos.org/openxr/openxr/merge_requests/4057),
    [internal issue 2625](https://gitlab.khronos.org/openxr/openxr/issues/2625))

## OpenXR SDK 1.1.54 (2025-12-02)

This release updates the C++ standards version used by the package, and includes
changes to hello_xr to bring it closer in line with the OpenXR-CTS (which come
from a shared codebase) and allow optional submission of depth textures where
supported, among other changes.

- SDK
  - Change: Update C++ standard version used from C++14 to C++17.
    ([internal MR 4096](https://gitlab.khronos.org/openxr/openxr/merge_requests/4096))
  - Chore: Add extra compiler validation for the OpenXR loader.
    ([internal MR 4086](https://gitlab.khronos.org/openxr/openxr/merge_requests/4086))
  - Chore: Fix -Wmissing-prototypes compiler warnings.
    ([internal MR 4107](https://gitlab.khronos.org/openxr/openxr/merge_requests/4107))
  - Improvement: Add build option `INSTALL_SYMBOLS` (enabled by default to maintain
    current behavior), to allow skipping installing `.PDB` files from MSVC builds.
    ([internal MR 4066](https://gitlab.khronos.org/openxr/openxr/merge_requests/4066),
    [internal issue 2634](https://gitlab.khronos.org/openxr/openxr/issues/2634))
  - Improvement: Clarify loader doc regarding removed permission requirement for
    installable broker.
    ([internal MR 4092](https://gitlab.khronos.org/openxr/openxr/merge_requests/4092))
  - Validation Layer: Fix the `library_path` of
    `XrApiLayer_best_practices_validation.json` on Windows
    ([internal MR 4080](https://gitlab.khronos.org/openxr/openxr/merge_requests/4080),
    [internal issue 2629](https://gitlab.khronos.org/openxr/openxr/issues/2629))
  - hello_xr: Revise rendering to be more similar to the CTS, and add optional
    support for `XR_KHR_composition_layer_depth`.
    ([internal MR 4023](https://gitlab.khronos.org/openxr/openxr/merge_requests/4023),
    [internal MR 4118](https://gitlab.khronos.org/openxr/openxr/merge_requests/4118),
    [internal MR 4121](https://gitlab.khronos.org/openxr/openxr/merge_requests/4121))
- Registry
  - Change: Deprecate the Wayland part of `XR_KHR_opengl_enable` and bump version.
    ([internal MR 4052](https://gitlab.khronos.org/openxr/openxr/merge_requests/4052),
    [OpenXR-SDK-Source issue 282](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/282),
    [internal issue 1634](https://gitlab.khronos.org/openxr/openxr/issues/1634),
    [internal issue 2559](https://gitlab.khronos.org/openxr/openxr/issues/2559))
  - Chore: Reserve extensions.
    ([internal MR 4067](https://gitlab.khronos.org/openxr/openxr/merge_requests/4067),
    [internal MR 4079](https://gitlab.khronos.org/openxr/openxr/merge_requests/4079),
    [internal MR 4098](https://gitlab.khronos.org/openxr/openxr/merge_requests/4098),
    [internal MR 4099](https://gitlab.khronos.org/openxr/openxr/merge_requests/4099),
    [internal MR 4100](https://gitlab.khronos.org/openxr/openxr/merge_requests/4100))
  - Fix: Correctly define `XR_EXT_dpad_binding` paths for
    /interaction_profiles/bytedance/pico_ultra_controller_bd in XML.
    ([internal MR 4049](https://gitlab.khronos.org/openxr/openxr/merge_requests/4049))
  - Improvement: Add description for `XR_SPACE_COMPONENT_TYPE_TRIANGLE_MESH_META`.
    ([internal MR 4070](https://gitlab.khronos.org/openxr/openxr/merge_requests/4070))
  - Schematron: Add validation to ensure bitmasks are non-plural before
    `Flags`/`FlagBits`.
    ([internal MR 2678](https://gitlab.khronos.org/openxr/openxr/merge_requests/2678))
  - Schematron: Add validation to check additional scenarios of `next` pointer
    const-ness.
    ([internal MR 2740](https://gitlab.khronos.org/openxr/openxr/merge_requests/2740))
  - Schematron: Add validation for interaction profile extension binding additions.
    ([internal MR 3982](https://gitlab.khronos.org/openxr/openxr/merge_requests/3982))
  - Schematron: Remove unneeded exception from "Param/member naming conventions:
    camelCase" pattern.
    ([internal MR 4072](https://gitlab.khronos.org/openxr/openxr/merge_requests/4072))
  - Schematron: Fix check for multiple arrays bounded by the same `len` param.
    ([internal MR 4077](https://gitlab.khronos.org/openxr/openxr/merge_requests/4077))
  - Schematron: Add validation for the comment attribute of
    `XrSpatialComponentTypeEXT` values. The comment must reference the list and
    data structs for the component.
    ([internal MR 4083](https://gitlab.khronos.org/openxr/openxr/merge_requests/4083))
  - Schematron: Add validation for the comment attribute of
    `XrSpatialCapabilityFeatureEXT` values. The comment must have references to the
    feature's config structs.
    ([internal MR 4084](https://gitlab.khronos.org/openxr/openxr/merge_requests/4084))
  - Schematron: Add check for orphaned structs, enforcing `structextends` or
    `parentstruct` when appropriate.
    ([internal MR 4091](https://gitlab.khronos.org/openxr/openxr/merge_requests/4091),
    [internal MR 4094](https://gitlab.khronos.org/openxr/openxr/merge_requests/4094))
  - Update: Bump XR_EXT_future version due to clarifying what the runtime may do in
    a completion function of an `XrFutureEXT`.
    ([internal MR 3796](https://gitlab.khronos.org/openxr/openxr/merge_requests/3796),
    [internal MR 4028](https://gitlab.khronos.org/openxr/openxr/merge_requests/4028))
  - Update: Add `XrEnvironmentDepthImageTimestampMETA` to
    XR_META_environment_depth, update spec version to 2.
    ([internal MR 4024](https://gitlab.khronos.org/openxr/openxr/merge_requests/4024))

## OpenXR SDK 1.1.53 (2025-10-14)

This release contains three new vendor extensions and some code cleanups. The
test_runtime expansion is primarily intended for CTS self-testing but may be
useful in other development processes as well.

- SDK
  - Improvement: fix clang-tidy bugprone-branch-clone warning
    ([internal MR 3864](https://gitlab.khronos.org/openxr/openxr/merge_requests/3864),
    [internal MR 4027](https://gitlab.khronos.org/openxr/openxr/merge_requests/4027))
  - Improvement: Expand the "test_runtime" to implement all OpenXR 1.0 / OpenXR 1.1
    functions for self-test purposes.
    ([internal MR 3989](https://gitlab.khronos.org/openxr/openxr/merge_requests/3989))
  - Improvement: Add stricter compiler flags to the loader and hello_xr, fixing
    issues found.
    ([internal MR 4028](https://gitlab.khronos.org/openxr/openxr/merge_requests/4028),
    [internal MR 4042](https://gitlab.khronos.org/openxr/openxr/merge_requests/4042))
  - Loader: Fix: Remove unused function definition `xrInitializeLoaderKHR` to fix
    pedantic builds.
    ([internal MR 4020](https://gitlab.khronos.org/openxr/openxr/merge_requests/4020))
- Registry
  - Fix: Bump `XR_ANDROID_trackables_object` version for fix to orientation.
    ([internal MR 4018](https://gitlab.khronos.org/openxr/openxr/merge_requests/4018))
  - New vendor extension: `XR_BD_facial_simulation` for vendor-specific access to
    facial expressions.
    ([internal MR 2925](https://gitlab.khronos.org/openxr/openxr/merge_requests/2925))
  - New vendor extension: `XR_BD_ultra_controller_interaction` for PICO Ultra
    controller interaction profile.
    ([internal MR 3789](https://gitlab.khronos.org/openxr/openxr/merge_requests/3789))
  - New vendor extension specification: `XR_ANDROID_face_tracking` for vendor-
    specific access to facial expressions.
    ([internal MR 3613](https://gitlab.khronos.org/openxr/openxr/merge_requests/3613))
  - Update: Bump version of `XR_MND_headless` for clarification.
    ([internal MR 3961](https://gitlab.khronos.org/openxr/openxr/merge_requests/3961))

## OpenXR SDK 1.1.52 (2025-09-19)

This release contains a new ratified Khronos extension, a new vendor extension,
fixes for issues in the XML registry, and API layer improvements.

- SDK
  - Improvement: Adjust the enabled clang-tidy checks after resolving some minor
    identified warnings.
    ([internal MR 3898](https://gitlab.khronos.org/openxr/openxr/merge_requests/3898))
  - Layers: Fix: Skip checking members other than `type` and `next` on structs
    marked `returnedonly="true"` in core validation layer.
    ([internal MR 3973](https://gitlab.khronos.org/openxr/openxr/merge_requests/3973))
  - Layers: Fix: Rename `KHR_best_practices_validation` ->
    `KHRONOS_best_practices_validation`
    ([internal MR 3984](https://gitlab.khronos.org/openxr/openxr/merge_requests/3984))
  - Loader: Improvement: Further clean up defines related to XR_KHR_loader_init.
    ([internal MR 3937](https://gitlab.khronos.org/openxr/openxr/merge_requests/3937),
    [internal MR 3948](https://gitlab.khronos.org/openxr/openxr/merge_requests/3948))
- Registry
  - New ratified Khronos extension: `XR_KHR_generic_controller`
    ([internal MR 3018](https://gitlab.khronos.org/openxr/openxr/merge_requests/3018))
  - Update: Ratified `XR_EXT_debug_utils`
    ([internal MR 3899](https://gitlab.khronos.org/openxr/openxr/merge_requests/3899))
  - Update: Ratified `XR_EXT_render_model`
    ([internal MR 3900](https://gitlab.khronos.org/openxr/openxr/merge_requests/3900))
  - Update: Ratified `XR_EXT_interaction_render_model`
    ([internal MR 3901](https://gitlab.khronos.org/openxr/openxr/merge_requests/3901))
  - New vendor extension: `XR_META_spatial_entity_discovery`.
    ([internal MR 2880](https://gitlab.khronos.org/openxr/openxr/merge_requests/2880),
    [internal MR 4001](https://gitlab.khronos.org/openxr/openxr/merge_requests/4001))
  - Change: Revert `XrTrackablePlaneANDROID::vertexCountOutput` to a pointer, and
    increment extension version number.
    ([internal MR 3998](https://gitlab.khronos.org/openxr/openxr/merge_requests/3998))
  - Chore: Reserve numbers for extensions.
    ([internal MR 4004](https://gitlab.khronos.org/openxr/openxr/merge_requests/4004))
  - Fix: Add XML for missing `grip_surface` paths for
    "/interaction_profiles/ext/hand_interaction_ext". Note that these paths were
    already listed in the spec prose. Update `XR_EXT_hand_interaction` extension
    version to `2` for clarity.
    ([internal MR 3963](https://gitlab.khronos.org/openxr/openxr/merge_requests/3963))
  - Fix: Mark a few structure members that are bitmasks as optional, to match prose
    and/or usage intent.
    ([internal MR 3973](https://gitlab.khronos.org/openxr/openxr/merge_requests/3973))
  - Fix: Mark `XrCompositionLayerPassthroughFB.space` as optional to match spec
    prose.
    ([internal MR 3973](https://gitlab.khronos.org/openxr/openxr/merge_requests/3973))
  - Fix: Add XML for missing `palm_ext/pose` and `grip_surface/pose` paths for
    "/interaction_profiles/logitech/mx_ink_stylus_logitech".  Note that these paths
    were already listed in the spec prose.
    ([internal MR 3996](https://gitlab.khronos.org/openxr/openxr/merge_requests/3996))

## OpenXR SDK 1.1.51 (2025-08-28)

This release adds a new API layer to assist developers in making an OpenXR
application that follows best practices, along with a number of miscellaneous
other fixes and improvements.

- SDK
  - Added: New "best practices validation" API layer to check for application
    behavior that is valid but contradicts best practices.
    ([internal MR 3735](https://gitlab.khronos.org/openxr/openxr/merge_requests/3735),
    [internal MR 3976](https://gitlab.khronos.org/openxr/openxr/merge_requests/3976))
  - Fix: Include command alias in `XR_LIST_FUNCTION_` macros in
    `openxr_reflection.h`.
    ([internal MR 3915](https://gitlab.khronos.org/openxr/openxr/merge_requests/3915),
    [internal issue 2222](https://gitlab.khronos.org/openxr/openxr/issues/2222))
  - Fix: Consistent usage of `XR_KHR_LOADER_INIT_SUPPORT` defines.
    ([internal MR 3936](https://gitlab.khronos.org/openxr/openxr/merge_requests/3936))
  - Fix: Migrate scripts to publish Android OpenXR loader AAR to Maven Central via
    new process.
    ([internal MR 3978](https://gitlab.khronos.org/openxr/openxr/merge_requests/3978),
    [internal issue 2499](https://gitlab.khronos.org/openxr/openxr/issues/2499),
    [internal MR 3975](https://gitlab.khronos.org/openxr/openxr/merge_requests/3975))
  - Improvement: enable clang-tidy bugprone-unused-local-non-trivial-variable
    check.
    ([internal MR 3892](https://gitlab.khronos.org/openxr/openxr/merge_requests/3892))
  - Improvement: Fixed unused parameters in gfxwrapper.
    ([internal MR 3894](https://gitlab.khronos.org/openxr/openxr/merge_requests/3894))
  - Improvement: Provide more useful metadata in the Android OpenXR loader AAR POM
    file.
    ([internal MR 3978](https://gitlab.khronos.org/openxr/openxr/merge_requests/3978),
    [internal issue 2499](https://gitlab.khronos.org/openxr/openxr/issues/2499),
    [internal MR 3975](https://gitlab.khronos.org/openxr/openxr/merge_requests/3975))
  - hello_xr: Fix Vulkan resource destruction bugs of ShaderProgram and Pipeline.
    ([OpenXR-SDK-Source PR 538](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/538))
- Registry
  - Chore: Reserve extension numbers.
    ([internal MR 3916](https://gitlab.khronos.org/openxr/openxr/merge_requests/3916),
    [internal MR 3956](https://gitlab.khronos.org/openxr/openxr/merge_requests/3956))
  - Fix: Missing parent struct for `XrSpatialCapabilityConfigurationAnchorEXT`.
    ([internal MR 3932](https://gitlab.khronos.org/openxr/openxr/merge_requests/3932))
  - Fix: Missing `XR_EXT_dpad_binding` paths for
    `/interaction_profiles/facebook/touch_controller_pro` and
    `/interaction_profiles/meta/touch_controller_plus`.
    ([internal MR 3945](https://gitlab.khronos.org/openxr/openxr/merge_requests/3945))
  - Fix: incorrect placement of `*` in
    `XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING` error description.
    ([internal MR 3960](https://gitlab.khronos.org/openxr/openxr/merge_requests/3960))
  - Fix: Missing `XR_EXT_dpad_binding` paths for
    `/interaction_profiles/varjo/xr-4_controller`.
    ([internal MR 3965](https://gitlab.khronos.org/openxr/openxr/merge_requests/3965))
  - Fix: Missing `XR_EXT_dpad_binding`, `XR_EXT_palm_pose`, and
    `XR_EXT_hand_interaction` bindings for
    `/interaction_profiles/oppo/mr_controller_oppo`.
    ([internal MR 3966](https://gitlab.khronos.org/openxr/openxr/merge_requests/3966))

## OpenXR SDK 1.1.50 (2025-07-24)

This release features a new loader extension for some specific automation use
cases, several new vendor extensions, and code quality improvements, among other
changes.

- SDK
  - Fix: Revise how OpenGL compatibility is detected in the build system on
    Windows.
    ([internal MR 3883](https://gitlab.khronos.org/openxr/openxr/merge_requests/3883))
  - Fix: Add missing include directive in loader code.
    ([OpenXR-SDK-Source PR 554](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/554))
  - Improvement: Add support for sanitizers in CMake with sanitizers-cmake project.
    ([internal MR 3716](https://gitlab.khronos.org/openxr/openxr/merge_requests/3716))
  - Improvement: add `.clang-tidy` file.
    ([internal MR 3802](https://gitlab.khronos.org/openxr/openxr/merge_requests/3802))
  - Improvement: Enable the clang-tidy cert-dcl16-c, bugprone-too-small-loop-
    variable, and bugprone-switch-missing-default-case checks, making fixes
    required to satisfy them.
    ([internal MR 3882](https://gitlab.khronos.org/openxr/openxr/merge_requests/3882),
    [internal MR 3893](https://gitlab.khronos.org/openxr/openxr/merge_requests/3893))
  - Improvement: Disable the clang-tidy bugprone-reserved-identifier, cert-dcl37-c,
    and cert-dcl51-cpp checks.
    ([internal MR 3882](https://gitlab.khronos.org/openxr/openxr/merge_requests/3882),
    [internal MR 3893](https://gitlab.khronos.org/openxr/openxr/merge_requests/3893))
  - Loader: Implement `XR_EXT_loader_init_properties` extension and enable
    `XR_KHR_loader_init` on all platforms.
    ([internal MR 2454](https://gitlab.khronos.org/openxr/openxr/merge_requests/2454))
  - Loader: Refactor loader data class and handling, preparing for loader data
    properties.
    ([internal MR 3834](https://gitlab.khronos.org/openxr/openxr/merge_requests/3834),
    [internal MR 3839](https://gitlab.khronos.org/openxr/openxr/merge_requests/3839))
  - Loader: Ensure that only the first Android property is used.
    ([internal MR 3834](https://gitlab.khronos.org/openxr/openxr/merge_requests/3834),
    [internal MR 3839](https://gitlab.khronos.org/openxr/openxr/merge_requests/3839))
  - Scripts: Handle the case where "current_ref_page" is a string rather than an
    object.
    ([internal MR 3834](https://gitlab.khronos.org/openxr/openxr/merge_requests/3834),
    [internal MR 3839](https://gitlab.khronos.org/openxr/openxr/merge_requests/3839))
  - Validation Layer: Accept unknown or duplicated structure types in the `next`
    chain, and dump debug messages for them.
    ([internal MR 3828](https://gitlab.khronos.org/openxr/openxr/merge_requests/3828))
  - hello_xr: Optimize graphics synchronization when using Vulkan.
    ([internal MR 3681](https://gitlab.khronos.org/openxr/openxr/merge_requests/3681))
- Registry
  - Change: Update the `XrSpatialAnchorCreateCompletionBD` structure, and increment
    the revision of `XR_BD_spatial_anchor`.
    ([internal MR 3876](https://gitlab.khronos.org/openxr/openxr/merge_requests/3876))
  - Fix: Remove `XR_EXT_palm_pose` paths from `XR_HTCX_vive_tracker_interaction`,
    as the /user/hand/left|right paths are not valid paths for Vive Trackers.
    ([internal MR 3844](https://gitlab.khronos.org/openxr/openxr/merge_requests/3844))
  - Fix: Include `XR_ERROR_SESSION_NOT_RUNNING` in list of errors
    `xrEnumerateInteractionRenderModelIdsEXT` can return. (Already in specification
    prose.)
    ([internal MR 3889](https://gitlab.khronos.org/openxr/openxr/merge_requests/3889))
  - Fix: Typo in comment for `XR_ERROR_SPATIAL_PERSISTENCE_SCOPE_INCOMPATIBLE_EXT`.
    ([internal MR 3897](https://gitlab.khronos.org/openxr/openxr/merge_requests/3897))
  - Fix: Add explicit dependency to `XR_ML_compat` for
    `XrCoordinateSpaceCreateInfoML`.
    ([internal MR 3907](https://gitlab.khronos.org/openxr/openxr/merge_requests/3907))
  - Improvement: Add comments for the `XrResult` values added by
    `XR_BD_spatial_anchor` and `XR_BD_spatial_anchor_sharing`.
    ([internal MR 3861](https://gitlab.khronos.org/openxr/openxr/merge_requests/3861),
    [internal issue 2535](https://gitlab.khronos.org/openxr/openxr/issues/2535))
  - New multi-vendor extension: `XR_EXT_loader_init_properties`
    ([internal MR 2454](https://gitlab.khronos.org/openxr/openxr/merge_requests/2454))
  - New vendor extension: `XR_META_body_tracking_calibration`.
    ([internal MR 2963](https://gitlab.khronos.org/openxr/openxr/merge_requests/2963))
  - New vendor extension: `XR_ANDROID_passthrough_camera_state`.
    ([internal MR 3614](https://gitlab.khronos.org/openxr/openxr/merge_requests/3614))
  - New vendor extension: `XR_ANDROID_trackables`.
    ([internal MR 3615](https://gitlab.khronos.org/openxr/openxr/merge_requests/3615))
  - New vendor extension: `XR_ANDROID_raycast`.
    ([internal MR 3620](https://gitlab.khronos.org/openxr/openxr/merge_requests/3620))
  - New vendor extension: `XR_ANDROID_trackables_object`.
    ([internal MR 3623](https://gitlab.khronos.org/openxr/openxr/merge_requests/3623))
  - New vendor extension: `XR_ANDROID_device_anchor_persistence`.
    ([internal MR 3626](https://gitlab.khronos.org/openxr/openxr/merge_requests/3626))
  - New vendor extension: `XR_ANDROID_anchor_sharing_export`.
    ([internal MR 3627](https://gitlab.khronos.org/openxr/openxr/merge_requests/3627))
  - New vendor extension: `XR_ANDROID_trackables_marker`.
    ([internal MR 3774](https://gitlab.khronos.org/openxr/openxr/merge_requests/3774))

## OpenXR SDK 1.1.49 (2025-06-10)

This release primarily adds new extensions: a collection of ratified
multi-vendor extensions related to "spatial entities", multi-vendor extensions
to work with interaction render models, and a vendor extension.

- SDK
  - Improvement: Fix multiple clang-format formatting issues.
    ([internal MR 3845](https://gitlab.khronos.org/openxr/openxr/merge_requests/3845))
  - Validation Layer: Add support for handles created by async operations.
    ([internal MR 3030](https://gitlab.khronos.org/openxr/openxr/merge_requests/3030))
- Registry
  - New ratified multi-vendor extension: `XR_EXT_spatial_entity`
    ([internal MR 3030](https://gitlab.khronos.org/openxr/openxr/merge_requests/3030),
    [internal MR 3874](https://gitlab.khronos.org/openxr/openxr/merge_requests/3874))
  - New ratified multi-vendor extension: `XR_EXT_spatial_anchor`
    ([internal MR 3286](https://gitlab.khronos.org/openxr/openxr/merge_requests/3286),
    [internal MR 3874](https://gitlab.khronos.org/openxr/openxr/merge_requests/3874))
  - New ratified multi-vendor extension: `XR_EXT_spatial_plane_tracking`
    ([internal MR 3402](https://gitlab.khronos.org/openxr/openxr/merge_requests/3402))
  - New ratified multi-vendor extension: `XR_EXT_spatial_marker_tracking`
    ([internal MR 3414](https://gitlab.khronos.org/openxr/openxr/merge_requests/3414))
  - New ratified multi-vendor extension: `XR_EXT_spatial_persistence`
    ([internal MR 3533](https://gitlab.khronos.org/openxr/openxr/merge_requests/3533),
    [internal MR 3874](https://gitlab.khronos.org/openxr/openxr/merge_requests/3874))
  - New ratified multi-vendor extension: `XR_EXT_spatial_persistence_operations`
    ([internal MR 3606](https://gitlab.khronos.org/openxr/openxr/merge_requests/3606))
  - New multi-vendor extension: `XR_EXT_render_model`
    ([internal MR 2464](https://gitlab.khronos.org/openxr/openxr/merge_requests/2464),
    [internal MR 2095](https://gitlab.khronos.org/openxr/openxr/merge_requests/2095),
    [internal MR 3225](https://gitlab.khronos.org/openxr/openxr/merge_requests/3225))
  - New multi-vendor extension: `XR_EXT_interaction_render_models`
    ([internal MR 2615](https://gitlab.khronos.org/openxr/openxr/merge_requests/2615),
    [internal issue 2353](https://gitlab.khronos.org/openxr/openxr/issues/2353),
    [internal MR 3551](https://gitlab.khronos.org/openxr/openxr/merge_requests/3551),
    [internal MR 3629](https://gitlab.khronos.org/openxr/openxr/merge_requests/3629),
    [internal MR 3710](https://gitlab.khronos.org/openxr/openxr/merge_requests/3710))
  - New vendor extension: `XR_BD_spatial_plane`
    ([internal MR 3777](https://gitlab.khronos.org/openxr/openxr/merge_requests/3777))

## OpenXR SDK 1.1.48 (2025-06-03)

This release makes a switch to 16KB page sizes for Android, to improve
compatibility of the loader binaries, among other build system adjustments. The
"gfxwrapper" utility used by hello_xr (and the CTS) now uses the widely-used
"GLAD2" wrapper to load OpenGL and related functionality, rather than internal
hand-coded wrappers.

- SDK
  - Improvement: Adapt gfxwrapper to use GLAD2 to load OpenGL, OpenGL ES, and EGL functions.
    ([internal MR 3125](https://gitlab.khronos.org/openxr/openxr/merge_requests/3125),
    [internal MR 3126](https://gitlab.khronos.org/openxr/openxr/merge_requests/3126))
  - Improvement: Minor loader_test debugging improvements.
    ([internal MR 3836](https://gitlab.khronos.org/openxr/openxr/merge_requests/3836))
  - Improvement: Correctly detect availability of OpenGL on less common Windows
    platform variants, to fix 32-bit ARM non-UWP builds.
    ([internal MR 3838](https://gitlab.khronos.org/openxr/openxr/merge_requests/3838))
  - Improvement: hello_xr: Warning fixes.
    ([OpenXR-SDK-Source PR 516](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/516))
  - Improvement: Enable 16KB page sizes when building for Android, required for
    targeting Android 15+.
    ([OpenXR-SDK-Source PR 548](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/548),
    [OpenXR-SDK-Source issue 546](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/546))
- Registry
  - Change: Increment the revision of `XR_BD_spatial_anchor_sharing`.
    ([internal MR 3855](https://gitlab.khronos.org/openxr/openxr/merge_requests/3855))
  - Chore: Reserve extension numbers.
    ([internal MR 3696](https://gitlab.khronos.org/openxr/openxr/merge_requests/3696),
    [internal MR 3797](https://gitlab.khronos.org/openxr/openxr/merge_requests/3797),
    [internal MR 3815](https://gitlab.khronos.org/openxr/openxr/merge_requests/3815),
    [internal MR 3827](https://gitlab.khronos.org/openxr/openxr/merge_requests/3827))
  - Fix: Explicitly list the "x" and "y" thumbstick components on
    "pico_g3_controller" in the XML.
    ([internal MR 3851](https://gitlab.khronos.org/openxr/openxr/merge_requests/3851))
  - Increment graphics binding extension revision numbers due to
    rephrase/reorganization.
    ([internal MR 3028](https://gitlab.khronos.org/openxr/openxr/merge_requests/3028),
    [internal MR 3742](https://gitlab.khronos.org/openxr/openxr/merge_requests/3742))
  - New vendor extension: `XR_META_simultaneous_hands_and_controllers`
    ([internal MR 2755](https://gitlab.khronos.org/openxr/openxr/merge_requests/2755))
  - New vendor extension: `XR_META_body_tracking_full_body`
    ([internal MR 2961](https://gitlab.khronos.org/openxr/openxr/merge_requests/2961))
  - New vendor extension: `XR_BD_future_progress`
    ([internal MR 3724](https://gitlab.khronos.org/openxr/openxr/merge_requests/3724))

## OpenXR SDK 1.1.47 (2025-04-08)

This release features several new vendor extensions, one of which required a
modification to the XML schema for extending interaction profiles to represent
accurate. It also contains substantial fixes to the `core_validation` layer to
make it more usable and fix common false-positive validation errors, among other
improvements.

- Registry
  - Addition to XML registry schema: Specify interaction profile additions by
    constructing a predicate, and allow adding new top level /user paths to
    existing profiles. See SDK changelog and style guide for details.
    ([internal MR 2467](https://gitlab.khronos.org/openxr/openxr/merge_requests/2467))
  - Chore: Reserve extension numbers.
    ([internal MR 3729](https://gitlab.khronos.org/openxr/openxr/merge_requests/3729),
    [internal MR 3744](https://gitlab.khronos.org/openxr/openxr/merge_requests/3744),
    [internal MR 3745](https://gitlab.khronos.org/openxr/openxr/merge_requests/3745))
  - Fix: Reflect requirement of `XR_META_hand_tracking_microgestures` for
    `XR_EXT_hand_interaction` in `xr.xml`.
    ([internal MR 3741](https://gitlab.khronos.org/openxr/openxr/merge_requests/3741))
  - Fix: Added missing comment on `XR_EYE_POSITION_COUNT_FB` to remove warning
    during build.
    ([internal MR 3748](https://gitlab.khronos.org/openxr/openxr/merge_requests/3748))
  - Fix: typo in the documentation of `XR_ERROR_SPACE_GROUP_NOT_FOUND_META`
    ([internal MR 3749](https://gitlab.khronos.org/openxr/openxr/merge_requests/3749))
  - Improvement: Fix schematron runner on Mac.
    ([internal MR 3759](https://gitlab.khronos.org/openxr/openxr/merge_requests/3759))
  - New vendor extension: `XR_META_detached_controllers`
    ([internal MR 2467](https://gitlab.khronos.org/openxr/openxr/merge_requests/2467))
  - New vendor extension: `XR_BD_spatial_sensing`
    ([internal MR 3429](https://gitlab.khronos.org/openxr/openxr/merge_requests/3429))
  - New vendor extension: `XR_BD_spatial_anchor`
    ([internal MR 3435](https://gitlab.khronos.org/openxr/openxr/merge_requests/3435))
  - New vendor extension: `XR_BD_spatial_anchor_sharing`
    ([internal MR 3436](https://gitlab.khronos.org/openxr/openxr/merge_requests/3436))
  - New vendor extension: `XR_BD_spatial_scene`
    ([internal MR 3438](https://gitlab.khronos.org/openxr/openxr/merge_requests/3438))
  - New vendor extension: `XR_BD_spatial_mesh`
    ([internal MR 3439](https://gitlab.khronos.org/openxr/openxr/merge_requests/3439))
  - schema: Allow aliases of function pointers, primarily for use in extension
    promotion.
    ([internal MR 2989](https://gitlab.khronos.org/openxr/openxr/merge_requests/2989))
- SDK
  - Validation Layer: Fix: Fixes validation layer check for next chain validation
    by changing it from recursive to iterative. This allows for subsequent struct
    to validate off the initial set of valid chained structs.
    ([internal MR 3684](https://gitlab.khronos.org/openxr/openxr/merge_requests/3684),
    [internal issue 2434](https://gitlab.khronos.org/openxr/openxr/issues/2434))
  - Validation Layer: Fix: Fixes validation layer check for structs that inherit
    off a base struct. Inherited structs also have the same list of valid chained
    structs as their parent.
    ([internal MR 3684](https://gitlab.khronos.org/openxr/openxr/merge_requests/3684),
    [internal issue 2434](https://gitlab.khronos.org/openxr/openxr/issues/2434))
  - Validation Layer: Fix: Fixes null dereference bugs in generated code in layer.
    ([internal MR 3684](https://gitlab.khronos.org/openxr/openxr/merge_requests/3684),
    [internal issue 2434](https://gitlab.khronos.org/openxr/openxr/issues/2434))
  - Validation Layer: Fix: Adds a validation layer exception to
    `xrGetRecommendedLayerResolutionMETA` which allows a `XR_NULL_HANDLE` to be
    valid when checking the `XrSwapchainSubImage` struct.
    ([internal MR 3689](https://gitlab.khronos.org/openxr/openxr/merge_requests/3689),
    [internal issue 2425](https://gitlab.khronos.org/openxr/openxr/issues/2425))
  - Validation Layer: Improvement: Add time stamp to the log messages
    ([internal MR 3723](https://gitlab.khronos.org/openxr/openxr/merge_requests/3723))
  - Validation Layer: Improvement: Generate debug messages when `core_validation`
    layer starts / exits
    ([internal MR 3723](https://gitlab.khronos.org/openxr/openxr/merge_requests/3723))
  - hello_xr: Improvement: Update Android target SDK version to 34 to eliminate
    Play Protect warning on install. (Minimum SDK remains 24.)
    ([internal MR 3719](https://gitlab.khronos.org/openxr/openxr/merge_requests/3719))
  - list_json: Improvement: Use `XrInstanceCreateInfoAndroidKHR` on Android for
    improved compatibility.
    ([internal MR 3775](https://gitlab.khronos.org/openxr/openxr/merge_requests/3775))

## OpenXR SDK 1.1.46 (2025-03-04)

This release includes a new ratified Khronos extension, new vendor extensions,
and the ratification of several existing multi-vendor extensions. In the SDK
specifically, Android AAR packages for the loader are now easier to use with a
copy of the loader library in a path supported by older-style build systems, as
well as inclusion of a top-level architecture-independent CMake config/version
file when unpacked for easier use with pure-CMake build systems.

- Registry
  - Chore: Reserve extension numbers.
    ([internal MR 3701](https://gitlab.khronos.org/openxr/openxr/merge_requests/3701),
    [internal MR 3711](https://gitlab.khronos.org/openxr/openxr/merge_requests/3711),
    [internal MR 3722](https://gitlab.khronos.org/openxr/openxr/merge_requests/3722))
  - Chore: Register Sony author ID.
    ([internal MR 3709](https://gitlab.khronos.org/openxr/openxr/merge_requests/3709))
  - Fix: XML contained incorrect paths for dpad emulation bindings for
    `.../bytedance/pico_neo3_controller`, `.../bytedance/pico4_controller`,
    `.../bytedance/pico_g3_controller`, `.../yvr/touch_controller_yvr`, and
    `.../microsoft/xbox_controller`.
    ([internal MR 3674](https://gitlab.khronos.org/openxr/openxr/merge_requests/3674))
  - Improvement: Ratify a list of shared vendor extensions as well as add support
    for the 'ratified' attribute in XML and html generation. See
    <https://registry.khronos.org/OpenXR/specs/1.1/extprocess.html> for more
    information.
    ([internal MR 3494](https://gitlab.khronos.org/openxr/openxr/merge_requests/3494),
    [internal issue 2404](https://gitlab.khronos.org/openxr/openxr/issues/2404),
    [internal MR 3677](https://gitlab.khronos.org/openxr/openxr/merge_requests/3677))
  - New ratified Khronos extension: `XR_KHR_extended_struct_name_lengths`
    ([internal MR 3254](https://gitlab.khronos.org/openxr/openxr/merge_requests/3254),
    [internal issue 1664](https://gitlab.khronos.org/openxr/openxr/issues/1664))
  - New vendor extension: `XR_LOGITECH_mx_ink_stylus_interaction`
    ([internal MR 3242](https://gitlab.khronos.org/openxr/openxr/merge_requests/3242),
    [internal MR 3583](https://gitlab.khronos.org/openxr/openxr/merge_requests/3583),
    [internal MR 3584](https://gitlab.khronos.org/openxr/openxr/merge_requests/3584),
    [internal MR 3585](https://gitlab.khronos.org/openxr/openxr/merge_requests/3585))
  - New vendor extension: `XR_META_hand_tracking_microgestures`
    ([internal MR 3433](https://gitlab.khronos.org/openxr/openxr/merge_requests/3433),
    [internal MR 3725](https://gitlab.khronos.org/openxr/openxr/merge_requests/3725))
- SDK
  - Fix: Require both "xcb" and "xcb-glx" modules correctly in order to configure
    build for use of xcb headers.
    ([internal MR 3703](https://gitlab.khronos.org/openxr/openxr/merge_requests/3703),
    [internal issue 2467](https://gitlab.khronos.org/openxr/openxr/issues/2467))
  - Improvement: Refactored usage of Android logging to be more consistent.
    ([internal MR 2909](https://gitlab.khronos.org/openxr/openxr/merge_requests/2909))
  - Improvement: In the Android loader AAR, place a copy of the loader library in
    the `/jni/ARCH/` directory for older build systems.
    ([internal MR 3261](https://gitlab.khronos.org/openxr/openxr/merge_requests/3261),
    [internal issue 2285](https://gitlab.khronos.org/openxr/openxr/issues/2285))
  - Improvement: Include cross-architecture CMake config/version files in the root
    of the Android `.aar` artifact, allowing it to be unpacked and used easily by
    software not using the Android Gradle Plugin and Prefab.
    ([internal MR 3658](https://gitlab.khronos.org/openxr/openxr/merge_requests/3658))
  - Update: Upgrade bundled jsoncpp to 1.9.6.
    ([internal MR 3502](https://gitlab.khronos.org/openxr/openxr/merge_requests/3502))

## OpenXR SDK 1.1.45 (2025-02-05)

This release includes a new multi-vendor extension, a new vendor extension,
improvements to Android builds and artifacts, and a revision to a new
architecture added in 1.1.42.

Note that SDK release 1.1.44 was skipped to keep up with a monthly cadence for
patch releases.

- Registry
  - Extension reservation: Reserve 15 extensions for EpicGames.
    ([internal MR 3649](https://gitlab.khronos.org/openxr/openxr/merge_requests/3649))
  - Improvement: Clean up spacing in some functions, improving specification and
    header output appearance.
    ([internal MR 3660](https://gitlab.khronos.org/openxr/openxr/merge_requests/3660))
  - New multi-vendor extension: `XR_EXT_frame_synthesis`
    ([internal MR 2200](https://gitlab.khronos.org/openxr/openxr/merge_requests/2200),
    [OpenXR-Docs PR 122](https://github.com/KhronosGroup/OpenXR-Docs/pull/122))
  - New vendor extension: `XR_BD_body_tracking`
    ([internal MR 2867](https://gitlab.khronos.org/openxr/openxr/merge_requests/2867))
- SDK
  - Change: Update the ABI identifier of LoongArch64 in
    `specification/loader/runtime.adoc` and the loader. This introduces a small
    incompatibility on this platform, but only if you were already decorating your
    manifests with ABI/architecture, which is unlikely.
    ([OpenXR-SDK-Source PR 523](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/523))
  - Improvement: Update Gradle (for hello_xr and other tests) from 7.5/7.5.1 to to
    8.5, and Android Gradle Plugin to 8.1.4.
    ([internal MR 3640](https://gitlab.khronos.org/openxr/openxr/merge_requests/3640))
  - Improvement: Adjust build so that it is possible to build a new binary `.aar`
    file from an unzipped sources.jar file, by calling something like `bash
    org/khronos/openxr/openxr_loader_for_android/maintainer-scripts/build-aar.sh`.
    ([internal MR 3642](https://gitlab.khronos.org/openxr/openxr/merge_requests/3642))
  - Improvement: In `build-aar.sh`, skip making a sources.jar automatically if we
    lack the requirements (such as when we are already building from one.)
    ([internal MR 3642](https://gitlab.khronos.org/openxr/openxr/merge_requests/3642))

## OpenXR SDK 1.1.43 (2024-11-27)

This release has a few fixes and minor improvements, as well as support for
several new vendor extensions. It also improves the "loader_test" which was
broken on Android. CMake config files are now also included in the Android
loader AAR distributed for this release, for those not using Gradle and the
Android Gradle Plugin's support of "Prefab" format native libraries to consume
the loader.

- Registry
  - Bump version of `XR_KHR_vulkan_swapchain_format_list` and note that it depends
    on either `XR_KHR_vulkan_enable` or `XR_KHR_vulkan_enable2`.
    ([internal MR 3418](https://gitlab.khronos.org/openxr/openxr/merge_requests/3418))
  - Fix: Correctly mark `XR_FB_touch_controller_pro` and
    `XR_META_touch_controller_plus` as promoted to 1.1.
    ([internal MR 3586](https://gitlab.khronos.org/openxr/openxr/merge_requests/3586))
  - Improvement: Small XML formatting/organization cleanups.
    ([internal MR 3610](https://gitlab.khronos.org/openxr/openxr/merge_requests/3610))
  - New vendor extension: `XR_ML_facial_expression`
    ([internal MR 3100](https://gitlab.khronos.org/openxr/openxr/merge_requests/3100))
  - New vendor extension: `XR_META_passthrough_layer_resumed_event`
    ([internal MR 3106](https://gitlab.khronos.org/openxr/openxr/merge_requests/3106))
  - New vendor extensions: `XR_META_colocation_discovery`,
    `XR_META_spatial_entity_sharing`, and `XR_META_spatial_entity_group_sharing`
    ([internal MR 2782](https://gitlab.khronos.org/openxr/openxr/merge_requests/2782))
  - Reservation: Reserve numbers for spatial extensions.
    ([internal MR 3577](https://gitlab.khronos.org/openxr/openxr/merge_requests/3577))
- SDK
  - Fix: loader_test: API version in Android assets, fixes test breakage since
    1.1.x.
    ([internal MR 3598](https://gitlab.khronos.org/openxr/openxr/merge_requests/3598))
  - Improvement: Loader test: Update Catch2 from v3.3.2 to v3.7.1. Provides build-
    time and runtime performance improvements, among other changes.
    ([internal MR 2893](https://gitlab.khronos.org/openxr/openxr/merge_requests/2893))
  - Improvement: Accept command line options to `build-aar.sh`, including one that
    allows including CMake configs in case you are building for Android but not
    using Gradle and Android Gradle Plugin to consume the `.aar`.
    ([internal MR 3423](https://gitlab.khronos.org/openxr/openxr/merge_requests/3423))
  - Improvement: Loader: Update jnipp, used for Android builds. New version
    includes a build fix for some environments, as well as a crash fix.
    ([internal MR 3589](https://gitlab.khronos.org/openxr/openxr/merge_requests/3589))
  - Improvement: Add `disable_environment` field to the output of
    `generate_api_layer_manifest.py` script.
    ([internal MR 3591](https://gitlab.khronos.org/openxr/openxr/merge_requests/3591))
  - Improvement: hello_xr: Add Vulkan debug messages during Vulkan instance
    creation.
    ([internal MR 3592](https://gitlab.khronos.org/openxr/openxr/merge_requests/3592))
  - Improvement: Loader test: Use Catch2 idiomatic assertions and captures to make
    it easier to debug.
    ([internal MR 3599](https://gitlab.khronos.org/openxr/openxr/merge_requests/3599))

## OpenXR SDK 1.1.42 (2024-10-25)

This release updates a vendor extension with renamed enumerants, adds
architecture support for `loong64`, and delivers substantial improvements and
fixes to the XML registry, particularly the description of interaction profile

- Registry
  - Fix: Update schema to reflect that `XrPathString_t` should allow dash in
    interaction profile paths.
    ([internal MR 3493](https://gitlab.khronos.org/openxr/openxr/merge_requests/3493))
  - Fix: `XR_VARJO_xr4_controller_interaction` did not properly define its
    interaction profile in XML.
    ([internal MR 3493](https://gitlab.khronos.org/openxr/openxr/merge_requests/3493),
    [internal MR 3548](https://gitlab.khronos.org/openxr/openxr/merge_requests/3548))
  - Fix: Correct XML description of OpenXR 1.1 related additions to the promoted
    Meta Touch Plus, Touch Pro, and Touch (Rift CV1) controller interaction
    profiles.
    ([internal MR 3513](https://gitlab.khronos.org/openxr/openxr/merge_requests/3513),
    [internal issue 2350](https://gitlab.khronos.org/openxr/openxr/issues/2350),
    [internal issue 2375](https://gitlab.khronos.org/openxr/openxr/issues/2375))
  - Fix: Add missing XML description of `EXT_hand_interaction` additions to several
    interaction profiles, and add comments to clarify where profile additions
    should be located.
    ([internal MR 3517](https://gitlab.khronos.org/openxr/openxr/merge_requests/3517),
    [internal MR 3541](https://gitlab.khronos.org/openxr/openxr/merge_requests/3541),
    [internal MR 3552](https://gitlab.khronos.org/openxr/openxr/merge_requests/3552))
  - Fix: Corrections to the Schema chapter of the style guide.
    ([internal MR 3521](https://gitlab.khronos.org/openxr/openxr/merge_requests/3521))
  - Improvement: Small consistency clean-up.
    ([internal MR 3512](https://gitlab.khronos.org/openxr/openxr/merge_requests/3512))
  - Improvement: Clean up `.rnc` schema to improve readability.
    ([internal MR 3521](https://gitlab.khronos.org/openxr/openxr/merge_requests/3521))
  - Scripts: Improve `update_version.py` used in release process.
    ([internal MR 3543](https://gitlab.khronos.org/openxr/openxr/merge_requests/3543))
  - Update: Change naming convention in `XR_HTC_facial_expression`: rename
    `XR_LIP_EXPRESSION_MOUTH_SMILE_RIGHT_HTC` to
    `XR_LIP_EXPRESSION_MOUTH_RAISER_RIGHT_HTC`,
    `XR_LIP_EXPRESSION_MOUTH_SMILE_LEFT_HTC` to
    `XR_LIP_EXPRESSION_MOUTH_RAISER_LEFT_HTC`,
    `XR_LIP_EXPRESSION_MOUTH_SAD_RIGHT_HTC` to
    `XR_LIP_EXPRESSION_MOUTH_STRETCHER_RIGHT_HTC` and
    `XR_LIP_EXPRESSION_MOUTH_SAD_LEFT_HTC` to
    `XR_LIP_EXPRESSION_MOUTH_STRETCHER_LEFT_HTC`, providing the old names as
    compatibility aliases.
    ([internal MR 3408](https://gitlab.khronos.org/openxr/openxr/merge_requests/3408))
- SDK
  - Loader: Fix build error on `loong64`, and add `loong64` in the architecture
    table in the loader documentation.
    ([OpenXR-SDK-Source PR 479](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/479))

## OpenXR SDK 1.1.41 (2024-09-25)

This release features several new vendor extensions, as well as some small
improvements and fixes to the software.

- Registry
  - Change: Allow structs that can extend multiple other structs in the RelaxNG
    schema, as already permitted by the Vulkan schema.
    ([internal MR 2869](https://gitlab.khronos.org/openxr/openxr/merge_requests/2869))
  - New vendor extension: `XR_HTC_body_tracking`
    ([internal MR 2549](https://gitlab.khronos.org/openxr/openxr/merge_requests/2549))
  - New vendor extension: `XR_ML_spatial_anchors`
    ([internal MR 2803](https://gitlab.khronos.org/openxr/openxr/merge_requests/2803))
  - New vendor extension: `XR_ML_spatial_anchors_storage`
    ([internal MR 2804](https://gitlab.khronos.org/openxr/openxr/merge_requests/2804))
  - New vendor extension: `XR_ML_system_notifications`
    ([internal MR 2946](https://gitlab.khronos.org/openxr/openxr/merge_requests/2946))
  - New vendor extension: `XR_ML_world_mesh_detection`
    ([internal MR 2950](https://gitlab.khronos.org/openxr/openxr/merge_requests/2950))
  - New vendor extension: `XR_ML_view_configuration_depth_range_change`
    ([internal MR 3036](https://gitlab.khronos.org/openxr/openxr/merge_requests/3036))
- SDK
  - Fix: Do not enforce overly-strict requirements on structs using `*BaseHeader`
    types in the code generation scripts, fixing a build-time warning for the
    layers.
    ([internal MR 3434](https://gitlab.khronos.org/openxr/openxr/merge_requests/3434))
  - Improvement: Migrate CMake build system away from using
    `find_package(PythonInterpreter)`, deprecated since CMake 3.12. Use
    `find_package(Python3 COMPONENTS Interpreter)` instead.
    ([OpenXR-SDK-Source PR 486](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/486),
    [internal MR 3472](https://gitlab.khronos.org/openxr/openxr/merge_requests/3472))
  - Validation layer: Improvement: Clean up `generate_vuid_database` script, used
    to analyze the validation layer.
    ([internal MR 2895](https://gitlab.khronos.org/openxr/openxr/merge_requests/2895))
  - ci: Remove now-redundant gradle-wrapper-validation job from GitHub Actions
    ([OpenXR-SDK-Source PR 500](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/500))
  - ci: Add GitHub Action for macOS building
    ([OpenXR-SDK-Source PR 501](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/501))
  - doc: Add command to build OpenXR targets on macOS
    ([OpenXR-SDK-Source PR 501](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/501))
  - hello_xr: Addition: Log Vulkan extensions requested by runtime and by app,
    visible when running with `--verbose`.
    ([OpenXR-SDK-Source PR 403](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/403))

## OpenXR SDK 1.1.40 (2024-08-22)

This release features a new ratified graphics API binding extension,
`XR_KHR_metal_enable`, including support in hello_xr. The loader test has had
substantial improvements as well. There are also an assortment of smaller fixes
and improvements.

- Registry
  - Add: New ratified Khronos extension: `XR_KHR_metal_enable`.
    ([internal MR 2721](https://gitlab.khronos.org/openxr/openxr/merge_requests/2721))
  - Chore: Reserve 15 extension id numbers for ByteDance.
    ([internal MR 3409](https://gitlab.khronos.org/openxr/openxr/merge_requests/3409))
  - Fix: Clarified that views in `XR_VARJO_quad_views` needs to have identical
    poses for each eye.
    ([internal MR 3396](https://gitlab.khronos.org/openxr/openxr/merge_requests/3396))
  - Fix: Add missing interaction profile extensions for OpenXR 1.1 promoted Meta
    interaction profiles.
    ([internal MR 3398](https://gitlab.khronos.org/openxr/openxr/merge_requests/3398))
  - Fix: Correctly mark the Magic Leap home button as a system button in the XML.
    ([internal MR 3405](https://gitlab.khronos.org/openxr/openxr/merge_requests/3405))
  - Fix: Add `XR_ERROR_VALIDATION_FAILURE` to all functions from
    `XR_EXT_conformance_automation`.
    ([internal MR 3417](https://gitlab.khronos.org/openxr/openxr/merge_requests/3417))
- SDK
  - API dump layer: Improvement: Move a non-generated function out of the Python-
    generated source file.
    ([internal MR 3336](https://gitlab.khronos.org/openxr/openxr/merge_requests/3336))
  - API dump layer: Improvement: Clean up usage of RAII mutex lock guards.
    ([internal MR 3336](https://gitlab.khronos.org/openxr/openxr/merge_requests/3336))
  - Layers and loader: Improvement: Disambiguate `XrGeneratedDispatchTable` between
    loader and API layers.
    ([internal MR 3406](https://gitlab.khronos.org/openxr/openxr/merge_requests/3406))
  - Loader test: Improvement: Migrate to use Catch2 (matching the CTS) instead of
    an ad-hoc test framework.
    ([internal MR 3337](https://gitlab.khronos.org/openxr/openxr/merge_requests/3337))
  - hello_xr: Add: Metal graphics plugin (use `-g Metal`) for running it on macOS
    with `XR_KHR_metal_enable` extension.
    ([internal MR 3009](https://gitlab.khronos.org/openxr/openxr/merge_requests/3009),
    [internal MR 3456](https://gitlab.khronos.org/openxr/openxr/merge_requests/3456))
  - hello_xr: Improvement: Use `XrMatrix4x4f_CreateFromRigidTransform` in place of
    `XrMatrix4x4f_CreateTranslationRotationScale` for known rigid transforms.
    ([internal MR 3349](https://gitlab.khronos.org/openxr/openxr/merge_requests/3349))

**Note**: There is no 1.1.39 release: it was skipped to keep the monthly patch
version increment cadence given the lack of a release in July.

## OpenXR SDK 1.1.38 (2024-06-09)

This is a fairly small release, with one new extension and a handful of fixes.

- Registry
  - Addition: New multi-vendor extension: `XR_EXT_composition_layer_inverted_alpha`
    ([internal MR 3085](https://gitlab.khronos.org/openxr/openxr/merge_requests/3085),
    [internal MR 3385](https://gitlab.khronos.org/openxr/openxr/merge_requests/3385))
  - Chore: Reserve an extension for Logitech.
    ([internal MR 3384](https://gitlab.khronos.org/openxr/openxr/merge_requests/3384))
  - Chore: Register author tag for Deep Mirror.
    ([OpenXR-Docs PR 171](https://github.com/KhronosGroup/OpenXR-Docs/pull/171))
  - Fix: `XrCompositionLayerPassthroughFB` has a "parentstruct" of
    `XrCompositionLayerBaseHeader` (it is based on this type), rather than
    "structextends" (in the next chain). Bump extension revision.
    ([internal MR 3305](https://gitlab.khronos.org/openxr/openxr/merge_requests/3305))
  - Fix: `XR_EXT_plane_detection`: Fix extents description and plane axis to match
    CTS and implementations.
    ([internal MR 3374](https://gitlab.khronos.org/openxr/openxr/merge_requests/3374),
    [internal issue 2281](https://gitlab.khronos.org/openxr/openxr/issues/2281))
  - Fix: Correct typo in `XR_FB_keyboard_tracking` flag description.
    ([internal MR 3393](https://gitlab.khronos.org/openxr/openxr/merge_requests/3393))
- SDK
  - No significant changes

## OpenXR SDK 1.1.37 (2024-05-23)

This release primarily adds new defines for easier use of both OpenXR 1.0 and
1.1 with up-to-date headers, some documentation improvements, and improvements
for Android, including support for using the "API Dump" and "Validation" API
layers in your own APK during the development process.

- Registry
  - Addition: New `XR_API_VERSION_1_0` and `XR_API_VERSION_1_1` defines to allow
    applications to easily specify OpenXR "major" and "minor" version while passing
    through the "patch" version.
    ([internal MR 3329](https://gitlab.khronos.org/openxr/openxr/merge_requests/3329),
    [internal MR 3354](https://gitlab.khronos.org/openxr/openxr/merge_requests/3354),
    [internal issue 2254](https://gitlab.khronos.org/openxr/openxr/issues/2254))
  - Addition: Register Razer vendor ID.
    ([internal MR 3340](https://gitlab.khronos.org/openxr/openxr/merge_requests/3340))
  - Fix: Add "palm_pose" to "touch_controller_pro" and "touch_controller_plus" in
    XML.
    ([internal MR 3363](https://gitlab.khronos.org/openxr/openxr/merge_requests/3363))
  - Improvement: Add Schematron rule to avoid triggering edge cases of vendor tags
    ending with X.
    ([internal MR 3341](https://gitlab.khronos.org/openxr/openxr/merge_requests/3341))
  - Reservation: Reserve extension numbers for a few new EXT extensions.
    ([internal MR 3285](https://gitlab.khronos.org/openxr/openxr/merge_requests/3285),
    [internal MR 3292](https://gitlab.khronos.org/openxr/openxr/merge_requests/3292))
  - Update: Bump version of `XR_FB_composition_layer_alpha_blend` due to spec text
    clarification.
    ([internal MR 3317](https://gitlab.khronos.org/openxr/openxr/merge_requests/3317))
- SDK
  - Addition: Ship `open-in-docker.sh` script for use building the loader design
    doc.
    ([internal MR 3352](https://gitlab.khronos.org/openxr/openxr/merge_requests/3352),
    [internal issue 2283](https://gitlab.khronos.org/openxr/openxr/issues/2283),
    [OpenXR-SDK-Source issue 476](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/476))
  - Fix: Fix references to Docker container in spec build instructions and clarify
    that most parts do not apply to the SDK-Source repo.
    ([internal MR 3352](https://gitlab.khronos.org/openxr/openxr/merge_requests/3352),
    [internal issue 2283](https://gitlab.khronos.org/openxr/openxr/issues/2283),
    [OpenXR-SDK-Source issue 476](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/476))
  - Fix: Do not load all Android app-supplied layers as explicit, but rather as
    their actual type.
    ([OpenXR-SDK-Source PR 475](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/475),
    [internal issue 2284](https://gitlab.khronos.org/openxr/openxr/issues/2284))
  - Improvement: Use new `XR_API_VERSION_1_0` and `XR_API_VERSION_1_1` defines.
    ([internal MR 3329](https://gitlab.khronos.org/openxr/openxr/merge_requests/3329),
    [internal issue 2254](https://gitlab.khronos.org/openxr/openxr/issues/2254))
  - Improvement: Add Android support to "api_dump" and "core_validation" API
    layers.
    ([internal MR 3330](https://gitlab.khronos.org/openxr/openxr/merge_requests/3330))

## OpenXR SDK 1.1.36 (2024-04-15)

This is a substantial update to the OpenXR specification. The OpenXR loader in
this release supports both OpenXR 1.0 and 1.1, and sample applications such as
hello_xr continue to only require OpenXR 1.0. The schema associated with the
`xr.xml` description of OpenXR has received a small but breaking change, so
software that parses it may need an update accordingly. Additionally, the
protocol for the OpenXR loader on Android to communicate with system or
installable runtime brokers has been extended for improvfed backward- and
forward-compatibility; see the changes to the loader design document for more
information.

- Registry
  - New ratified Khronos extension: `XR_KHR_locate_spaces`
    ([internal MR 2272](https://gitlab.khronos.org/openxr/openxr/merge_requests/2272),
    [internal issue 1706](https://gitlab.khronos.org/openxr/openxr/issues/1706))
  - New ratified Khronos extension: `XR_KHR_maintenance1`
    ([internal MR 3053](https://gitlab.khronos.org/openxr/openxr/merge_requests/3053))
  - New ratified OpenXR version: `XR_VERSION_1_1` - OpenXR 1.1.
    ([internal MR 3053](https://gitlab.khronos.org/openxr/openxr/merge_requests/3053))
  - New multi-vendor extension: `XR_EXT_future`
    ([internal MR 2610](https://gitlab.khronos.org/openxr/openxr/merge_requests/2610))
  - New vendor extension: `XR_META_environment_depth`
    ([internal MR 2771](https://gitlab.khronos.org/openxr/openxr/merge_requests/2771),
    [internal MR 3271](https://gitlab.khronos.org/openxr/openxr/merge_requests/3271))
  - Mark `XR_OCULUS_android_session_state_enable` as deprecated.
    ([internal MR 3255](https://gitlab.khronos.org/openxr/openxr/merge_requests/3255))
  - Update the XML schema to change how dependencies are described (replacing
    `requiresCore` and `requires` attributes of `extension`, and `feature` and
    `extension` attributes of `require`, with a new `depends` attribute.). This is
    a **breaking change** of the XML schema, though in an infrequently processed
    attribute. This corresponds to the change made in Vulkan 1.3.241.
    ([internal MR 3260](https://gitlab.khronos.org/openxr/openxr/merge_requests/3260))
- SDK
  - API dump layer: Handle opaque futures defined by `XR_EXT_future`.
    ([internal MR 2610](https://gitlab.khronos.org/openxr/openxr/merge_requests/2610))
  - API dump layer: Zero initialize out-param in calls to `xrResultToString` and
    `xrStructureTypeToString`.
    ([internal MR 3284](https://gitlab.khronos.org/openxr/openxr/merge_requests/3284))
  - Android loader: Build using NDK 23.2.
    ([internal MR 2992](https://gitlab.khronos.org/openxr/openxr/merge_requests/2992))
  - Android loader: Allow the loader to check multiple records from the broker, for
    backward- and forward-compatibility
    ([internal MR 3269](https://gitlab.khronos.org/openxr/openxr/merge_requests/3269),
    [internal issue 2226](https://gitlab.khronos.org/openxr/openxr/issues/2226))
  - Loader: Improve error logging in the case that the Windows registry
    `ActiveRuntime` path cannot be parsed or found.
    ([internal MR 3015](https://gitlab.khronos.org/openxr/openxr/merge_requests/3015),
    [internal issue 2125](https://gitlab.khronos.org/openxr/openxr/issues/2125))
  - Loader: Remove path separator parsing from Windows registry `ActiveRuntime`
    path to fix bug.
    ([internal MR 3015](https://gitlab.khronos.org/openxr/openxr/merge_requests/3015))
  - Loader: Fix build issue for ARMv6 architectures, and note architecture naming
    quirk of Raspberry Pi OS in the architecture table in the loader documentation.
    ([OpenXR-SDK-Source PR 464](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/464),
    [OpenXR-SDK-Source issue 463](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/463))
  - Reduce duplication of environment variable getters and setters.
    ([internal MR 3039](https://gitlab.khronos.org/openxr/openxr/merge_requests/3039))
  - Updates to scripts and software to handle aliases and promoted functionality in
    `XR_KHR_maintenance1` and `XR_VERSION_1_1`
    ([internal MR 3053](https://gitlab.khronos.org/openxr/openxr/merge_requests/3053))
  - hello_xr: Fix Windows mirror window acquire, wait, present loop.
    ([internal MR 3289](https://gitlab.khronos.org/openxr/openxr/merge_requests/3289))
  - hello_xr and other samples: Update Android compile SDK version (to 33), NDK
    version (to 23.2), and build tools version (to 34.0.0).
    ([internal MR 2992](https://gitlab.khronos.org/openxr/openxr/merge_requests/2992))
  - hello_xr and runtime list: Request an OpenXR 1.0 instance by default.
    ([internal MR 3320](https://gitlab.khronos.org/openxr/openxr/merge_requests/3320))
  - loader_test: Build fixes to allow loader_test to compile / run on Android.
    ([internal MR 3153](https://gitlab.khronos.org/openxr/openxr/merge_requests/3153))

## OpenXR SDK 1.0.34 (2024-02-16)

This release features a number of new multi-vendor and vendor extensions,
additional functionality in the reflection header, as well as compatibility
improvements for the loader on Android.

- Registry
  - Extension reservation: Register author ID and reserve extensions for Leia.
    ([internal MR 3203](https://gitlab.khronos.org/openxr/openxr/merge_requests/3203))
  - Fix: Remove erroneous interaction profile component additions from extensions.
    ([internal MR 3223](https://gitlab.khronos.org/openxr/openxr/merge_requests/3223))
  - New multi-vendor extension: `XR_EXT_user_presence`
    ([internal MR 2706](https://gitlab.khronos.org/openxr/openxr/merge_requests/2706),
    [internal issue 1585](https://gitlab.khronos.org/openxr/openxr/issues/1585))
  - New vendor extension: `XR_META_recommended_layer_resolution`
    ([internal MR 2570](https://gitlab.khronos.org/openxr/openxr/merge_requests/2570))
  - New vendor extension: `XR_META_automatic_layer_filter`
    ([internal MR 2696](https://gitlab.khronos.org/openxr/openxr/merge_requests/2696))
  - New vendor extension: `XR_META_spatial_entity_mesh`
    ([internal MR 2773](https://gitlab.khronos.org/openxr/openxr/merge_requests/2773))
  - New vendor extension: `XR_FB_face_tracking2`
    ([internal MR 2811](https://gitlab.khronos.org/openxr/openxr/merge_requests/2811))
  - New vendor extension: `XR_VARJO_xr4_controller_interaction`
    ([internal MR 3078](https://gitlab.khronos.org/openxr/openxr/merge_requests/3078))
  - `XR_FB_scene`: Update to spec version 4.
    ([internal MR 2774](https://gitlab.khronos.org/openxr/openxr/merge_requests/2774))
  - `XR_META_headset_id` and `XR_FB_spatial_entity`: Drop `XR_EXT_uuid` dependency,
    they use the data structure but do not require any runtime support specific to
    `XR_EXT_uuid`
    ([internal MR 2577](https://gitlab.khronos.org/openxr/openxr/merge_requests/2577))
- SDK
  - API Layers: Add version-script for linking API Layers on Linux and Android.
    ([internal MR 3112](https://gitlab.khronos.org/openxr/openxr/merge_requests/3112))
  - Fix typo in `gfxwrapper_opengl` that did not affect the use in this repository
    directly, but may affect downstream users of this code.
    ([internal MR 3215](https://gitlab.khronos.org/openxr/openxr/merge_requests/3215))
  - Loader: fix to Android Loader so that the
    `/<path_to_apk>/my_apk_file.apk!/libs/libstuff.so` will not get blocked
    ([internal MR 3054](https://gitlab.khronos.org/openxr/openxr/merge_requests/3054))
  - Loader: Add missing ifdef guards for `XR_KHR_LOADER_INIT_SUPPORT`.
    ([internal MR 3152](https://gitlab.khronos.org/openxr/openxr/merge_requests/3152),
    [internal MR 3159](https://gitlab.khronos.org/openxr/openxr/merge_requests/3159))
  - Loader: Fix crash in case of calling `xrEnumerateInstanceExtensionProperties`
    before calling `xrInitializeLoaderKHR` on Android.
    ([internal MR 3159](https://gitlab.khronos.org/openxr/openxr/merge_requests/3159))
  - Loader design: Add a note about environment variables being ignored when run on
    Windows as admin.
    ([internal MR 3214](https://gitlab.khronos.org/openxr/openxr/merge_requests/3214))
  - `openxr_reflection.h`: Add macros to list functions provided by each feature /
    extension.
    ([internal MR 3129](https://gitlab.khronos.org/openxr/openxr/merge_requests/3129))
  - external: Update Jinja2 Python module shipped with repository (for source code
    generation) to 2.11.3.
    ([internal MR 3221](https://gitlab.khronos.org/openxr/openxr/merge_requests/3221),
    [internal MR 3237](https://gitlab.khronos.org/openxr/openxr/merge_requests/3237))

## OpenXR SDK 1.0.33 (2024-01-03)

This release primarily adds new ratified functionality describing the loader
interaction with runtimes and API layers. Corresponding definitions are now in
the official `openxr_loader_negotiation.h` generated header, rather than the
`loader_interfaces.h` header previously shipped only with the OpenXR-SDK-Source
repository. This change only affects vendors of runtimes and API layers as well
as contributors to the OpenXR loader: applications do not directly use this API,
the loader uses it on their behalf. A number of other small fixes are also
included.

- Registry
  - Extension reservation: Update author ID and reserve extensions for Varjo.
    ([internal MR 3083](https://gitlab.khronos.org/openxr/openxr/merge_requests/3083))
  - Extension reservation: Reserve 10 extension ids each for `ANDROIDX` &
    `ANDROIDSYS`.
    ([internal MR 3086](https://gitlab.khronos.org/openxr/openxr/merge_requests/3086))
  - Khronos ratified addition: Specify the existing loader negotiation functions
    (without modification) in the XML, moving from `loader_interfaces.h` to a new
    generated header `openxr_loader_negotiation.h`.
    ([internal MR 2807](https://gitlab.khronos.org/openxr/openxr/merge_requests/2807),
    [internal issue 1953](https://gitlab.khronos.org/openxr/openxr/issues/1953))
  - `XR_KHR_android_thread_settings`: Fix the description of
    `XrAndroidThreadTypeKHR` enum values - they were swapped relative to their
    implicit meaning from their name.
    ([internal MR 3077](https://gitlab.khronos.org/openxr/openxr/merge_requests/3077))
  - `XR_MNDX_egl_enable`: Update version to 2 to reflect function pointer type
    change released in 1.0.29.
    ([OpenXR-Docs PR 159](https://github.com/KhronosGroup/OpenXR-Docs/pull/159))
- SDK
  - Loader: Fix loader build on Universal Windows Platform: build-system-only
    change. (Included in SDK hotfix 1.0.32.1.)
    ([internal MR 3071](https://gitlab.khronos.org/openxr/openxr/merge_requests/3071))
  - Loader: Correctly destroy the LoaderInstance when loader is done.
    ([internal MR 3041](https://gitlab.khronos.org/openxr/openxr/merge_requests/3041))
  - Remove obsolete `loader_interfaces.h` header, migrating uses (in loader and
    layers) to use the newly specified and ratified `openxr_loader_negotiation.h`,
    and adjust scripts for the addition of the loader negotiation APIs.
    ([internal MR 2807](https://gitlab.khronos.org/openxr/openxr/merge_requests/2807),
    [internal issue 1953](https://gitlab.khronos.org/openxr/openxr/issues/1953),
    [internal MR 3122](https://gitlab.khronos.org/openxr/openxr/merge_requests/3122))
- Misc
  - Update/correct names.
  - Ship a `.mailmap` file in the public repositories, maintained separately
    from the larger one used in the private monorepo, to correct names/emails
    and unify contributor identities.
  - Update Khronos Group copyright dates.

## OpenXR SDK 1.0.32 (2023-11-29)

This release contains a number of vendor extensions, plus a new ratified
revision to the `XR_KHR_loader_init` extension that specifies forwarding the
init calls to API layers. **Vendors of API layers**, primarily on Android, must
verify they can handle being passed `XR_NULL_HANDLE` for the instance parameter
of `xrGetInstanceProcAddr`, to avoid bugs when using the updated loader. This
release also contains a number of build system cleanups and fixes. Users of the
Android Gradle Plugin and our official loader AAR file can now use the
`OpenXR::headers` target just like on desktop: there is now metadata for the
"prefab" tool to generate for CMake both this header/include-only target and the
normal `OpenXR::openxr_loader` imported library target. The shipped AAR is much
smaller due to stripping debug data from the binaries, which helps in case
application build systems do not automatically strip native binaries. A bug in
the loader Android manifest as shipped in 1.0.31 has also been fixed.

- Registry
  - Extension reservation: Reserve extension id for `XR_KHR_maintenance1`
    ([internal MR 3010](https://gitlab.khronos.org/openxr/openxr/merge_requests/3010))
  - Extension reservation: Reserve extension id for `XR_KHR_game_controller`
    ([internal MR 3019](https://gitlab.khronos.org/openxr/openxr/merge_requests/3019))
  - New vendor extension: `XR_HTC_anchor`
    ([internal MR 2667](https://gitlab.khronos.org/openxr/openxr/merge_requests/2667))
  - New vendor extension: `XR_META_touch_controller_plus`
    ([internal MR 2702](https://gitlab.khronos.org/openxr/openxr/merge_requests/2702))
  - New vendor extension: `XR_ML_marker_understanding`
    ([internal MR 2750](https://gitlab.khronos.org/openxr/openxr/merge_requests/2750))
  - New vendor extension: `XR_ML_localization_map`
    ([internal MR 2802](https://gitlab.khronos.org/openxr/openxr/merge_requests/2802),
    [internal MR 3045](https://gitlab.khronos.org/openxr/openxr/merge_requests/3045),
    [internal MR 3047](https://gitlab.khronos.org/openxr/openxr/merge_requests/3047))
  - `XR_KHR_loader_init`: New Khronos ratified revision, adds support for
    forwarding loader init calls to API layers
    ([internal MR 2703](https://gitlab.khronos.org/openxr/openxr/merge_requests/2703))
- SDK
  - Loader: Pass `xrInitializeLoaderKHR` calls to enabled API layers if
    `XR_KHR_loader_init` is enabled, per ratified update to that extension.
    ([internal MR 2703](https://gitlab.khronos.org/openxr/openxr/merge_requests/2703))
  - Loader: Partial fix for the loader not honoring
    `BUILD_LOADER_WITH_EXCEPTION_HANDLING` on Android.
    ([internal MR 2870](https://gitlab.khronos.org/openxr/openxr/merge_requests/2870),
    [OpenXR-SDK-Source PR 405](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/405),
    [internal issue 1999](https://gitlab.khronos.org/openxr/openxr/issues/1999))
  - Loader Android AAR: Strip binaries before inclusion in AAR, as loader is stable
    (and mostly shared with all platforms) and size difference is substantial.
  - Loader Android AAR: Expose `OpenXR::headers` prefab imported target just as on
    desktop builds
    ([internal MR 2886](https://gitlab.khronos.org/openxr/openxr/merge_requests/2886))
  - Loader Android AAR: Generate a source jar file for completeness.
    ([internal MR 2886](https://gitlab.khronos.org/openxr/openxr/merge_requests/2886))
  - Loader Android AAR: Add `<uses-sdk>` elements to Android loader AAR manifest,
    to prevent the manifest merger from assuming a version < 4 and adding unneeded
    permissions accordingly.
    ([internal MR 3029](https://gitlab.khronos.org/openxr/openxr/merge_requests/3029))
    ([internal MR 3032](https://gitlab.khronos.org/openxr/openxr/merge_requests/3032))
  - Clean up our CMake build substantially, correcting dependencies and narrowing
    the scope of includes.
    ([internal MR 2886](https://gitlab.khronos.org/openxr/openxr/merge_requests/2886),
    [OpenXR-SDK-Source issue 344](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/344),
    [internal issue 1872](https://gitlab.khronos.org/openxr/openxr/issues/1872),
    [OpenXR-SDK-Source issue 419](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/419),
    [internal issue 2071](https://gitlab.khronos.org/openxr/openxr/issues/2071),
    [internal MR 2987](https://gitlab.khronos.org/openxr/openxr/merge_requests/2987))
  - Fix build in directories containing spaces.
    ([internal MR 2886](https://gitlab.khronos.org/openxr/openxr/merge_requests/2886),
    [OpenXR-SDK-Source issue 344](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/344),
    [internal issue 1872](https://gitlab.khronos.org/openxr/openxr/issues/1872),
    [OpenXR-SDK-Source issue 419](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/419),
    [internal issue 2071](https://gitlab.khronos.org/openxr/openxr/issues/2071),
    [internal MR 2987](https://gitlab.khronos.org/openxr/openxr/merge_requests/2987))
  - Fix linking to GLX when glvnd is not found on the system
    ([internal MR 3000](https://gitlab.khronos.org/openxr/openxr/merge_requests/3000))
  - Fix use of `OpenXR::headers` target when not building the loader.
    ([internal MR 2886](https://gitlab.khronos.org/openxr/openxr/merge_requests/2886),
    [OpenXR-SDK-Source issue 344](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/344),
    [internal issue 1872](https://gitlab.khronos.org/openxr/openxr/issues/1872),
    [OpenXR-SDK-Source issue 419](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/419),
    [internal issue 2071](https://gitlab.khronos.org/openxr/openxr/issues/2071),
    [internal MR 2987](https://gitlab.khronos.org/openxr/openxr/merge_requests/2987))
  - scripts: Migrate `namedtuple` usage to dataclass, and expose the definitions
    for reuse.
    ([internal MR 2183](https://gitlab.khronos.org/openxr/openxr/merge_requests/2183))
  - scripts: Clean up formatting, clean up some issues found by type-aware Python
    editors, and improve the experience of editing Python scripts in some editors
    by adding a `.env` file.
    ([internal MR 2183](https://gitlab.khronos.org/openxr/openxr/merge_requests/2183))
  - scripts: Support base header types with no derived types defined yet.
    ([internal MR 2802](https://gitlab.khronos.org/openxr/openxr/merge_requests/2802))

## OpenXR SDK 1.0.31 (2023-10-18)

This release features two new vendor extensions and minor extension XML
revisions, in addition to compatibility and logging improvements in the
software.

- Registry
  - Extension reservation: Reserve extensions for NVIDIA.
    ([internal MR 2952](https://gitlab.khronos.org/openxr/openxr/merge_requests/2952))
  - Extension reservation: Reserve extensions for Meta.
    ([internal MR 2959](https://gitlab.khronos.org/openxr/openxr/merge_requests/2959))
  - Extension reservation: Reserve extensions for Android.
    ([internal MR 2966](https://gitlab.khronos.org/openxr/openxr/merge_requests/2966))
  - Extension reservation: Reserve an extension for `XR_KHR_egl_enable`.
    ([internal MR 2982](https://gitlab.khronos.org/openxr/openxr/merge_requests/2982))
  - New vendor extension: `XR_MSFT_scene_marker`
    ([internal MR 2601](https://gitlab.khronos.org/openxr/openxr/merge_requests/2601))
  - New vendor extension: `XR_ML_user_calibration`
    ([internal MR 2849](https://gitlab.khronos.org/openxr/openxr/merge_requests/2849))
  - Schematron: Allow chained structs that extend a two-call-idiom struct to carry
    only a `*CapacityInput` member.
    ([internal MR 2892](https://gitlab.khronos.org/openxr/openxr/merge_requests/2892),
    [internal issue 2059](https://gitlab.khronos.org/openxr/openxr/issues/2059))
  - `XR_FB_render_model`: Fix `structextends` attribute and remove `returnedonly`
    attribute of `XrRenderModelCapabilitiesRequestFB`, to match the specification
    prose.
    ([internal MR 2765](https://gitlab.khronos.org/openxr/openxr/merge_requests/2765),
    [OpenXR-Docs issue 153](https://github.com/KhronosGroup/OpenXR-Docs/issues/153),
    [internal issue 2017](https://gitlab.khronos.org/openxr/openxr/issues/2017))
  - xml: Fixed a few errors in MSFT extensions discovered by Schematron checks.
    ([internal MR 2892](https://gitlab.khronos.org/openxr/openxr/merge_requests/2892))
- SDK
  - API Layers: Add logging on API layer negotiation failure.
    ([internal MR 2926](https://gitlab.khronos.org/openxr/openxr/merge_requests/2926))
  - Fix: Enable build with clang-cl on Windows through Visual Studio.
    ([internal MR 2948](https://gitlab.khronos.org/openxr/openxr/merge_requests/2948))
  - Fix: Remove unused pthread prototypes declaration in `_USE_GNU` ifdef, fixing
    builds on some systems.
    ([internal MR 2981](https://gitlab.khronos.org/openxr/openxr/merge_requests/2981))
  - Fix comment typo in platform utils header.
    ([internal MR 2991](https://gitlab.khronos.org/openxr/openxr/merge_requests/2991))
  - gfxwrapper: Add OpenGL 3.3 functions to an internal utility library used by
    hello_xr, shared with the CTS.
    ([internal MR 2941](https://gitlab.khronos.org/openxr/openxr/merge_requests/2941))
  - loader: Modify `jnipp`, used by the loader on Android, to no longer use
    `basic_string<>` with types that are technically not in the C++ specification
    as permissible, to fix compatibility with an upcoming `libc++` update.
    ([internal MR 2974](https://gitlab.khronos.org/openxr/openxr/merge_requests/2974),
    [internal issue 2094](https://gitlab.khronos.org/openxr/openxr/issues/2094),
    [OpenXR-SDK-Source PR 426](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/426))
  - loader_test: Refactor to use existing macros for all test to avoid repetition.
    ([internal MR 2922](https://gitlab.khronos.org/openxr/openxr/merge_requests/2922))
  - scripts: Small fixes and cleanups
    ([internal MR 2998](https://gitlab.khronos.org/openxr/openxr/merge_requests/2998),
    [internal MR 2894](https://gitlab.khronos.org/openxr/openxr/merge_requests/2894),
    [internal MR 2896](https://gitlab.khronos.org/openxr/openxr/merge_requests/2896))

## OpenXR SDK 1.0.30 (2023-09-20)

This release is primarily a quality improvement release, fixing a range of
issues in the registry and SDK, including fixing a loader bug related to layers,
in addition to a new vendor extension and an updated vendor extension.

- Registry
  - Add missing enum tags for enum-sized array struct members.
    ([internal MR 2731](https://gitlab.khronos.org/openxr/openxr/merge_requests/2731))
  - Fix EGL "get proc addr" function pointer typedef.
    ([internal MR 2939](https://gitlab.khronos.org/openxr/openxr/merge_requests/2939))
  - New vendor extension: `XR_YVR_controller_interaction`
    ([internal MR 2841](https://gitlab.khronos.org/openxr/openxr/merge_requests/2841))
  - `XR_BD_controller_interaction`: Add support for G3 devices
    ([internal MR 2872](https://gitlab.khronos.org/openxr/openxr/merge_requests/2872))
  - Fix specification errors highlighted by fixed tooling.
    ([internal MR 2923](https://gitlab.khronos.org/openxr/openxr/merge_requests/2923))
- SDK
  - Add installable manual page for `openxr_runtime_list_json`.
    ([internal MR 2899](https://gitlab.khronos.org/openxr/openxr/merge_requests/2899))
  - Remove unused diagram exports from loader directory.
    ([internal MR 2907](https://gitlab.khronos.org/openxr/openxr/merge_requests/2907))
  - Update URLs in manual pages.
    ([internal MR 2935](https://gitlab.khronos.org/openxr/openxr/merge_requests/2935))
  - Validation Layer: Remove conditional `XR_KHR_headless` support as the extension
    is not part of OpenXR 1.0.
    ([internal MR 2901](https://gitlab.khronos.org/openxr/openxr/merge_requests/2901))
  - build system: Add CTest support for running the loader test.
    ([internal MR 2289](https://gitlab.khronos.org/openxr/openxr/merge_requests/2289),
    [OpenXR-SDK-Source issue 309](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/309),
    [internal issue 1733](https://gitlab.khronos.org/openxr/openxr/issues/1733))
  - hello_xr: Clean up how we specify the default graphics plugin on Android.
    ([internal MR 2917](https://gitlab.khronos.org/openxr/openxr/merge_requests/2917))
  - list_json: Add missing return statement for exit code.
    ([internal MR 2936](https://gitlab.khronos.org/openxr/openxr/merge_requests/2936))
  - loader: fix for implicit/explicit api layer loading logic
    ([OpenXR-SDK-Source PR 421](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/421),
    [internal issue 2079](https://gitlab.khronos.org/openxr/openxr/issues/2079))

## OpenXR SDK 1.0.29 (2023-08-25)

This release contains several fixes to the specification registry, improvements
to the loader, layers, and loader test, as well as enhancements to the loader
documentation/specification to support architecture and ABI specific active
runtime manifest names on Linux and Android.

- Registry
  - Change `PFNEGLGETPROCADDRESSPROC` (for `eglGetProcAddress`) to a locally
    defined type to avoid compiler errors.
    ([internal MR 2468](https://gitlab.khronos.org/openxr/openxr/merge_requests/2468))
  - Extension reservation: Register author ID and reserve vendor extensions for
    YVR.
    ([internal MR 2832](https://gitlab.khronos.org/openxr/openxr/merge_requests/2832))
  - New vendor extension: `XR_META_passthrough_preferences`
    ([internal MR 2694](https://gitlab.khronos.org/openxr/openxr/merge_requests/2694))
  - `XR_HTCX_vive_tracker_interaction`: Added new role paths for wrists and ankles.
    ([internal MR 2728](https://gitlab.khronos.org/openxr/openxr/merge_requests/2728))
- SDK
  - Changes also included in 1.0.28.1 SDK hotfix release
    - layers: Build with `/bigobj` or equivalent on Windows due to increased number
      of generated functions with spec growth.
      ([internal MR 2837](https://gitlab.khronos.org/openxr/openxr/merge_requests/2837),
      [internal issue 2051](https://gitlab.khronos.org/openxr/openxr/issues/2051),
      [OpenXR-SDK-Source PR 414](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/414))
  - Changes also included in 1.0.28.2 SDK hotfix release
    - Android AAR artifacts (loader) and hello_xr: Fix `<queries>` element contents.
      ([internal MR 2840](https://gitlab.khronos.org/openxr/openxr/merge_requests/2840),
      [internal issue 2053](https://gitlab.khronos.org/openxr/openxr/issues/2053))
    - Android AAR artifacts: Fix C++ standard library selection for Android artifacts
      in `build-aar.sh`
      ([internal MR 2836](https://gitlab.khronos.org/openxr/openxr/merge_requests/2836),
      [internal issue 2052](https://gitlab.khronos.org/openxr/openxr/issues/2052))
    - Android AAR artifacts: Use `jar` instead of 7-zip to perform archiving, and
      document requirements in `build-aar.sh`
      ([internal MR 2836](https://gitlab.khronos.org/openxr/openxr/merge_requests/2836),
      [OpenXR-SDK-Source issue 303](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/303),
      [internal issue 1711](https://gitlab.khronos.org/openxr/openxr/issues/1711))
    - build system: Support SDK hotfix versions (fourth version component).
      ([internal MR 2836](https://gitlab.khronos.org/openxr/openxr/merge_requests/2836))
  - Add XrVector2f length function to `xr_linear.h`
    ([internal MR 2876](https://gitlab.khronos.org/openxr/openxr/merge_requests/2876))
  - Add build.gradle files for list_json, c_compile_test.
    ([internal MR 2908](https://gitlab.khronos.org/openxr/openxr/merge_requests/2908))
  - Change `PFNEGLGETPROCADDRESSPROC` (for `eglGetProcAddress`) to a locally
    defined type to avoid compiler errors.
    ([internal MR 2468](https://gitlab.khronos.org/openxr/openxr/merge_requests/2468))
  - Enable `loader_test` tests which require a valid extension
    ([internal MR 2790](https://gitlab.khronos.org/openxr/openxr/merge_requests/2790))
  - Fix building hello_xr with mingw compiler.
    ([internal MR 2850](https://gitlab.khronos.org/openxr/openxr/merge_requests/2850))
  - Improvement: Reduce size of dispatch table in OpenXR loader. (Full size table
    still shipped in OpenXR-SDK even though it is not used by the loader anymore.)
    ([internal MR 2810](https://gitlab.khronos.org/openxr/openxr/merge_requests/2810),
    [internal MR 2842](https://gitlab.khronos.org/openxr/openxr/merge_requests/2842))
  - Maintenance script updates.
    ([internal MR 2900](https://gitlab.khronos.org/openxr/openxr/merge_requests/2900))
  - loader: Add support for architecture-specific active runtime manifests for
    Linux, macOS, and Android.
    ([internal MR 2450](https://gitlab.khronos.org/openxr/openxr/merge_requests/2450),
    [internal issue 2066](https://gitlab.khronos.org/openxr/openxr/issues/2066),
    [internal MR 2871](https://gitlab.khronos.org/openxr/openxr/merge_requests/2871))
  - loader: refactor to use jnipp on Android
    ([internal MR 2812](https://gitlab.khronos.org/openxr/openxr/merge_requests/2812))
  - loader: disable `loader_test` if api layer building is disabled
    ([internal MR 2843](https://gitlab.khronos.org/openxr/openxr/merge_requests/2843))
  - loader_test: Replace session test with action test to make test more
    maintainable.
    ([internal MR 2786](https://gitlab.khronos.org/openxr/openxr/merge_requests/2786))
  - validation layer: Fix deadlock when calling `XR_EXT_debug_utils` functions.
    ([internal MR 2865](https://gitlab.khronos.org/openxr/openxr/merge_requests/2865))

## OpenXR SDK 1.0.28 (2023-07-10)

This release contains improved compatibility and code quality fixes for the
loader, support for loading certain API layers on Android-based devices, and a
number of other improvements, in addition to the new extensions. Additionally,
the loader documentation now describes how OpenXR handles compatibility with
Android API levels of 30 and above: runtimes may need to update accordingly to
support this compatibility solution.

- **OpenXR SDK 1.0.28.2** hotfix release includes the following fixes
  - Loader spec: Fix description of `<queries>` element contents: existing
    description would fail to install.
    ([internal MR 2840](https://gitlab.khronos.org/openxr/openxr/merge_requests/2840),
    [internal issue 2053](https://gitlab.khronos.org/openxr/openxr/issues/2053))
  - Android AAR artifacts (loader) and hello_xr: Fix `<queries>` element contents.
    ([internal MR 2840](https://gitlab.khronos.org/openxr/openxr/merge_requests/2840),
    [internal issue 2053](https://gitlab.khronos.org/openxr/openxr/issues/2053))
  - Android AAR artifacts: Fix C++ standard library selection for Android artifacts
    in `build-aar.sh`
    ([internal MR 2836](https://gitlab.khronos.org/openxr/openxr/merge_requests/2836),
    [internal issue 2052](https://gitlab.khronos.org/openxr/openxr/issues/2052))
  - Android AAR artifacts: Use `jar` instead of 7-zip to perform archiving, and
    document requirements in `build-aar.sh`
    ([internal MR 2836](https://gitlab.khronos.org/openxr/openxr/merge_requests/2836),
    [OpenXR-SDK-Source issue 303](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/303),
    [internal issue 1711](https://gitlab.khronos.org/openxr/openxr/issues/1711))
  - build system: Support SDK hotfix versions (fourth version component).
    ([internal MR 2836](https://gitlab.khronos.org/openxr/openxr/merge_requests/2836))
- **OpenXR SDK 1.0.28.1** hotfix release includes the following fix
  - API dump layer: Fix build on Windows on ARM64.
    ([OpenXR-SDK-Source PR 414](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/414))
- Registry
  - Added defines to `xr.xml` for extension enum base and enum stride.
    ([internal MR 2693](https://gitlab.khronos.org/openxr/openxr/merge_requests/2693),
    [OpenXR-Docs issue 148](https://github.com/KhronosGroup/OpenXR-Docs/issues/148),
    [internal issue 1979](https://gitlab.khronos.org/openxr/openxr/issues/1979))
  - Extension reservation: Reserve an extension for `XR_EXT_future`
    ([internal MR 2631](https://gitlab.khronos.org/openxr/openxr/merge_requests/2631))
  - Extension reservation: Register `ANDROID` author ID and reserve 15 extensions
    for it.
    ([internal MR 2690](https://gitlab.khronos.org/openxr/openxr/merge_requests/2690))
  - Extension reservation: Reserve extensions for "user presence" and "locate
    spaces"
    ([internal MR 2705](https://gitlab.khronos.org/openxr/openxr/merge_requests/2705))
  - Extension reservation: Reserve 25 extensions for Magic Leap.
    ([internal MR 2778](https://gitlab.khronos.org/openxr/openxr/merge_requests/2778))
  - Extension reservation: Reserve extension for `XR_KHR_extendable_action_binding`
    ([internal MR 2779](https://gitlab.khronos.org/openxr/openxr/merge_requests/2779))
  - Fix spelling.
    ([internal MR 2766](https://gitlab.khronos.org/openxr/openxr/merge_requests/2766))
  - Fixed the error code specification for `xrGetControllerModelPropertiesMSFT`
    function.
    ([internal MR 2600](https://gitlab.khronos.org/openxr/openxr/merge_requests/2600))
  - New multi-vendor extension: `XR_EXT_hand_interaction`
    ([internal MR 2116](https://gitlab.khronos.org/openxr/openxr/merge_requests/2116))
  - New multi-vendor extension: `XR_EXT_plane_detection`
    ([internal MR 2510](https://gitlab.khronos.org/openxr/openxr/merge_requests/2510),
    [internal MR 2791](https://gitlab.khronos.org/openxr/openxr/merge_requests/2791))
  - New multi-vendor extension: `XR_EXT_hand_tracking_data_source`
    ([internal MR 2568](https://gitlab.khronos.org/openxr/openxr/merge_requests/2568))
  - New vendor extension: `XR_META_passthrough_color_lut`
    ([internal MR 2507](https://gitlab.khronos.org/openxr/openxr/merge_requests/2507))
  - New vendor extension: `XR_META_virtual_keyboard`
    ([internal MR 2555](https://gitlab.khronos.org/openxr/openxr/merge_requests/2555))
  - New vendor extension: `XR_OPPO_controller_interaction`
    ([OpenXR-Docs PR 146](https://github.com/KhronosGroup/OpenXR-Docs/pull/146))
  - Update Magic Leap contact
    ([internal MR 2699](https://gitlab.khronos.org/openxr/openxr/merge_requests/2699))
  - `XR_FB_face_tracking`: Non-functional registry change, fixing a problem with
    standalone headers.
    ([internal MR 2663](https://gitlab.khronos.org/openxr/openxr/merge_requests/2663))
  - `XR_FB_scene`: Introduce `XrSemanticLabelsSupportInfoFB` and bump spec version
    to 3.
    ([internal MR 2682](https://gitlab.khronos.org/openxr/openxr/merge_requests/2682))
  - `XR_FB_spatial_entity` and `XR_FB_scene`: Add `XR_ERROR_SIZE_INSUFFICIENT`
    return code to functions which use the two-call idiom.
    ([internal MR 2718](https://gitlab.khronos.org/openxr/openxr/merge_requests/2718))
  - `XR_FB_touch_controller_pro`: Fix XML to require the `touch_controller_pro`
    interaction profile for the extension
    ([internal MR 2806](https://gitlab.khronos.org/openxr/openxr/merge_requests/2806))
  - registry: Remove extraneous whitespace from some commands.
    ([OpenXR-SDK-Source PR 397](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/397))
  - schematron: Improve self tests.
    ([internal MR 2680](https://gitlab.khronos.org/openxr/openxr/merge_requests/2680))
  - schematron: Require vendor tag on interaction profile paths introduced by
    extensions.
    ([internal MR 2684](https://gitlab.khronos.org/openxr/openxr/merge_requests/2684))
  - scripts: Allow schematron to check an alternate XML file.
    ([internal MR 2670](https://gitlab.khronos.org/openxr/openxr/merge_requests/2670))
- SDK
  - Allow compilation of OpenXR SDK on Mac
    ([internal MR 2788](https://gitlab.khronos.org/openxr/openxr/merge_requests/2788),
    [internal MR 2789](https://gitlab.khronos.org/openxr/openxr/merge_requests/2789),
    [internal MR 2790](https://gitlab.khronos.org/openxr/openxr/merge_requests/2790),
    [internal MR 2800](https://gitlab.khronos.org/openxr/openxr/merge_requests/2800))
  - Common: Add `stdint.h` include to `platform_utils.hpp` for GCC 13+
    ([OpenXR-SDK-Source PR 406](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/406))
  - Describe building OpenXR SDK on macOS with Xcode
    ([internal MR 2768](https://gitlab.khronos.org/openxr/openxr/merge_requests/2768))
  - Handle clang-format-16 in `runClangFormat.sh`, and adjust source files so its
    output matches the earlier version used on CI.
    ([internal MR 2666](https://gitlab.khronos.org/openxr/openxr/merge_requests/2666),
    [internal MR 2814](https://gitlab.khronos.org/openxr/openxr/merge_requests/2814))
  - Improvement: Fix clang warning `-Wundef`.
    ([internal MR 2717](https://gitlab.khronos.org/openxr/openxr/merge_requests/2717))
  - Improvement: Fix leftover warnings when building with `-Wall`.
    ([internal MR 2754](https://gitlab.khronos.org/openxr/openxr/merge_requests/2754),
    [OpenXR-SDK-Source PR 410](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/410))
  - Loader: On Android, use a single logcat tag for all parts of the loader.
    ([internal MR 2688](https://gitlab.khronos.org/openxr/openxr/merge_requests/2688))
  - Loader: Update the required `queries` elements for an OpenXR application on
    Android, so that runtime and layer components loaded in the application process
    may access their own package in API >29.
    ([internal MR 2708](https://gitlab.khronos.org/openxr/openxr/merge_requests/2708))
  - Loader: Search system directories for API layer manifests on Android
    ([internal MR 2709](https://gitlab.khronos.org/openxr/openxr/merge_requests/2709))
  - Loader: Add Product and OEM partition to active runtime search path on Android
    ([internal MR 2709](https://gitlab.khronos.org/openxr/openxr/merge_requests/2709))
  - Loader: Improve casting to `uint32_t` edge case handling.
    ([internal MR 2745](https://gitlab.khronos.org/openxr/openxr/merge_requests/2745))
  - Loader: Clear possible dangling `next` pointers in `XR_EXT_debug_utils` label
    structures.
    ([internal MR 2764](https://gitlab.khronos.org/openxr/openxr/merge_requests/2764))
  - Validation Layer: Fix the validation_layer_generator to not check static array
    addresses.
    ([OpenXR-SDK-Source PR 399](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/399))
  - api_layers: Update API Layers spec section in README.md
    ([internal MR 2753](https://gitlab.khronos.org/openxr/openxr/merge_requests/2753))
  - cmake: Set up alias targets `OpenXR::openxr_loader` and `OpenXR::headers` so
    that the loader and headers may be used the same whether you used
    `find_package(OpenXR)` on binaries or have included the source tree as a
    subproject.
    ([internal MR 2793](https://gitlab.khronos.org/openxr/openxr/merge_requests/2793))
  - gradle: Add license for gradlew and gradlew.bat
    ([internal MR 2725](https://gitlab.khronos.org/openxr/openxr/merge_requests/2725))
  - gradle: General cleanup and updates of Android build system.
    ([internal MR 2796](https://gitlab.khronos.org/openxr/openxr/merge_requests/2796))
  - hello_xr: Enable building with latest Android Studio canary
    ([OpenXR-SDK-Source PR 393](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/393))
  - layers: Code cleanup and calling convention fixes
    ([internal MR 2784](https://gitlab.khronos.org/openxr/openxr/merge_requests/2784))
  - loader test: Fix for Windows 32-bit
    ([internal MR 2784](https://gitlab.khronos.org/openxr/openxr/merge_requests/2784))
  - loader test: Fix CMake dependencies.
    ([internal MR 2776](https://gitlab.khronos.org/openxr/openxr/merge_requests/2776))

## OpenXR SDK 1.0.27 (2023-03-21)

This release contains a large list of improvements, including interaction
profile definitions in machine-readable format in the XML, consistent tool-based
formatting of the XML, a new `list_json` tool to ease updates to
[OpenXR-Inventory][], and a wide variety of new vendor and multi-vendor
extensions, in addition to a collection of smaller improvements.

[OpenXR-Inventory]: https://github.com/KhronosGroup/OpenXR-Inventory

- Registry
  - Add interaction profile definitions to `xr.xml`
    ([internal MR 2485](https://gitlab.khronos.org/openxr/openxr/merge_requests/2485))
  - Chore: Format the full XML API registry with
    [PrettyRegistryXML](https://github.com/rpavlik/PrettyRegistryXml), making some
    small changes by hand to clean up.
    ([internal MR 2540](https://gitlab.khronos.org/openxr/openxr/merge_requests/2540),
    [internal MR 2329](https://gitlab.khronos.org/openxr/openxr/merge_requests/2329),
    [OpenXR-SDK-Source PR 373](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/373),
    [OpenXR-Docs PR 14](https://github.com/KhronosGroup/OpenXR-Docs/pull/14),
    [OpenXR-CTS PR 50](https://github.com/KhronosGroup/OpenXR-CTS/pull/50),
    [OpenXR-SDK PR 12](https://github.com/KhronosGroup/OpenXR-SDK/pull/12))
  - Document how to generate a standalone header file for an extension.
    ([internal MR 2627](https://gitlab.khronos.org/openxr/openxr/merge_requests/2627))
  - Extension reservation: Register author ID and reserve vendor extensions for
    Logitech.
    ([internal MR 2504](https://gitlab.khronos.org/openxr/openxr/merge_requests/2504))
  - Extension reservation: Reserve an extension number for a multi-vendor
    extension.
    ([internal MR 2520](https://gitlab.khronos.org/openxr/openxr/merge_requests/2520))
  - Extension reservation: Reserve an extension for `XR_EXT_hand_tracking_usage`
    ([internal MR 2550](https://gitlab.khronos.org/openxr/openxr/merge_requests/2550))
  - Extension reservation: Reserve extension id 430 for `XR_EXT_plane_detection`
    ([internal MR 2565](https://gitlab.khronos.org/openxr/openxr/merge_requests/2565))
  - Extension reservation: Reserve vendor extensions for Monado.
    ([internal MR 2613](https://gitlab.khronos.org/openxr/openxr/merge_requests/2613))
  - Extension reservation: Reserve vendor extensions for ACER.
    ([OpenXR-Docs PR 142](https://github.com/KhronosGroup/OpenXR-Docs/pull/142))
  - Extension reservation: Reserve a vendor extension for OPPO.
    ([OpenXR-Docs PR 145](https://github.com/KhronosGroup/OpenXR-Docs/pull/145))
  - New vendor extension: `XR_FB_composition_layer_depth_test`
    ([internal MR 2208](https://gitlab.khronos.org/openxr/openxr/merge_requests/2208),
    [internal issue 1657](https://gitlab.khronos.org/openxr/openxr/issues/1657))
  - New vendor extension: `XR_META_foveation_eye_tracked`
    ([internal MR 2239](https://gitlab.khronos.org/openxr/openxr/merge_requests/2239),
    [internal MR 2273](https://gitlab.khronos.org/openxr/openxr/merge_requests/2273),
    [internal MR 2332](https://gitlab.khronos.org/openxr/openxr/merge_requests/2332))
  - New vendor extension: `XR_QCOM_tracking_optimization_settings`
    ([internal MR 2261](https://gitlab.khronos.org/openxr/openxr/merge_requests/2261),
    [internal issue 1703](https://gitlab.khronos.org/openxr/openxr/issues/1703))
  - New vendor extension: `XR_META_local_dimming`
    ([internal MR 2267](https://gitlab.khronos.org/openxr/openxr/merge_requests/2267),
    [internal MR 2595](https://gitlab.khronos.org/openxr/openxr/merge_requests/2595))
  - New vendor extension: `XR_FB_spatial_entity_sharing`
    ([internal MR 2274](https://gitlab.khronos.org/openxr/openxr/merge_requests/2274))
  - New vendor extension: `XR_FB_scene_capture`
    ([internal MR 2286](https://gitlab.khronos.org/openxr/openxr/merge_requests/2286))
  - New vendor extension: `XR_FB_spatial_entity_storage_batch`
    ([internal MR 2312](https://gitlab.khronos.org/openxr/openxr/merge_requests/2312))
  - New vendor extension: `XR_FB_haptic_amplitude_envelope`
    ([internal MR 2326](https://gitlab.khronos.org/openxr/openxr/merge_requests/2326))
  - New vendor extension: `XR_FB_touch_controller_pro`
    ([internal MR 2327](https://gitlab.khronos.org/openxr/openxr/merge_requests/2327),
    [internal issue 1916](https://gitlab.khronos.org/openxr/openxr/issues/1916))
  - New vendor extension: `XR_FB_haptic_pcm`
    ([internal MR 2329](https://gitlab.khronos.org/openxr/openxr/merge_requests/2329))
  - New vendor extension: `FB_face_tracking`
    ([internal MR 2334](https://gitlab.khronos.org/openxr/openxr/merge_requests/2334),
    [internal MR 2539](https://gitlab.khronos.org/openxr/openxr/merge_requests/2539))
  - New vendor extension: `XR_FB_eye_tracking_social`
    ([internal MR 2336](https://gitlab.khronos.org/openxr/openxr/merge_requests/2336),
    [internal MR 2576](https://gitlab.khronos.org/openxr/openxr/merge_requests/2576))
  - New vendor extension: `XR_FB_body_tracking`
    ([internal MR 2339](https://gitlab.khronos.org/openxr/openxr/merge_requests/2339),
    [internal MR 2575](https://gitlab.khronos.org/openxr/openxr/merge_requests/2575))
  - New vendor extension: `XR_OCULUS_external_camera`
    ([internal MR 2397](https://gitlab.khronos.org/openxr/openxr/merge_requests/2397),
    [internal MR 2344](https://gitlab.khronos.org/openxr/openxr/merge_requests/2344))
  - New vendor extension: `XR_FB_spatial_entity_user`
    ([internal MR 2407](https://gitlab.khronos.org/openxr/openxr/merge_requests/2407))
  - New vendor extension: `XR_FB_touch_controller_proximity`
    ([internal MR 2412](https://gitlab.khronos.org/openxr/openxr/merge_requests/2412))
  - New vendor extension: `XR_ML_global_dimmer`
    ([internal MR 2461](https://gitlab.khronos.org/openxr/openxr/merge_requests/2461))
  - New vendor extension: `XR_ML_frame_end_info`
    ([internal MR 2462](https://gitlab.khronos.org/openxr/openxr/merge_requests/2462),
    [internal MR 2536](https://gitlab.khronos.org/openxr/openxr/merge_requests/2536))
  - New vendor extension: `XR_ML_compat`
    ([internal MR 2473](https://gitlab.khronos.org/openxr/openxr/merge_requests/2473))
  - New vendor extension: `XR_EXT_local_floor`
    ([internal MR 2503](https://gitlab.khronos.org/openxr/openxr/merge_requests/2503),
    [internal issue 746](https://gitlab.khronos.org/openxr/openxr/issues/746),
    [internal issue 1606](https://gitlab.khronos.org/openxr/openxr/issues/1606),
    [OpenXR-Docs issue 103](https://github.com/KhronosGroup/OpenXR-Docs/issues/103))
  - New vendor extension: `XR_BD_controller_interaction`
    ([internal MR 2527](https://gitlab.khronos.org/openxr/openxr/merge_requests/2527))
  - New vendor extension: `XR_MNDX_force_feedback_curl`
    ([OpenXR-Docs PR 136](https://github.com/KhronosGroup/OpenXR-Docs/pull/136))
  - Register author ID for Matthieu Bucchianeri.
    ([OpenXR-Docs PR 143](https://github.com/KhronosGroup/OpenXR-Docs/pull/143))
  - Rename tag name to a short one for ByteDance.
    ([internal MR 2502](https://gitlab.khronos.org/openxr/openxr/merge_requests/2502))
  - Schema: Add initial tests for Schematron rules.
    ([internal MR 2512](https://gitlab.khronos.org/openxr/openxr/merge_requests/2512))
  - Schema: Add author ID schematron checks and change duplicate name/number report
    to an assert
    ([internal MR 2514](https://gitlab.khronos.org/openxr/openxr/merge_requests/2514))
  - Schema: Fix Relax-NG checks of naming convention, and add naming convention
    checks to Schematron.
    ([internal MR 2538](https://gitlab.khronos.org/openxr/openxr/merge_requests/2538))
  - Schematron: Update extension naming rule to allow for vendor tags to be
    followed by an X for experimental and a version number
    ([internal MR 2518](https://gitlab.khronos.org/openxr/openxr/merge_requests/2518))
  - scripts: Let `deprecated` override `provisional` when choosing extension table
    of contents section.
    ([internal MR 2547](https://gitlab.khronos.org/openxr/openxr/merge_requests/2547))
  - scripts: Fix leftover exclusion of `extensions/meta` from `checkMarkup` now
    that it no longer generated files.
    ([internal MR 2560](https://gitlab.khronos.org/openxr/openxr/merge_requests/2560))
- SDK
  - Experimental Extension Naming: Allow vendor tags to be followed by an "X" for
    experimental and an optional version number (e.g. XR_EXTX2_hand_tracking).
    Update source generator vendor checks accordingly
    ([internal MR 2518](https://gitlab.khronos.org/openxr/openxr/merge_requests/2518))
  - Fix typo in API Dump generation script
    ([internal MR 2608](https://gitlab.khronos.org/openxr/openxr/merge_requests/2608))
  - Loader: Fix dynamic build on MinGW.
    ([OpenXR-SDK-Source PR 362](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/362),
    [OpenXR-SDK-Source issue 367](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/367))
  - Loader and layers: In debug builds, log when non-empty environment variables
    are being ignored due to executing with elevated privilege.
    ([OpenXR-SDK-Source PR 336](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/336))
  - Loader doc: Minor cleanups to API layer section.
    ([internal MR 2581](https://gitlab.khronos.org/openxr/openxr/merge_requests/2581))
  - Loader doc: Fix incorrect markup/dead links.
    ([internal MR 2598](https://gitlab.khronos.org/openxr/openxr/merge_requests/2598))
  - Remove third-party dependencies in `external/include/utils`.
    ([internal MR 2528](https://gitlab.khronos.org/openxr/openxr/merge_requests/2528))
  - Update all XrStructureType initialization to use standard OpenXR style.
    ([internal MR 2557](https://gitlab.khronos.org/openxr/openxr/merge_requests/2557))
  - Update URLs with branch names in manpages.
    ([internal MR 2648](https://gitlab.khronos.org/openxr/openxr/merge_requests/2648))
  - Validation layer: Fix function signature for xrNegotiateLoaderApiLayerInterface
    in core validation api layer
    ([internal MR 2607](https://gitlab.khronos.org/openxr/openxr/merge_requests/2607),
    [OpenXR-SDK-Source issue 378](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/378),
    [internal issue 1929](https://gitlab.khronos.org/openxr/openxr/issues/1929))
  - clang-format: Add clang-format-15 as acceptable clang formats
    ([OpenXR-SDK-Source PR 359](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/359))
  - doc: Add VS 2022 version code to BUILDING.md
    ([OpenXR-SDK-Source PR 381](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/381))
  - headers: Remove spurious space in preprocessor conditional, that was causing
    `defined` to be treated as an operator.
    ([internal MR 2491](https://gitlab.khronos.org/openxr/openxr/merge_requests/2491))
  - hello_xr: Correct domain in Android package identifier.
    ([internal MR 2513](https://gitlab.khronos.org/openxr/openxr/merge_requests/2513))
  - hello_xr: Update Vulkan plugin to use the newer `VK_EXT_debug_utils` extension,
    and provide names for most Vulkan objects used by the app to aid in debugging.
    (Utility code shared with CTS.)
    ([internal MR 2524](https://gitlab.khronos.org/openxr/openxr/merge_requests/2524),
    [internal MR 2579](https://gitlab.khronos.org/openxr/openxr/merge_requests/2579),
    [internal MR 2637](https://gitlab.khronos.org/openxr/openxr/merge_requests/2637))
  - hello_xr: Zero initialize XrSwapchainImage* structs
    ([internal MR 2551](https://gitlab.khronos.org/openxr/openxr/merge_requests/2551))
  - hello_xr: Export built-in NativeActivity
    ([OpenXR-SDK-Source PR 358](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/358))
  - hello_xr: Use correct lost event count
    ([OpenXR-SDK-Source PR 359](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/359))
  - loader: Prefer cstdio and cstdlib for c++ files
    ([OpenXR-SDK-Source PR 357](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/357))
  - loader,api_layers: Fix finding wayland-client.h on linux
    ([OpenXR-SDK-Source PR 346](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/346))
  - sdk-source: Add `list_json`, a small app to print json similar to the schema
    used by [OpenXR-Inventory](https://github.com/KhronosGroup/OpenXR-Inventory).
    ([internal MR 2541](https://gitlab.khronos.org/openxr/openxr/merge_requests/2541),
    [internal MR 2658](https://gitlab.khronos.org/openxr/openxr/merge_requests/2658))
  - xr_linear.h: Add some extra linear algebra functions
    ([internal MR 2532](https://gitlab.khronos.org/openxr/openxr/merge_requests/2532))

## OpenXR SDK 1.0.26 (2022-11-18)

This release contains new reflection headers, fixes and improvements to the
loader and hello_xr (especially on Android), some spec clarifications,
improvements to tooling, and a variety of new vendor and multi-vendor
extensions.

- Registry
  - Add new `XR_EXT_active_action_set_priority` vendor extension.
    ([internal MR 2288](https://gitlab.khronos.org/openxr/openxr/merge_requests/2288),
    [internal issue 1699](https://gitlab.khronos.org/openxr/openxr/issues/1699))
  - Add new `XR_HTC_passthrough` vendor extension.
    ([internal MR 2349](https://gitlab.khronos.org/openxr/openxr/merge_requests/2349))
  - Add new `XR_HTC_foveation` vendor extension.
    ([internal MR 2377](https://gitlab.khronos.org/openxr/openxr/merge_requests/2377))
  - Add a warning to `XR_COMPOSITION_LAYER_CORRECT_CHROMATIC_ABERRATION_BIT` saying
    that it is not in use and planned for deprecation
    ([internal MR 2378](https://gitlab.khronos.org/openxr/openxr/merge_requests/2378),
    [internal issue 1751](https://gitlab.khronos.org/openxr/openxr/issues/1751))
  - Add new `XR_META_headset_id` vendor extension.
    ([internal MR 2410](https://gitlab.khronos.org/openxr/openxr/merge_requests/2410))
  - Improve Schematron rules for the registry XML and update the tool version used.
    ([internal MR 2418](https://gitlab.khronos.org/openxr/openxr/merge_requests/2418),
    [internal MR 2426](https://gitlab.khronos.org/openxr/openxr/merge_requests/2426),
    [internal MR 2457](https://gitlab.khronos.org/openxr/openxr/merge_requests/2457),
    [internal MR 2460](https://gitlab.khronos.org/openxr/openxr/merge_requests/2460),
    [internal MR 2465](https://gitlab.khronos.org/openxr/openxr/merge_requests/2465))
  - Register author ID and reserve vendor extensions for ByteDance.
    ([internal MR 2482](https://gitlab.khronos.org/openxr/openxr/merge_requests/2482),
    [OpenXR-Docs PR 137](https://github.com/KhronosGroup/OpenXR-Docs/pull/137))
  - Register author ID for danwillm and reserve vendor extensions.
    ([OpenXR-Docs PR 138](https://github.com/KhronosGroup/OpenXR-Docs/pull/138))
  - Reserve vendor extensions for Microsoft.
    ([internal MR 2478](https://gitlab.khronos.org/openxr/openxr/merge_requests/2478))
  - `XR_EXTX_overlay`: Fix XML markup to correct generated valid usage for the
    event structure.
    ([internal MR 2307](https://gitlab.khronos.org/openxr/openxr/merge_requests/2307))
  - `XR_EXT_performance_settings`: Fix XML markup to correct generated valid usage,
    bump revision.
    ([internal MR 2306](https://gitlab.khronos.org/openxr/openxr/merge_requests/2306))
  - `XR_HTCX_vive_tracker_interaction`: Fix XML markup to correct generated valid
    usage for the event structure.
    ([internal MR 2310](https://gitlab.khronos.org/openxr/openxr/merge_requests/2310))
  - `XR_HTC_facial_tracking`: Update vendor extension to version 2.
    ([internal MR 2416](https://gitlab.khronos.org/openxr/openxr/merge_requests/2416))
  - specification/scripts: Added new functionality in codegen scripts to support
    creating single extension headers. Usage: `python3 scripts/genxr.py -registry
    registry/xr.xml -standaloneExtension XR_FB_color_space standalone_header`
    ([internal MR 2417](https://gitlab.khronos.org/openxr/openxr/merge_requests/2417))
- SDK
  - In-line comments added to `openxr_reflection.h`
    ([internal MR 2357](https://gitlab.khronos.org/openxr/openxr/merge_requests/2357))
  - New `openxr_reflection_structs.h` and `openxr_reflection_parent_structs.h`
    reflection headers, containing additional, limited reflection expansion macro
    definitions.
    ([internal MR 2357](https://gitlab.khronos.org/openxr/openxr/merge_requests/2357))
  - loader: Add missing `RegCloseKey` call.
    ([internal MR 2433](https://gitlab.khronos.org/openxr/openxr/merge_requests/2433))
  - loader: Report STL usage as "none" in script-built Android AAR because we
    expose no C++ symbols.
    ([OpenXR-SDK-Source PR 332](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/332),
    [internal issue 1829](https://gitlab.khronos.org/openxr/openxr/issues/1829),
    [internal issue 1831](https://gitlab.khronos.org/openxr/openxr/issues/1831))
  - loader: Minor changes to fix a missing-prototypes warning/error.
    ([OpenXR-SDK-Source PR 345](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/345))
  - hello_xr: Correctly handle the case of 0 items returned in the Vulkan plugin.
    ([internal MR 2363](https://gitlab.khronos.org/openxr/openxr/merge_requests/2363))
  - hello_xr: Android exit should use `ANativeActivity_finish`.
    ([internal MR 2409](https://gitlab.khronos.org/openxr/openxr/merge_requests/2409),
    [OpenXR-SDK-Source issue 329](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/329),
    [internal issue 1820](https://gitlab.khronos.org/openxr/openxr/issues/1820))
  - hello_xr: Simplify platform plugin for POSIX platforms.
    ([internal MR 2443](https://gitlab.khronos.org/openxr/openxr/merge_requests/2443),
    [internal MR 2436](https://gitlab.khronos.org/openxr/openxr/merge_requests/2436))
  - hello_xr: Minor tidy up of initialization code.
    ([internal MR 2449](https://gitlab.khronos.org/openxr/openxr/merge_requests/2449))
  - hello_xr: Add `android.permission.VIBRATE` permission needed by some runtimes
    for the controller haptics.
    ([internal MR 2486](https://gitlab.khronos.org/openxr/openxr/merge_requests/2486))
  - hello_xr: Bump Android Gradle Plugin usage to 7.0.4 to fix building of hello_xr
    on M1 device
    ([OpenXR-SDK-Source PR 334](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/334))
  - cmake: Use standard `CMAKE_INSTALL_INCLUDEDIR` to specify included directories.
    ([OpenXR-SDK-Source PR 347](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/347))
  - Android: Remove Gradle build files from loader directory: they were unused
    because the Android Gradle Plugin could not build our AAR file correctly as
    desired.
    ([internal MR 2453](https://gitlab.khronos.org/openxr/openxr/merge_requests/2453))
  - Android: Upgrade to gradle version 7.
    ([internal MR 2474](https://gitlab.khronos.org/openxr/openxr/merge_requests/2474))
  - Enable dependabot for GitHub Actions.
    ([OpenXR-SDK-Source PR 351](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/351),
    [OpenXR-SDK-Source PR 352](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/352),
    [OpenXR-SDK-Source PR 256](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/256))
  - Fix CI generation of NuGet packages.
    ([OpenXR-SDK-Source PR 350](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/350))
  - Improve GitHub CI for OpenXR-SDK-Source.
    ([OpenXR-SDK-Source PR 351](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/351),
    [OpenXR-SDK-Source PR 352](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/352),
    [OpenXR-SDK-Source PR 256](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/256))

## OpenXR SDK 1.0.25 (2022-09-02)

This release contains a few specification clarifications and consistency
improvements, as well as some new vendor extensions. The OpenXR loader for
Android now supports API layers packaged in the application APK, which is
important for running the conformance tests, and which may also be used for
running with the validation layer enabled during application development, for
example. The loader design doc has been updated accordingly. The spec generation
toolchain scripts have been synchronized with Vulkan. Hello_XR now models the
recommended approach for selecting an environment blend mode, among other fixes.

- Registry
  - Add new `XR_ML_ml2_controller_interaction` vendor extension.
    ([internal MR 2344](https://gitlab.khronos.org/openxr/openxr/merge_requests/2344))
  - Clarification: Note that all specialized swapchain image structures are
    "returnedonly", which removes some unneeded generated implicit valid usage.
    ([internal MR 2303](https://gitlab.khronos.org/openxr/openxr/merge_requests/2303))
  - Clarification: Note that all event structs are "returnedonly", which removes
    some unneeded generated implicit valid usage.
    ([internal MR 2305](https://gitlab.khronos.org/openxr/openxr/merge_requests/2305))
  - Register author ID for Oppo.
    ([OpenXR-Docs PR 129](https://github.com/KhronosGroup/OpenXR-Docs/pull/129))
  - Register author ID for Fred Emmott.
    ([OpenXR-Docs PR 131](https://github.com/KhronosGroup/OpenXR-Docs/pull/131))
  - Register author ID for Acer.
    ([OpenXR-Docs PR 132](https://github.com/KhronosGroup/OpenXR-Docs/pull/132))
  - Reserve extension numbers for anticipated cross-vendor and Khronos extensions.
    ([internal MR 2337](https://gitlab.khronos.org/openxr/openxr/merge_requests/2337),
    [internal MR 2338](https://gitlab.khronos.org/openxr/openxr/merge_requests/2338),
    [internal MR 2389](https://gitlab.khronos.org/openxr/openxr/merge_requests/2389))
  - Reserve a vendor extension for Huawei.
    ([internal MR 2356](https://gitlab.khronos.org/openxr/openxr/merge_requests/2356))
  - Reserve vendor extensions for MNDX.
    ([OpenXR-Docs PR 133](https://github.com/KhronosGroup/OpenXR-Docs/pull/133))
  - Update `XR_MSFT_scene_understanding` and
    `XR_MSFT_scene_understanding_serialization` vendor extensions to list error
    codes that may be returned by functions.
    ([internal MR 2316](https://gitlab.khronos.org/openxr/openxr/merge_requests/2316))
  - `XR_FB_color_space`: Mark `XrSystemColorSpacePropertiesFB` as "returned-only"
    for consistency and to correct the implicit valid usage.
    ([internal MR 2304](https://gitlab.khronos.org/openxr/openxr/merge_requests/2304))
  - `XR_FB_display_refresh_rate`: Mark `XrEventDataDisplayRefreshRateChangedFB` as
    "returned only" for consistency.
    ([internal MR 2308](https://gitlab.khronos.org/openxr/openxr/merge_requests/2308))
  - `XR_FB_hand_tracking_mesh`: Fix two-call-idiom markup for
    `XrHandTrackingMeshFB`, affecting implicit valid usage, and increment the
    revision.
    ([internal MR 2311](https://gitlab.khronos.org/openxr/openxr/merge_requests/2311))
  - `XR_FB_passthrough`: Add `XrSystemPassthroughProperties2FB` and
    `XR_PASSTHROUGH_LAYER_DEPTH_BIT_FB`, update spec version to 3.
    ([internal MR 2333](https://gitlab.khronos.org/openxr/openxr/merge_requests/2333))
  - `XR_FB_render_model`: Mark `XrRenderModelCapabilitiesRequestFB` as "returned-
    only" for consistency and to correct the implicit valid usage.
    ([internal MR 2309](https://gitlab.khronos.org/openxr/openxr/merge_requests/2309))
- SDK
  - Loader design doc: Correct a wrong description of extension implementation
    chosen by the loader when duplicates.
    ([internal MR 2324](https://gitlab.khronos.org/openxr/openxr/merge_requests/2324),
    [internal issue 1731](https://gitlab.khronos.org/openxr/openxr/issues/1731))
  - hello_xr: Model the recommended behavior of choosing first blend mode
    enumerated by xrEnumerateEnvironmentBlendModes that is supported by the app.
    ([internal MR 2352](https://gitlab.khronos.org/openxr/openxr/merge_requests/2352))
  - hello_xr: Fix exit on Android.
    ([internal MR 2403](https://gitlab.khronos.org/openxr/openxr/merge_requests/2403))
  - loader: Add Android support for API Layers bundled in the application APK.
    ([internal MR 2350](https://gitlab.khronos.org/openxr/openxr/merge_requests/2350))
  - loader: Move validation checks before initialization to avoid potential nullptr
    dereference
    ([internal MR 2365](https://gitlab.khronos.org/openxr/openxr/merge_requests/2365))
  - loader: On Android, make sure we always build with the same C++ standard
    library (static) whether using shell script or gradle.
    ([internal MR 2366](https://gitlab.khronos.org/openxr/openxr/merge_requests/2366))
  - loader: add -DXR_OS_APPLE define on macOS (fixes compilation on macOS)
    ([OpenXR-SDK-Source PR 323](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/323))
  - scripts: Synchronize scripts with Vulkan, and move all generated files into a
    single target directory.
    ([internal MR 2335](https://gitlab.khronos.org/openxr/openxr/merge_requests/2335),
    [internal issue 1693](https://gitlab.khronos.org/openxr/openxr/issues/1693),
    [internal MR 2393](https://gitlab.khronos.org/openxr/openxr/merge_requests/2393),
    [internal MR 2400](https://gitlab.khronos.org/openxr/openxr/merge_requests/2400))
  - scripts: Remove spurious warning from codegen script.
    ([internal MR 2341](https://gitlab.khronos.org/openxr/openxr/merge_requests/2341))
  - validation layer: Fix output to `XR_EXT_debug_utils` when no labels/names have
    been defined.
    ([internal MR 2375](https://gitlab.khronos.org/openxr/openxr/merge_requests/2375))

## OpenXR SDK 1.0.24 (2022-06-23)

- Registry
  - Add new `XR_EXT_palm_pose` multi-vendor extension.
    ([internal MR 2112](https://gitlab.khronos.org/openxr/openxr/merge_requests/2112))
  - Add new `XR_FB_scene` vendor extension.
    ([internal MR 2237](https://gitlab.khronos.org/openxr/openxr/merge_requests/2237))
  - Fix structure definition in `XR_FB_spatial_entity_container`.
    ([internal MR 2278](https://gitlab.khronos.org/openxr/openxr/merge_requests/2278))
  - scripts: Teach xr_conventions that 2D, 3D, etc. are words for the purposes of
    structure type enum generation.
    ([internal MR 2237](https://gitlab.khronos.org/openxr/openxr/merge_requests/2237))
- SDK
  - Loader: Fix filename and native lib dir sequence for log
    ([OpenXR-SDK-Source PR 311](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/311))
  - Loader: Fix loader building with Gradle and add CI checking for loader building
    with Gradle
    ([OpenXR-SDK-Source PR 312](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/312))
  - hello_xr: Pick background clear color based on the selected environment blend
    mode.
    ([internal MR 2275](https://gitlab.khronos.org/openxr/openxr/merge_requests/2275))
  - hello_xr: Defer Vulkan CPU sync until the next frame begins.
    ([OpenXR-SDK-Source PR 277](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/277))
  - hello_xr: Fix shader compile on Mali driver
    ([OpenXR-SDK-Source PR 310](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/310))
  - scripts: Delegate generating structure types to the conventions object as done
    elsewhere in the repo.
    ([internal MR 2237](https://gitlab.khronos.org/openxr/openxr/merge_requests/2237))

## OpenXR SDK 1.0.23 (2022-05-27)

This release primarily features a large number of new vendor and multi-vendor
extensions, as well as some updates to existing extensions. Some improvements
and fixes were made in SDK as well.

- Registry
  - Add new `XR_ULTRALEAP_hand_tracking_forearm` vendor extension.
    ([internal MR 2154](https://gitlab.khronos.org/openxr/openxr/merge_requests/2154))
  - Add new `XR_EXT_dpad_binding` multi-vendor extension.
    ([internal MR 2159](https://gitlab.khronos.org/openxr/openxr/merge_requests/2159))
  - Add "externally synchronized" markup for `xrBeginFrame` and `xrEndFrame` so
    they get the matching box and their session parameters are included in the list
    of externally-synchronized parameters in the "Threading" section.
    ([internal MR 2179](https://gitlab.khronos.org/openxr/openxr/merge_requests/2179),
    [OpenXR-Docs issue 23](https://github.com/KhronosGroup/OpenXR-Docs/issues/23),
    [internal issue 1216](https://gitlab.khronos.org/openxr/openxr/issues/1216))
  - Add new `XR_FB_spatial_entity` vendor extension.
    ([internal MR 2194](https://gitlab.khronos.org/openxr/openxr/merge_requests/2194))
  - Add new `XR_FB_spatial_entity_storage` vendor extension.
    ([internal MR 2194](https://gitlab.khronos.org/openxr/openxr/merge_requests/2194))
  - Add new `XR_FB_spatial_entity_query` vendor extension.
    ([internal MR 2194](https://gitlab.khronos.org/openxr/openxr/merge_requests/2194))
  - Add new `XR_FB_composition_layer_settings` vendor extension.
    ([internal MR 2221](https://gitlab.khronos.org/openxr/openxr/merge_requests/2221))
  - Add new `XR_FB_spatial_entity_container` vendor extension.
    ([internal MR 2236](https://gitlab.khronos.org/openxr/openxr/merge_requests/2236))
  - Add new `XR_HTC_vive_wrist_tracker_interaction` vendor extension.
    ([internal MR 2252](https://gitlab.khronos.org/openxr/openxr/merge_requests/2252))
  - Add XR_HTC_hand_interaction extension.
    ([internal MR 2254](https://gitlab.khronos.org/openxr/openxr/merge_requests/2254))
  - Add new `XR_VARJO_view_offset` vendor extension.
    ([internal MR 2255](https://gitlab.khronos.org/openxr/openxr/merge_requests/2255))
  - Add new `XR_META_performance_metrics` vendor extension.
    ([internal MR 2256](https://gitlab.khronos.org/openxr/openxr/merge_requests/2256))
  - Add new `XR_META_vulkan_swapchain_create_info` vendor extension.
    ([internal MR 2257](https://gitlab.khronos.org/openxr/openxr/merge_requests/2257))
  - Change the XML type of `XR_MIN_COMPOSITION_LAYERS_SUPPORTED` so it outputs an
    includable snippet for the spec text.
    ([internal MR 2201](https://gitlab.khronos.org/openxr/openxr/merge_requests/2201),
    [internal issue 1652](https://gitlab.khronos.org/openxr/openxr/issues/1652),
    [OpenXR-Docs issue 117](https://github.com/KhronosGroup/OpenXR-Docs/issues/117))
  - Fix registry consistency script and codegen scripts to allow extension of KHR
    and EXT enumerations with vendor-specific members.
    ([internal MR 2213](https://gitlab.khronos.org/openxr/openxr/merge_requests/2213),
    [internal MR 2243](https://gitlab.khronos.org/openxr/openxr/merge_requests/2243))
  - Fix warning print statement arguments in header generation/validation script.
    ([internal MR 2244](https://gitlab.khronos.org/openxr/openxr/merge_requests/2244))
  - Reserve the extension number for multi-vendor hand interaction profile
    extension.
    ([internal MR 2206](https://gitlab.khronos.org/openxr/openxr/merge_requests/2206))
  - Reserve vendor extensions 304-317 for Qualcomm
    ([internal MR 2258](https://gitlab.khronos.org/openxr/openxr/merge_requests/2258))
  - Reserve vendor extensions 318-370 for HTC.
    ([internal MR 2266](https://gitlab.khronos.org/openxr/openxr/merge_requests/2266))
  - `KHR_composition_layer_depth`: Update spec version to 6 for updated spec text.
    ([internal MR 2207](https://gitlab.khronos.org/openxr/openxr/merge_requests/2207),
    [internal issue 1651](https://gitlab.khronos.org/openxr/openxr/issues/1651))
  - `XR_EXT_eye_gaze_interaction`: Update the spec version for spec text change.
    ([internal MR 2227](https://gitlab.khronos.org/openxr/openxr/merge_requests/2227))
  - `XR_EXT_uuid`: Add enum tags to `XR_UUID_SIZE_EXT` to ensure it is defined
    before `XrUuidEXT` in generated header
    ([internal MR 2234](https://gitlab.khronos.org/openxr/openxr/merge_requests/2234),
    [internal issue 1673](https://gitlab.khronos.org/openxr/openxr/issues/1673))
  - `XR_FB_hand_aim_tracking`, `XR_FB_hand_tracking_capsule`,
    `XR_FB_hand_tracking_mesh`: Fix documentation to specify correct `next` chain
    usage.
    ([internal MR 2229](https://gitlab.khronos.org/openxr/openxr/merge_requests/2229))
  - `XR_FB_hand_tracking_capsules`: Update `XrHandCapsuleFB` and
    `XrHandTrackingCapsulesStateFB` to use
    `XR_HAND_TRACKING_CAPSULE_POINT_COUNT_FB` and
    `XR_HAND_TRACKING_CAPSULE_COUNT_FB` enums when defining arrays so they match
    the usual practice for vendor extensions
    ([internal MR 2216](https://gitlab.khronos.org/openxr/openxr/merge_requests/2216))
  - `XR_FB_passthrough_keyboard_hands`: Add
    `XR_PASSTHROUGH_LAYER_PURPOSE_TRACKED_KEYBOARD_MASKED_HANDS_FB`, update spec
    version to 2.
    ([internal MR 2270](https://gitlab.khronos.org/openxr/openxr/merge_requests/2270))
  - `XR_FB_passthrough`: add `XrPassthroughBrightnessContrastSaturationFB`, update
    spec version to 2
    ([internal MR 2222](https://gitlab.khronos.org/openxr/openxr/merge_requests/2222))
  - `XR_FB_render_model`: Add capability support levels, bump spec version to 2.
    ([internal MR 2264](https://gitlab.khronos.org/openxr/openxr/merge_requests/2264))
  - `XR_FB_space_warp`: Add
    `XR_COMPOSITION_LAYER_SPACE_WARP_INFO_FRAME_SKIP_BIT_FB` into
    `XrCompositionLayerSpaceWarpInfoFlagBitsFB`, update spec version to 2.
    ([internal MR 2193](https://gitlab.khronos.org/openxr/openxr/merge_requests/2193))
  - `XR_HTC_vive_focus3_controller_interaction`: Support component path
    "/input/squeeze/value", update spec version to 2.
    ([internal MR 2253](https://gitlab.khronos.org/openxr/openxr/merge_requests/2253))
  - `XR_KHR_D3D11_enable` and `XR_KHR_D3D12_enable`: Update to describe error
    conditions for `XR_ERROR_GRAPHICS_DEVICE_INVALID`.
    ([internal MR 2176](https://gitlab.khronos.org/openxr/openxr/merge_requests/2176),
    [internal issue 1617](https://gitlab.khronos.org/openxr/openxr/issues/1617))
  - `XR_MSFT_spatial_graph_bridge`: Update to revision 2.
    ([internal MR 2182](https://gitlab.khronos.org/openxr/openxr/merge_requests/2182))
- SDK
  - Add `org.khronos.openxr.intent.category.IMMERSIVE_HMD` category to intent-
    filter for `AndroidManifest.xml`, to indicate immersive application
    ([internal MR 2219](https://gitlab.khronos.org/openxr/openxr/merge_requests/2219))
  - Common: Fix definitions in `xr_linear.h` so that it can be compiled as C or
    C++.
    ([internal MR 2217](https://gitlab.khronos.org/openxr/openxr/merge_requests/2217))
  - Fix warnings raised by Clang on various platforms.
    ([internal MR 2197](https://gitlab.khronos.org/openxr/openxr/merge_requests/2197))
  - Fix source-generation script and codegen scripts to allow extension of KHR and
    EXT enumerations with vendor-specific members.
    ([internal MR 2240](https://gitlab.khronos.org/openxr/openxr/merge_requests/2240),
    [internal MR 2243](https://gitlab.khronos.org/openxr/openxr/merge_requests/2243))
  - Fix warning print statement arguments in header generation/validation script.
    ([internal MR 2244](https://gitlab.khronos.org/openxr/openxr/merge_requests/2244))
  - Loader: Adjust Android loader build to use the static C++ runtime, since we do
    not expose any C++ interfaces.
    ([OpenXR-SDK-Source PR 307](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/307),
    [internal issue 1712](https://gitlab.khronos.org/openxr/openxr/issues/1712))
  - Remove "Draft" status accidentally left on the loader design doc/spec.
    ([OpenXR-SDK-Source PR 300](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/300),
    [internal issue 1688](https://gitlab.khronos.org/openxr/openxr/issues/1688))
  - Validation Layer: Functions that start with `xrTryCreate` will receive the same
    warnings as functions that start with`xrCreate`.
    ([internal MR 2182](https://gitlab.khronos.org/openxr/openxr/merge_requests/2182))
  - cmake: Install pkgconfig file in mingw
    ([OpenXR-SDK-Source PR 308](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/308))
  - hello_xr: Shutdown OpenGL graphics to allow it to be restarted
    ([internal MR 2241](https://gitlab.khronos.org/openxr/openxr/merge_requests/2241))
  - hello_xr: remove call to swapbuffers to fix OpenGL frame timing.
    ([internal MR 2249](https://gitlab.khronos.org/openxr/openxr/merge_requests/2249))
  - hello_xr: Fix typo in declspec keyword
    ([OpenXR-SDK-Source PR 302](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/302),
    [internal issue 1691](https://gitlab.khronos.org/openxr/openxr/issues/1691))

## OpenXR SDK 1.0.22 (2022-01-12)

This release features a number of new extensions, as well as some software
updates and fixes, especially for Android. If you are using the bundled jsoncpp,
this is also a security release as the bundled jsoncpp was upgraded to
incorporate security improvements from upstream.

- Registry
  - Add new `XR_FB_render_model` vendor extension.
    ([internal MR 2117](https://gitlab.khronos.org/openxr/openxr/merge_requests/2117),
    [internal MR 2169](https://gitlab.khronos.org/openxr/openxr/merge_requests/2169))
  - Add new `XR_HTC_facial_expression` vendor extension.
    ([internal MR 2120](https://gitlab.khronos.org/openxr/openxr/merge_requests/2120))
  - Add new `XR_FB_keyboard_tracking` vendor extension.
    ([internal MR 2128](https://gitlab.khronos.org/openxr/openxr/merge_requests/2128))
  - Add new `XR_EXT_uuid` multi-vendor extension.
    ([internal MR 2152](https://gitlab.khronos.org/openxr/openxr/merge_requests/2152))
  - Add new `XR_FB_passthrough_keyboard_hands` vendor extension.
    ([internal MR 2162](https://gitlab.khronos.org/openxr/openxr/merge_requests/2162))
  - Add new `XR_HTC_vive_focus3_controller_interaction` vendor extension.
    ([internal MR 2178](https://gitlab.khronos.org/openxr/openxr/merge_requests/2178))
  - Add new `XR_ALMALENCE_digital_lens_control` vendor extension.
    ([OpenXR-Docs PR 104](https://github.com/KhronosGroup/OpenXR-Docs/pull/104),
    [internal issue 1615](https://gitlab.khronos.org/openxr/openxr/issues/1615))
  - Correct winding order for `XR_MSFT_hand_tracking_mesh` extension to clockwise
    to match runtime behavior.
    ([internal MR 2151](https://gitlab.khronos.org/openxr/openxr/merge_requests/2151))
  - Fix typos/naming convention errors in `XR_FB_hand_tracking_capsules`: rename
    `XR_FB_HAND_TRACKING_CAPSULE_POINT_COUNT` to
    `XR_HAND_TRACKING_CAPSULE_POINT_COUNT_FB` and
    `XR_FB_HAND_TRACKING_CAPSULE_COUNT` to `XR_HAND_TRACKING_CAPSULE_COUNT_FB`,
    providing the old names as compatibility aliases.
    ([internal MR 1547](https://gitlab.khronos.org/openxr/openxr/merge_requests/1547),
    [internal issue 1519](https://gitlab.khronos.org/openxr/openxr/issues/1519))
  - Reserve vendor extensions 208 - 299 for Facebook.
    ([internal MR 2158](https://gitlab.khronos.org/openxr/openxr/merge_requests/2158))
  - Reserve extension numbers for anticipated multi-vendor extensions.
    ([internal MR 2173](https://gitlab.khronos.org/openxr/openxr/merge_requests/2173))
- SDK
  - Android loader: Update vendored jnipp project, including crash/exception fixes
    if an application manually attached or detached a thread.
    ([OpenXR-SDK-Source PR 286](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/286),
    [OpenXR-SDK-Source PR 285](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/285))
  - Docs: Fixed typo in docs.
    ([OpenXR-SDK-Source PR 284](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/284))
  - Fix detection of std::filesystem options on GCC 11 and newer.
    ([OpenXR-SDK-Source PR 276](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/276),
    [OpenXR-SDK-Source issue 260](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/260),
    [internal issue 1571](https://gitlab.khronos.org/openxr/openxr/issues/1571))
  - Loader: Add `ifdef` guards around contents of Android-specific files so all
    platforms may still glob all source files in OpenXR-SDK to build the loader
    with a custom build system.
    ([OpenXR-SDK-Source PR 274](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/274))
  - Loader: Fixed incorrect return value when no broker is present on Android but
    runtime defined via `active_runtime.json`.
    ([OpenXR-SDK-Source PR 284](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/284))
  - Loader: Added `/system` to the search path on Android as per documentation.
    ([OpenXR-SDK-Source PR 284](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/284))
  - Loader doc: Use `nativeLibraryDir` (property, part of API) instead of
    `getNativeLibraryDir()` (function generated by wrapping library)
    ([OpenXR-SDK-Source PR 278](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/278))
  - Update vendored copy of jsoncpp from 1.8.4 to 1.9.5 for security and other
    fixes.
    ([internal MR 2168](https://gitlab.khronos.org/openxr/openxr/merge_requests/2168),
    [OpenXR-SDK-Source issue 265](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/265),
    [internal issue 1582](https://gitlab.khronos.org/openxr/openxr/issues/1582))
  - Update android-jni-wrappers to fix missing include.
    ([OpenXR-SDK-Source PR 280](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/280),
    [OpenXR-SDK-Source issue 275](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/275),
    [internal issue 1616](https://gitlab.khronos.org/openxr/openxr/issues/1616))
  - Update jnipp to fix crash on Android if app detaches thread from JVM (e.g. on
    shutdown).
    ([OpenXR-SDK-Source PR 280](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/280))
  - scripts: Populate `ext_name` in `HandleData` too, for use by language wrapper
    generation scripts.
    ([internal MR 2184](https://gitlab.khronos.org/openxr/openxr/merge_requests/2184))

## OpenXR SDK 1.0.21 (2022-01-10)

This release was withdrawn due to a typo noticed after initial publication.
All changes are now listed under 1.0.22.

## OpenXR SDK 1.0.20 (2021-10-04)

This release includes a proposed cross-vendor OpenXR loader for Android, Android
build system for hello_xr, and a number of new vendor extensions.

- Registry
  - Add new `XR_HTCX_vive_tracker_interaction` provisional vendor extension.
    ([internal MR 1983](https://gitlab.khronos.org/openxr/openxr/merge_requests/1983))
  - Add new `XR_VARJO_marker_tracking` vendor extension.
    ([internal MR 2129](https://gitlab.khronos.org/openxr/openxr/merge_requests/2129))
  - Add new `XR_FB_triangle_mesh` vendor extension.
    ([internal MR 2130](https://gitlab.khronos.org/openxr/openxr/merge_requests/2130))
  - Add new `XR_FB_passthrough` vendor extension.
    ([internal MR 2130](https://gitlab.khronos.org/openxr/openxr/merge_requests/2130))
  - Reserve vendor extensions for Facebook.
    ([internal MR 2131](https://gitlab.khronos.org/openxr/openxr/merge_requests/2131))
  - Reserve a vendor extension for Almalence.
    ([OpenXR-Docs PR 99](https://github.com/KhronosGroup/OpenXR-Docs/pull/99))
  - XR_FB_color_space: Fix XML markup to indicate that
    `XrSystemColorSpacePropertiesFB` is chained to `XrSystemProperties`.
    ([internal MR 2143](https://gitlab.khronos.org/openxr/openxr/merge_requests/2143))
- SDK
  - Loader specification: Describe a cross-vendor loader for use on Android.
    ([internal MR 1949](https://gitlab.khronos.org/openxr/openxr/merge_requests/1949),
    [internal issue 1425](https://gitlab.khronos.org/openxr/openxr/issues/1425))
  - hello_xr: Add Android build system, using new cross-vendor loader, and make
    some improvements/fixes.
    ([internal MR 1949](https://gitlab.khronos.org/openxr/openxr/merge_requests/1949),
    [internal issue 1425](https://gitlab.khronos.org/openxr/openxr/issues/1425))
  - loader: Implement cross-vendor loader for Android, with AAR Prefab packaging.
    ([internal MR 1949](https://gitlab.khronos.org/openxr/openxr/merge_requests/1949),
    [internal issue 1425](https://gitlab.khronos.org/openxr/openxr/issues/1425))

## OpenXR SDK 1.0.19 (2021-08-24)

This release features a number of new or updated vendor extensions, as well as
some minor cleanups and bug fixes in the SDK.

- Registry
  - Add `XR_SESSION_NOT_FOCUSED` as a possible success return code to
    `xrApplyHapticFeedback` and `xrStopHapticFeedback`.
    ([internal MR 2106](https://gitlab.khronos.org/openxr/openxr/merge_requests/2106),
    [internal issue 1270](https://gitlab.khronos.org/openxr/openxr/issues/1270))
  - Add new `XR_FB_hand_tracking_mesh` vendor extension.
    ([internal MR 2089](https://gitlab.khronos.org/openxr/openxr/merge_requests/2089))
  - Add new `XR_FB_hand_tracking_capsules` vendor extension.
    ([internal MR 2089](https://gitlab.khronos.org/openxr/openxr/merge_requests/2089))
  - Add new `XR_FB_hand_tracking_aim` vendor extension.
    ([internal MR 2089](https://gitlab.khronos.org/openxr/openxr/merge_requests/2089))
  - Add version 1 of new `XR_FB_space_warp` vendor extension.
    ([internal MR 2115](https://gitlab.khronos.org/openxr/openxr/merge_requests/2115))
  - Register new Author ID for Almalence.
    ([OpenXR-Docs PR 92](https://github.com/KhronosGroup/OpenXR-Docs/pull/92),
    [OpenXR-Docs PR 93](https://github.com/KhronosGroup/OpenXR-Docs/pull/93))
  - Update to version 2 of `XR_VALVE_analog_threshold`.
    ([internal MR 2113](https://gitlab.khronos.org/openxr/openxr/merge_requests/2113))
- SDK
  - scripts: Some typing annotations and type-related cleanup found by using type-
    aware Python editors.
    ([internal MR 2100](https://gitlab.khronos.org/openxr/openxr/merge_requests/2100))
  - `xr_linear.h`: Fix bug in `XrVector3f_Cross`
    ([internal MR 2111](https://gitlab.khronos.org/openxr/openxr/merge_requests/2111))

## OpenXR SDK 1.0.18 (2021-07-30)

This release mostly adds new extensions. It also includes some fixes to the
included layers, as well as text in the loader documentation describing how
runtimes can register themselves for manual selection. This is not used by the
loader itself and does not require any changes to the loader, but it may be
useful to developer-focused supporting software.

- Registry
  - Add ratified `XR_KHR_swapchain_usage_input_attachment_bit` Khronos extension.
    (Promotion of `XR_MND_swapchain_usage_input_attachment_bit`, which is now
    deprecated.)
    ([internal MR 2045](https://gitlab.khronos.org/openxr/openxr/merge_requests/2045))
  - Add new `XR_FB_foveation`, `XR_FB_foveation_configuration`, and
    `XR_FB_foveation_vulkan` vendor extensions.
    ([internal MR 2050](https://gitlab.khronos.org/openxr/openxr/merge_requests/2050))
  - Add additional extension dependencies to `XR_FB_swapchain_update_state`.
    ([internal MR 2072](https://gitlab.khronos.org/openxr/openxr/merge_requests/2072),
    [internal issue 1572](https://gitlab.khronos.org/openxr/openxr/issues/1572))
  - Add new `XR_FB_composition_layer_secure_content` vendor extension.
    ([internal MR 2075](https://gitlab.khronos.org/openxr/openxr/merge_requests/2075))
  - Add new `XR_FB_composition_layer_alpha_blend` vendor extension.
    ([internal MR 2078](https://gitlab.khronos.org/openxr/openxr/merge_requests/2078))
  - Add new `XR_FB_composition_layer_image_layout` vendor extension.
    ([internal MR 2090](https://gitlab.khronos.org/openxr/openxr/merge_requests/2090))
  - Add new `XR_MSFT_spatial_anchor_persistence` vendor extension.
    ([internal MR 2093](https://gitlab.khronos.org/openxr/openxr/merge_requests/2093))
  - Add some simple [Schematron](https://schematron.com) rules and a script to
    check the XML registry against them.
    ([internal MR 2103](https://gitlab.khronos.org/openxr/openxr/merge_requests/2103))
  - Register author ID and Reserve vendor extensions for Unity.
    ([internal MR 2105](https://gitlab.khronos.org/openxr/openxr/merge_requests/2105))
  - Reserve extension ID range 187-196 for LIV Inc.
    ([internal MR 2102](https://gitlab.khronos.org/openxr/openxr/merge_requests/2102))
- SDK
  - Describe how runtimes may register themselves at installation time for manual
    selection.
    ([internal MR 2081](https://gitlab.khronos.org/openxr/openxr/merge_requests/2081),
    [internal MR 2109](https://gitlab.khronos.org/openxr/openxr/merge_requests/2109),
    [internal issue 1574](https://gitlab.khronos.org/openxr/openxr/issues/1574))
  - Include sRGB in list of supported swapchain texture formats for the HelloXR
    OpenGLES plugin.
    ([internal MR 2066](https://gitlab.khronos.org/openxr/openxr/merge_requests/2066))
  - layers: Refactor generated `xrGetInstanceProcAddr` implementations to avoid
    deeply-nested `if ... else` blocks. (Some compilers have limits we were nearing
    or hitting.)
    ([internal MR 2050](https://gitlab.khronos.org/openxr/openxr/merge_requests/2050))
  - validation layer: Set default logging mode to stdout ("text") instead of none.
    ([OpenXR-SDK-Source PR 262](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/262))
  - validation layer: Fix invalid struct type error message to show the expected
    type instead of the actual type.
    ([OpenXR-SDK-Source PR 263](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/263))

## OpenXR SDK 1.0.17 (2021-06-08)

This release features an important fix to the loader for an invalid-iterator bug
introduced in 1.0.16. All developers shipping the loader are strongly encouraged
to upgrade. It also includes a variety of new vendor extensions.

- Registry
  - Add `XR_MSFT_scene_understanding` vendor extension.
    ([internal MR 2032](https://gitlab.khronos.org/openxr/openxr/merge_requests/2032))
  - Add `XR_MSFT_scene_understanding_serialization` vendor extension.
    ([internal MR 2032](https://gitlab.khronos.org/openxr/openxr/merge_requests/2032))
  - Add `XR_MSFT_composition_layer_reprojection` vendor extension.
    ([internal MR 2033](https://gitlab.khronos.org/openxr/openxr/merge_requests/2033))
  - Add `XR_OCULUS_audio_device_guid` vendor extension.
    ([internal MR 2053](https://gitlab.khronos.org/openxr/openxr/merge_requests/2053))
  - Add version 3 of `XR_FB_swapchain_update_state` vendor extension, which splits
    platform and graphics API specific structs into separate extensions.
    ([internal MR 2059](https://gitlab.khronos.org/openxr/openxr/merge_requests/2059))
  - Apply formatting to registry XML by selectively committing changes made by
    <https://github.com/rpavlik/PrettyRegistryXml>.
    ([internal MR 2070](https://gitlab.khronos.org/openxr/openxr/merge_requests/2070),
    [OpenXR-SDK-Source/#256](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/256))
  - Enforce that all `xrCreate` functions must be able to return
    `XR_ERROR_LIMIT_REACHED` and `XR_ERROR_OUT_OF_MEMORY`, and adjust lists of
    error codes accordingly.
    ([internal MR 2064](https://gitlab.khronos.org/openxr/openxr/merge_requests/2064))
  - Fix a usage of `>` without escaping as an XML entity.
    ([internal MR 2064](https://gitlab.khronos.org/openxr/openxr/merge_requests/2064))
  - Fix all cases of a success code (most often `XR_SESSION_LOSS_PENDING`)
    appearing in the `errorcodes` attribute of a command.
    ([internal MR 2064](https://gitlab.khronos.org/openxr/openxr/merge_requests/2064),
    [internal issue 1566](https://gitlab.khronos.org/openxr/openxr/issues/1566))
  - Improve comments for several enum values.
    ([internal MR 1982](https://gitlab.khronos.org/openxr/openxr/merge_requests/1982))
  - Perform some script clean-up and refactoring, including selective type
    annotation and moving the Conventions abstract base class to `spec_tools`.
    ([internal MR 2064](https://gitlab.khronos.org/openxr/openxr/merge_requests/2064))
  - Sort return codes, with some general, popular codes made to be early. Script
    `sort_codes.py` can be used to maintain this, though it mangles other XML
    formatting, so use it with care. <https://github.com/rpavlik/PrettyRegistryXml>
    can format, and eventually sort return codes (currently sort order does not
    match).
    ([internal MR 2064](https://gitlab.khronos.org/openxr/openxr/merge_requests/2064),
    [OpenXR-SDK-Source/#256](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/256))
- SDK
  - Loader: Fix iteration over explicit layer manifests.
    ([OpenXR-SDK-Source/#256](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/256))
  - validation layer: Don't try to apply `strlen` to `wchar_t`-based output
    buffers.
    ([internal MR 2053](https://gitlab.khronos.org/openxr/openxr/merge_requests/2053))

## OpenXR SDK 1.0.16 (2021-05-11)

This release contains an update to define a new error code,
`XR_ERROR_RUNTIME_UNAVAILABLE`, now returned by the loader at `xrCreateInstance`
and `xrEnumerateInstanceProperties` when it cannot find or load a runtime for
some reason. This should be more clear for developers when encountering it, as
well as helpful when troubleshooting errors hit by users. (The
previously-returned error was typically `XR_ERROR_INSTANCE_LOST`, which is
confusing when returned when trying to create an instance.) This release also
includes a new multi-vendor extension, a new vendor extension, and improved
concurrency handling in the loader, among smaller fixes.

- Registry
  - Add new `XR_ERROR_RUNTIME_UNAVAILABLE` error code, add
    `XR_ERROR_RUNTIME_UNAVAILABLE` as a supported error code to `xrCreateInstance`
    and `xrEnumerateInstanceProperties`, and remove `XR_ERROR_INSTANCE_LOST` as a
    supported error code from `xrCreateInstance`.
    ([internal MR 2024](https://gitlab.khronos.org/openxr/openxr/merge_requests/2024),
    [internal issue 1552](https://gitlab.khronos.org/openxr/openxr/issues/1552),
    [OpenXR-SDK-Source/#177](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/177))
  - Add `XR_EXT_hand_joint_motion_range` multi-vendor extension.
    ([internal MR 1995](https://gitlab.khronos.org/openxr/openxr/merge_requests/1995))
  - Add `XR_FB_swapchain_update_state` vendor extension.
    ([internal MR 1997](https://gitlab.khronos.org/openxr/openxr/merge_requests/1997))
  - Fix missing `XR_ERROR_INSTANCE_LOST` return codes for extension functions in
    `XR_EXT_performance_settings`, `XR_EXT_debug_utils`,
    `XR_EXT_conformance_automation`, and `XR_EXT_thermal_query`.
    ([internal MR 2023](https://gitlab.khronos.org/openxr/openxr/merge_requests/2023),
    [OpenXR-Docs/#10](https://github.com/KhronosGroup/OpenXR-Docs/issues/10),
    [internal issue 1256](https://gitlab.khronos.org/openxr/openxr/issues/1256))
  - Reserve extension 166 for working group use.
    ([internal MR 2025](https://gitlab.khronos.org/openxr/openxr/merge_requests/2025))
- SDK
  - Loader: Change runtime part to return `XR_ERROR_RUNTIME_UNAVAILABLE` when
    there is an error loading a runtime.
    ([internal MR 2024](https://gitlab.khronos.org/openxr/openxr/merge_requests/2024),
    [internal issue 1552](https://gitlab.khronos.org/openxr/openxr/issues/1552),
    [OpenXR-SDK-Source/#177](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/177))
  - Loader: Simplify in areas where code paths were dead.
    ([internal MR 2024](https://gitlab.khronos.org/openxr/openxr/merge_requests/2024))
  - Loader: Improved locking around a few areas of the loader that aren't robust
    against usual concurrent calls.
    ([OpenXR-SDK-Source/#252](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/252))
  - validation layer: Fix generated code when a protected extension contains a base
    header type.
    ([internal MR 1997](https://gitlab.khronos.org/openxr/openxr/merge_requests/1997))

## OpenXR SDK 1.0.15 (2021-04-13)

The main SDK change in this release is that the OpenXR headers **no longer
expose extension function prototypes** because extension functions are not
exported by the loader. This should prevent some confusion during development
without affecting code that correctly compiles and links with older SDKs. Code
that was compiled but not linked (for instance, the automated tests of example
source in the specification) and that would not have successfully linked may
have their defects highlighted by this change, however. If you need those
prototypes still available, there is a preprocessor define that can re-enable
them. The function pointer definitions are always available.

In addition to that header change, this release contains three new vendor
extensions plus an assortment of SDK fixes.

- Registry
  - Add `XR_VARJO_foveated_rendering` vendor extension.
    ([internal MR 1981](https://gitlab.khronos.org/openxr/openxr/merge_requests/1981))
  - Add `XR_VARJO_composition_layer_depth_test` vendor extension.
    ([internal MR 1998](https://gitlab.khronos.org/openxr/openxr/merge_requests/1998))
  - Add `XR_VARJO_environment_depth_estimation` vendor extension.
    ([internal MR 1998](https://gitlab.khronos.org/openxr/openxr/merge_requests/1998))
  - Add `uint16_t` to `openxr_platform_defines` (and associated scripts) so it may
    be used easily by extensions.
    ([internal MR 2017](https://gitlab.khronos.org/openxr/openxr/merge_requests/2017))
  - Reserve extension 149 for working group use.
    ([internal MR 1999](https://gitlab.khronos.org/openxr/openxr/merge_requests/1999))
  - Reserve extension numbers 150 to 155 for ULTRALEAP extensions
    ([internal MR 2006](https://gitlab.khronos.org/openxr/openxr/merge_requests/2006))
  - Reserve extension numbers 156-165 for Facebook.
    ([internal MR 2018](https://gitlab.khronos.org/openxr/openxr/merge_requests/2018))
- SDK
  - Hide prototypes for extension functions unless explicitly requested by defining
    `XR_EXTENSION_PROTOTYPES`. These functions are not exported from the loader, so
    having their prototypes available is confusing and leads to link errors, etc.
    ([OpenXR-SDK-Source/#251](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/251),
    [OpenXR-SDK-Source/#174](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/174),
    [internal issue 1554](https://gitlab.khronos.org/openxr/openxr/issues/1554),
    [internal issue 1338](https://gitlab.khronos.org/openxr/openxr/issues/1338))
  - Also list API layers in list tool.
    ([internal MR 1991](https://gitlab.khronos.org/openxr/openxr/merge_requests/1991))
  - Ensure we expose the OpenXR headers in the build-time interface of the loader,
    as well as the install-time interface, for use with FetchContent.cmake.
    ([OpenXR-SDK-Source/#242](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/242),
    [OpenXR-SDK-Source/#195](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/195),
    [internal issue 1409](https://gitlab.khronos.org/openxr/openxr/issues/1409))
  - Improve `BUILDING.md`, including adding details on how to specify architecture
    for VS2019.
    ([OpenXR-SDK-Source/#245](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/245),
    [OpenXR-SDK-Source/#253](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/253))
  - Loader: Fix loader failing to load on Windows 7 due to `pathcch` dependency.
    ([OpenXR-SDK-Source/#239](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/239),
    [OpenXR-SDK-Source/#214](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/214),
    [internal issue 1471](https://gitlab.khronos.org/openxr/openxr/issues/1471),
    [OpenXR-SDK-Source/#236](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/236),
    [internal issue 1519](https://gitlab.khronos.org/openxr/openxr/issues/1519))
  - Loader: Fix conflicting filename in `openxr_loader.def` causing a linker warning
    when building debug for Windows.
    ([OpenXR-SDK-Source/#246](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/246))
  - Update `cgenerator.py` to generate header comments in `openxr.h` to show when a
    struct extends another struct
    ([internal MR 2005](https://gitlab.khronos.org/openxr/openxr/merge_requests/2005))
  - hello_xr: Check for `shaderStorageImageMultisample` feature in Vulkan plugin
    before using it.
    ([OpenXR-SDK-Source/#234](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/234),
    [OpenXR-SDK-Source/#233](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/233),
    [internal issue 1518](https://gitlab.khronos.org/openxr/openxr/issues/1518))
  - hello_xr: Make sure `common.h` includes the reflection header that it uses.
    ([OpenXR-SDK-Source/#247](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/247))
  - layers: Revise documentation, re-formatting and updating to refer to real
    functions and URLs.
    ([internal MR 2012](https://gitlab.khronos.org/openxr/openxr/merge_requests/2012))
  - loader: Check the instance handle passed to `xrGetInstanceProcAddr`.
    ([internal MR 1980](https://gitlab.khronos.org/openxr/openxr/merge_requests/1980))
  - loader: Fix building OpenXR-SDK with CMake's multi-config Ninja generator.
    ([OpenXR-SDK-Source/#249](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/249),
    [OpenXR-SDK-Source/#231](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/231))
  - `openxr_reflection.h`: Make reproducible/deterministic by sorting protection
    defines in the script.
    ([internal MR 1993](https://gitlab.khronos.org/openxr/openxr/merge_requests/1993),
    [internal issue 1424](https://gitlab.khronos.org/openxr/openxr/issues/1424))
  - xr_dependencies (shared utility): Include `unknwn.h` on Windows, even without
    D3D enabled.
    ([OpenXR-SDK-Source/#250](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/250),
    [OpenXR-SDK-Source/#237](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/237))

## OpenXR SDK 1.0.14 (2021-01-27)

This release contains a collection of fixes and improvements, including one new
vendor extension. Notably, we have relicensed all files that become part of the
loader, so the loader may be "Apache-2.0 OR MIT" for downstream license
compatibility.

- Registry
  - Add new `XR_FB_android_surface_swapchain_create` vendor extension.
    ([internal MR 1939](https://gitlab.khronos.org/openxr/openxr/merge_requests/1939),
    [internal issue 1493](https://gitlab.khronos.org/openxr/openxr/issues/1493),
    [internal MR 1968](https://gitlab.khronos.org/openxr/openxr/merge_requests/1968))
  - Add missing `optional` attributes to `XR_KHR_vulkan_enable2` structs. Fixes
    validation layer.
    ([OpenXR-Docs/#72](https://github.com/KhronosGroup/OpenXR-Docs/pull/72))
  - Correction to `locationFlags` field in `XrHandJointLocationEXT` to be optional.
    ([internal MR 1945](https://gitlab.khronos.org/openxr/openxr/merge_requests/1945))
  - Reserve vendor extensions for Varjo.
    ([internal MR 1935](https://gitlab.khronos.org/openxr/openxr/merge_requests/1935))
  - Reserve vendor extensions for Magic Leap.
    ([internal MR 1967](https://gitlab.khronos.org/openxr/openxr/merge_requests/1967),
    [internal MR 1970](https://gitlab.khronos.org/openxr/openxr/merge_requests/1970))
  - Reserve extension number 143 to 148 for MSFT extensions.
    ([internal MR 1969](https://gitlab.khronos.org/openxr/openxr/merge_requests/1969))
  - Update Magic Leap ID and contact information.
    ([internal MR 1967](https://gitlab.khronos.org/openxr/openxr/merge_requests/1967))
- SDK
  - Add `./` to the start of the library name in API layer manifests on Windows, so
    they are treated as a relative path.
    ([internal MR 1975](https://gitlab.khronos.org/openxr/openxr/merge_requests/1975))
  - Fix searching for prerequisites in generated CMake config files.
    ([internal MR 1963](https://gitlab.khronos.org/openxr/openxr/merge_requests/1963))
  - Start shipping the OpenXR API layers with the release artifacts.
    ([internal MR 1975](https://gitlab.khronos.org/openxr/openxr/merge_requests/1975))
  - cmake: Debug library uses d suffix on Windows. CMake `OPENXR_DEBUG_POSTFIX`
    variable can be set to something else to change it.
    ([OpenXR-SDK-Source/#229](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/229))
  - hello_xr: Remove redundant call to `xrInitializeLoaderKHR`.
    ([internal MR 1933](https://gitlab.khronos.org/openxr/openxr/merge_requests/1933))
  - hello_xr: Return supported sample count as 1 for GLES, GL and D3D11.
    ([internal MR 1962](https://gitlab.khronos.org/openxr/openxr/merge_requests/1962))
  - hello_xr: Use `android.app.NativeActivity` correctly in place of NativeActivity
    subclass.
    ([internal MR 1976](https://gitlab.khronos.org/openxr/openxr/merge_requests/1976))
  - hello_xr: On Vulkan, explicitly add surface extensions for mirror window.
    ([OpenXR-SDK-Source/#230](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/230),
    [internal MR 1934](https://gitlab.khronos.org/openxr/openxr/merge_requests/1934))
  - loader: Relicense all files that become part of the loader, so the loader may
    be "Apache-2.0 OR MIT" for downstream license compatibility.
    ([internal MR 1937](https://gitlab.khronos.org/openxr/openxr/merge_requests/1937),
    [internal issue 1449](https://gitlab.khronos.org/openxr/openxr/issues/1449),
    [OpenXR-SDK-Source/#205](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/205))
  - loader: Protect against the application overriding loader symbols.
    ([internal MR 1961](https://gitlab.khronos.org/openxr/openxr/merge_requests/1961))
  - loader: Handle JSON files in the search path that are not objects.
    ([internal MR 1979](https://gitlab.khronos.org/openxr/openxr/merge_requests/1979))

## OpenXR SDK 1.0.13 (2020-11-24)

The SDK in this release features some fixes to the loader's layer parsing:
upgrading is recommended. The hello_xr example has also been improved. The
registry for this release features a new ratified Khronos extension which will
serve as the basis of other extensions, as well as a number of new vendor
extensions.

- Registry
  - Add `XR_HTC_vive_cosmos_controller_interaction` vendor extension.
    ([internal MR 1907](https://gitlab.khronos.org/openxr/openxr/merge_requests/1907))
  - Add `XR_FB_display_refresh_rate` vendor extension.
    ([internal MR 1909](https://gitlab.khronos.org/openxr/openxr/merge_requests/1909))
  - Add `XR_MSFT_perception_anchor_interop` vendor extension.
    ([internal MR 1929](https://gitlab.khronos.org/openxr/openxr/merge_requests/1929))
  - Added ratified `KHR_binding_modifications` Khronos extension.
    ([internal MR 1878](https://gitlab.khronos.org/openxr/openxr/merge_requests/1878),
    [internal issue 1413](https://gitlab.khronos.org/openxr/openxr/issues/1413))
  - Reserve vendor extensions for HTC.
    ([internal MR 1907](https://gitlab.khronos.org/openxr/openxr/merge_requests/1907))
  - Reserve vendor extension numbers 109-120 for Facebook extensions.
    ([internal MR 1913](https://gitlab.khronos.org/openxr/openxr/merge_requests/1913))
- SDK
  - Fix build errors under mingw-w64.
    ([OpenXR-SDK-Source/#212](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/212))
  - Include PDB symbols to go along with the openxr_loader.dll Windows artifacts.
    ([OpenXR-SDK-Source/#225](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/225))
  - `XrMatrix4x4f_CreateProjection`: Explicitly define matrix values as floats.
    Prevents potential division by zero.
    ([OpenXR-SDK-Source/#219](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/219))
  - build: Normalize how we detect and utilize threading libraries in the build
    process.
    ([internal MR 1910](https://gitlab.khronos.org/openxr/openxr/merge_requests/1910))
  - build: Search for OpenGL ES and other things needed on Android.
    ([internal MR 1910](https://gitlab.khronos.org/openxr/openxr/merge_requests/1910))
  - build: Normalize how we detect and utilize Vulkan in the build process.
    ([internal MR 1910](https://gitlab.khronos.org/openxr/openxr/merge_requests/1910))
  - build/ci: Have Windows loader artifacts organize themselves by
    architecture/platform, and bundle the CMake config files and a "meta" CMake
    config.
    ([OpenXR-SDK-Source/#224](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/224),
    [OpenXR-SDK-Source/#185](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/185))
  - documentation: Make API Layer manifest example for "disable_environment" and
    "enable_environment" match the loader behavior
    ([internal MR 1917](https://gitlab.khronos.org/openxr/openxr/merge_requests/1917),
    [OpenXR-SDK-Source/#213](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/213))
  - hello_xr: Don't use subaction paths for quit_session action, it's unnecessary.
    ([internal MR 1898](https://gitlab.khronos.org/openxr/openxr/merge_requests/1898))
  - hello_xr: Add initial build system support for building for Android. (No gradle
    support yet.)
    ([internal MR 1910](https://gitlab.khronos.org/openxr/openxr/merge_requests/1910))
  - hello_xr: Call `xrInitializeLoaderKHR` and dynamically load `openxr_loader` on
    Android.
    ([internal MR 1910](https://gitlab.khronos.org/openxr/openxr/merge_requests/1910))
  - hello_xr: Fix printing of action bindings and make it prettier.
    ([internal MR 1914](https://gitlab.khronos.org/openxr/openxr/merge_requests/1914))
  - hello_xr: Fix break on Oculus Quest.
    ([internal MR 1921](https://gitlab.khronos.org/openxr/openxr/merge_requests/1921))
  - hello_xr: The D3D12 and Vulkan graphics plugins sometimes did not update their
    swapchain image context maps due to rare key collisions.
    ([OpenXR-SDK-Source/#217](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/217))
  - loader: Stub in some preliminary code for Android loader support - not a
    complete port.
    ([internal MR 1910](https://gitlab.khronos.org/openxr/openxr/merge_requests/1910))
  - loader: Add Android logcat logger.
    ([internal MR 1910](https://gitlab.khronos.org/openxr/openxr/merge_requests/1910))
  - loader: Fix parsing of XR_ENABLE_API_LAYERS environment variable
    ([internal MR 1912](https://gitlab.khronos.org/openxr/openxr/merge_requests/1912))
  - loader: Fix issues around `xrInitializeLoaderKHR`.
    ([internal MR 1922](https://gitlab.khronos.org/openxr/openxr/merge_requests/1922))
  - loader: Replace `#if _WIN32` with `#ifdef _WIN32`.
    ([OpenXR-SDK-Source/#215](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/215))

## OpenXR SDK 1.0.12 (2020-09-25)

This release features a number of new ratified KHR extensions, as well as a new
vendor extension.

- Registry
  - Add ratified `XR_KHR_vulkan_enable2` Khronos extension.
    ([internal MR 1627](https://gitlab.khronos.org/openxr/openxr/merge_requests/1627),
    [internal issue 1249](https://gitlab.khronos.org/openxr/openxr/issues/1249),
    [internal issue 1283](https://gitlab.khronos.org/openxr/openxr/issues/1283),
    [internal MR 1863](https://gitlab.khronos.org/openxr/openxr/merge_requests/1863))
  - Add ratified `XR_KHR_loader_init` Khronos extension.
    ([internal MR 1744](https://gitlab.khronos.org/openxr/openxr/merge_requests/1744))
  - Add ratified `XR_KHR_loader_init_android` Khronos extension.
    ([internal MR 1744](https://gitlab.khronos.org/openxr/openxr/merge_requests/1744))
  - Add ratified `XR_KHR_composition_layer_equirect2` Khronos extension.
    ([internal MR 1746](https://gitlab.khronos.org/openxr/openxr/merge_requests/1746))
  - Add ratified `XR_KHR_composition_layer_color_scale_bias` Khronos extension.
    ([internal MR 1762](https://gitlab.khronos.org/openxr/openxr/merge_requests/1762))
  - Add `XR_MSFT_controller_model` extension.
    ([internal MR 1832](https://gitlab.khronos.org/openxr/openxr/merge_requests/1832))
  - Add vendor tag `LIV` for LIV Inc.
    ([internal MR 1896](https://gitlab.khronos.org/openxr/openxr/merge_requests/1896))
  - Fix `structextends` attribute of `XrHandPoseTypeInfoMSFT`.
    ([OpenXR-SDK-Source/#207](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/207))
  - schema: Update to permit aliases for commands and struct types. (Already
    supported by tooling.)
    ([internal MR 1627](https://gitlab.khronos.org/openxr/openxr/merge_requests/1627))
- SDK
  - cmake: fix openxr_loader target export when installing both Release and Debug
    config on Windows.
    ([OpenXR-SDK-Source/#206](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/206))
  - hello_xr: Support the new `XR_KHR_vulkan_enable2` extension.
    ([internal MR 1627](https://gitlab.khronos.org/openxr/openxr/merge_requests/1627))
  - hello_xr: Use the `XR_KHR_loader_init_android` extension on Android.
    ([internal MR 1903](https://gitlab.khronos.org/openxr/openxr/merge_requests/1903))
  - layers: Fix ARM builds by re-adding function attributes.
    ([OpenXR-SDK-Source/#193](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/193))
- Misc
  - Clean up trailing whitespace, byte-order marks, anda ensure trailing newlines.
    ([OpenXR-SDK-Source/#208](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/208))

## OpenXR SDK 1.0.11 (2020-08-14)

This release is mainly for SDK improvements, with only small changes to the
docs. A new error code is provided for `xrCreateSession` for developers
convenience.

- Registry
  - Register `ULTRALEAP` author ID for Ultraleap.
    ([internal MR 1877](https://gitlab.khronos.org/openxr/openxr/merge_requests/1877))
  - Reserve the extension number 98 to 101 for future MSFT extensions.
    ([internal MR 1879](https://gitlab.khronos.org/openxr/openxr/merge_requests/1879))
  - schema: Distinguish `parentstruct` and `structextends` attributes in comments.
    ([internal MR 1881](https://gitlab.khronos.org/openxr/openxr/merge_requests/1881),
    [OpenXR-Docs/#51](https://github.com/KhronosGroup/OpenXR-Docs/issues/51),
    [internal issue 1396](https://gitlab.khronos.org/openxr/openxr/issues/1396))
  - Add a new result code, `XR_ERROR_GRAPHICS_REQUIREMENTS_CALL_MISSING`, for
    runtimes to return if `xrBeginSession` is called before calling one of the
    `xrGetGraphicsRequirements` calls.
    ([internal MR 1882](https://gitlab.khronos.org/openxr/openxr/merge_requests/1882),
    [OpenXR-Docs/#53](https://github.com/KhronosGroup/OpenXR-Docs/issues/53),
    [internal issue 1397](https://gitlab.khronos.org/openxr/openxr/issues/1397))
- SDK
  - Improve language usage in code and comments to be more respectful.
    ([internal MR 1881](https://gitlab.khronos.org/openxr/openxr/merge_requests/1881))
  - Loader: Correct type of "extension_version" in API layer manifest files to
    string, while maintaining backwards compatibility. Remove undocumented and
    unused "device_extensions" and "entrypoints" keys.
    ([internal MR 1867](https://gitlab.khronos.org/openxr/openxr/merge_requests/1867),
    [internal issue 1411](https://gitlab.khronos.org/openxr/openxr/issues/1411))
  - Replace usage of `std::filesystem::canonical` with `PathCchCanonicalize` on
    Windows platform to work around bug on UWP platforms. This also replaces
    `PathCanonicalize` with `PathCchCanonicalize` and adds the appropriate library
    for linking in.
    ([OpenXR-SDK-Source/#198](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/198))
  - Support for building more projects when targeting UWP, and support for all
    architectures when targeting Win32.
    ([OpenXR-SDK-Source/#199](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/199))
  - hello_xr: fix Vulkan image layout transitions.
    ([internal MR 1876](https://gitlab.khronos.org/openxr/openxr/merge_requests/1876))
  - validation: Enable three additional checks (on optional arrays with non-
    optional counts) that were missing because of a script error.
    ([internal MR 1881](https://gitlab.khronos.org/openxr/openxr/merge_requests/1881))

## OpenXR SDK 1.0.10 (2020-07-28)

Note the relicensing of the registry XML file and some include files provided by
or generated by this repository (first item in each changelog section). Each
file's header, or an adjacent file with `.license` appended to the filename, is
the best reference for its license terms. We are currently working on ensuring
all files have an SPDX license identifier tag either in them or in an adjacent
file. This is still in progress but mostly complete.

- Registry
  - Relicense registry XML from MIT-like "Khronos Free Use License for Software and
    Documentation" to, at your option, either the Apache License, Version 2.0,
    found at
    <http://www.apache.org/licenses/LICENSE-2.0>, or the MIT License, found at
    <http://opensource.org/licenses/MIT>, for broader license compatibility with
    downstream projects. (SPDX License Identifier expression "Apache-2.0 OR MIT")
    ([internal MR 1814](https://gitlab.khronos.org/openxr/openxr/merge_requests/1814),
    [OpenXR-Docs/#3](https://github.com/KhronosGroup/OpenXR-Docs/issues/3),
    [internal issue 958](https://gitlab.khronos.org/openxr/openxr/issues/958))
  - Add `XR_MSFT_holographic_window_attachment` vendor extension.
    ([internal MR 1833](https://gitlab.khronos.org/openxr/openxr/merge_requests/1833))
  - Add `XR_EXT_hp_mixed_reality_controller` multi-vendor extension.
    ([internal MR 1834](https://gitlab.khronos.org/openxr/openxr/merge_requests/1834))
  - Add `XR_EXT_samsung_odyssey_controller` multi-vendor extension.
    ([internal MR 1835](https://gitlab.khronos.org/openxr/openxr/merge_requests/1835))
  - Add `XR_VALVE_analog_threshold` vendor extension.
    ([internal MR 1859](https://gitlab.khronos.org/openxr/openxr/merge_requests/1859))
  - Add `XR_MND_swapchain_usage_input_attachment_bit` vendor extension.
    ([internal MR 1865](https://gitlab.khronos.org/openxr/openxr/merge_requests/1865))
  - Reserve extension numbers 71 to 78 for Facebook extensions.
    ([internal MR 1839](https://gitlab.khronos.org/openxr/openxr/merge_requests/1839))
  - Reserve extension numbers 79 to 88 for Valve extensions.
    ([internal MR 1842](https://gitlab.khronos.org/openxr/openxr/merge_requests/1842))
  - Reserve extension numbers 89 to 92 for Khronos extensions.
    ([internal MR 1844](https://gitlab.khronos.org/openxr/openxr/merge_requests/1844))
  - Reserve extension numbers 93 to 94 for `EXT_unbounded_reference_space` and
    `EXT_spatial_anchor`.
    ([internal MR 1854](https://gitlab.khronos.org/openxr/openxr/merge_requests/1854))
  - `XR_EPIC_view_configuration_fov`: Fix `recommendedFov` incorrectly being named
    `recommendedMutableFov`. This is a **source-incompatible change** to a vendor
    extension.
    ([internal MR 1812](https://gitlab.khronos.org/openxr/openxr/merge_requests/1812))
  - schema: Adjust to permit bitmask expansion in extensions, already supported by
    toolchain thanks to Vulkan.
    ([internal MR 1865](https://gitlab.khronos.org/openxr/openxr/merge_requests/1865))
  - scripts: Teach xml-consistency to handle bitmask values defined in extensions.
    ([internal MR 1865](https://gitlab.khronos.org/openxr/openxr/merge_requests/1865))
- SDK
  - Relicense generated headers `openxr.h`, `openxr_platform.h`,
    `openxr_reflection.h`, and static header `openxr_platform_defines.h` from the
    Apache License, version 2.0, to, at your option, either the Apache License,
    Version 2.0, found at
    <http://www.apache.org/licenses/LICENSE-2.0>, or the MIT License, found at
    <http://opensource.org/licenses/MIT>, for broader license compatibility with
    downstream projects. (SPDX License Identifier expression "Apache-2.0 OR MIT")
    ([internal MR 1814](https://gitlab.khronos.org/openxr/openxr/merge_requests/1814),
    [OpenXR-Docs/#3](https://github.com/KhronosGroup/OpenXR-Docs/issues/3),
    [internal issue 958](https://gitlab.khronos.org/openxr/openxr/issues/958))
  - Loader: Fix loading relative runtime libraries on Linux.
    ([internal MR 1817](https://gitlab.khronos.org/openxr/openxr/merge_requests/1817))
  - Loader: Fix error on xrCreateInstance when explicitly trying to enable an
    implicit API layer.
    ([internal MR 1858](https://gitlab.khronos.org/openxr/openxr/merge_requests/1858))
  - Modify Azure DevOps build pipeline to automatically generate a NuGet package.
    ([OpenXR-SDK-Source/#196](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/196))
  - Partially revert build system changes related to detecting Direct3D, to fix
    builds.
    ([internal MR 1802](https://gitlab.khronos.org/openxr/openxr/merge_requests/1802))
  - Portability fixes, including checking for `timespec_get` before enabling
    `XR_USE_TIMESPEC`.
    ([internal MR 1804](https://gitlab.khronos.org/openxr/openxr/merge_requests/1804))
  - cmake: export `OpenXRConfig.cmake` during install. Two targets can be imported
    by another CMake application: `OpenXR::openxr_loader` and `OpenXR::headers`.
    ([OpenXR-SDK-Source/#191](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/191),
    [OpenXR-SDK-Source/#185](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/185))
  - hello_xr: Fix disparity between swapchain and render pass sample count in
    Vulkan in the case where implementation recommends a value higher than one.
    ([internal MR 1794](https://gitlab.khronos.org/openxr/openxr/merge_requests/1794))
  - hello_xr: Fix build on a minimal Linux install by ensuring we check for all
    dependencies we use. We had missed checking for xcb_glx.
    ([internal MR 1799](https://gitlab.khronos.org/openxr/openxr/merge_requests/1799),
    [internal issue 1360](https://gitlab.khronos.org/openxr/openxr/issues/1360))
  - hello_xr: Fix a Vulkan crash on Windows related to the mirror window.
    ([internal MR 1823](https://gitlab.khronos.org/openxr/openxr/merge_requests/1823))
  - hello_xr: Use more proper linear formats
    ([internal MR 1840](https://gitlab.khronos.org/openxr/openxr/merge_requests/1840))
  - hello_xr: Enable use of glslangValidator to compile shaders if shaderc is not
    available.
    ([internal MR 1857](https://gitlab.khronos.org/openxr/openxr/merge_requests/1857))
  - hello_xr: Fix verbose per-layer information.
    ([internal MR 1866](https://gitlab.khronos.org/openxr/openxr/merge_requests/1866))
  - hello_xr: Add Valve Index Controller bindings. Also use trigger value instead
    of squeeze click for grab action on Vive Wand controller.
    ([OpenXR-SDK-Source/#163](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/163))
  - openxr_reflection: Add `XR_LIST_STRUCT_` expansion macros for structure types,
    as well as `XR_LIST_STRUCTURE_TYPES` macro associating types with
    `XrStructureType` values.
    ([internal MR 1495](https://gitlab.khronos.org/openxr/openxr/merge_requests/1495))
  - openxr_reflection: Adds `XR_LIST_EXTENSIONS()` macro, which will call your
    supplied macro name with the name and extension number of all known extensions.
    ([internal MR 1864](https://gitlab.khronos.org/openxr/openxr/merge_requests/1864))

## OpenXR SDK 1.0.9 (2020-05-29)

- Registry
  - Add an author ID, and reserve a vendor extension for Huawei.
    ([OpenXR-Docs/#46](https://github.com/KhronosGroup/OpenXR-Docs/pull/46))
  - Reserve vendor extensions for future LunarG overlay and input focus
    functionality.
    ([internal MR 1720](https://gitlab.khronos.org/openxr/openxr/merge_requests/1720))
  - Reserve vendor extensions for Microsoft.
    ([internal MR 1723](https://gitlab.khronos.org/openxr/openxr/merge_requests/1723))
  - Add `XR_EXT_hand_tracking` multi-vendor extension.
    ([internal MR 1554](https://gitlab.khronos.org/openxr/openxr/merge_requests/1554),
    [internal issue 1266](https://gitlab.khronos.org/openxr/openxr/issues/1266),
    [internal issue 1267](https://gitlab.khronos.org/openxr/openxr/issues/1267),
    [internal issue 1268](https://gitlab.khronos.org/openxr/openxr/issues/1268),
    [internal issue 1269](https://gitlab.khronos.org/openxr/openxr/issues/1269))
  - Add `XR_HUAWEI_controller_interaction` vendor extension.
    ([OpenXR-Docs/#47](https://github.com/KhronosGroup/OpenXR-Docs/pull/47))
  - Add `XR_MNDX_egl_enable` provisional vendor extension.
    ([OpenXR-Docs/#48](https://github.com/KhronosGroup/OpenXR-Docs/pull/48))
  - Add `XR_MSFT_spatial_graph_bridge` vendor extension.
    ([internal MR 1730](https://gitlab.khronos.org/openxr/openxr/merge_requests/1730))
  - Add `XR_MSFT_secondary_view_configuration` and `XR_MSFT_first_person_observer`
    vendor extensions.
    ([internal MR 1731](https://gitlab.khronos.org/openxr/openxr/merge_requests/1731))
  - Add `XR_MSFT_hand_mesh_tracking` vendor extension.
    ([internal MR 1736](https://gitlab.khronos.org/openxr/openxr/merge_requests/1736))
  - Fix missing space in XML definition of `XrSpatialAnchorCreateInfoMSFT`.
    ([internal MR 1742](https://gitlab.khronos.org/openxr/openxr/merge_requests/1742),
    [internal issue 1351](https://gitlab.khronos.org/openxr/openxr/issues/1351),
    [OpenXR-SDK-Source/#187](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/187))
  - Update a number of contacts for author/vendor tags.
    ([internal MR 1788](https://gitlab.khronos.org/openxr/openxr/merge_requests/1788),
    [internal issue 1326](https://gitlab.khronos.org/openxr/openxr/issues/1326))
- SDK
  - Replaced usage of the `_DEBUG` macro with `NDEBUG`.
    ([internal MR 1756](https://gitlab.khronos.org/openxr/openxr/merge_requests/1756))
  - Allow disabling of `std::filesystem` usage via CMake, and detect if it's
    available and what its requirements are.
    ([OpenXR-SDK-Source/#192](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/192),
    [OpenXR-SDK-Source/#188](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/188))
  - CI: Modifications to Azure DevOps build pipeline. Now builds UWP loader DLLs in
    addition to Win32 loader DLLs. No longer builds static loader libraries due to
    linkability concerns. Re-arranged release artifact zip to distinguish
    architecture from 32-bit or 64-bit.
  - Loader: Replace global static initializers with functions that return static
    locals. With this change, code that includes OpenXR doesn't have to page in
    this code and initialize these during startup.
    ([OpenXR-SDK-Source/#173](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/173))
  - Loader: Unload runtime when `xrCreateInstance` fails.
    ([internal MR 1778](https://gitlab.khronos.org/openxr/openxr/merge_requests/1778))
  - Loader: Add "info"-level debug messages listing all the places that we look for
    the OpenXR active runtime manifest.
    ([OpenXR-SDK-Source/#190](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/190))
  - Validation Layer: Fix crash in dereferencing a nullptr optional array handle
    when the `count > 0`.
    ([internal MR 1709](https://gitlab.khronos.org/openxr/openxr/merge_requests/1709),
    [OpenXR-SDK-Source/#161](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/161),
    [internal issue 1322](https://gitlab.khronos.org/openxr/openxr/issues/1322))
  - Validation Layer: Fix static analysis error and possible loss of validation
    error.
    ([internal MR 1715](https://gitlab.khronos.org/openxr/openxr/merge_requests/1715),
    [OpenXR-SDK-Source/#160](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/160),
    [internal issue 1321](https://gitlab.khronos.org/openxr/openxr/issues/1321))
  - Validation Layer: Simplify some generated code, and minor performance
    improvements.
    ([OpenXR-SDK-Source/#176](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/176))
  - API Dump Layer: Fix crash in dereferencing a `nullptr` while constructing a
    `std::string`.
    ([internal MR 1712](https://gitlab.khronos.org/openxr/openxr/merge_requests/1712),
    [OpenXR-SDK-Source/#162](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/162),
    [internal issue 1323](https://gitlab.khronos.org/openxr/openxr/issues/1323))
  - hello_xr: Fix releasing a swapchain image with the incorrect image layout.
    ([internal MR 1755](https://gitlab.khronos.org/openxr/openxr/merge_requests/1755))
  - hello_xr: Prefer `VK_LAYER_KHRONOS_validation` over
    `VK_LAYER_LUNARG_standard_validation` when available.
    ([internal MR 1755](https://gitlab.khronos.org/openxr/openxr/merge_requests/1755))
  - hello_xr: Optimizations to D3D12 plugin to avoid GPU pipeline stall.
    ([internal MR 1770](https://gitlab.khronos.org/openxr/openxr/merge_requests/1770))
    ([OpenXR-SDK-Source/#175](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/175))
  - hello_xr: Fix build with Vulkan headers 1.2.136.
    ([OpenXR-SDK-Source/#181](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/181),
    [OpenXR-SDK-Source/#180](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/180),
    [internal issue 1347](https://gitlab.khronos.org/openxr/openxr/issues/1347))
  - hello_xr: Fix build with Visual Studio 16.6.
    ([OpenXR-SDK-Source/#186](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/186),
    [OpenXR-SDK-Source/#184](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/184))

## OpenXR SDK 1.0.8 (2020-03-27)

Patch release for the 1.0 series.

- Registry
  - `XR_EXTX_overlay`: upgrade overlay bit names to match the convention, and
    increase extension version number. This is a **source-incompatible change** to
    a provisional multi-vendor extension.
    ([internal MR 1697](https://gitlab.khronos.org/openxr/openxr/merge_requests/1697),
    [internal issue 1318](https://gitlab.khronos.org/openxr/openxr/issues/1318),
    [internal issue 42](https://gitlab.khronos.org/openxr/openxr/issues/42),
    [internal MR 171](https://gitlab.khronos.org/openxr/openxr/merge_requests/171))
  - Introduce `XR_EXT_eye_gaze_interaction` extension for eye gaze interaction
    profile.
    ([internal MR 1556](https://gitlab.khronos.org/openxr/openxr/merge_requests/1556))
  - Add SPDX license identifier tag to registry schema.
    ([internal MR 1686](https://gitlab.khronos.org/openxr/openxr/merge_requests/1686))
  - Add missing error codes to `xrCreateActionSet`, `xrCreateAction`, and
    `xrGetInputSourceLocalizedName`.
    ([internal MR 1698](https://gitlab.khronos.org/openxr/openxr/merge_requests/1698))
- SDK
  - Add SPDX license identifier tags to nearly all (code) files, including
    generated files.
    ([internal MR 1686](https://gitlab.khronos.org/openxr/openxr/merge_requests/1686))
  - Fix build system behavior with MSVC building in Release mode: only attempt
    to copy PDB files if they exist.
    ([internal MR 1701](https://gitlab.khronos.org/openxr/openxr/merge_requests/1701))

## OpenXR SDK 1.0.7 (2020-03-20)

Patch release for the 1.0 series.

Note: Changelogs are now being assembled with the help of the
[Proclamation](https://pypi.org/project/proclamation/) tool, so the format has
changed somewhat.

- Registry
  - Introduce `XR_MSFT_hand_interaction` extension for hand interaction profile.
    ([internal MR 1601](https://gitlab.khronos.org/openxr/openxr/merge_requests/1601))
  - Introduce `XR_EPIC_view_configuration_fov` extension for system field-of-view
    queries.
    ([internal MR 1170](https://gitlab.khronos.org/openxr/openxr/merge_requests/1170))
  - Indicate that `xrBeginFrame` returns `XR_ERROR_CALL_ORDER_INVALID` when not
    paired with a corresponding `xrWaitFrame` call.
    ([internal MR 1673](https://gitlab.khronos.org/openxr/openxr/merge_requests/1673))
  - Update the version number of `XR_KHR_D3D12_enable` extension.
    ([internal MR 1681](https://gitlab.khronos.org/openxr/openxr/merge_requests/1681))
  - Introduce `XR_EXTX_overlay` extension for Overlay sessions (which can
    provide overlay composition layers).
    ([internal MR 1665](https://gitlab.khronos.org/openxr/openxr/merge_requests/1665))
- SDK
  - loader: Add linker export map/version script to avoid exporting implementation
    symbols from C++ on non-MSVC platforms.
    ([internal MR 1641](https://gitlab.khronos.org/openxr/openxr/merge_requests/1641),
    [OpenXR-SDK-Source/#159](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/159))
  - Add tracking and destruction of debug messengers in the loader.
    ([internal MR 1668](https://gitlab.khronos.org/openxr/openxr/merge_requests/1668),
    [OpenXR-SDK-Source/#29](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/29),
    [internal issue 1284](https://gitlab.khronos.org/openxr/openxr/issues/1284))
  - Fix issue in `hello_xr` breaking the build in certain limited conditions.
    ([OpenXR-SDK-Source/#170](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/170))
  - Add initial (partial) Android support for `hello_xr`.
    ([internal MR 1680](https://gitlab.khronos.org/openxr/openxr/merge_requests/1680))
  - Fix a mismatched type signature, breaking compiles of hello_xr in at least some
    Linux environments.
    ([OpenXR-SDK-Source/#164](https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/164),
    [internal MR 166](https://gitlab.khronos.org/openxr/openxr/merge_requests/166))
  - Explicitly link in `advapi32` for many of the APIs the loader uses on Windows,
    needed when building for ARM/ARM64 (non-UWP only).
    ([internal MR 1664](https://gitlab.khronos.org/openxr/openxr/merge_requests/1664))
  - Remove "Dev Build" string from loader resources and fix version. ([internal MR
    1664](https://gitlab.khronos.org/openxr/openxr/merge_requests/1664))
  - Add manual pages for `openxr_runtime_list` and `hello_xr` (based on their
    `--help`), and install in the standard location on non-Windows platforms.
    ([OpenXR-SDK-Source/#169](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/169))
  - Silence some noisy warnings in hello_xr and the layers.
    ([OpenXR-SDK-Source/#165](https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/165))

## OpenXR 1.0.6 release (24-January-2020)

Patch release for the 1.0 series.

This release contains, among other things, a substantial simplification and
cleanup of the loader, which should fix a number of issues and also make it
forward compatible with extensions newer than the loader itself. As a part of
this change, the loader itself now only supports a single `XrInstance` active at
a time per process. If you attempt to create a new instance while an existing
one remains (such as in the case of application code leaking an `XrInstance`
handle), the loader will now return `XR_ERROR_LIMIT_REACHED`.

### GitHub Pull Requests

These had been integrated into the public repo incrementally.

- hello_xr
  - Initialize hand_scale to 1.0
    <https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/157>
  - Fix Vulkan CHECK_CBSTATE build under newer MSVC
    <https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/154>
  - Initialize hand_scale to 1.0 to still show controller cubes even if
    grabAction not available on startup.
    <https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/157>
- Loader
  - Single instance loader refactor with forward compatibility
    <https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/146> (and internal
    MRs 1599, 1621)
  - Fix bug in loading API layers that could result in not loading an available
    and enabled layer
    <https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/155>
- Build
  - Clean up linking, build loader and layers with all available
    platform/presentation support, fix pkg-config file, rename `runtime_list`
    test executable to `openxr_runtime_list`
    <https://github.com/KhronosGroup/OpenXR-SDK-Source/pull/149>

### Internal issues

- Registry
  - Fix typo in visibility mesh enum comment.
  - Add `XR_EXT_win32_appcontainer_compatible` extension.
- Scripts
  - Fix comment typos.
  - Sync scripts with Vulkan. (internal MR 1625)
- Loader
  - Allow use of `/` in paths in FileSysUtils on Windows.
- Build
  - Improve messages
- hello_xr
  - Add D3D12 graphics plugin (internal MR 1616)
  - Fix comment typo.

## OpenXR 1.0.5 release (6-December-2019)

Patch release for the 1.0 series.

This release primarily contains extension reservations and small specification
clarifications/fixes.

### GitHub Pull Requests

These had been integrated into the public repo incrementally.

- Loader tests
  - #147 - Small bugfix and output extension

### Internal issues

- Registry
  - Reserve Microsoft extension numbers (Internal MR 1613)

## OpenXR 1.0.4 release (21-November-2019)

Patch release for the 1.0 series.

This release includes some fixes, extensions, and a small build system change:
the build system is now configured to use C++14. No code changes in the loader
or layers have yet taken place that require C++14. **Please file an issue** in
OpenXR-SDK-Source if there is some deployment platform where you would be unable
to use a loader making use of C++14 features.

### GitHub Pull Requests

These had been integrated into the public repo incrementally.

- General, Build, Other
  - #141 - Support system libs better (permit system jsoncpp, etc. for easier
    packaging)
- hello_xr
  - #144 - Fix hello_xr when running under Linux OpenGL X11
- Registry
  - Reserve a Monado EGL extension
    <https://github.com/KhronosGroup/OpenXR-Docs/pull/39>

### Internal issues

- General, Build, Other
  - Switch C++ standard version to C++14 (internal MR 1602)
  - Remove unused/unneeded files (internal MR 1609)
- Loader
  - Fix typo in parameter/member names (internal MR 1607, internal issue 1233)
  - Fix deprecated usage of JsonCpp (internal MR 1604, internal issue 1212)
- hello_xr
  - Resolve misleading use of `xrLocateViews` before `xrWaitFrame` in helloXR
    and spec (internal MR 1584, internal issue 1227, public issue
    <https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/134>)
- Registry
  - Add `XR_EXT_conformance_automation` extension, for use **only** by
    conformance testing (internal MR 1577, 1608)

## OpenXR 1.0.3 release (7-October-2019)

Patch release for the 1.0 series.

Note that this release includes changes to adjust the symbol exports from
dynamic library versions of the loader to align with the specification. Only
**core** symbols are currently exported. All extension symbols must be retrieved
using `xrGetInstanceProcAddr`.

### GitHub Pull Requests

These had been integrated into the public repo incrementally.

- General, Build, Other
  - #139 - Write output atomically at the end of generator scripts
  - #119 - Loader test updates.
  - #116 - Static analysis cleanups.
- Loader
  - #140 - Permit broader valid usage re: layers
  - #133 - Remove shwapi dependency
  - #132 - Fix directory searching for layers
  - #130 - Fix exporting of symbols on Windows.
  - #129 - Remove debug ext only when added by loader - fixes usage of debug ext
    on runtimes that do not provide it themselves.
  - #125 - Include a `OutputDebugString` logger for Win32
- Layers
  - #138 - Don't validate output enum buffer values
  - #137 - Fix incorrect filenames in the generated API layer JSON

### Internal issues

- General, Build, Other
  - Fix warnings in MSVC static code analysis mode (internal MR 1574)
  - Validation layer improvements and fixes (internal MR 1568)
  - Update vendored jsoncpp to 1.9.1 (internal MR 1523)
- Loader
  - Add ability to quiet the loader's default output (internal MR 1576)
  - Fix conformance of loader in
    `xrEnumerateApiLayerProperties`/`xrEnumerateInstanceExtensionProperties`
- hello_xr
  - Simplify action usage in hello_xr (internal MR 1553)
- Registry
  - Add `XR_EXT_view_configuration_depth_range` extension (internal MR 1502,
    internal issue 1201)
  - Reserve a Monado extension (internal MR 1541)

## OpenXR 1.0.2 release (27-August-2019)

Patch release for the 1.0 series.

Note that the loader on Windows has a **security fix**: All developers incorporating
the OpenXR loader should update immediately.

### GitHub Pull Requests

These had been integrated into the public repo incrementally.

- General, Build, Other
  - #112 - Update active runtime search documentation
  - #106 - List app changes
  - #114 - Support for building WindowsStore loader and layers, and simplified
    filename
  - #96 - Misc cleanup: build simplification, install hello_xr,
    allow building as subproject, fix null deref in validation layer.
- Loader
  - #102 - Default to catching exceptions, since not being able to catch
    (and having a non-throwing standard library) is less common
  - #109 - Factor out some debug-utils related code from the loader,
    and migrate validation layer to that shared code.
  - #108 - Update json_stream initialization to improve compatibility
  - #118 - Fix logic error in Linux active runtime search
  - #115, #117 - Simplification and refactoring.
- Layers
  - #111 - Some fixes to Validation Layer (as found applying to the UE4 OpenXR
    plugin)
  - #110 - Fix cleaning up session labels in validation layer
- From OpenXR-Docs:
  - #26 - Proposal for unbounded space and spatial anchor extensions (vendor
    extensions)

### Internal issues

- General, Build, Other
  - Allow project to be included in a parent project. (Internal MR 1512)
- hello_xr
  - Fix OpenGL version number to be XrVersion. (Internal MR 1515)
  - Make D3D11 debug device handling more friendly. (Internal MR 1504)
- Registry
  - Fix error in extension-added function. (Internal MR 1510)
  - Add Oculus Android extension. (Internal MR 1518)
  - Reserve additional extension number for Oculus. (Internal MR 1517)
- Loader
  - **Security fix**: Do not use HKEY_CURRENT_USER or environment variables when
    the process is running higher than medium-integrity on Windows.
    (Internal issue 1205, internal MR 1511)
  - Small updates to the loader documentation.

### New extension

- `XR_OCULUS_android_session_state_enable`

## OpenXR 1.0.1 release (2-August-2019)

Patch release for the 1.0 series.

### GitHub Pull Requests

These had been integrated into the public repo incrementally.

- General, Build, Other
  - #87 - Fix makefiles
  - #88 - Remove unneeded generation (corresponds to issue #74, internal issue
    1139, internal MR 1491)
  - #101 - Fix install of header and loader.
- Loader
  - #91 - Fix a loader bug which prevented Layers from not implementing all XR
    functions
  - #95 - Guard config includes/defines (relates to #81, #92)
  - #97 - Remove a constant static std::vector, use a std::array instead.
- Layers
  - #84 - Fix Linux warning for apidump
- From OpenXR-Docs:
  - #26 - Proposal for unbounded space and spatial anchor extensions (vendor
    extensions)

### Internal issues

- General, Build, Other
  - Makefile cleanups (internal MR 1469, 1489)
  - Add release scripts (internal MR 1496)
- Registry
  - Reserve Oculus extension numbers (internal MR 1493)
  - Add Monado headless (vendor extension) (internal MR 1482)
- Loader
  - Remove unnecessary `#ifdef _WIN32` in loader. (internal MR 1487)

### New extensions

- `XR_MND_headless`
- `XR_MSFT_spatial_anchor`
- `XR_MSFT_unbounded_reference_space`

## OpenXR 1.0.0 release (29-July-2019)

Incorporates spec changes from OpenXR 1.0,
all public pull requests incorporated in the 0.90 series,
and additional fixes and improvements not previously published.

## Change log for OpenXR 0.90 provisional spec updates post-0.90.1

### GitHub Pull Requests

These had been integrated into the public repo incrementally.

- General, Build, Other
  - #40 - Update BUILDING.md with some Linux pre-requisites
  - #43 - Make manifest file more compatible
  - #44 - Remove pkg-config dependency from xlib backend
  - #46 - Support building with "embedded" Python
  - #48 - Install layers and pkg-config file on Linux
  - #66 - Install the layers libraries on Linux
  - #71 - Validation layer: fix logic error
- hello_xr
  - #49 - Fix hello_xr to properly use two call idiom
- Loader
  - #38 - Remove dead file-locking code
  - #51 - Idiomatic Linux active_runtime.json search logic
  - #55, #58, #68 - Purge std::map bracket operations that might do inadvertent
    insertions
  - #56 - Make `filesystem_util.cc` `#define UNICODE`-compatible
  - #57 - Make it possible to bypass macro that checks which `filesystem` to use
  - #60 - Fix build error with shlwapi function
  - #62 - Don't limit contents of `XristanceCreateInfo` next chain
  - #65 - Fix minor substr error
  - #69 - Simplify loader
  - #70, #76 - Make loader exception free
  - #72 - filesystem: fix theoretical bug on Linux
  - #73 - Loader proper UNICODE support
  - #75 - Clang tidy
  - #80 - Switchable exceptions
  - #82 - Add folder properties to all CMake targets.

## Change log for OpenXR 0.90.1 provisional spec update (8-May-2019)

No API changes, and only minimal consistency changes to the spec/registry.
Mostly an update for tooling, layers, loader, and sample code. Header version
has been bumped to 43, but no symbols that should have actually been in use have
changed.

### GitHub Pull Requests

These had been integrated into the public repo incrementally.

- General, Build, Other
  - #8, #11, #12 - Improve BUILDING and README
  - #9 - Make Vulkan SDK dependency optional
  - #17 - Add install target to CMake files
  - #17 - API dump layer, build: timespec extension fixes
  - #19 - build: fix CMAKE_PRESENTATION_BACKEND default on linux
  - #34 - list: Fix list test output
- validation layer
  - #18, #22, #23 - Fix build and execution
  - #24 - Fix crash and refactor
- hello_xr
  - #13 - Do not query GL context API version before creating context
  - #26 - Fix a warning
- Loader
  - #3 - Don't cross 32/64 registry silos
  - #14 - Initialize XrExtensionProperties array parameter for
    rt_xrEnumerateInstanceExtensionProperties
  - #20 - Fix Linux manifest file search
  - #30 - Add default implementations of API functions to dispatch chains
  - #32 - Avoid crash when evaluating layer disable environment vars
  - #35 - Add 'unknown' strings to loader's *ToString fallback functions
  - #36 - Allow null instance in xrGetInstanceProcAddr() for certain entry
    points
  - #39 - Default to static loader only on Windows

### Internal Issues

- General, Build, Other
  - Unify (for the most part) the OpenXR and Vulkan generator scripts. (internal
    MR 1166)
  - List instance extensions in the "list" test. (internal MR 1169)
  - Avoid dllexport for all apps compiled with `openxr_platform_defines.h`
    (internal MR 1187)
  - Don't offer `BUILD_SPECIFICATION` unless the spec makefile is there.
    (internal MR 1179)
  - Add simple input example to hello_xr. (internal MR 1178)
  - Add a clang-format script for ease of development.
- API Registry and Headers
  - Remove impossible and undocumented error codes. (internal MR 1185 and 1189)
  - Mark layers in `XrFrameEndInfo` as optional. (internal MR 1151, internal
    issue 899)
  - Remove unused windows types from `openxr_platform.h` (internal MR 1197)
  - Make `openxr_platform.h` include `openxr.h` on which it depends. (internal
    MR 1140, internal issue 918)
  - Remove unused, undocumented defines. (internal MR 1238, internal issue 1012)
- Loader
  - Fix loader regkey search logic so 64bit application loads 64bit regkey
    value. (internal MR 1180)
  - Modify loader to be friendly to UWP (Universal Windows Platform) build
    target. (internal MR 1198)

## OpenXR 0.90.0 - Initial public provisional release at GDC
