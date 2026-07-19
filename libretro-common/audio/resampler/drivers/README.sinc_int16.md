# Integer (s16) SINC resampler — integration notes

This adds a deterministic, integer-only counterpart to the float `sinc`
resampler (`drivers/sinc_resampler.c`).  It exists to:

1. **Determinism.** The float driver is built with `-ffast-math`; its output
   is not bit-reproducible across compilers/archs/FMA settings. This driver is
   integer-only and bit-identical everywhere — reproducible output for
   validation and regression comparison. (Resampling is downstream of the
   core, so this does not affect netplay or rewind, which synchronise on
   inputs and savestates.)
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

- **Init** (`audio_driver_init_internal`): after `retro_resampler_realloc`, an
  int16 instance is created for the selected backend when one exists — this is
  no longer sinc-only: `"sinc"` (`sinc_resampler_int16_init(src_ratio_orig,
  <mapped quality>)`), `"nearest"` and `"cc"` all have int16 variants. Logs
  `"[Audio] <ident> resampler: integer s16 fast path available"`.
- **Deinit** (`audio_driver_deinit_resampler`): frees the int16 handle.
- **Flush** (`audio_driver_flush`): a branch after the existing `write_raw`
  fast path takes the integer route when int16 core audio arrives and an int16
  handle exists. The gate has been relaxed since this note was first written:
  volume is applied in fixed point (non-unity gain stays integer), MIDI synth
  output is folded in as s16, fast-forward pitch tracking applies to the
  integer ratio, and int16-capable DSP chains run in the integer domain — only
  an incompatible (float-only) DSP filter forces the float path. See the
  comment block above the gate in `audio_driver_flush` for the authoritative
  list. It runs the same DRC
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

### Menu setting

Exposed as **Settings > Audio > Resampler > "Resample to Fixed Integer (Hint)"**
(`audio_fastpath_s16`, default on). When on and a core outputs 16-bit integer
audio, any needed resampling uses the integer SINC driver instead of the float
one. Ignored for float-output cores and whenever DSP / mixer / MIDI synth is
active. Toggling it flips the logged active path (above) without an audio
reinit.

### Volume

Volume/mute is applied inside the fast path, so non-unity gain no longer forces
the float route:

- **s16-output** driver: a deterministic Q16 gain multiply + saturate over the
  output buffer (skipped at unity gain).
- **float-output** driver: folded into the single s16->float pass via
  `convert_s16_to_float(..., audio_volume_gain)` - free.

Because resampling is linear, attenuation is bit-clean; a boost above 0 dB
interacts with s16 saturation slightly differently than the float path's
input-side gain, which is acceptable for s16-source content.

### DSP filters (int16 chain)

Previously any active audio DSP filter forced the float path. The DSP filter
plugin ABI is now **version 2**, adding an optional `process_i16` entry point
and `dspfilter_{input,output}_i16` structs (`libretro_dspfilter.h`). The
framework gained:

- `retro_dsp_filter_supports_int16(dsp)` - true only if every filter in the
  chain provides `process_i16` (API version >= 2);
- `retro_dsp_filter_process_int16(dsp, data)` - runs the chain in int16.

`audio_driver_flush()` now keeps DSP inside the integer fast path when
`retro_dsp_filter_supports_int16()` is true: it copies the core's s16 into an
int16 scratch (`input_data_int16`), runs the int16 chain in place, then feeds
the result to the integer resampler. If any filter in the chain is float-only,
the whole flush falls back to the float path as before.

The version check was relaxed from strict equality to a range, so existing
float-only v1 plugins keep loading (their shorter struct is never over-read
because `process_i16` is only touched after an `api_version >= 2` check).

`panning` is converted as the reference v2 filter: the 2x2 mix runs in Q16
fixed point (int64 accumulate, round-half-away, saturate) and measures 1 LSB
worst case vs the float version. Other bundled filters remain float-only
(`process_i16 == NULL`) until converted, so they transparently keep using the
float path.

### Notes

- For a float-output driver the integer path saturates to the s16 range before
  the s16->float pass, so it does not reproduce sinc overshoot headroom a float
  endpoint could otherwise carry; intended for s16-source content and
  determinism. Turn the hint off to keep the float path (and that headroom).
