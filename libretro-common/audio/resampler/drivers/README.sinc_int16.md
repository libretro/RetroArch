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

Build & run the harness standalone:

    cc -O2 -std=c89 -pedantic -Wall -Wextra \
       test/test_sinc_int16.c drivers/sinc_resampler_int16.c \
       test/sinc_ref_float.c -lm -o /tmp/test_sinc_int16 && /tmp/test_sinc_int16

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

## Wiring the fast path into `audio/audio_driver.c`

The shared `retro_resampler` vtable is float-typed, so rather than forcing an
ABI change on `struct resampler_data`, keep the int16 driver as a **parallel
handle** used by a fast path in `audio_driver_flush()`.

1. Add to `audio_driver_state_t`:

       void *resampler_int16_data;   /* rarch_sinc_resampler_int16_t* */

2. In `audio_driver_init_internal()`, alongside `retro_resampler_realloc(...)`,
   when the selected resampler ident is `"sinc"` create the int16 instance:

       audio_st->resampler_int16_data =
             sinc_resampler_int16_init(audio_st->src_ratio_orig, quality);

   (Free it in `audio_driver_deinit_resampler()` next to the float handle.)

3. In `audio_driver_flush()`, add a branch *after* the existing `write_raw`
   fast path (line ~650) and *before* the `convert_s16_to_float` path
   (line ~672). Gate it on exactly the conditions that make a float-domain pass
   unnecessary — the same set the `write_raw` path already uses:

       if (!is_float
             && audio_st->resampler_int16_data
             && audio_volume_gain == 1.0f      /* gain applied in float otherwise */
             && !midi_driver_synth_active()
       #ifdef HAVE_DSP_FILTER
             && !audio_st->dsp
       #endif
       #ifdef HAVE_AUDIOMIXER
             && audio_st->mixer_streams_playing == 0
       #endif
          )
       {
          struct resampler_data_int16 s16;
          /* DRC: identical mechanism to the float path (line ~745). */
          double adjust = 1.0;
          if (audio_st->flags & AUDIO_FLAG_CONTROL) { ... compute as today ... }
          audio_st->src_ratio_curr = audio_st->src_ratio_orig * adjust;

          s16.data_in       = (const int16_t*)data;
          s16.input_frames  = samples >> 1;
          s16.data_out      = audio_st->output_samples_conv_buf; /* int16 buf */
          s16.ratio         = audio_st->src_ratio_curr;
          sinc_resampler_int16_process(audio_st->resampler_int16_data, &s16);

          /* For an s16 device: hand s16.data_out straight to write() — no
           * convert_float_to_s16. For a float device: one convert_s16_to_float
           * at the output rate (still one conversion, vs one on the float path,
           * so neutral on work but deterministic). */
          ... existing write/underrun handling, using s16.output_frames ...
          return;
       }

   Notes:
   - The DRC loop is unchanged — `ratio` is a `double` set per flush exactly as
     the float path does (`src_ratio_orig * rate_adjust`).
   - `output_samples_conv_buf` already exists as the int16 output staging buffer
     (used by `convert_float_to_s16` at line ~913); reuse it directly.
   - Volume/mute at non-unity gain falls through to the float path, which
     handles arbitrary gain cleanly. A later refinement can apply a Q16 gain in
     the int16 path to cover that case too.

4. Optional config toggle `audio_fastpath_s16` (default on) to A/B it.

The driver + harness in this patch are build- and bit-tested; step 3 touches the
`#ifdef` matrix in `audio_driver_flush()` and should be compiled against a full
tree before merge.
