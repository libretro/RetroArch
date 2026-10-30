# Runtime snapshots

The optional `RETRO_ENVIRONMENT_SET_SNAPSHOT_INTERFACE` preserves a running
core's complete state for run-ahead and preemptive frames, including enhanced
rendering that ordinary savestates may omit. The command is experimental and
requires libretro API review. Its contract is in `libretro.h`.

Registration does not enable prediction. Unadapted cores keep serialization;
an incompatible interface never silently falls back to incomplete state.
Snapshots belong to one core instance, so they use same-instance run-ahead.
Ordinary disk savestates retain their format.

## Core integration

- `create` returns an owned empty handle; release it once before core unload.
  Copying its address only borrows it.
- `save` replaces a capture only on success. `load` is repeatable and does not
  consume it. Preserve authoritative state and retained rendering information;
  reconstructible caches may be invalidated.
- Validate and reserve before mutation. `UNAVAILABLE` preserves live behavior;
  failure after mutation starts is `FATAL`.
- Execute callbacks on the core thread with its graphics context current.
  `supported` and `valid` inspect metadata; support does not reserve storage.

Execution is ordinary, canonical (retained frontend advancement), or
speculative (discardable prediction/replay). Cores keep their own prediction
policy in ordinary execution; otherwise the frontend owns prediction. Hidden
frames preserve future rendering dependencies. Defer irreversible effects.

## Frontend behavior

Run-ahead and preemptive frames share storage but retain separate schedulers.
Storage is acquired only while prediction needs it. Allocation, resource
sharing and synchronization remain core-owned.

Replay retains its oldest anchor and input until it catches up. Rejected
restoration cannot resume a speculative timeline. Fatal failures block
execution, persistence and memory consumers while retaining recovery ownership.
A failed ordinary load or a void reset does not prove recovery. Resume only
after a confirmed restore/load or content reload; release and shutdown remain
available while blocked.

Reset, media, controller and external renderer changes invalidate history.
The displayed image must survive restoration of core resources.

Test actual consumers after divergent work, independent owners, repeated
loads, failure after success, invalidation and recovery. `samples/runahead`
tests frontend storage/scheduling; cores must test their renderers separately.

On Windows, ANGLE D3D11 uses the existing single-context handoff between core
and video threads. Threaded video remains available; no ANGLE patch is needed.
