# D3D11 upload regression

`python3 upload.py` extracts the shipping upload, texture load/update and
release routines. It checks idle/busy byte parity, independent packed 10-bit
channel and alpha expectations, padded and inferred pitches, native formats,
converted menu and RGB565 fallbacks, allocation refusal, cached growth/reuse,
map errors, image load/update parity, stream drops and teardown. A second
blocking Map is rejected by the API fixture. Set `CC` and `RUNNER` to run the
same test with MinGW and Wine.

`python3 gpu.py` uses the shipping upload routine on D3D11. The context proxy
forwards real GPU operations and can force only the initial staging Map busy.
The oracle checks every 10-bit channel value and all ignored X codes through
Load and linear SampleLevel, including padded source rows and cached reuse.
It shares the checked shader bytecode and source hash with the D3D12 sampling
oracle. Set `REQUIRE_D3D11=1` to require a compute-capable runtime. Native Linux
builds report that the D3D11 runtime is unavailable. GPU waits and readback are
test-only.

`texture_lend_per_write` opts D3D11 into renewable BGRA8/FP16 upload maps.
The context owner maps read/write with DO_NOT_WAIT; the producer receives the current
address and pitch only after readiness. Submission unmaps that slot and
copies its staging resource directly, without dxgi_copy. Separate staging
resources serve the two loans and the existing copied-upload fallback.
Read access permits surface preservation and replacement loads; the GPU
only reads these upload resources. Packed 10-bit and other converted source
layouts are not lent.

The wrapper withdraws a consumed descriptor and publishes fresh memory and
pitch before releasing readiness. A failed map or a pitch incompatible with
a packed producer revokes the loan to surface-owned storage. A CPU writer
retains a replaced texture until slot_end, including its still-mapped memory.
Software jobs fence CPU writes before publishing READY or FINISHED, including
partial failures, because mapped memory may be write-combined. Hardware jobs
keep their existing publication. No GPU flush, wait, lock, readback or
producer-thread context call is added.

`lend.py` forces different mapped addresses and pitches, poisons expired
memory, and checks cancellation, busy/error handling and zero CPU copies on
lent updates. `lend_gpu.py` checks raw BGRA8/FP16 bytes on a real device,
including producer-thread writes, repeated remaps and failed-job completion
preserving the last texture.
`samples/gfx/threaded_video/lend_refresh.py` exercises the shipping wrapper
publication with 2,000 worker handoffs and address reuse. The surface remap
fixture covers direct/threaded submission, busy/spare slots, revocation and
deferred unload. The GPU upload copy remains; no frame-time gain is claimed.
