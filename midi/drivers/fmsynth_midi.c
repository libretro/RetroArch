/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* fmsynth_midi: an in-process, OS-independent General MIDI synthesizer.
 *
 * Unlike winmm/alsa/coremidi, this driver does NOT route to an external OS
 * MIDI device. It renders MIDI events to PCM itself and that PCM is mixed
 * into RetroArch's audio output stream. This makes it the only MIDI backend
 * that works under WASAPI exclusive mode (no shared OS mixer required) and
 * the only one that is identical across every platform RetroArch builds for.
 *
 * This translation unit contains pure integer/float arithmetic only: no
 * platform headers, no syscalls, no audio API. The synthesized samples are
 * produced into a caller-supplied buffer by fmsynth_render(); the platform-
 * specific audio path that consumes that buffer lives in audio_driver.c and
 * is untouched here.
 *
 * Commit 1 (this file): driver shell. Registers, parses MIDI channel-voice
 * messages into a voice table, and exposes a silent fmsynth_render() seam.
 * The 2-operator FM oscillator math and the GM->FM patch table land in
 * follow-up commits.
 */

#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <math.h>

#include <boolean.h>
#include <lists/string_list.h>

#include "../midi_driver.h"
#include "fmsynth_patches.h"

#define FMSYNTH_MAX_VOICES   64
#define FMSYNTH_NUM_CHANNELS 16

/* Sine lookup table (one cycle). Power-of-two for cheap phase masking. */
#define FMSYNTH_SINE_BITS    11
#define FMSYNTH_SINE_SIZE    (1 << FMSYNTH_SINE_BITS)
#define FMSYNTH_SINE_MASK    (FMSYNTH_SINE_SIZE - 1)

/* Default 2-operator FM patch (per-program timbres arrive with the GM->FM
 * table in a later commit; for now every program uses this one voice). */
#define FMSYNTH_MOD_RATIO    1.0f   /* modulator freq / carrier freq        */
#define FMSYNTH_MOD_INDEX    0.55f  /* modulation depth, in carrier cycles  */
#define FMSYNTH_MASTER_GAIN  0.16f  /* headroom for polyphony               */
#define FMSYNTH_PITCHBEND_SEMI 2.0f /* default +/- bend range in semitones  */

/* Mod-wheel (CC1) vibrato: a per-voice pitch LFO. */
#define FMSYNTH_VIBRATO_HZ     5.5f
#define FMSYNTH_VIBRATO_SEMI   0.5f  /* max depth at full mod wheel          */
#define FMSYNTH_SEMI_TO_RATE   0.05776f /* small-signal 2^(x/12)-1 per semi  */

/* Envelopes and the per-voice LFO are updated at "control rate", once every
 * FMSYNTH_CONTROL samples (libfmsynth uses 32); the tight per-sample loop in
 * between only runs the oscillator bank and the modulation matrix. */
#define FMSYNTH_CONTROL        32

/* GM percussion lives on MIDI channel 10 (0-based index 9). */
#define FMSYNTH_DRUM_CHANNEL 9

/* Velocity -> brightness: how much harder hits open up the FM index.
 * idx scales between (1-DEPTH) and 1.0 across velocity 0..127. */
#define FMSYNTH_VEL_BRIGHT_DEPTH 0.65f

/* Keyboard scaling (libfmsynth-style): higher notes decay/release faster,
 * lower notes ring longer, like real instruments. Decay/release rates are
 * multiplied by 2^((note-60)*FMSYNTH_KEY_SCALE), clamped to a sane span. */
#define FMSYNTH_KEY_SCALE      0.0417f  /* ~doubles per 2 octaves            */
#define FMSYNTH_KEY_SCALE_MIN  0.5f
#define FMSYNTH_KEY_SCALE_MAX  2.5f

/* Schroeder reverb (4 comb + 2 allpass). Buffers are sized for the highest
 * common output rate; per-rate lengths are scaled from the 44.1k tunings and
 * clamped to these maxima. */
#define FMSYNTH_COMB_MAX     6144
#define FMSYNTH_ALLPASS_MAX  2048
#define FMSYNTH_REVERB_FB    0.84f   /* comb feedback (room size)            */
#define FMSYNTH_REVERB_DAMP  0.20f   /* high-frequency damping in feedback   */
#define FMSYNTH_REVERB_WET   0.22f   /* wet mix added on top of dry          */
#define FMSYNTH_REVERB_GAIN  0.015f  /* input scaling into the comb bank     */
#define FMSYNTH_REVERB_TAIL  1.6f    /* seconds the tail is serviced after   */
#define FMSYNTH_REVERB_SPREAD 23     /* right-channel delay offset (Freeverb) */
                                     /* the last voice ends                  */

/* Output stage: a DC blocker (one-pole high-pass, FMSYNTH_DC_HZ corner) clears
 * the small DC the self-feedback operators inject, and a soft clipper keeps
 * dense polyphony from hard-clipping. The clipper is transparent (identity)
 * below +/-FMSYNTH_CLIP_T and curves smoothly toward +/-1.0 above it, so
 * normal playing is untouched and only loud stacks are gently limited. */
#define FMSYNTH_DC_HZ        8.0f
#define FMSYNTH_CLIP_T       0.75f

/* Envelope stages: three linear ramps to the operator's targets, then a held
 * sustain at the final target, then release. (Percussion reuses OFF/RELEASE.) */
#define FMSYNTH_ENV_OFF      0
#define FMSYNTH_ENV_STAGE0   1
#define FMSYNTH_ENV_STAGE1   2
#define FMSYNTH_ENV_STAGE2   3
#define FMSYNTH_ENV_SUSTAIN  4
#define FMSYNTH_ENV_RELEASE  5
/* Back-compat aliases for the percussion path's note gate. */
#define FMSYNTH_ENV_ATTACK   FMSYNTH_ENV_STAGE0
#define FMSYNTH_ENV_DECAY    FMSYNTH_ENV_STAGE1

/* MIDI status nibbles (high nibble of a channel-voice status byte). */
#define MIDI_NOTE_OFF        0x80
#define MIDI_NOTE_ON         0x90
#define MIDI_POLY_PRESSURE   0xA0
#define MIDI_CONTROL_CHANGE  0xB0
#define MIDI_PROGRAM_CHANGE  0xC0
#define MIDI_CHAN_PRESSURE   0xD0
#define MIDI_PITCH_BEND      0xE0
#define MIDI_SYSTEM          0xF0

typedef struct
{
   const fmsynth_patch_t *patch; /* melodic FM patch; NULL for percussion   */
   float    car_phase;   /* carrier phase, in cycles [0,1)  (percussion)    */
   float    mod_phase;    /* modulator phase, in cycles [0,1) (percussion)  */
   float    op_phase[FMSYNTH_OPS]; /* per-operator phase (melodic)          */
   float    op_env[FMSYNTH_OPS];   /* per-operator envelope level [0,1]     */
   float    op_out[FMSYNTH_OPS];   /* last per-operator output (matrix)     */
   int      op_stage[FMSYNTH_OPS]; /* per-operator FMSYNTH_ENV_* stage      */
   float    op_amp[FMSYNTH_OPS];   /* control-rate effective amplitude      */
   float    op_inc_c[FMSYNTH_OPS]; /* control-rate effective phase increment*/
   int      lfo_ctr;      /* samples until next control-rate update          */
   float    base_inc;     /* carrier phase increment/sample (note pitch)    */
   float    env;          /* primary-carrier env level (voice-steal metric) */
   int      stage;        /* FMSYNTH_ENV_* note gate (ATTACK.. / RELEASE)   */
   float    mod_env;      /* current modulator-index envelope level [0,1]   */
   int      mod_stage;    /* FMSYNTH_ENV_* modulator envelope stage         */
   float    drum_dec;     /* percussion amp decay/sample (patch==NULL)      */
   float    drum_index;   /* percussion FM index (pitched hits)             */
   uint32_t rng;          /* per-voice noise LCG state (percussion)         */
   uint32_t age;          /* note_counter at allocation (older = lower)     */
   uint8_t  is_noise;     /* percussion noise hit                           */
   uint8_t  held;         /* note-off deferred by the sustain pedal         */
   float    lfo_phase;    /* mod-wheel vibrato LFO phase, in cycles         */
   float    plfo_phase;   /* per-patch tremolo/vibrato LFO phase, in cycles */
   uint8_t  active;       /* voice is in use (stage != OFF)                 */
   uint8_t  channel;      /* owning MIDI channel 0..15                      */
   uint8_t  note;         /* MIDI note number 0..127                        */
   uint8_t  velocity;     /* note-on velocity 1..127                        */
} fmsynth_voice_t;

typedef struct
{
   fmsynth_voice_t voices[FMSYNTH_MAX_VOICES];
   uint8_t  program[FMSYNTH_NUM_CHANNELS];   /* current GM program per chan */
   uint8_t  volume[FMSYNTH_NUM_CHANNELS];    /* CC7   channel volume         */
   uint8_t  expression[FMSYNTH_NUM_CHANNELS];/* CC11  expression             */
   uint8_t  pan[FMSYNTH_NUM_CHANNELS];       /* CC10  pan                    */
   uint16_t pitch_bend[FMSYNTH_NUM_CHANNELS];/* 14-bit, 0x2000 = centre      */
   uint8_t  sustain[FMSYNTH_NUM_CHANNELS];   /* CC64  pedal: 0/1             */
   uint8_t  mod_wheel[FMSYNTH_NUM_CHANNELS]; /* CC1   vibrato depth 0..127   */
   uint8_t  bend_range[FMSYNTH_NUM_CHANNELS];/* RPN 0,0 bend range, semitones*/
   uint8_t  rpn_msb[FMSYNTH_NUM_CHANNELS];   /* selected RPN MSB (0x7F=null) */
   uint8_t  rpn_lsb[FMSYNTH_NUM_CHANNELS];   /* selected RPN LSB (0x7F=null) */
   unsigned output_rate;                     /* synth render rate in Hz      */
   float    atk_rate;                        /* envelope increments/sample   */
   float    dec_rate;
   float    rel_rate;
   float    sustain_level;
   uint32_t note_counter;                    /* monotonic, for voice age     */

   /* Schroeder reverb state (stereo: [0]=left, [1]=right). */
   float    comb_buf[2][4][FMSYNTH_COMB_MAX];
   float    comb_store[2][4];
   int      comb_len[2][4];
   int      comb_idx[2][4];
   float    allpass_buf[2][2][FMSYNTH_ALLPASS_MAX];
   int      allpass_len[2][2];
   int      allpass_idx[2][2];
   long     reverb_tail;                     /* samples of tail left to run  */

   /* Output stage: DC blocker coefficient and per-channel filter memory. */
   float    dc_r;                            /* high-pass pole (rate-scaled) */
   float    dc_x[2];                         /* previous input  per channel  */
   float    dc_y[2];                         /* previous output per channel  */
} fmsynth_t;

/* ---- shared sine table (read-only after init) -------------------------- */

static float fmsynth_sine_tbl[FMSYNTH_SINE_SIZE];
static int   fmsynth_sine_ready = 0;

static void fmsynth_init_sine(void)
{
   unsigned i;
   if (fmsynth_sine_ready)
      return;
   for (i = 0; i < FMSYNTH_SINE_SIZE; i++)
   {
      double ph = (2.0 * 3.14159265358979323846 * (double)i)
                / (double)FMSYNTH_SINE_SIZE;
      fmsynth_sine_tbl[i] = (float)sin(ph);
   }
   fmsynth_sine_ready = 1;
}

/* Look up sin(2*pi*cycles); 'cycles' may be any value (wrapped via mask).
 * Linearly interpolates between adjacent table entries: at 2048 points the
 * raw lookup floor is only ~55 dB, and because modulator quantization noise
 * turns into FM sidebands, interpolation (which lifts this past ~85 dB) is
 * well worth one extra fetch and a lerp per operator. */
static float fmsynth_sine(float cycles)
{
   float pos;
   float frac;
   float a;
   float b;
   int   idx;
   /* reduce to [0,1) without fmod: keep only the fractional part */
   cycles -= (float)((int)cycles);
   if (cycles < 0.0f)
      cycles += 1.0f;
   pos  = cycles * (float)FMSYNTH_SINE_SIZE;
   idx  = (int)pos;
   frac = pos - (float)idx;
   a    = fmsynth_sine_tbl[idx & FMSYNTH_SINE_MASK];
   b    = fmsynth_sine_tbl[(idx + 1) & FMSYNTH_SINE_MASK];
   return a + (b - a) * frac;
}

/* max of two floats (used to floor envelope times) */
static float fmsynth_maxf(float a, float b)
{
   return a > b ? a : b;
}

/* Soft clipper: identity within +/-FMSYNTH_CLIP_T, then a tanh knee that
 * asymptotes to +/-1.0 so the output can never exceed unity. Transparent at
 * normal levels; only loud polyphony is gently compressed. */
static float fmsynth_softclip(float x)
{
   const float t = FMSYNTH_CLIP_T;
   const float k = 1.0f - FMSYNTH_CLIP_T;
   if (x >  t) return  (t + k * (float)tanh((double)((x - t) / k)));
   if (x < -t) return -(t + k * (float)tanh((double)((-x - t) / k)));
   return x;
}

/* Carrier phase increment (cycles/sample) for a MIDI note at 'rate' Hz. */
static float fmsynth_note_inc(uint8_t note, unsigned rate)
{
   double freq = 440.0 * pow(2.0, ((double)note - 69.0) / 12.0);
   if (rate == 0)
      return 0.0f;
   return (float)(freq / (double)rate);
}

/* Schroeder reverb tunings at 44.1kHz (Freeverb-derived), scaled per rate.
 * The right channel's delays are offset by FMSYNTH_REVERB_SPREAD samples so
 * the two networks decorrelate and the tail spreads across the stereo field. */
static void fmsynth_reverb_init(fmsynth_t *fm, unsigned rate)
{
   static const int comb44[4]    = { 1116, 1188, 1277, 1356 };
   static const int allpass44[2] = {  556,  441 };
   double scale = (double)rate / 44100.0;
   unsigned ch;
   unsigned c;

   for (ch = 0; ch < 2; ch++)
   {
      int spread = (ch == 1) ? FMSYNTH_REVERB_SPREAD : 0;
      for (c = 0; c < 4; c++)
      {
         int len = (int)((comb44[c] + spread) * scale);
         if (len < 1) len = 1;
         if (len > FMSYNTH_COMB_MAX) len = FMSYNTH_COMB_MAX;
         fm->comb_len[ch][c]   = len;
         fm->comb_idx[ch][c]   = 0;
         fm->comb_store[ch][c] = 0.0f;
         memset(fm->comb_buf[ch][c], 0, sizeof(fm->comb_buf[ch][c]));
      }
      for (c = 0; c < 2; c++)
      {
         int len = (int)((allpass44[c] + spread) * scale);
         if (len < 1) len = 1;
         if (len > FMSYNTH_ALLPASS_MAX) len = FMSYNTH_ALLPASS_MAX;
         fm->allpass_len[ch][c] = len;
         fm->allpass_idx[ch][c] = 0;
         memset(fm->allpass_buf[ch][c], 0, sizeof(fm->allpass_buf[ch][c]));
      }
   }
}

/* Process one mono input sample through reverb channel 'ch' (0=L,1=R). */
static float fmsynth_reverb(fmsynth_t *fm, float x, int ch)
{
   float in  = x * FMSYNTH_REVERB_GAIN;
   float out = 0.0f;
   unsigned c;

   for (c = 0; c < 4; c++)
   {
      float y = fm->comb_buf[ch][c][fm->comb_idx[ch][c]];
      fm->comb_store[ch][c] = y * (1.0f - FMSYNTH_REVERB_DAMP)
                            + fm->comb_store[ch][c] * FMSYNTH_REVERB_DAMP;
      fm->comb_buf[ch][c][fm->comb_idx[ch][c]] =
         in + fm->comb_store[ch][c] * FMSYNTH_REVERB_FB;
      if (++fm->comb_idx[ch][c] >= fm->comb_len[ch][c])
         fm->comb_idx[ch][c] = 0;
      out += y;
   }
   /* comb outputs summed in 'out'; run that through the allpass chain. */
   for (c = 0; c < 2; c++)
   {
      float bufout = fm->allpass_buf[ch][c][fm->allpass_idx[ch][c]];
      float y      = -out + bufout;
      fm->allpass_buf[ch][c][fm->allpass_idx[ch][c]] = out + bufout * 0.5f;
      if (++fm->allpass_idx[ch][c] >= fm->allpass_len[ch][c])
         fm->allpass_idx[ch][c] = 0;
      out = y;   /* series: feed into the next allpass stage */
   }
   return out;
}

/* Recompute envelope increments for the current output rate. Called at init
 * and whenever audio_driver.c updates the rate (commit 3). */
static void fmsynth_set_rate(fmsynth_t *fm, unsigned rate)
{
   unsigned old = fm->output_rate;
   if (rate == 0)
      rate = 44100;
   /* base_inc (freq/rate) and drum_dec (1/(time*rate)) are baked per voice
    * at note-on from the rate in force then. If the output rate changes
    * afterwards - including the common case of a note arriving on the very
    * first frame, before audio_driver.c has supplied the real rate - rescale
    * any active voices by old/new so their pitch and decay stay correct. */
   if (old != 0 && old != rate)
   {
      float s = (float)old / (float)rate;
      unsigned i;
      for (i = 0; i < FMSYNTH_MAX_VOICES; i++)
      {
         if (fm->voices[i].active)
         {
            fm->voices[i].base_inc *= s;
            fm->voices[i].drum_dec *= s;
         }
      }
   }
   fm->output_rate   = rate;
   fm->atk_rate      = 1.0f  / (0.004f * (float)rate); /* 4ms attack   */
   fm->dec_rate      = 0.45f / (0.060f * (float)rate); /* 60ms decay   */
   fm->rel_rate      = 0.55f / (0.090f * (float)rate); /* 90ms release */
   fm->sustain_level = 0.55f;
   /* one-pole high-pass corner -> pole position for the output DC blocker */
   fm->dc_r          = (float)exp(-2.0 * 3.14159265358979323846
                     * (double)FMSYNTH_DC_HZ / (double)rate);
   fmsynth_reverb_init(fm, rate);
}

/* ---- voice allocation -------------------------------------------------- */

static fmsynth_voice_t *fmsynth_find_voice(fmsynth_t *fm,
      uint8_t channel, uint8_t note)
{
   unsigned i;
   for (i = 0; i < FMSYNTH_MAX_VOICES; i++)
   {
      fmsynth_voice_t *v = &fm->voices[i];
      if (v->active && v->channel == channel && v->note == note)
         return v;
   }
   return NULL;
}

static fmsynth_voice_t *fmsynth_alloc_voice(fmsynth_t *fm)
{
   unsigned i;
   fmsynth_voice_t *best = NULL;

   /* 1: a free voice. */
   for (i = 0; i < FMSYNTH_MAX_VOICES; i++)
   {
      if (!fm->voices[i].active)
         return &fm->voices[i];
   }

   /* 2: the quietest voice already in its release stage. */
   for (i = 0; i < FMSYNTH_MAX_VOICES; i++)
   {
      fmsynth_voice_t *v = &fm->voices[i];
      if (v->stage == FMSYNTH_ENV_RELEASE)
      {
         if (!best || v->env < best->env)
            best = v;
      }
   }
   if (best)
      return best;

   /* 3: no releasing voice; steal the oldest. */
   for (i = 0; i < FMSYNTH_MAX_VOICES; i++)
   {
      fmsynth_voice_t *v = &fm->voices[i];
      if (!best || v->age < best->age)
         best = v;
   }
   return best ? best : &fm->voices[0];
}

static void fmsynth_note_on(fmsynth_t *fm,
      uint8_t channel, uint8_t note, uint8_t velocity)
{
   fmsynth_voice_t *v;

   /* Running status note-on with velocity 0 is a note-off. */
   if (velocity == 0)
   {
      v = fmsynth_find_voice(fm, channel, note);
      if (v)
      {
         if (fm->sustain[channel])
            v->held = 1;                      /* defer until pedal up */
         else
            v->stage = FMSYNTH_ENV_RELEASE;
      }
      return;
   }

   v             = fmsynth_alloc_voice(fm);
   v->active     = 1;
   v->age        = ++fm->note_counter;
   v->channel    = channel;
   v->note       = note;
   v->velocity   = velocity;
   v->car_phase  = 0.0f;
   v->mod_phase  = 0.0f;
   v->env        = 0.0f;
   v->mod_env    = 0.0f;
   v->stage      = FMSYNTH_ENV_ATTACK;
   v->mod_stage  = FMSYNTH_ENV_ATTACK;
   v->rng        = 0x2545f491u ^ ((uint32_t)note * 2654435761u);
   v->is_noise   = 0;
   v->held       = 0;
   v->lfo_phase  = 0.0f;
   v->plfo_phase = 0.0f;
   v->drum_dec   = 0.0f;
   v->drum_index = 0.0f;

   if (channel == FMSYNTH_DRUM_CHANNEL)
   {
      const fmsynth_drum_t *d = fmsynth_drum_for_note(note);
      v->patch    = NULL;            /* percussion render path */
      v->env      = 1.0f;            /* hits start full, decay out */
      v->stage    = FMSYNTH_ENV_RELEASE;
      if (d)
      {
         float dec   = fmsynth_maxf(d->decay, 0.001f);
         v->drum_dec   = 1.0f / (dec * (float)fm->output_rate);
         v->drum_index = d->index;
         v->is_noise   = (uint8_t)((d->kind == FMSYNTH_DRUM_NOISE) ? 1 : 0);
         v->base_inc   = (d->kind == FMSYNTH_DRUM_FM)
                       ? d->pitch / (float)fm->output_rate : 0.0f;
      }
      else
      {
         v->is_noise = 1;            /* unmapped note: short tick */
         v->base_inc = 0.0f;
         v->drum_dec = 1.0f / (0.05f * (float)fm->output_rate);
      }
   }
   else
   {
      unsigned o;
      v->patch    = fmsynth_patch_for_program(fm->program[channel]);
      v->base_inc = fmsynth_note_inc(note, fm->output_rate);
      v->lfo_ctr  = 0;            /* force a control update on first sample */
      for (o = 0; o < FMSYNTH_OPS; o++)
      {
         v->op_phase[o] = 0.0f;
         v->op_env[o]   = 0.0f;
         v->op_out[o]   = 0.0f;
         v->op_stage[o] = FMSYNTH_ENV_ATTACK;
      }
   }
}

static void fmsynth_note_off(fmsynth_t *fm, uint8_t channel, uint8_t note)
{
   fmsynth_voice_t *v = fmsynth_find_voice(fm, channel, note);
   if (v)
   {
      if (fm->sustain[channel])
         v->held = 1;                         /* defer until pedal up */
      else
         v->stage = FMSYNTH_ENV_RELEASE;
   }
}

static void fmsynth_all_notes_off(fmsynth_t *fm, uint8_t channel)
{
   unsigned i;
   for (i = 0; i < FMSYNTH_MAX_VOICES; i++)
   {
      if (fm->voices[i].active && fm->voices[i].channel == channel)
         fm->voices[i].stage = FMSYNTH_ENV_RELEASE;
   }
}

/* ---- MIDI message dispatch --------------------------------------------- */

static void fmsynth_handle_message(fmsynth_t *fm,
      const uint8_t *data, size_t size)
{
   uint8_t status;
   uint8_t channel;

   if (!data || size < 1)
      return;

   status  = data[0];
   if (status < 0x80)
      return; /* expecting a status byte */

   if ((status & 0xF0) == MIDI_SYSTEM)
      return; /* system/realtime messages are ignored by the synth */

   channel = (uint8_t)(status & 0x0F);

   switch (status & 0xF0)
   {
      case MIDI_NOTE_ON:
         if (size >= 3)
            fmsynth_note_on(fm, channel, data[1], data[2]);
         break;
      case MIDI_NOTE_OFF:
         if (size >= 3)
            fmsynth_note_off(fm, channel, data[1]);
         break;
      case MIDI_PROGRAM_CHANGE:
         if (size >= 2)
            fm->program[channel] = (uint8_t)(data[1] & 0x7F);
         break;
      case MIDI_PITCH_BEND:
         if (size >= 3)
            fm->pitch_bend[channel] =
               (uint16_t)((data[1] & 0x7F) | ((data[2] & 0x7F) << 7));
         break;
      case MIDI_CONTROL_CHANGE:
         if (size >= 3)
         {
            uint8_t cc  = (uint8_t)(data[1] & 0x7F);
            uint8_t val = (uint8_t)(data[2] & 0x7F);
            switch (cc)
            {
               case 1:   fm->mod_wheel[channel]  = val; break;
               case 7:   fm->volume[channel]     = val; break;
               case 10:  fm->pan[channel]        = val; break;
               case 11:  fm->expression[channel] = val; break;
               case 6:   /* Data Entry MSB: targets the selected RPN */
                  if (fm->rpn_msb[channel] == 0 && fm->rpn_lsb[channel] == 0)
                     fm->bend_range[channel] = (uint8_t)(val ? val : 1);
                  break;
               case 64:  /* sustain pedal */
                  if (val >= 64)
                     fm->sustain[channel] = 1;
                  else
                  {
                     unsigned k;
                     fm->sustain[channel] = 0;
                     for (k = 0; k < FMSYNTH_MAX_VOICES; k++)
                     {
                        fmsynth_voice_t *hv = &fm->voices[k];
                        if (hv->active && hv->channel == channel && hv->held)
                        {
                           hv->held  = 0;
                           hv->stage = FMSYNTH_ENV_RELEASE;
                        }
                     }
                  }
                  break;
               case 100: fm->rpn_lsb[channel]     = val; break;
               case 101: fm->rpn_msb[channel]     = val; break;
               case 120: /* all sound off    */
               case 123: /* all notes off    */
                  fmsynth_all_notes_off(fm, channel);
                  break;
               default:
                  break;
            }
         }
         break;
      default:
         break;
   }
}

/* ---- render seam -------------------------------------------------------
 * Produces 'frames' stereo float samples into 'out' (interleaved L/R).
 * audio_driver.c calls this once per core frame and sums the result into
 * the outgoing mix buffer. Commit 1 renders silence; the FM oscillator and
 * envelope math replace the memset in commit 2.
 * ----------------------------------------------------------------------- */

static bool fmsynth_render(void *p, float *out, size_t frames, unsigned rate)
{
   fmsynth_t *fm = (fmsynth_t*)p;
   unsigned i;
   size_t   n;
   int      any = 0;

   if (!fm || !out)
      return false;

   if (rate != 0 && rate != fm->output_rate)
      fmsynth_set_rate(fm, rate);

   memset(out, 0, frames * 2 * sizeof(float));

   for (i = 0; i < FMSYNTH_MAX_VOICES; i++)
   {
      fmsynth_voice_t *v = &fm->voices[i];
      float car_inc;
      float mod_inc;
      float vgain;
      float panL;
      float panR;
      float bend;
      uint8_t ch;

      if (!v->active)
         continue;

      any   = 1;
      ch    = v->channel;

      /* Pitch bend (constant across this block), RPN-configurable range. */
      bend  = ((float)fm->pitch_bend[ch] - 8192.0f) / 8192.0f
            * (float)fm->bend_range[ch];
      bend  = (float)pow(2.0, (double)bend / 12.0);
      car_inc = v->base_inc * bend;

      /* Per-voice gain: velocity * channel volume * expression * master. */
      vgain = ((float)v->velocity      / 127.0f)
            * ((float)fm->volume[ch]    / 127.0f)
            * ((float)fm->expression[ch] / 127.0f)
            * FMSYNTH_MASTER_GAIN;

      /* Constant-power pan from CC10. */
      {
         float pan = (float)fm->pan[ch] / 127.0f;
         panL = (float)cos(pan * 1.5707963267948966);
         panR = (float)sin(pan * 1.5707963267948966);
      }

      if (v->patch)
      {
         /* ---- melodic FM: 8 operators, free modulation matrix ----
          * Modelled on libfmsynth: per-operator envelope, velocity, keyboard
          * scaling and LFO depth; any-to-any modulation (including self) via
          * the sparse connection matrix; per-carrier panning; and control-rate
          * envelope/LFO updates with a tight per-sample oscillator+matrix loop. */
         const fmsynth_patch_t *pt = v->patch;
         float rate    = (float)fm->output_rate;
         float velf    = (float)v->velocity / 127.0f;
         float vg      = ((float)fm->volume[ch]     / 127.0f)
                       * ((float)fm->expression[ch] / 127.0f)
                       * FMSYNTH_MASTER_GAIN;
         float pan01   = (float)fm->pan[ch] / 127.0f;
         float chpan   = pan01 * 2.0f - 1.0f;            /* -1..+1 channel pan */
         float lfo_rate= pt->lfo_rate > 0.0f ? pt->lfo_rate : FMSYNTH_VIBRATO_HZ;
         float lfo_inc = lfo_rate / rate;
         float mw_semi = ((float)fm->mod_wheel[ch] / 127.0f) * FMSYNTH_VIBRATO_SEMI;
         float inc0[FMSYNTH_OPS];
         float amp0[FMSYNTH_OPS];
         float eslope[FMSYNTH_OPS][3];   /* per-stage envelope ramp / sample */
         float erel[FMSYNTH_OPS];        /* release ramp magnitude / sample  */
         float cpanL[FMSYNTH_OPS];
         float cpanR[FMSYNTH_OPS];
         int   first_car = -1;
         unsigned o;

         for (o = 0; o < FMSYNTH_OPS; o++)
         {
            const fmsynth_op_t *op = &pt->op[o];
            float dn     = (float)v->note - op->ks_mid;
            float ksc    = (dn >= 0.0f)
                         ? (float)pow(2.0, (double)(dn / 12.0f * op->ks_high))
                         : (float)pow(2.0, (double)(dn / 12.0f * op->ks_low));
            float velfac = (1.0f - op->vel_sens) + op->vel_sens * velf;
            float t0     = op->env_target[0];
            float t1     = op->env_target[1];
            float t2     = op->env_target[2];
            if (ksc < FMSYNTH_KEY_SCALE_MIN) ksc = FMSYNTH_KEY_SCALE_MIN;
            if (ksc > FMSYNTH_KEY_SCALE_MAX) ksc = FMSYNTH_KEY_SCALE_MAX;
            inc0[o] = car_inc * op->ratio + op->offset / rate;
            amp0[o] = op->amp * velfac;
            /* attack ramp runs at note rate; decays and release scale w/ pitch */
            eslope[o][0] =       (t0 - 0.0f)
                         / (fmsynth_maxf(op->env_time[0], 0.0005f) * rate);
            eslope[o][1] = ksc * (t1 - t0)
                         / (fmsynth_maxf(op->env_time[1], 0.0005f) * rate);
            eslope[o][2] = ksc * (t2 - t1)
                         / (fmsynth_maxf(op->env_time[2], 0.0005f) * rate);
            erel[o]      = ksc *  t2
                         / (fmsynth_maxf(op->release,     0.0005f) * rate);
            if (op->carrier)
            {
               float pp = op->pan + chpan;
               float a;
               if (pp < -1.0f) pp = -1.0f;
               if (pp >  1.0f) pp =  1.0f;
               a = (pp + 1.0f) * 0.5f * 1.5707963267948966f;
               cpanL[o] = (float)cos((double)a);
               cpanR[o] = (float)sin((double)a);
               if (first_car < 0) first_car = (int)o;
            }
            else { cpanL[o] = 0.0f; cpanR[o] = 0.0f; }
         }
         if (first_car < 0) first_car = 0;

         for (n = 0; n < frames; n++)
         {
            float modin[FMSYNTH_OPS];
            float newout[FMSYNTH_OPS];
            float sL = 0.0f;
            float sR = 0.0f;
            unsigned c;
            unsigned j;

            /* ---- control rate: advance envelopes + LFO every FMSYNTH_CONTROL ---- */
            if (v->lfo_ctr <= 0)
            {
               int   step   = FMSYNTH_CONTROL;
               int   alloff = 1;
               float lfo;
               if ((size_t)step > frames - n) step = (int)(frames - n);
               lfo = fmsynth_sine(v->lfo_phase);
               v->lfo_phase += lfo_inc * (float)step;
               while (v->lfo_phase >= 1.0f) v->lfo_phase -= 1.0f;
               for (o = 0; o < FMSYNTH_OPS; o++)
               {
                  const fmsynth_op_t *op = &pt->op[o];
                  int   stg = v->op_stage[o];
                  float env = v->op_env[o];
                  float fscale;
                  if (v->stage == FMSYNTH_ENV_RELEASE
                   && stg != FMSYNTH_ENV_OFF && stg != FMSYNTH_ENV_RELEASE)
                     stg = FMSYNTH_ENV_RELEASE;
                  switch (stg)
                  {
                     case FMSYNTH_ENV_STAGE0:
                        env += eslope[o][0] * (float)step;
                        if ((eslope[o][0] >= 0.0f) ? (env >= op->env_target[0])
                                                   : (env <= op->env_target[0]))
                        { env = op->env_target[0]; stg = FMSYNTH_ENV_STAGE1; }
                        break;
                     case FMSYNTH_ENV_STAGE1:
                        env += eslope[o][1] * (float)step;
                        if ((eslope[o][1] >= 0.0f) ? (env >= op->env_target[1])
                                                   : (env <= op->env_target[1]))
                        { env = op->env_target[1]; stg = FMSYNTH_ENV_STAGE2; }
                        break;
                     case FMSYNTH_ENV_STAGE2:
                        env += eslope[o][2] * (float)step;
                        if ((eslope[o][2] >= 0.0f) ? (env >= op->env_target[2])
                                                   : (env <= op->env_target[2]))
                        { env = op->env_target[2]; stg = FMSYNTH_ENV_SUSTAIN; }
                        break;
                     case FMSYNTH_ENV_SUSTAIN:
                        env = op->env_target[2];
                        break;
                     case FMSYNTH_ENV_RELEASE:
                        env -= erel[o] * (float)step;
                        if (env <= 0.0f) { env = 0.0f; stg = FMSYNTH_ENV_OFF; }
                        break;
                     default:
                        break;
                  }
                  v->op_env[o]   = env;
                  v->op_stage[o] = stg;
                  v->op_amp[o]   = amp0[o] * env * (1.0f + op->lfo_amp * lfo);
                  fscale         = 1.0f
                     + (op->lfo_freq + op->mod_sens * mw_semi)
                     * FMSYNTH_SEMI_TO_RATE * lfo;
                  v->op_inc_c[o] = inc0[o] * fscale;
                  if (op->carrier && stg != FMSYNTH_ENV_OFF)
                     alloff = 0;
               }
               if (alloff)
               {
                  v->active = 0;
                  v->stage  = FMSYNTH_ENV_OFF;
                  break;
               }
               v->lfo_ctr = step;
            }
            v->lfo_ctr--;

            /* ---- per sample: modulation matrix, then oscillator bank ----
             * This is phase modulation (the DX7 form): the matrix output is
             * added to each operator's phase at lookup, not integrated into
             * it. libfmsynth's README describes phase-integration FM instead,
             * but the two are the SAME synthesis - they yield identical Bessel
             * sideband spectra for a given modulation index. They differ only
             * in parameterisation: PM's index is the phase deviation and is
             * independent of pitch (consistent timbre across the keyboard),
             * whereas integrated FM's index is deviation/modulator-frequency,
             * so matching it would mean scaling every depth by the modulator
             * frequency (same result, more work) or, unscaled, brightening low
             * notes wildly and risking DC drift on the feedback operators.
             * PM is kept deliberately; the patch depths are calibrated for it. */
            for (o = 0; o < FMSYNTH_OPS; o++) modin[o] = 0.0f;
            for (c = 0; c < pt->nconn; c++)
               modin[pt->conn[c].to] +=
                  pt->conn[c].depth * v->op_out[pt->conn[c].from];
            for (j = 0; j < FMSYNTH_OPS; j++)
            {
               newout[j] = fmsynth_sine(v->op_phase[j] + modin[j]) * v->op_amp[j];
               v->op_phase[j] += v->op_inc_c[j];
               if (v->op_phase[j] >= 1.0f)      v->op_phase[j] -= 1.0f;
               else if (v->op_phase[j] < 0.0f)  v->op_phase[j] += 1.0f;
               if (pt->op[j].carrier)
               {
                  sL += newout[j] * cpanL[j];
                  sR += newout[j] * cpanR[j];
               }
            }
            for (o = 0; o < FMSYNTH_OPS; o++) v->op_out[o] = newout[o];

            out[n * 2]     += sL * vg;
            out[n * 2 + 1] += sR * vg;

            v->env = v->op_env[first_car];     /* voice-steal metric */
         }
      }
      else
      {
         /* ---- percussion: single-decay amp env; FM hit or noise ---- */
         mod_inc = car_inc * 2.0f;

         for (n = 0; n < frames; n++)
         {
            float s;

            v->env -= v->drum_dec;
            if (v->env <= 0.0f) { v->env = 0.0f; v->stage = FMSYNTH_ENV_OFF; v->active = 0; }

            if (v->is_noise)
            {
               v->rng = v->rng * 1664525u + 1013904223u;
               s = ((float)(v->rng & 0xFFFF) / 32768.0f) - 1.0f;
            }
            else
            {
               float m = fmsynth_sine(v->mod_phase) * v->drum_index;
               s = fmsynth_sine(v->car_phase + m);
               v->car_phase += car_inc;
               v->mod_phase += mod_inc;
               if (v->car_phase >= 1.0f) v->car_phase -= 1.0f;
               if (v->mod_phase >= 1.0f) v->mod_phase -= 1.0f;
            }

            s *= v->env * vgain;
            out[n * 2]     += s * panL;
            out[n * 2 + 1] += s * panR;

            if (!v->active)
               break;
         }
      }
   }

   /* Keep servicing the reverb for a tail after the last voice ends so it
    * rings out instead of being cut off. */
   if (any)
      fm->reverb_tail = (long)(FMSYNTH_REVERB_TAIL * (float)fm->output_rate);

   if (fm->reverb_tail > 0)
   {
      for (n = 0; n < frames; n++)
      {
         float l    = out[n * 2];
         float r    = out[n * 2 + 1];
         float mono = (l + r) * 0.5f;
         float wetL = fmsynth_reverb(fm, mono, 0);
         float wetR = fmsynth_reverb(fm, mono, 1);
         out[n * 2]     = l + wetL * FMSYNTH_REVERB_WET;
         out[n * 2 + 1] = r + wetR * FMSYNTH_REVERB_WET;
      }
      fm->reverb_tail -= (long)frames;
      any = 1;                                /* output stage must still run */
   }

   /* Output stage: DC blocker then soft clip, on both channels, every frame.
    * Runs over the whole block (including the reverb tail) so DC introduced by
    * self-feedback operators is removed and dense chords cannot hard-clip. */
   for (n = 0; n < frames; n++)
   {
      unsigned ch2;
      for (ch2 = 0; ch2 < 2; ch2++)
      {
         float x = out[n * 2 + ch2];
         float y = x - fm->dc_x[ch2] + fm->dc_r * fm->dc_y[ch2];
         fm->dc_x[ch2] = x;
         fm->dc_y[ch2] = y;
         out[n * 2 + ch2] = fmsynth_softclip(y);
      }
   }

   return any != 0;
}

/* ---- midi_driver_t vtable implementation ------------------------------- */

static bool fmsynth_midi_get_avail_inputs(struct string_list *inputs)
{
   /* Output-only synth: no MIDI inputs. The "Off" entry is already present. */
   (void)inputs;
   return true;
}

static bool fmsynth_midi_get_avail_outputs(struct string_list *outputs)
{
   union string_list_elem_attr attr;
   attr.i = 0;
   if (!outputs)
      return false;
   return string_list_append(outputs, "FM Synth", attr);
}

static void *fmsynth_midi_init(const char *input, const char *output)
{
   fmsynth_t *fm;
   unsigned i;

   /* Output-only software synth. 'input' is ignored, and 'output' must NOT
    * be required for construction: the selected output device only gates
    * the frontend's output_enabled flag. Failing here when output is "Off"
    * would abort midi_driver_init, free the device list, and leave the
    * synth impossible to select (empty MIDI Output menu). */
   (void)input;
   (void)output;

   fm = (fmsynth_t*)calloc(1, sizeof(*fm));
   if (!fm)
      return NULL;

   fmsynth_init_sine();

   /* ADSR + rate defaults (44.1k until audio_driver.c supplies the real
    * rate via fmsynth_set_rate in commit 3). */
   fmsynth_set_rate(fm, 44100);

   for (i = 0; i < FMSYNTH_NUM_CHANNELS; i++)
   {
      fm->program[i]    = 0;
      fm->volume[i]     = 100;
      fm->expression[i] = 127;
      fm->pan[i]        = 64;
      fm->pitch_bend[i] = 0x2000;
      fm->sustain[i]    = 0;
      fm->mod_wheel[i]  = 0;
      fm->bend_range[i] = (uint8_t)FMSYNTH_PITCHBEND_SEMI; /* GM default +/-2 */
      fm->rpn_msb[i]    = 0x7F;  /* null RPN until explicitly selected     */
      fm->rpn_lsb[i]    = 0x7F;
   }

   return fm;
}

static void fmsynth_midi_free(void *p)
{
   if (p)
      free(p);
}

static bool fmsynth_midi_set_input(void *p, const char *input)
{
   (void)p;
   /* Only "Off" (NULL) is valid for an output-only driver. */
   return input == NULL;
}

static bool fmsynth_midi_set_output(void *p, const char *output)
{
   /* Accept both selecting "FM Synth" and disabling (NULL): whether output
    * is actually produced is gated by the frontend's output_enabled. */
   (void)output;
   return p != NULL;
}

static bool fmsynth_midi_read(void *p, midi_event_t *event)
{
   (void)p;
   (void)event;
   return false; /* no input path */
}

static bool fmsynth_midi_write(void *p, const midi_event_t *event)
{
   fmsynth_t *fm = (fmsynth_t*)p;
   if (!fm || !event || !event->data)
      return false;
   fmsynth_handle_message(fm, event->data, event->data_size);
   return true;
}

static bool fmsynth_midi_flush(void *p)
{
   /* Events are applied immediately in write(); nothing is queued. */
   return p != NULL;
}

midi_driver_t midi_fmsynth = {
   "fmsynth",
   fmsynth_midi_get_avail_inputs,
   fmsynth_midi_get_avail_outputs,
   fmsynth_midi_init,
   fmsynth_midi_free,
   fmsynth_midi_set_input,
   fmsynth_midi_set_output,
   fmsynth_midi_read,
   fmsynth_midi_write,
   fmsynth_midi_flush,
   fmsynth_render
};
