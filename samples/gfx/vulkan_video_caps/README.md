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

`make render-check` compiles the real HEVC backend together with the
production Vulkan importer and its compiled compute shader. Each decoded
picture's raw NV12/P010 samples match independent FFmpeg hashes, and
its converted RGBA shader input matches standard CPU color equations within
two 8-bit steps. Imports are queued before the oracle waits for raw readback;
GPU submission uses the decoder's actual ready/done timelines. Successful
imports are not abandoned or signalled a second time by the CPU. Test-only
readbacks supply the oracle, never the renderer input.

On the RTX 5090, all 120 Main/Main10 fixture frames and the optional 24-frame
Main8 long-term-reference case pass. The six H.264 regressions also pass.
The original `decode-check` retains its separate held-lease/backpressure and
close-with-outstanding-output checks; render-check consumes output normally.
Linux/Wine llvmpipe skips render-check without a selected HEVC family.
This validates actual hardware decode through RGBA shader-input creation,
not the movie UI, custom shader presets, A/V sync or seek/reset on Windows.


### Sampled Vulkan video imports

The decoder prefers mutable NV12/P010 output images with sampleable Y/UV
views. The importer reads those planes directly in its compute shader,
removing the full-frame image-to-buffer copy. The 64-byte color header uses push constants; sampled imports need no
staging buffer or header transfer command. The decoder's DPB-to-output copy and RGBA conversion remain.
Allocation/view failure or an unavailable sampled pipeline retains buffer
conversion. Ready/done timelines, abandonment and deferred close cover the
image and its views together; thumbnail and movie descriptors forward both
views through the existing threaded handoff.

`import-check` compares 48 NV12/P010 color/format cases: ordinary buffer
conversion, direct image sampling, and a forced missing-pipeline fallback.
A command hook checks zero importer image-to-buffer copies when sampling
and one in either fallback. Busy-slot checks leave the done timeline unchanged.
RTX 5090 and llvmpipe pass; llvmpipe also passes synchronization validation.

Set `VULKAN_TEST_REQUIRE_SAMPLED=1` for native `hevc_render_test` to require
plane views on every decoded picture. All 120 Main/Main10 fixture pictures
pass through sampling and RGBA comparison on the RTX 5090. Set
`VULKAN_TEST_REJECT_SAMPLED=1` or `=2` instead to inject sampled-image or
second-plane-view creation failures and verify the copied fallback and cleanup;
both complete the same reference corpus. These variables are test-only.
The six native H.264 decode regressions also pass. This is not full UI/A/V,
HDR, or wider-device acceptance, and does not add Vulkan VP9 or AV1 decoding.


For the shipping threaded-video harness, `HARNESS_SAMPLED_IMPORT=1` supplies
sampled plane views from its synthetic producer. With `HARNESS_MOVIE_GPU_ONLY=1`,
llvmpipe passes the surface lane and 20 direct/threaded movie cases under Vulkan
validation, covering NV12/P010, resize, cached redraw and software transition.
Host writes and viewport readbacks are test-only. The separate
`HARNESS_MOVIE_ONLY=1` lane checks both view handles and slice/crop metadata
through 130 leases, including clear, backpressure and teardown.


Sampled imports allocate no staging buffers. Copied fallback allocates only
the selected idle slot, sizes storage for its actual NV12/P010 layout and
retains capacity across later format switches. No buffer or descriptor in a
busy slot is replaced. Failed allocation or growth preserves the old buffer
and done timeline, and releases temporary allocations.

The 48-case import oracle starts with sampled NV12/P010 under a simulated
zero storage-buffer limit, tests copied fallback and format switches, then
returns to sampling with retained fallback buffers. Command hooks require
one push-constant update and zero buffer updates/copies for sampled imports;
copied imports require one buffer update/copy and no push-constant update.
It checks zero sampled staging allocations, no steady-state reallocations,
three injected failures (create/allocate/bind), retry, and a forced busy slot.
Native HEVC sampling tests require both staging buffers to remain absent.

### VP9 hardware decoding

The Vulkan backend registers with the existing VP9 hardware session when
the selected decode family supports `VK_KHR_video_decode_vp9` and its
`videoDecodeVP9` feature. Both frontend device-creation paths enable the
feature; an explicit core request to disable it is respected. Builds without
`HAVE_RVP9` leave VP9 disabled. The bundled headers carry the VP9 definitions
from Khronos Vulkan-Headers v1.4.328, without changing the rest of the SDK.

Profile 0/8-bit and Profile 2/10-bit use the existing NV12/P010 output leases,
sampled plane views and copied-import fallback. Named references that alias
one slot are bound once. Effective loop-filter/segmentation state, compressed
header/tile offsets, and per-slot range/matrix metadata are preserved.
The existing WebM and media-core hardware selection uses this registration.
Decode references stay GPU-resident; the DPB-to-output copy and GPU RGBA
conversion remain. Production decode/export adds no CPU readback or wait.

`make check` covers picture translation, malformed input and codec/family
selection on Linux and MinGW/Wine. Set `VP9_TEST_IVF` and `VP9_TEST_RAW` for
`hevc_decode_test` or `hevc_render_test` to require the optional VP9 hardware
fixture after the existing HEVC/H.264 regressions. The IVF contains individual
coded VP9 frames; the independent raw oracle is NV12 for 8-bit or P010LE for
10-bit, with no more than 1024 displayed frames. For example:

```sh
ffmpeg -f lavfi -i testsrc2=size=192x128:rate=12 -frames:v 12 \
  -c:v libvpx-vp9 -pix_fmt yuv420p10le -profile:v 2 \
  -colorspace bt709 -color_range pc -f ivf vp9.ivf
ffmpeg -i vp9.ivf -pix_fmt p010le -f rawvideo vp9.raw
VP9_TEST_IVF=vp9.ivf VP9_TEST_RAW=vp9.raw \
  VULKAN_TEST_REQUIRE_SAMPLED=1 ./hevc_render_test
```

The RTX 5090 passes four 12-frame 8/10-bit BT.601-limited/BT.709-full
fixtures through both exported-plane and sampled-to-RGBA comparisons.
The oracle checks show-existing replay without another decode, output-ring
backpressure, retained leases read after close, and color metadata. Injected
sampled-image and second-view rejection also pass the 10-bit/full-range corpus.
Oracle readbacks and waits are test-only.

Current limits: fixed geometry/profile, even dimensions, 4:2:0, and the
supported BT.601/BT.709 color paths. Other configurations are refused before
decoding. Wider hardware, actual File Browser/movie A/V, HDR, resolution
changes and AV1 remain separate acceptance work.
