# Retained Metal frame leases

`python3 samples/gfx/metal_frame/run.py` checks the C89 geometry contract.
It also builds with MinGW (`CC` and `RUNNER` select compiler and executor).
`--mock` requires clang, GNUstep Foundation and BlocksRuntime on Linux.
It extracts the shipping lease, packed importer and carrier declaration,
checking four formats through GPU completion, asynchronous discard, GPU error
and encoder refusal, then packed copies, cropping, allocation failure, color
refusal and persistent texture reuse.
`OBJCFLAGS` can enable sanitizers; GNUstep owns process-lifetime globals.
`--native` builds with Apple frameworks and runs actual Metal wait, copy,
signal and discard cases. It also extracts the shipping packed importer and
private-texture allocator into a thin driver/context harness with real Metal
resources. Five two-frame cases check same-image reuse, width and height
replacement, BGRA-to-RGBA channel order and FP16 values above reference white.
Both imports and readbacks are encoded before readiness advances; the public
handle is released before GPU execution. Exact cropped pixels verify that
queued commands retain the images they reference. The harness supplies an
active linear-pipeline gate; it does not test HDR display compositing.
Exit 77 means shared events are unavailable.
Only the native fixture performs CPU waits and GPU readback.

The internal factory borrows source objects and exports one retained lease.
Encoder refusal leaves no commands using the source planes; a harmless GPU
readiness wait may remain in the command buffer. Every created lease must be submitted successfully exactly once or abandoned
exactly once. Failed validation leaves ownership with the caller. Accepted
submission owns the lease until command-buffer completion. Its callback must
finish encoding before returning; the caller commits the buffer. GPU failure
and discard defer release until the producer's ready event advances. Producers
must advance readiness after their last write, including on failure. Each slot
owns its done event/value; advancing another lease past that value early is
invalid. Completion does not access driver state. Immutable metadata and plane
views belong to the lease; callback views may not escape the encoding call.

The packed importer copies cropped native sRGB BGRA/RGBA8 or linear BT.709
FP16 into a persistent private texture. It requires explicit full-range RGB
metadata and an equal output format (FP16 also requires an active HDR linear
pipeline), and refuses mismatched or unknown color,
mipmapped output, planar sources and active CPU loans. Size/format changes
prepare a replacement image before encoding. Success preserves the public
handle while queued GPU readers retain the old image. Allocation or encoder
failure leaves the handle and producer ownership intact. GPU copying preserves
FP16 values without a transfer curve or paper-white scaling. The command buffer
must retain its referenced resources; destination storage must not alias source.

The Metal driver has an import callback but still advertises zero import formats. CoreVideo plane
creation, conversion, importer integration and native HDR acceptance remain
separate gates. This change makes no copy-count or performance claim.

`metal_frame_create_cv` accepts a caller-owned reusable CVMetalTextureCache
and an NV12/P010 CVPixelBuffer (limited or full range). It creates Metal plane
views and retains the pixel buffer and both CVMetalTexture wrappers in the
lease. Source planes/backing must be empty; crop, color metadata, events and
listener come from the caller. Range comes from the pixel format; conflicting
explicit range is refused. Bottom-left views and textures from another device are
refused. No CPU mapping, cache flush, conversion or readiness wait is added.
The existing producer readiness and per-slot done contract still applies.

The mock adds 32 CoreVideo ownership/refusal cases, including partial view
creation failures and GPU error before readiness. Native coverage adds eight real IOSurface-backed NV12/P010
copy/discard cases. The caller releases its pixel buffer and cache references
before readiness advances. Test-only CPU mapping initializes spatial plane
patterns; GPU copies and readback compare the active plane bytes exactly.
This handoff is not a VideoToolbox decoder or a planar RGB converter.

`metal_frame_sdr_pipeline_create` compiles an owned compute pipeline once,
outside frame submission (release its CFTypeRef when finished).
`metal_frame_convert_sdr` consumes a lease on successful encoding into an
independent shader-writable RGBA8Unorm texture. It requires BT.709 primaries
and matrix, explicit full/limited range, and BT.709 or sRGB transfer metadata.
BT.709 inverse transfer and sRGB encoding are applied; sRGB input retains its
transfer. Chroma uses nearest 2x2 reconstruction. P010 high-ten-bit samples
use their exact ten-bit luma/chroma ranges. HDR and unknown metadata refuse.
The command buffer must retain resource references. Refusal keeps the lease
with the caller, following the same harmless-wait rule as packed copying.
The helper adds no CPU pixel mapping, upload, readback or completion wait.

The mock adds 32 compute-binding, refusal and retirement cases. Native tests
convert eight spatially varying cropped CoreVideo frames (four pixel formats
with two transfer functions). Independent reference pixels allow one UNORM8
code of GPU rounding error. The C89 fixture also verifies the 64-byte constant
layout and exact black/white/chroma-center levels on Linux and Windows.
The driver importer now accepts these explicit SDR planar leases into a
persistent RGBA8 texture. Its context creates the conversion pipeline during
setup; pipeline failure leaves planar imports refused. Allocation or encoder
refusal preserves the prior public handle/image and caller lease. Compatible
images are reused, and format/size replacements publish only after encoding.
The mock adds 48 importer replacement/refusal/retirement cases. The native
pixel fixture runs its eight conversions through the shipping driver callback.
`import_formats` remains zero: VideoToolbox producer/session integration,
playback acceptance, HDR conversion and display acceptance remain gated.

CoreVideo leases also reconcile image-buffer primaries, transfer and matrix
attachments with the source metadata. Supported attachment names map BT.709
and BT.2020 primaries/matrix plus BT.709, sRGB, PQ and HLG transfer functions.
Missing attachments preserve the source, including missing fields; they do
not imply BT.709. Attached values fill absent or unspecified (code point 2)
fields. Explicit conflicts, unknown names and wrong attachment types refuse
before plane construction. Other source mastering/content-light metadata is
preserved. This transports HDR tags without enabling HDR conversion.
The mock adds 60 attachment and consistency cases across four pixel formats.
Native CoreVideo fixtures supply real SDR and BT.2020/PQ attachments; the
SDR importer pixel cases recover their color fields from those attachments.

Native SDR acceptance runs both the original eight serial 7x5 pixel cases
and an additional eight-frame batch. The batch encodes all leases and
readbacks before committing, delays producer readiness until after submission,
and checks five dimension replacements plus two image reuses under one stable
public handle. The handle and setup-owned pipeline are released before GPU
execution. Each readback is compared with its own cropped reference after
bounded test-only completion, including reads of replaced images. This adds
no production frame-path waits or readbacks.
