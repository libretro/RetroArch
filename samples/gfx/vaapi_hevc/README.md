VA HEVC ownership and Vulkan conversion checks
============================================

`make clean check SANITIZER=address,undefined` runs the portable parameter,
submission, surface-pool, PRIME export/import and lifetime fault checks.
Linux needs libva/libdrm development headers. The native binding probe
does not decode or export a hardware surface.

Opaque VA leases also test stale keys, abandonment, take/restore, GPU
retirement and producer-only native returns. Linux runs 10000 handoffs
through the existing SPSC queue. `make tsan-check CC=gcc` uses a separate
race-detection binary; set RUNNER to the host ASLR wrapper when needed.

Completed frame leases may return to the decoder while their imported
images stay cached. Re-adoption requires identical worker-captured
DMA-BUF device/inode identities, layout and decoder slot, followed by
crop/color and ticket reconfiguration. FD numbers are never cache keys.
Native identity capture refuses legacy non-DMA-BUF anonymous inodes.

The renderer lease bridge claims the opaque carrier, adopts a matching
prepared import, configures the picture and submits conversion. Refusal
restores the pending lease; completion publishes its return for decoder
collection. Uncertain work retains ownership until exact device teardown.
`lease_test` covers this path with the typed Vulkan command fixture.

`resources_test` faults each cold sampler/pipeline/descriptor/command/fence
creation step for one and thirty-two slots. The GPU oracle uses this same
production resource owner, including its commands, fences and shader variants.

`cache_test` exercises bounded renderer ownership: warm reuse without native
creation, paused-tick returns, identity-change eviction, cold allocation
faults, refusal recovery and deferred exact-device teardown. Deferred caches
retain external leases until actual device destruction and retry failed return
publication. Creation covers capacities one and thirty-two; imported-image
cache scenarios use one slot. Native decoder factory integration remains open.

`renderer_test` executes the shipping Vulkan adapter with typed native
operations, including loader/prerequisite refusal, crop validation, queue
serialization and uncertain destination retention. Configure detects optional
Linux VA headers; --disable-vaapi omits these frontend objects. Runtime VA
libraries remain dynamically loaded.

`decoder_test` sends parsed HEVC pictures through the production adapter and
native-pool fixture, then exports completed pictures as opaque carriers. It
checks pending output, crop/color transport, export failure and detach while a
renderer still claims a frame. Failed native ticket returns remain retryable.
The adapter borrows its native pool; the factory must keep that pool, session
and runtime alive through deferred retirement and drive collection on owner
ticks. Native device acceptance remains open.

`owner_test` composes the cold runtime, native pool, packet workspace and
decoder adapter. It faults allocations, native symbols/libraries and adapter
setup, retains exports across detach, polls pending work once per pass and
retries termination without further decoder calls. Cleanup belongs on the
existing cold retirement executor; the owner is not itself an executor or a
registered decoder factory. Native device acceptance remains separate.

`backend_test` checks the shipping Vulkan admission helper and the native-owner
factory with typed owner operations. The process-lifetime offer snapshots atomic
DRM identity/epoch fields; reinit does not expose a renderer pointer to decoder
setup. Native Vulkan HEVC keeps priority. Linux PRIME fallback resolves the
actual render node through libdrm and verifies its FD identity in the runtime.
Eight sessions including retirement bound seek storms; each cold open creates
its retirement worker before native startup. Close and failed setup publish
ownership to that worker without joining. Outstanding exports and failed native
termination retain their owner for retry. The existing eventcount timeout runs
only on that cold executor. Threadless/non-lock-free builds refuse admission.

The backend fixture faults path lookup, allocation, eventcount and thread
startup, holds retirement ownership, verifies operation forwarding and pauses
native setup across renderer replacement. It tests publisher notification
lifetime and epoch exhaustion. `make tsan-check` additionally executes its
worker and setup/withdrawal lanes. Native factory registration does not prove
actual VA decode, DRM modifier import pixels, seeks or shutdown acceptance.
The shipping factory borrows the live decoder on its owner thread and selects
NORMAL when advertised (or when the slice-mode attribute is absent), with BASE
retained for BASE-only drivers. Parser/data workspaces are allocated at cold
setup. Detach clears the parser borrow before source destruction; deferred
retirement polls native transactions and exported leases without querying it.
The long-slice mapper is exercised separately by `long_slice_test` and
the parsed Main8/Main10 I/P/B sequence fixtures. It maps coded short/long-term
reference order and list modifications to picture-buffer indices, effective
weights to VA deltas/offsets, filtering flags and escaped-header accounting.
It preserves output on refusal and performs no native calls or allocation.
The separate NORMAL packet helper packs escaped NALs without start codes,
128-byte tail padding and coded WPP offsets in a preallocated owner workspace.
All headers and subset extents pass before payload copy or native submission.
Submission owns up to five buffers, including an optional subset array; failure
and destruction retries retain the existing transaction ownership rules.
`long_packet_test` checks synthetic Main8/Main10 I/P/B WPP headers and refusal
without payload writes. The parsed sequence fixture also submits NORMAL layouts
through typed callbacks. Both pool modes exercise parsed sequence backpressure
and incarnation reuse. The owner fixture decodes a parsed Main8/Main10 picture
through typed VA calls, holds exports through detach/source destruction and
retries termination. These tests do not establish native NORMAL decode/export/
import pixel accuracy.

`make gpu-check SANITIZER=address,undefined` additionally needs the Vulkan
loader, development library, validation layer and a device supporting
Vulkan 1.1, external-memory FD and foreign queue ownership. Mesa lavapipe
is sufficient. Missing capabilities fail this explicit test; they are
not reported as a successful or skipped hardware decode check.

The GPU oracle executes the production VA conversion submission helper
and existing YUV shaders on exportable OPAQUE_FD Vulkan plane images.
It checks cropped NV12/P010, full/limited range, BT.601/709 SDR and
BT.2020 PQ/HLG, RGBA8/linear FP16 output, three destination layouts,
repeated foreign ownership transfers, fences and validation errors.
Waits, staging uploads and pixel readback belong only to this oracle.

Ticket metadata is synthetic. No VA display, hardware decoder, DMA-BUF
FD import or DRM modifier image is created by the GPU oracle. Actual
VA decode/export/import pixels and native seek/shutdown acceptance
remain separate device gates.

`make native-decode-check VAAPI_RENDER_NODE=/dev/dri/renderD128` is an explicit
native decode/export CRC gate on Linux. Supply the actual render node. Missing
nodes or refused hardware admission return a nonzero unavailable result; the
oracle never accepts software fallback. It runs the eight existing Main8/Main10
full/limited I/P/B sequences twice, checks every display-order Y/U/V CRC against
the FFmpeg fixture oracle, keeps the last export claimed across source close,
reads that retained surface again, and requires retirement to restore the FD
baseline. The selected NORMAL/BASE mask is reported rather than inferred.

This executable alone uses VAImage CPU transfer, mapping and bounded polling
delays. Production decoding/rendering does not gain readback or waits. The host
`native_pixels_test` checks independent planar CRCs, padding/crop, P010 low bits,
plane storage order and invalid image extents. Compiling the native executable
or passing this host helper is not native-device acceptance. DRM-modifier Vulkan
import pixels, actual WPP subset decoding, simultaneous imports, player seeks
and native shutdown remain separate acceptance work.
