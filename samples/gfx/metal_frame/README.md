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
signal and discard cases. Exit 77 means shared events are unavailable.
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
