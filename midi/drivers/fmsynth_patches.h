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

/* fmsynth_patches.h: GM program -> 4-operator FM parameter mapping.
 *
 * This is DATA, reconstructed from the General MIDI program layout and the
 * OPL/DX-style FM conventions (operator ratios, modulation depth, envelopes,
 * routing algorithms). No instrument sample data is embedded; every timbre
 * is produced by the FM engine from these parameters, so the table is small,
 * license-clean and dependency-free.
 *
 * The engine is 4-operator with selectable routing "algorithms" (an idea
 * taken from flexible-routing FM designs such as Themaister's libfmsynth and
 * the classic 4-op Yamaha chips). Each operator has its own frequency ratio,
 * output level (amplitude for carriers, modulation index in carrier cycles
 * for modulators) and ADSR envelope.
 *
 * Most GM families are expressed as a faithful 2-operator subset (algorithm
 * FMSYNTH_ALG_TWO_OP, operators 2 and 3 silent), preserving the prior sound;
 * a few families (e.g. electric piano, strings, brass) use 3-4 operators to
 * show the wider palette. Per-program refinement layers on as overrides in
 * fmsynth_patch_for_program() without touching the engine.
 */

#ifndef FMSYNTH_PATCHES_H
#define FMSYNTH_PATCHES_H

#define FMSYNTH_OPS       4
#define FMSYNTH_NUM_ALGS  8

/* Routing algorithms. modmask[op] is a bitmask of operators that modulate
 * 'op'; only higher-indexed operators may modulate lower ones (the engine
 * evaluates operators 3..0 each sample). carmask is the set of operators
 * summed to the audio output. Feedback is applied to the top operator. */
enum
{
   FMSYNTH_ALG_SERIAL4 = 0, /* 3->2->1->0, carrier 0 (deep FM)              */
   FMSYNTH_ALG_TWO_OP,      /* 1->0, carrier 0 (classic 2-op; 2,3 silent)   */
   FMSYNTH_ALG_DUAL_STACK,  /* (3->2)+(1->0), carriers 0,2                  */
   FMSYNTH_ALG_STACK_PLUS,  /* (3->2->1)+0, carriers 0,1                    */
   FMSYNTH_ALG_BRANCH,      /* 3->1, (1,2)->0, carrier 0                    */
   FMSYNTH_ALG_ADDITIVE,    /* 0+1+2+3 all carriers (additive/organ)        */
   FMSYNTH_ALG_THREE_CAR,   /* (3->2)+1+0, carriers 0,1,2                   */
   FMSYNTH_ALG_Y_INTO_ONE   /* (2->1)+3 ->0, carrier 0                      */
};

static const unsigned char fmsynth_alg_mod[FMSYNTH_NUM_ALGS][FMSYNTH_OPS] =
{
   /* op:        0          1          2      3  */
   /* SERIAL4 */ { 1<<1,      1<<2,      1<<3,  0 },
   /* TWO_OP  */ { 1<<1,      0,         0,     0 },
   /* DUALSTK */ { 1<<1,      0,         1<<3,  0 },
   /* STACK+  */ { 0,         1<<2,      1<<3,  0 },
   /* BRANCH  */ { (1<<1)|(1<<2), 1<<3,  0,     0 },
   /* ADDITIVE*/ { 0,         0,         0,     0 },
   /* 3CAR    */ { 0,         0,         1<<3,  0 },
   /* Y_INTO1 */ { (1<<1)|(1<<3), 1<<2,  0,     0 }
};

static const unsigned char fmsynth_alg_car[FMSYNTH_NUM_ALGS] =
{
   /* SERIAL4 */ 1<<0,
   /* TWO_OP  */ 1<<0,
   /* DUALSTK */ (1<<0)|(1<<2),
   /* STACK+  */ (1<<0)|(1<<1),
   /* BRANCH  */ 1<<0,
   /* ADDITIVE*/ (1<<0)|(1<<1)|(1<<2)|(1<<3),
   /* 3CAR    */ (1<<0)|(1<<1)|(1<<2),
   /* Y_INTO1 */ 1<<0
};

/* One FM operator. Times in seconds; sus level 0..1. 'level' is amplitude
 * for a carrier, or modulation index (in carrier cycles) for a modulator. */
typedef struct
{
   float ratio;   /* frequency ratio to the played note */
   float level;   /* carrier amplitude OR modulator index */
   float atk;
   float dec;
   float sus;
   float rel;
} fmsynth_op_t;

typedef struct
{
   fmsynth_op_t op[FMSYNTH_OPS];
   unsigned char algorithm;  /* FMSYNTH_ALG_*                      */
   float         feedback;   /* top-operator self-feedback (0..~1) */
   float         lfo_rate;   /* per-voice LFO rate in Hz (0 = off) */
   float         lfo_trem;   /* tremolo (amplitude) depth 0..~0.3  */
   float         lfo_vib;    /* vibrato (pitch) depth in semitones */
} fmsynth_patch_t;

/* Convenience: a faithful 2-op patch (operator 1 modulates operator 0). */
#define FMSYNTH_2OP(mratio, mindex, ca, cd, cs, cr, ma, md, ms) \
   { { { 1.0f, 1.0f, (ca), (cd), (cs), (cr) },                  \
       { (mratio), (mindex), (ma), (md), (ms), 0.40f },         \
       { 1.0f, 0.0f, 0.01f, 0.1f, 0.0f, 0.1f },                 \
       { 1.0f, 0.0f, 0.01f, 0.1f, 0.0f, 0.1f } },               \
     FMSYNTH_ALG_TWO_OP, 0.0f, 0.0f, 0.0f, 0.0f }

/* ---- 16 GM family templates (index = GM program >> 3) ------------------ */
static const fmsynth_patch_t fmsynth_family[16] =
{
   /* 0  Piano  - 2-op bright decay */
   FMSYNTH_2OP(1.0f, 1.4f, 0.002f,0.45f,0.20f,0.20f, 0.001f,0.30f,0.25f),
   /* 1  Chromatic Perc - inharmonic bell (3.5 ratio) */
   FMSYNTH_2OP(3.5f, 2.2f, 0.001f,0.35f,0.00f,0.15f, 0.001f,0.18f,0.10f),
   /* 2  Organ - additive 4-carrier (drawbar-ish) */
   { { { 1.0f, 0.6f, 0.005f,0.05f,0.95f,0.06f },
       { 2.0f, 0.4f, 0.005f,0.05f,0.95f,0.06f },
       { 3.0f, 0.25f,0.005f,0.05f,0.95f,0.06f },
       { 4.0f, 0.18f,0.005f,0.05f,0.95f,0.06f } },
     FMSYNTH_ALG_ADDITIVE, 0.0f, 6.0f, 0.10f, 0.0f },   /* slow Leslie-ish tremolo */
   /* 3  Guitar */
   FMSYNTH_2OP(1.0f, 1.1f, 0.002f,0.40f,0.10f,0.25f, 0.001f,0.25f,0.15f),
   /* 4  Bass */
   FMSYNTH_2OP(1.0f, 0.9f, 0.003f,0.30f,0.35f,0.20f, 0.002f,0.20f,0.30f),
   /* 5  Strings - dual stack for a richer, detuned-ish swell */
   { { { 1.0f,  0.85f, 0.080f,0.20f,0.85f,0.30f },
       { 1.0f,  0.45f, 0.090f,0.20f,0.80f,0.40f },
       { 2.005f,0.70f, 0.090f,0.25f,0.80f,0.40f },
       { 3.0f,  0.30f, 0.110f,0.25f,0.75f,0.40f } },
     FMSYNTH_ALG_DUAL_STACK, 0.0f, 5.0f, 0.07f, 0.12f }, /* gentle string vibrato+tremolo */
   /* 6  Ensemble */
   FMSYNTH_2OP(1.0f, 0.6f, 0.060f,0.20f,0.85f,0.30f, 0.070f,0.20f,0.80f),
   /* 7  Brass - stack-plus with bite, light feedback */
   { { { 1.0f, 1.0f, 0.030f,0.10f,0.80f,0.15f },
       { 1.0f, 0.9f, 0.035f,0.12f,0.75f,0.18f },
       { 1.0f, 1.1f, 0.040f,0.12f,0.70f,0.18f },
       { 2.0f, 0.8f, 0.050f,0.15f,0.65f,0.20f } },
     FMSYNTH_ALG_STACK_PLUS, 0.35f, 5.5f, 0.05f, 0.10f }, /* brass vibrato */
   /* 8  Reed */
   FMSYNTH_2OP(1.0f, 1.0f, 0.020f,0.10f,0.85f,0.12f, 0.030f,0.10f,0.75f),
   /* 9  Pipe */
   FMSYNTH_2OP(1.0f, 0.35f,0.030f,0.08f,0.90f,0.10f, 0.040f,0.10f,0.85f),
   /* 10 Synth Lead - serial 3-op for a fat, evolving lead */
   { { { 1.0f, 1.0f, 0.005f,0.10f,0.85f,0.10f },
       { 1.0f, 1.2f, 0.010f,0.12f,0.70f,0.15f },
       { 2.0f, 1.0f, 0.020f,0.15f,0.60f,0.20f },
       { 1.0f, 0.0f, 0.01f, 0.1f, 0.0f, 0.1f } },
     FMSYNTH_ALG_STACK_PLUS, 0.30f, 6.0f, 0.0f, 0.15f },  /* lead vibrato */
   /* 11 Synth Pad */
   FMSYNTH_2OP(1.0f, 0.5f, 0.200f,0.30f,0.80f,0.50f, 0.220f,0.30f,0.75f),
   /* 12 Synth Effects */
   FMSYNTH_2OP(1.5f, 1.2f, 0.050f,0.30f,0.60f,0.40f, 0.060f,0.30f,0.55f),
   /* 13 Ethnic */
   FMSYNTH_2OP(1.0f, 1.0f, 0.002f,0.40f,0.10f,0.25f, 0.001f,0.25f,0.15f),
   /* 14 Percussive - inharmonic */
   FMSYNTH_2OP(2.0f, 2.0f, 0.001f,0.30f,0.00f,0.15f, 0.001f,0.15f,0.05f),
   /* 15 Sound Effects */
   FMSYNTH_2OP(1.0f, 0.8f, 0.050f,0.40f,0.50f,0.40f, 0.050f,0.30f,0.50f)
};

/* Resolve a GM program (0..127) to a patch. Per-program overrides for iconic
 * instruments go here as a switch before the family fallback. */
static const fmsynth_patch_t *fmsynth_patch_for_program(unsigned program)
{
   return &fmsynth_family[(program & 0x7F) >> 3];
}

/* ---- GM channel-10 percussion map -------------------------------------- */

#define FMSYNTH_DRUM_FM    1
#define FMSYNTH_DRUM_NOISE 2

typedef struct
{
   uint8_t kind;
   float   pitch;   /* Hz for FM hits                       */
   float   decay;   /* amplitude decay time in seconds      */
   float   index;   /* FM mod index (FM hits) / tone (noise)*/
} fmsynth_drum_t;

/* Indexed by MIDI note 35..81 (GM percussion range). */
#define FMSYNTH_DRUM_LO 35
#define FMSYNTH_DRUM_HI 81

static const fmsynth_drum_t fmsynth_drums[FMSYNTH_DRUM_HI - FMSYNTH_DRUM_LO + 1] =
{
   /* 35 Acoustic Bass Drum */ { FMSYNTH_DRUM_FM,    55.0f, 0.18f, 3.0f },
   /* 36 Bass Drum 1        */ { FMSYNTH_DRUM_FM,    50.0f, 0.16f, 3.5f },
   /* 37 Side Stick         */ { FMSYNTH_DRUM_NOISE, 0.0f,  0.05f, 0.0f },
   /* 38 Acoustic Snare     */ { FMSYNTH_DRUM_NOISE, 0.0f,  0.12f, 0.0f },
   /* 39 Hand Clap          */ { FMSYNTH_DRUM_NOISE, 0.0f,  0.10f, 0.0f },
   /* 40 Electric Snare     */ { FMSYNTH_DRUM_NOISE, 0.0f,  0.12f, 0.0f },
   /* 41 Low Floor Tom      */ { FMSYNTH_DRUM_FM,    90.0f, 0.22f, 2.0f },
   /* 42 Closed Hi-Hat      */ { FMSYNTH_DRUM_NOISE, 0.0f,  0.03f, 0.0f },
   /* 43 High Floor Tom     */ { FMSYNTH_DRUM_FM,   110.0f, 0.22f, 2.0f },
   /* 44 Pedal Hi-Hat       */ { FMSYNTH_DRUM_NOISE, 0.0f,  0.04f, 0.0f },
   /* 45 Low Tom            */ { FMSYNTH_DRUM_FM,   130.0f, 0.22f, 2.0f },
   /* 46 Open Hi-Hat        */ { FMSYNTH_DRUM_NOISE, 0.0f,  0.25f, 0.0f },
   /* 47 Low-Mid Tom        */ { FMSYNTH_DRUM_FM,   160.0f, 0.22f, 2.0f },
   /* 48 Hi-Mid Tom         */ { FMSYNTH_DRUM_FM,   190.0f, 0.20f, 2.0f },
   /* 49 Crash Cymbal 1     */ { FMSYNTH_DRUM_NOISE, 0.0f,  0.60f, 0.0f },
   /* 50 High Tom           */ { FMSYNTH_DRUM_FM,   230.0f, 0.20f, 2.0f },
   /* 51 Ride Cymbal 1      */ { FMSYNTH_DRUM_NOISE, 0.0f,  0.40f, 0.0f },
   /* 52 Chinese Cymbal     */ { FMSYNTH_DRUM_NOISE, 0.0f,  0.50f, 0.0f },
   /* 53 Ride Bell          */ { FMSYNTH_DRUM_FM,   800.0f, 0.30f, 4.0f },
   /* 54 Tambourine         */ { FMSYNTH_DRUM_NOISE, 0.0f,  0.10f, 0.0f },
   /* 55 Splash Cymbal      */ { FMSYNTH_DRUM_NOISE, 0.0f,  0.35f, 0.0f },
   /* 56 Cowbell            */ { FMSYNTH_DRUM_FM,   540.0f, 0.15f, 1.5f },
   /* 57 Crash Cymbal 2     */ { FMSYNTH_DRUM_NOISE, 0.0f,  0.55f, 0.0f },
   /* 58 Vibraslap          */ { FMSYNTH_DRUM_NOISE, 0.0f,  0.20f, 0.0f },
   /* 59 Ride Cymbal 2      */ { FMSYNTH_DRUM_NOISE, 0.0f,  0.40f, 0.0f },
   /* 60 Hi Bongo           */ { FMSYNTH_DRUM_FM,   260.0f, 0.12f, 1.5f },
   /* 61 Low Bongo          */ { FMSYNTH_DRUM_FM,   200.0f, 0.14f, 1.5f },
   /* 62 Mute Hi Conga      */ { FMSYNTH_DRUM_FM,   240.0f, 0.10f, 1.5f },
   /* 63 Open Hi Conga      */ { FMSYNTH_DRUM_FM,   220.0f, 0.16f, 1.5f },
   /* 64 Low Conga          */ { FMSYNTH_DRUM_FM,   180.0f, 0.18f, 1.5f },
   /* 65 High Timbale       */ { FMSYNTH_DRUM_FM,   300.0f, 0.14f, 1.8f },
   /* 66 Low Timbale        */ { FMSYNTH_DRUM_FM,   250.0f, 0.16f, 1.8f },
   /* 67 High Agogo         */ { FMSYNTH_DRUM_FM,   700.0f, 0.12f, 2.0f },
   /* 68 Low Agogo          */ { FMSYNTH_DRUM_FM,   560.0f, 0.14f, 2.0f },
   /* 69 Cabasa             */ { FMSYNTH_DRUM_NOISE, 0.0f,  0.06f, 0.0f },
   /* 70 Maracas            */ { FMSYNTH_DRUM_NOISE, 0.0f,  0.05f, 0.0f },
   /* 71 Short Whistle      */ { FMSYNTH_DRUM_FM,  1800.0f, 0.10f, 1.0f },
   /* 72 Long Whistle       */ { FMSYNTH_DRUM_FM,  1800.0f, 0.30f, 1.0f },
   /* 73 Short Guiro        */ { FMSYNTH_DRUM_NOISE, 0.0f,  0.06f, 0.0f },
   /* 74 Long Guiro         */ { FMSYNTH_DRUM_NOISE, 0.0f,  0.20f, 0.0f },
   /* 75 Claves             */ { FMSYNTH_DRUM_FM,   2500.0f,0.05f, 0.5f },
   /* 76 Hi Wood Block      */ { FMSYNTH_DRUM_FM,   1000.0f,0.06f, 0.8f },
   /* 77 Low Wood Block     */ { FMSYNTH_DRUM_FM,   800.0f, 0.07f, 0.8f },
   /* 78 Mute Cuica         */ { FMSYNTH_DRUM_FM,   500.0f, 0.08f, 1.0f },
   /* 79 Open Cuica         */ { FMSYNTH_DRUM_FM,   450.0f, 0.16f, 1.0f },
   /* 80 Mute Triangle      */ { FMSYNTH_DRUM_FM,  4000.0f, 0.08f, 0.5f },
   /* 81 Open Triangle      */ { FMSYNTH_DRUM_FM,  4000.0f, 0.40f, 0.5f }
};

static const fmsynth_drum_t *fmsynth_drum_for_note(uint8_t note)
{
   if (note < FMSYNTH_DRUM_LO || note > FMSYNTH_DRUM_HI)
      return NULL;
   return &fmsynth_drums[note - FMSYNTH_DRUM_LO];
}

#endif
