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

/* GM percussion lives on MIDI channel 10 (0-based index 9). */
#define FMSYNTH_DRUM_CHANNEL 9

/* Velocity -> brightness: how much harder hits open up the FM index.
 * idx scales between (1-DEPTH) and 1.0 across velocity 0..127. */
#define FMSYNTH_VEL_BRIGHT_DEPTH 0.65f

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
                                     /* the last voice ends                  */

/* Envelope stages. */
#define FMSYNTH_ENV_OFF      0
#define FMSYNTH_ENV_ATTACK   1
#define FMSYNTH_ENV_DECAY    2
#define FMSYNTH_ENV_SUSTAIN  3
#define FMSYNTH_ENV_RELEASE  4

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
   float    car_phase;   /* carrier phase, in cycles [0,1)                  */
   float    mod_phase;    /* modulator phase, in cycles [0,1)               */
   float    base_inc;     /* carrier phase increment/sample (note pitch)    */
   float    env;          /* current carrier envelope level [0,1]           */
   int      stage;        /* FMSYNTH_ENV_* carrier envelope stage           */
   float    mod_env;      /* current modulator-index envelope level [0,1]   */
   int      mod_stage;    /* FMSYNTH_ENV_* modulator envelope stage         */
   float    drum_dec;     /* percussion amp decay/sample (patch==NULL)      */
   float    drum_index;   /* percussion FM index (pitched hits)             */
   uint32_t rng;          /* per-voice noise LCG state (percussion)         */
   uint32_t age;          /* note_counter at allocation (older = lower)     */
   uint8_t  is_noise;     /* percussion noise hit                           */
   uint8_t  held;         /* note-off deferred by the sustain pedal         */
   float    lfo_phase;    /* per-voice vibrato LFO phase, in cycles         */
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

   /* Schroeder reverb state. */
   float    comb_buf[4][FMSYNTH_COMB_MAX];
   float    comb_store[4];
   int      comb_len[4];
   int      comb_idx[4];
   float    allpass_buf[2][FMSYNTH_ALLPASS_MAX];
   int      allpass_len[2];
   int      allpass_idx[2];
   long     reverb_tail;                     /* samples of tail left to run  */
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

/* Look up sin(2*pi*cycles); 'cycles' may be any value (wrapped via mask). */
static float fmsynth_sine(float cycles)
{
   int idx;
   /* reduce to [0,1) without fmod: keep only the fractional part */
   cycles -= (float)((int)cycles);
   if (cycles < 0.0f)
      cycles += 1.0f;
   idx = (int)(cycles * (float)FMSYNTH_SINE_SIZE) & FMSYNTH_SINE_MASK;
   return fmsynth_sine_tbl[idx];
}

/* max of two floats (used to floor envelope times) */
static float fmsynth_maxf(float a, float b)
{
   return a > b ? a : b;
}

/* Carrier phase increment (cycles/sample) for a MIDI note at 'rate' Hz. */
static float fmsynth_note_inc(uint8_t note, unsigned rate)
{
   double freq = 440.0 * pow(2.0, ((double)note - 69.0) / 12.0);
   if (rate == 0)
      return 0.0f;
   return (float)(freq / (double)rate);
}

/* Schroeder reverb tunings at 44.1kHz (Freeverb-derived), scaled per rate. */
static void fmsynth_reverb_init(fmsynth_t *fm, unsigned rate)
{
   static const int comb44[4]    = { 1116, 1188, 1277, 1356 };
   static const int allpass44[2] = {  556,  441 };
   double scale = (double)rate / 44100.0;
   unsigned c;

   for (c = 0; c < 4; c++)
   {
      int len = (int)(comb44[c] * scale);
      if (len < 1) len = 1;
      if (len > FMSYNTH_COMB_MAX) len = FMSYNTH_COMB_MAX;
      fm->comb_len[c]   = len;
      fm->comb_idx[c]   = 0;
      fm->comb_store[c] = 0.0f;
      memset(fm->comb_buf[c], 0, sizeof(fm->comb_buf[c]));
   }
   for (c = 0; c < 2; c++)
   {
      int len = (int)(allpass44[c] * scale);
      if (len < 1) len = 1;
      if (len > FMSYNTH_ALLPASS_MAX) len = FMSYNTH_ALLPASS_MAX;
      fm->allpass_len[c] = len;
      fm->allpass_idx[c] = 0;
      memset(fm->allpass_buf[c], 0, sizeof(fm->allpass_buf[c]));
   }
}

/* Process one mono sample through the reverb; returns the wet signal. */
static float fmsynth_reverb(fmsynth_t *fm, float x)
{
   float in  = x * FMSYNTH_REVERB_GAIN;
   float out = 0.0f;
   unsigned c;

   for (c = 0; c < 4; c++)
   {
      float y = fm->comb_buf[c][fm->comb_idx[c]];
      fm->comb_store[c] = y * (1.0f - FMSYNTH_REVERB_DAMP)
                        + fm->comb_store[c] * FMSYNTH_REVERB_DAMP;
      fm->comb_buf[c][fm->comb_idx[c]] = in + fm->comb_store[c] * FMSYNTH_REVERB_FB;
      if (++fm->comb_idx[c] >= fm->comb_len[c])
         fm->comb_idx[c] = 0;
      out += y;
   }
   /* comb outputs summed in 'out'; run that through the allpass chain. */
   for (c = 0; c < 2; c++)
   {
      float bufout = fm->allpass_buf[c][fm->allpass_idx[c]];
      float y      = -out + bufout;
      fm->allpass_buf[c][fm->allpass_idx[c]] = out + bufout * 0.5f;
      if (++fm->allpass_idx[c] >= fm->allpass_len[c])
         fm->allpass_idx[c] = 0;
      out = y;   /* series: feed into the next allpass stage */
   }
   return out;
}

/* Recompute envelope increments for the current output rate. Called at init
 * and whenever audio_driver.c updates the rate (commit 3). */
static void fmsynth_set_rate(fmsynth_t *fm, unsigned rate)
{
   if (rate == 0)
      rate = 44100;
   fm->output_rate   = rate;
   fm->atk_rate      = 1.0f  / (0.004f * (float)rate); /* 4ms attack   */
   fm->dec_rate      = 0.45f / (0.060f * (float)rate); /* 60ms decay   */
   fm->rel_rate      = 0.55f / (0.090f * (float)rate); /* 90ms release */
   fm->sustain_level = 0.55f;
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
      v->patch    = fmsynth_patch_for_program(fm->program[channel]);
      v->base_inc = fmsynth_note_inc(note, fm->output_rate);
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
         /* ---- melodic 2-op FM with per-patch envelopes ---- */
         const fmsynth_patch_t *pt = v->patch;
         float rate  = (float)fm->output_rate;
         float c_atk = 1.0f / (fmsynth_maxf(pt->c_atk, 0.0005f) * rate);
         float c_dec = (1.0f - pt->c_sus)
                     / (fmsynth_maxf(pt->c_dec, 0.0005f) * rate);
         float c_rel = (pt->c_sus > 0.0f ? pt->c_sus : 1.0f)
                     / (fmsynth_maxf(pt->c_rel, 0.0005f) * rate);
         float m_atk = 1.0f / (fmsynth_maxf(pt->m_atk, 0.0005f) * rate);
         float m_dec = (1.0f - pt->m_sus)
                     / (fmsynth_maxf(pt->m_dec, 0.0005f) * rate);
         /* velocity -> brightness: harder hits open the FM index */
         float vbright = (1.0f - FMSYNTH_VEL_BRIGHT_DEPTH)
                       + FMSYNTH_VEL_BRIGHT_DEPTH * ((float)v->velocity / 127.0f);
         /* mod-wheel (CC1) vibrato: depth in semitones, LFO step per sample */
         float vib_depth = ((float)fm->mod_wheel[ch] / 127.0f)
                         * FMSYNTH_VIBRATO_SEMI;
         float lfo_inc   = FMSYNTH_VIBRATO_HZ / rate;

         mod_inc = car_inc * pt->mod_ratio;

         for (n = 0; n < frames; n++)
         {
            float m;
            float s;
            float idx;
            float pmul = 1.0f;

            switch (v->stage)
            {
               case FMSYNTH_ENV_ATTACK:
                  v->env += c_atk;
                  if (v->env >= 1.0f) { v->env = 1.0f; v->stage = FMSYNTH_ENV_DECAY; }
                  break;
               case FMSYNTH_ENV_DECAY:
                  v->env -= c_dec;
                  if (v->env <= pt->c_sus) { v->env = pt->c_sus; v->stage = FMSYNTH_ENV_SUSTAIN; }
                  break;
               case FMSYNTH_ENV_RELEASE:
                  v->env -= c_rel;
                  if (v->env <= 0.0f) { v->env = 0.0f; v->stage = FMSYNTH_ENV_OFF; v->active = 0; }
                  break;
               default:
                  break;
            }

            switch (v->mod_stage)
            {
               case FMSYNTH_ENV_ATTACK:
                  v->mod_env += m_atk;
                  if (v->mod_env >= 1.0f) { v->mod_env = 1.0f; v->mod_stage = FMSYNTH_ENV_DECAY; }
                  break;
               case FMSYNTH_ENV_DECAY:
                  v->mod_env -= m_dec;
                  if (v->mod_env <= pt->m_sus) { v->mod_env = pt->m_sus; v->mod_stage = FMSYNTH_ENV_SUSTAIN; }
                  break;
               default:
                  break;
            }

            idx = pt->mod_index * v->mod_env * vbright;
            m   = fmsynth_sine(v->mod_phase) * idx;
            s   = fmsynth_sine(v->car_phase + m) * v->env * vgain;

            out[n * 2]     += s * panL;
            out[n * 2 + 1] += s * panR;

            if (vib_depth > 0.0f)
            {
               pmul = 1.0f + vib_depth * fmsynth_sine(v->lfo_phase)
                           * FMSYNTH_SEMI_TO_RATE;
               v->lfo_phase += lfo_inc;
               if (v->lfo_phase >= 1.0f) v->lfo_phase -= 1.0f;
            }

            v->car_phase += car_inc * pmul;
            v->mod_phase += mod_inc * pmul;
            if (v->car_phase >= 1.0f) v->car_phase -= 1.0f;
            if (v->mod_phase >= 1.0f) v->mod_phase -= 1.0f;

            if (!v->active)
               break;
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
         float l   = out[n * 2];
         float r   = out[n * 2 + 1];
         float wet = fmsynth_reverb(fm, (l + r) * 0.5f);
         out[n * 2]     = l + wet * FMSYNTH_REVERB_WET;
         out[n * 2 + 1] = r + wet * FMSYNTH_REVERB_WET;
      }
      fm->reverb_tail -= (long)frames;
      return true;
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
