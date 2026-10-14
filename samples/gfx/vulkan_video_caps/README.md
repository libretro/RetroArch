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
HEVC session/parameter creation, submission and renderer integration remain
the next implementation stage; this change does not register an HEVC backend.

`make probe` builds an optional read-only Vulkan loader/physical-device probe;
run `./probe`. Build with `make probe.exe CC=x86_64-w64-mingw32-gcc EXE=.exe`
and run the executable natively on Windows for hardware evidence. It never
submits video work. Missing loaders/devices skip. A selected HEVC family is
queried separately for Main8 and Main10 profile capabilities.

Native Windows evidence: NVIDIA GeForce RTX 5090 exposes H.264 and HEVC on
family 3; both HEVC Main8/Main10 capability queries succeed, with maximum
8192x8192 coded extent and 16 DPB slots. Linux llvmpipe exposes neither codec
and selects no decode family. These are capability queries; actual HEVC
session, format, decoding and import validation remain required.

References:
- https://docs.vulkan.org/refpages/latest/refpages/source/VkQueueFamilyVideoPropertiesKHR.html
- https://docs.vulkan.org/features/latest/features/proposals/VK_KHR_video_decode_h265.html
