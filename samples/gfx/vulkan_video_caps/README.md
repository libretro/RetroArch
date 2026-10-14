# Vulkan video codec negotiation

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
The HEVC parameter mapper and session tests below prepare the next backend
stage. Decode submission and renderer integration remain pending; this does
not register an HEVC backend.

`make probe` builds an optional read-only Vulkan loader/physical-device probe;
run `./probe`. Build with `make probe.exe CC=x86_64-w64-mingw32-gcc EXE=.exe`
and run the executable natively on Windows for hardware evidence. It never
submits video work. Missing loaders/devices skip. A selected HEVC family is
queried separately for Main8 and Main10 profile capabilities.

Native Windows evidence: NVIDIA GeForce RTX 5090 exposes H.264 and HEVC on
family 3; both HEVC Main8/Main10 capability queries succeed, with maximum
8192x8192 coded extent and 16 DPB slots. Linux llvmpipe exposes neither codec
and selects no decode family. These are capability queries; actual HEVC
decoding and import validation remain required.

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

References:
- https://docs.vulkan.org/refpages/latest/refpages/source/VkQueueFamilyVideoPropertiesKHR.html
- https://docs.vulkan.org/features/latest/features/proposals/VK_KHR_video_decode_h265.html
- https://docs.vulkan.org/spec/latest/chapters/videocoding.html
