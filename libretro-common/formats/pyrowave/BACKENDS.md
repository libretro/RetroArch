# Compute backend audit

This records the implemented Linux/Windows paths and the remaining platform
ports. Host code and reusable codec code are C89. The
independently authored compute programs use the required shader languages.

| Backend | Applicable capability | Current evidence and remaining work |
| --- | --- | --- |
| Vulkan | Compute queue, FP32, storage and uniform buffers | Full GPU DWT and packet codec run on native RTX 5090 with validation, including 1080p and external decoder interoperability. C89 asynchronous encoder and receiver adapters pass MSVC/MinGW native GPU tests and Linux llvmpipe ASan/UBSan tests, including code-value/scRGB conversion, source/display fence retirement, held compressed packets, cache fallbacks and 1080p. Explicit synchronization validation passes; removing buffer barrier access masks produces SYNC-HAZARD-READ-AFTER-WRITE. Actual sender and SDR receiver integration passes native Windows direct/threaded pixels, shader history, lifecycle, reconnect and recorded-decode cancellation with synchronization validation. Retained receiver submissions now use a cold queue marker and primary reset; direct/threaded Windows and Linux recovery tests pass, while premature marker destruction is rejected by native validation. PQ/BT.2020 to SDR receiver presentation passes the native pixel oracle. WASAPI audio-clock frame selection passes native direct/threaded validation. Native HDR output remains a separate extension. |
| D3D12 | Compute pipeline, structured SRV/UAV buffers | Full GPU DWT and packet codec run natively with the debug layer, including 1080p and external decoder interoperability. C89 asynchronous encoder and receiver adapters pass native MSVC/MinGW RTX 5090 tests, including signed scRGB, source/display retirement, held compressed packets, unsubmitted discard and 1080p. MSVC GPU-based validation passes. Device-removal sentinel injection retains pending resources; removing that guard fails the retirement test. Linux cross-compilation and old Windows SDK native execution pass. Actual sender and SDR receiver application integration passes direct/threaded Windows pixel, shader-history, lifecycle and reconnect checks; GPU-based validation reports zero errors. Receiver close/signal failures retire through a queue marker and preallocated replacement list; native direct/threaded logical-device removal and rebuild pass with zero validation errors. PQ/BT.2020 to SDR receiver presentation passes the native pixel oracle. WASAPI audio-clock frame selection passes native direct/threaded validation. Native HDR output remains a separate extension. |
| Desktop GL | OpenGL 4.3 compute and SSBOs; asynchronous adapters also require ARB_buffer_storage or GL 4.4 | DWT and coefficient packet encoding/decoding run natively, with 1080p and external decoder interoperability. Color conversion passes 1,536 native code-value/scRGB/SDR presentation configurations. The asynchronous adapters pass native ownership, backpressure, context-loss and signed HDR roundtrip tests. The receiver has native 130x98 direct-upload, RGB oracle, texture retirement, EOS and malformed-body checks. Decode and display fences protect lent RGBA32F textures. Eligibility checks actual context capabilities and storage limits, including a compatible gl2 context when capable, rather than driver name alone. Actual sender and builtin SDR receiver integration passes Windows and Linux direct/threaded application checks, including pixel comparison, PCM drain and reconnect. PQ/BT.2020 to SDR receiver presentation passes the native pixel oracle. WASAPI audio-clock frame selection passes native direct/threaded validation. Native HDR output remains a separate extension. |
| GLES | ES 3.1 compute and SSBOs; asynchronous encoder/receiver also require EXT_buffer_storage | ES 3.10 shader translation validates. Both directions compile as strict C89 against GLES 3 headers and resolve compute functions dynamically. Native GLES execution and renderer integration remain. ES 3.0 alone is insufficient. |
| D3D11 | Feature level 11 compute with Shader Model 5 | Applicable; the DWT already compiles as cs_5_0. The existing renderer uses compute for planar frames. Native execution and integration remain. |
| Metal | Metal 2.1 serial compute and tracked storage resources | Required by the user. All three independent kernels translate to MSL with explicit shared binding numbers. C89 encoder/decoder resource adapters and a C89 Objective-C-runtime bridge are implemented. Portable MSVC/MinGW/Linux tests check bindings, shared/managed visibility and asynchronous ownership; forced native-branch compilation passes. Renderer hooks and native Apple validation remain. The Apple GPU oracle and Metal compiler workflow are added but have not executed here; MSL translation is not a native compilation or execution pass. |
| Switch/deko3d | Public deko3d compute dispatch interface | A possible platform adapter, not an existing connection to this codec. The new deko3d renderer and hardware-frame ring do not yet supply PyroWave capture/compute hooks. Toolchain, resource ownership and native validation remain. |
| Switch 2 / libounce Vulkan | Vulkan compute capability must be queried on the device | The renderer has a Vulkan context. This makes the Vulkan adapter applicable in principle; neither device capability nor native execution has been validated here. |
| D3D10 / older D3D | Optional downlevel compute is exposed through D3D11, with restrictions | Not equivalent to the Shader Model 5 baseline. A future downlevel adapter must explicitly satisfy the complete kernel requirements. No raw GPU-pixel readback fallback is implied. |
| Legacy GL / GLES | Contexts without the required compute/storage facilities | Cannot run this compute pipeline. Capability failure must be explicit in frontend controls. CPU input can use an independently selected supported compute device; hardware frames require a valid GPU resource loan. |

The normal GPU path must keep pixels resident, transfer only compressed
data asynchronously, and retain each resource until its completion and
consumer ownership are both retired. The raw GPU readback in the offline
oracle executables is solely for checking actual numerical output.

Primary platform references:

- [OpenGL 4.3 specification](https://registry.khronos.org/OpenGL/specs/gl/glspec43.core.pdf)
- [OpenGL ES 3.1 specification](https://registry.khronos.org/OpenGL/specs/es/3.1/es_spec_3.1.pdf)
- [Microsoft compute shader feature levels](https://learn.microsoft.com/en-us/windows/win32/direct3d11/overviews-direct3d-11-devices-downlevel-compute-shaders)
- [D3D12 resource states](https://learn.microsoft.com/en-us/windows/win32/api/d3d12/ne-d3d12-d3d12_resource_states)
- [Metal compute command encoder](https://developer.apple.com/documentation/metal/mtlcomputecommandencoder)
- [deko3d public API primer](https://github.com/devkitPro/deko3d/blob/master/Primer.md)


The Linux/Windows receiver currently supports native audio presentation
through ALSA and WASAPI. Other audio backends may use explicit null-audio
playback; they do not silently claim an audio presentation clock. The
receiver presents PQ/BT.2020 streams through the explicit SDR709 conversion
path. Native HDR swapchain output is outside this initial receiver path.
Metal frontend routing, CoreAudio presentation and native Apple validation
are the Mac handover work; portable Metal adapters and kernels are included.
