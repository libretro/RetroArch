# PyroWave Metal handover

The reusable Metal compute encoder/decoder and Objective-C runtime host bridge
are implemented in C89. The independent HLSL kernels are translated to MSL 2.1
by `tools/pyrowave_shaders.py`. Portable ownership/ABI tests pass on Windows and
Linux. Native Metal compilation and execution have not run in this workspace.
A Mac was explicitly unavailable; these are the next checks on the Mac handover.

From the repository root, with Xcode selected and its Metal tools available:

```sh
brew install glslang spirv-cross spirv-tools
make -C samples/gfx/pyrowave_metal clean check native
make -C samples/gfx/pyrowave_metal native-check
```

The native executable must return 0 and report numerical encode/decode and
malformed-body checks passing on a real Metal device. Return 77 means no device
was available; it is not a GPU validation pass. Repeat with Metal API validation
and shader validation enabled in Xcode. Preserve compiler output, device/OS
identity and the native executable's full output. The `pyrowave-metal` job in
`.github/workflows/MacOS.yml` performs the same build and stores its GPU log;
a green job with the explicit no-device warning does not establish execution.

Inspect these shipping files when fixing native issues:

- `record/record_pyrowave_metal.h`: public resource/ownership contract.
- `record/record_pyrowave_metal.c`: bounded encoder/decoder queues and GPU loans.
- `record/record_pyrowave_metal_runtime.c`: C89 Objective-C runtime bridge.
- `record/record_pyrowave_metal_internal.h`: shared adapter/runtime definitions.
- `samples/gfx/pyrowave_metal/native_gpu_test.m`: offline Apple GPU oracle.
- `libretro-common/formats/pyrowave/shaders/`: independently authored kernels.

The Apple frontend integration is the remaining port. Add a Metal source and
player adapter using native texture/command-buffer loans; connect it to the
existing renderer-owner cold jobs and frame snapshots. Keep pixels resident;
only compressed packets cross to the network owner. Retain native resources
through both GPU completion and consumer release. Use the existing Metal
adapter's shared/managed visibility rules, and validate on Apple Silicon and,
where available, an Intel/managed-memory Mac. Do not add waits or allocation to
frame or audio callbacks.

Add a CoreAudio `audio_presentation_position_t` implementation before enabling
clocked Apple receiver audio. Report accepted and actually presented frames,
queue-loss epochs, and explicit silence provenance. Reuse the existing receiver
worker handshake, resampling provenance and due-frame scheduler. Do not infer
played frames from offered writes or from elapsed wall time. First validate
native audio drain, then direct/threaded A/V selection, reset, EOF, peer abort,
source/decoder failure and unload under real Metal validation.

The desktop Linux/Windows frontend currently supports Vulkan, D3D12 and desktop
OpenGL compute, with ALSA/WASAPI presentation clocks. RPWS is this project's
explicit framing protocol; raw codec interoperability does not imply support
for the upstream Pyrolight/Polaris application protocol. PQ/BT.2020 receiver
streams use explicit SDR709 presentation; native HDR swapchain output is a
separate extension. Preserve the existing FP16 frontend ABI when adding it.

Calling Metal from C has no inherent dispatch-speed advantage over Objective-C:
both ultimately use the same runtime calls. The C bridge keeps this adapter
C89-compatible. A complete Metal driver could be written this way, but would
need manual lifetime management, correctly typed ABI calls and Cocoa/block
bridges; it is not a performance reason to rewrite the existing Objective-C
driver. Keep that driver in its existing language during this port.
