# Vulkan video negotiation and HEVC decoding

`make check SANITIZER=address,undefined` tests the production extension and
queue-family selection helpers. `make check CC=x86_64-w64-mingw32-gcc
EXE=.exe RUNNER=wine` runs the same cases on Windows/Wine. Tests cover
missing common extensions, H.264-only and HEVC-only devices, split/shared
families, unavailable queues, graphics-family exclusion and enabled codec
extensions. Shared codec coverage is preferred; split families retain the
existing H.264 path.

The context records the selected queue family's codec mask after successful
device creation, including core-created devices and device caching. An HEVC
extension on another family does not advertise HEVC on the selected family.
The existing H.264 backend registers only when that codec is selected.
The HEVC backend registers independently on a selected HEVC family. It offers
progressive Main 8-bit/NV12 and Main10/P010 sessions. Unsupported profiles, field sequences, missing
VPS metadata and sessions exceeding device/slot limits fall back at open.
The owner currently requests one spare DPB slot, so a 16-picture SPS needs
17 slots and falls back on devices limited to 16.

`make probe` builds an optional read-only Vulkan loader/physical-device probe;
run `./probe`. Build with `make probe.exe CC=x86_64-w64-mingw32-gcc EXE=.exe`
and run the executable natively on Windows for hardware evidence. It never
submits video work. Missing loaders/devices skip. A selected HEVC family is
queried separately for Main8 and Main10 profile capabilities.

Native Windows evidence: NVIDIA GeForce RTX 5090 exposes H.264 and HEVC on
family 3; both HEVC Main8/Main10 capability queries succeed, with maximum
8192x8192 coded extent and 16 DPB slots. Linux llvmpipe exposes neither codec
and selects no decode family. These capability queries are separate from the
decoded-picture evidence below.

`make check` also tests the production HEVC StdVideo parameter mapper against
ten Main/Main10 MP4 fixtures, including WPP, B-pictures, crop and repeated
headers. The mapper uses the actual VPS/SPS/PPS, translates raster scaling
lists to diagonal order, and resolves predicted SPS reference sets to explicit
sets. Its large caller-owned bundle belongs on the heap and must not move
after building, since its pointers refer into that bundle. Unsupported VPS
HRD, extensions or multiple layers remain unavailable to the mapper without
changing software decoding. Missing metadata, unrepresentable reference
deltas and unknown profiles/levels fail instead of fabricating parameters.

`make session-check` loads Vulkan dynamically and creates a dedicated HEVC
device on the selected family. For each fixture it checks profile/format
capabilities, creates a video session, allocates and binds its memory, creates
its parameter object and destroys all resources. It submits no video work
and allocates no picture images. Missing loaders/devices/HEVC families and
unsupported fixture profiles/limits/formats skip. Setup failures after a
supported capability result fail. Run the Windows executable natively for
hardware evidence; MinGW/Wine also builds and runs it. The RTX 5090 accepts all ten session/parameter cases,
including Main10/P010. This proves setup acceptance, not decoded pixels or
P010 renderer support. llvmpipe skips because it has no HEVC decode family.
Keep this loader/driver check separate from the sanitizer-based portable
tests: Mesa's loader path can retain process-lifetime allocations.

`make decode-check` compiles the real backend and exercises registration,
rh265 opt-in, MP4 packets, decode/drain and exported frame leases. GPU pixels
are read back only by the test oracle, after the ready timeline, then the
done lease is signaled after reading finishes. All 120 Main8/Main10 fixture frames
match the independent FFmpeg hashes, including B-frame order, WPP, crop and
repeated headers, on the RTX 5090. Main10 output is checked as normalized 10-bit samples.
All six native Vulkan H.264 regression streams also match software after
sharing the command-recording helper. Hardware decode
and output share existing nonblocking timeline/ring/graveyard ownership.
Output uses one GPU crop/copy into an independent leased NV12/P010 image; no CPU
readback or RGB staging is added to production HEVC playback.

Optional `HEVC_CONFORMANCE_MP4` and `HEVC_CONFORMANCE_NV12` point to matching
caller-supplied MP4 and FFmpeg NV12 display-order files. Both are required
when either is set; each file is limited to 64 MiB, and the even-dimension
Main8 reference is limited to 32 frames. The existing D3D11 sample's
`prepare_hevc_conformance.py` creates these files without downloading media.
The 24-picture LTRPSPS_A_Qualcomm_1 case also matches every exported Vulkan
sample. Actual File Browser presentation, movie UI/shader validation,
loss/reset coverage and other GPU vendors remain separate acceptance work. The device test skips without a selected HEVC
family; it fails when an advertised device cannot run the backend fixtures.

References:
- https://docs.vulkan.org/refpages/latest/refpages/source/VkQueueFamilyVideoPropertiesKHR.html
- https://docs.vulkan.org/features/latest/features/proposals/VK_KHR_video_decode_h265.html
- https://docs.vulkan.org/spec/latest/chapters/videocoding.html

`make import-check` extracts the production importer and runs its compiled
compute shader on a graphics/compute device without requiring video decode.
Twelve NV12/P010/NV12 switching cases cover odd visible dimensions, BT.601
and BT.709, and limited/full range against independent RGB equations.
The test checks ready/done timelines and busy-slot drops. Linux llvmpipe
and the RTX 5090 pass. MinGW/Wine builds the same test and skips if no
compatible device is available. CPU upload/readback belongs only to the oracle.

Production imports copy decoder planes to a device-local buffer and convert
to SDR RGBA8 on the GPU; they do not read video pixels back to the CPU.
Direct plane sampling remains a further zero-copy opportunity. The importer
reserves P010 capacity for format switching where storage-buffer limits permit;
larger NV12-only allocations remain usable and refuse oversized P010 frames.
P010 import does not imply HDR tone mapping or a 10-bit display path.

Builtin movie playback reuses this importer through
`import_movie_frame`. The converted texture is the filter-chain input,
including view dimensions and format, without an additional upload/copy.
NULL refreshes retain the current image; cached hardware sentinels redraw
it without treating them as CPU pointers. Resizes and software-frame changes
retire textures through the existing deferred lifetime path. Both whole-frame
and per-view history receive the actual source format when switching back.
The video-thread wrapper carries import metadata and leases in its existing
frame ring, pairing imports and clears with the refresh that consumes them.
It copies no video pixels between threads and adds no GPU copy. Staging
backpressure retains the caller's lease for retry; a replaced ring frame or
a busy/refused worker import abandons its accepted lease once, without
waiting. Clears survive replacement, and teardown releases staged/pending
leases. Unsupported drivers and software filters retain software fallback.
HDR and full hardware movie UI/shader validation remain acceptance work. The portable `movie_gpu_handoff/run.py`
checks production callback, retry, resize, clear and cached-redraw decisions.

The shipping `threaded_video` harness checks 130 leases through the real
wrapper, including staging backpressure, GPU-busy/refused imports, oversized
movie dimensions, clear and teardown. `HARNESS_MOVIE_ONLY=1` selects this lane
for focused sanitizer runs. Linux ASan/UBSan and TSan pass; the full plain
wrapper suite passes. TSan on this WSL host needs `setarch x86_64 -R` to avoid
its startup address-map conflict. These metadata/ownership tests do not
claim end-to-end hardware movie presentation on Windows or other devices.

`make -C samples/gfx/threaded_video check-movie-gpu` selects the real Vulkan
surface-import and movie-presentation lanes (a matching built frontend and
X display are required on Linux). The movie lane renders twenty direct and
threaded NV12/P010 cases through the stock filter chain, covering BT.601/709,
limited/full range, format switches and resize. Two viewport reads per case
check nine positions against the CPU reference and exercise cached redraws.
Returning to software pixels is checked in both modes. Linux llvmpipe passes
with Vulkan validation and ASan/UBSan; instrumented runs assert zero CPU
frame copies during the GPU phase. Host image writes and viewport readbacks
are test-only. This proves presentation with a synthetic GPU producer,
not hardware HEVC decoding, custom presets, A/V sync or seek/reset.
