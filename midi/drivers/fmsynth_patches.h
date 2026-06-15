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

/* fmsynth_patches.h: GM program -> FM parameter mapping.
 *
 * The engine is modelled on Themaister's libfmsynth (MIT): up to 8 operators
 * with a free modulation matrix (any operator may modulate any other, even
 * itself), per-operator envelope, velocity sensitivity, keyboard scaling, a
 * per-voice sine LFO modulating both amplitude (tremolo) and frequency
 * (vibrato), and per-carrier stereo panning. Rather than store a dense 8x8
 * matrix per patch (verbose under C89), the modulation routing is held as a
 * sparse connection list - the same information, one entry per non-zero
 * matrix cell.
 *
 * This is DATA only: no instrument samples are embedded, every timbre is
 * produced by the engine from these parameters, so the table is small,
 * license-clean and dependency-free. Most GM families use 2-4 operators;
 * unused operators have amp 0 and cost nothing audible.
 */

#ifndef FMSYNTH_PATCHES_H
#define FMSYNTH_PATCHES_H

#define FMSYNTH_OPS       8   /* operators per voice (libfmsynth parity)     */
#define FMSYNTH_MAX_CONN  12  /* non-zero modulation-matrix cells per patch  */

/* One FM operator. Times in seconds; sus 0..1. 'amp' is carrier amplitude or
 * (for a modulator) its output level, which the modulation matrix then scales
 * into the target operator's phase, in carrier cycles. */
typedef struct
{
   float ratio;       /* frequency ratio to the played note            */
   float amp;         /* output level (carrier amplitude / mod source) */
   float pan;         /* carrier pan, -1 = left .. +1 = right          */
   float atk;
   float dec;
   float sus;
   float rel;
   float vel_sens;    /* velocity -> level sensitivity, 0..1           */
   float key_scale;   /* keyboard rate scaling exponent (per octave)   */
   float lfo_amp;     /* voice-LFO -> amplitude (tremolo) depth        */
   float lfo_freq;    /* voice-LFO -> frequency (vibrato) depth, semis */
   unsigned char carrier; /* 1 = summed to output                      */
} fmsynth_op_t;

/* One cell of the modulation matrix: operator 'from' modulates operator 'to'
 * with the given depth (in carrier cycles). to == from is self-feedback. */
typedef struct
{
   unsigned char to;
   unsigned char from;
   float         depth;
} fmsynth_conn_t;

typedef struct
{
   fmsynth_op_t   op[FMSYNTH_OPS];
   fmsynth_conn_t conn[FMSYNTH_MAX_CONN];
   unsigned char  nconn;     /* active entries in conn[]            */
   float          lfo_rate;  /* per-voice LFO rate in Hz (0 = off)  */
} fmsynth_patch_t;

/* Operator constructors keep the family table readable.
 * KEY_SCALE 0.0417 ~ doubles decay/release every two octaves. */
#define OPC(r,a,p, at,dc,su,rl) \
   { (r),(a),(p), (at),(dc),(su),(rl), 0.6f, 0.0417f, 0.0f, 0.0f, 1 }
#define OPM(r,a, at,dc,su,rl) \
   { (r),(a),0.0f, (at),(dc),(su),(rl), 1.0f, 0.0417f, 0.0f, 0.0f, 0 }
/* LFO-bearing carrier / modulator (tremolo la, vibrato lf semitones) */
#define OPCL(r,a,p, at,dc,su,rl, la,lf) \
   { (r),(a),(p), (at),(dc),(su),(rl), 0.6f, 0.0417f, (la),(lf), 1 }
#define OPML(r,a, at,dc,su,rl, la,lf) \
   { (r),(a),0.0f, (at),(dc),(su),(rl), 1.0f, 0.0417f, (la),(lf), 0 }
#define OPX  { 1.0f,0.0f,0.0f, 0.01f,0.1f,0.0f,0.1f, 0.0f,0.0417f,0.0f,0.0f, 0 }
#define OP2X OPX,OPX
#define OP4X OPX,OPX,OPX,OPX
#define OP6X OPX,OPX,OPX,OPX,OPX,OPX

/* A faithful 2-operator patch (operator 1 modulates operator 0). */
#define FMSYNTH_2OP(mr, mi, ca,cd,cs,cr, ma,md,ms) \
   { { OPC(1.0f, 1.0f, 0.0f, (ca),(cd),(cs),(cr)),  \
       OPM((mr), (mi), (ma),(md),(ms),0.40f),       \
       OP6X },                                       \
     { { 0, 1, 1.0f } }, 1, 0.0f }

/* ---- 16 GM family templates (index = GM program >> 3) ------------------ */
static const fmsynth_patch_t fmsynth_family[16] =
{
   /* 0  Piano */
   FMSYNTH_2OP(1.0f, 1.4f, 0.002f,0.45f,0.20f,0.20f, 0.001f,0.30f,0.25f),
   /* 1  Chromatic Perc - inharmonic bell */
   FMSYNTH_2OP(3.5f, 2.2f, 0.001f,0.35f,0.00f,0.15f, 0.001f,0.18f,0.10f),
   /* 2  Organ - 4 additive carriers, slow tremolo, gently spread */
   { { OPCL(1.0f, 0.6f, -0.15f, 0.005f,0.05f,0.95f,0.06f, 0.10f,0.0f),
       OPCL(2.0f, 0.4f, +0.10f, 0.005f,0.05f,0.95f,0.06f, 0.10f,0.0f),
       OPCL(3.0f, 0.25f,-0.10f, 0.005f,0.05f,0.95f,0.06f, 0.10f,0.0f),
       OPCL(4.0f, 0.18f,+0.15f, 0.005f,0.05f,0.95f,0.06f, 0.10f,0.0f),
       OP4X },
     { { 0, 0, 0.0f } }, 0, 6.0f },
   /* 3  Guitar */
   FMSYNTH_2OP(1.0f, 1.1f, 0.002f,0.40f,0.10f,0.25f, 0.001f,0.25f,0.15f),
   /* 4  Bass */
   FMSYNTH_2OP(1.0f, 0.9f, 0.003f,0.30f,0.35f,0.20f, 0.002f,0.20f,0.30f),
   /* 5  Strings - two stacks panned L/R, gentle vibrato + detune shimmer */
   { { OPCL(1.0f,  0.85f,-0.30f, 0.080f,0.20f,0.85f,0.30f, 0.06f,0.12f),
       OPM (1.0f,  0.45f,        0.090f,0.20f,0.80f,0.40f),
       OPCL(2.005f,0.70f,+0.30f, 0.090f,0.25f,0.80f,0.40f, 0.06f,0.12f),
       OPM (3.0f,  0.30f,        0.110f,0.25f,0.75f,0.40f),
       OP4X },
     { { 0, 1, 1.0f }, { 2, 3, 1.0f } }, 2, 5.0f },
   /* 6  Ensemble */
   FMSYNTH_2OP(1.0f, 0.6f, 0.060f,0.20f,0.85f,0.30f, 0.070f,0.20f,0.80f),
   /* 7  Brass - 3-deep stack into two carriers, feedback, vibrato */
   { { OPCL(1.0f, 1.0f, 0.0f, 0.030f,0.10f,0.80f,0.15f, 0.05f,0.10f),
       OPCL(1.0f, 0.9f, 0.0f, 0.035f,0.12f,0.75f,0.18f, 0.05f,0.10f),
       OPM (1.0f, 1.1f,       0.040f,0.12f,0.70f,0.18f),
       OPM (2.0f, 0.8f,       0.050f,0.15f,0.65f,0.20f),
       OP4X },
     { { 0, 2, 1.0f }, { 1, 2, 0.6f }, { 2, 3, 1.0f }, { 3, 3, 0.35f } },
     4, 5.5f },
   /* 8  Reed */
   FMSYNTH_2OP(1.0f, 1.0f, 0.020f,0.10f,0.85f,0.12f, 0.030f,0.10f,0.75f),
   /* 9  Pipe */
   FMSYNTH_2OP(1.0f, 0.35f,0.030f,0.08f,0.90f,0.10f, 0.040f,0.10f,0.85f),
   /* 10 Synth Lead - serial stack, feedback, vibrato */
   { { OPCL(1.0f, 1.0f, 0.0f, 0.005f,0.10f,0.85f,0.10f, 0.0f,0.15f),
       OPM (1.0f, 1.2f,       0.010f,0.12f,0.70f,0.15f),
       OPM (2.0f, 1.0f,       0.020f,0.15f,0.60f,0.20f),
       OPX,OPX,OPX,OPX,OPX },
     { { 0, 1, 1.0f }, { 1, 2, 1.0f }, { 2, 2, 0.30f } }, 3, 6.0f },
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
