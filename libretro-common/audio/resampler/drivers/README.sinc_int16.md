# Integer (s16) SINC resampler — integration notes

This adds a deterministic, integer-only counterpart to the float `sinc`
resampler (`drivers/sinc_resampler.c`).  It exists to:

1. **Determinism.** The float driver is built with `-ffast-math`; its output
   is not bit-reproducible across compilers/archs/FMA settings. This driver is
   integer-only and bit-identical everywhere — relevant for netplay and rewind.
2. **Round-trip removal.** The libretro audio callback is int16-only, so
   RetroArch converts s16→float for the float resampler on *every* core, every
   frame, and back to s16 for s16 output devices. The int16 driver skips both
   conversions.
3. **No FPU dependency** — helps on PS2 / 3DS / older ARM.

## Files

- `drivers/sinc_resampler_int16.{c,h}` — the driver (scalar, C89/MSVC-clean).
- `test/test_sinc_int16.c`, `test/sinc_ref_float.{c,h}` — bit-exactness harness.

Build & run the harness standalone (the public header now lives in
`libretro-common/include/audio/`, so point `-I` there):

    cc -O2 -std=c89 -pedantic -Wall -Wextra -Ilibretro-common/include \
       libretro-common/audio/resampler/test/test_sinc_int16.c \
       libretro-common/audio/resampler/drivers/sinc_resampler_int16.c \
       libretro-common/audio/resampler/test/sinc_ref_float.c \
       -lm -o /tmp/test_sinc_int16 && /tmp/test_sinc_int16

## Measured vs the float driver (int16 endpoint)

Across a rate × quality × signal matrix, the integer driver is **bit-transparent**
to the float driver:

- worst case **1 LSB**, on **< 0.5 %** of output samples (scales with tap count);
- **0 LSB — bit-identical** on integer ratios (48000→96000, 22050→44100);
- **THD+N SNR identical** to the float driver to ≤ 0.01 dB in every case
  (e.g. HIGHER/HIGHEST 48000→96000 both hit the 16-bit ceiling of ~96–97 dB;
  NORMAL 32040→96000 both 62.80 dB — that floor is the filter's own image
  rejection, not a fixed-point artifact).

The ≤1 LSB residual is the float32-accumulation-vs-int64 difference resolved at
the final 16-bit quantization; the integer path (int64 exact accumulate) is if
anything the more correct of the two.

## Fixed-point format

- Coefficients: **Q1.30 int32** (same table footprint as float32 — no memory cost).
- Samples: int16; products accumulate in **int64** (16-bit × 30-bit × ≤256 taps
  ≈ 54 bits — int64 is required for HIGHEST).
- Output: round-half-away-from-zero shift back to int16 with saturation.
  Rounding (not truncation) is mandatory — truncation costs a constant −0.5 LSB
  DC bias and ~2.7 dB at −80 dBFS.
- No implementation-defined signed shifts are used (all shifts operate on
  non-negative magnitudes), so the result is portable and deterministic.

## Fast path in `audio/audio_driver.c` (wired)

The shared `retro_resampler` vtable is float-typed, so rather than change the
float ABI, the integer driver is held as a parallel handle
(`audio_driver_state_t::resampler_data_int16`) and used by a fast path in
`audio_driver_flush()`.

- **Init** (`audio_driver_init_internal`): after `retro_resampler_realloc`, when
  the selected backend's `short_ident` is `"sinc"`, an int16 instance is created
  with `sinc_resampler_int16_init(src_ratio_orig, <mapped quality>)`. Logs
  `"[Audio] SINC resampler: integer s16 fast path available"`.
- **Deinit** (`audio_driver_deinit_resampler`): frees the int16 handle.
- **Flush** (`audio_driver_flush`): a branch after the existing `write_raw`
  fast path takes the integer route when **all** of these hold — int16 core
  audio, an int16 handle exists, unity gain, no MIDI synth, no fast-forward
  pitch EMA, and (config-gated) no DSP and no active mixer. It runs the same DRC
  (`src_ratio_orig * audio_driver_compute_rate_adjust`) and slow-motion scaling
  as the float path, resamples s16->s16 into `output_samples_conv_buf`, and
  writes directly (or does a single s16->float pass for float-output drivers).
  Any condition failing falls through to the float path unchanged.

### Which path is running — log

`audio_driver_flush()` logs on every **transition** (not per-flush):

    [Audio] SINC resampler active path: integer s16 (no float round-trip)
    [Audio] SINC resampler active path: float

So the line at startup tells you which path is live, and it re-logs whenever the
conditions flip (e.g. a DSP plugin is toggled, the mixer starts, volume leaves
0 dB, or fast-forward-with-speedup engages), which is exactly when it falls back
to float.

### Notes / follow-ups

- Non-unity volume/mute and active DSP/mixer/MIDI deliberately fall back to the
  float path; a later refinement can apply a Q16 gain in the integer path to
  cover volume too.
- For a **float-output** driver the integer path saturates to the s16 range
  before the s16->float pass, so it does not reproduce sinc overshoot headroom
  a float endpoint could otherwise carry — intended for s16-source content and
  determinism; the float path remains available for anyone who wants that
  headroom.
- Optional: gate the whole thing behind an `audio_fastpath_s16` setting
  (default on) to A/B without changing resampler backends.
