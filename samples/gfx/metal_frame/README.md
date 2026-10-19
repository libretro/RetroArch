# Retained Metal frame leases

`python3 samples/gfx/metal_frame/run.py` checks the C89 geometry contract.
It also builds with MinGW (`CC` and `RUNNER` select compiler and executor).
`--mock` requires clang, GNUstep Foundation and BlocksRuntime on Linux.
It extracts the shipping implementation and carrier declaration, checking
four formats through GPU completion, asynchronous discard and GPU error.
`OBJCFLAGS` can enable sanitizers; GNUstep owns process-lifetime globals.
`--native` builds with Apple frameworks and runs actual Metal wait, copy,
signal and discard cases. Exit 77 means shared events are unavailable.
Only the native fixture performs CPU waits and GPU readback.

The internal factory borrows source objects and exports one retained lease.
Every created lease must be submitted successfully exactly once or abandoned
exactly once. Failed validation leaves ownership with the caller. Accepted
submission owns the lease until command-buffer completion. Its callback must
finish encoding before returning; the caller commits the buffer. GPU failure
and discard defer release until the producer's ready event advances. Producers
must advance readiness after their last write, including on failure. Each slot
owns its done event/value; advancing another lease past that value early is
invalid. Completion does not access driver state. Immutable metadata and plane
views belong to the lease; callback views may not escape the encoding call.

This is the ownership and synchronization foundation. The Metal driver still
advertises zero import formats and has no import callback. CoreVideo plane
creation, conversion, importer integration and native HDR acceptance remain
separate gates. This change makes no copy-count or performance claim.
