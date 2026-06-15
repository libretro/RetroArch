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

/* fmsynth_patches.h: GM program -> 2-operator FM parameter mapping.
 *
 * This is DATA, reconstructed from the General MIDI program layout and the
 * OPL/GENMIDI instrument conventions (operator ratios, modulation depth,
 * envelope shapes). No instrument sample data is embedded; every timbre is
 * produced by the FM engine from these parameters, so the table is small,
 * license-clean and dependency-free.
 *
 * The 128 GM programs fall into 16 families of 8. v1 assigns one credible
 * FM template per family; per-program refinement (e.g. distinguishing a
 * Rhodes from a grand within the Piano family) is pure ear-tuning against
 * the already-working engine and can be layered on as per-program overrides
 * in fmsynth_patch_for_program() without touching the engine.
 */

#ifndef FMSYNTH_PATCHES_H
#define FMSYNTH_PATCHES_H

/* One 2-op FM patch. Times are in seconds; sustain levels are 0..1. */
typedef struct
{
   float mod_ratio;   /* modulator freq / carrier freq (1.0 = harmonic)    */
   float mod_index;   /* peak modulation depth, in carrier cycles          */
   float c_atk;       /* carrier amplitude envelope                        */
   float c_dec;
   float c_sus;
   float c_rel;
   float m_atk;       /* modulator-index envelope (timbre evolution)       */
   float m_dec;
   float m_sus;       /* fraction of mod_index held during sustain         */
} fmsynth_patch_t;

/* ---- 16 GM family templates -------------------------------------------
 * Index = GM program >> 3. These are deliberately broad-but-musical; the
 * point of v1 is that each family is recognizably different (a pad is not
 * a brass stab), not that every one of 128 programs is perfect.
 * fields: ratio index  c_atk c_dec c_sus c_rel  m_atk m_dec m_sus
 * --------------------------------------------------------------------- */
static const fmsynth_patch_t fmsynth_family[16] =
{
   /* 0  Piano            */ { 1.0f, 1.4f, 0.002f, 0.45f, 0.20f, 0.20f, 0.001f, 0.30f, 0.25f },
   /* 1  Chromatic Perc   */ { 3.5f, 2.2f, 0.001f, 0.35f, 0.00f, 0.15f, 0.001f, 0.18f, 0.10f },
   /* 2  Organ            */ { 1.0f, 0.6f, 0.005f, 0.05f, 0.95f, 0.06f, 0.005f, 0.05f, 0.90f },
   /* 3  Guitar           */ { 1.0f, 1.1f, 0.002f, 0.40f, 0.10f, 0.25f, 0.001f, 0.25f, 0.15f },
   /* 4  Bass             */ { 1.0f, 0.9f, 0.003f, 0.30f, 0.35f, 0.20f, 0.002f, 0.20f, 0.30f },
   /* 5  Strings          */ { 1.0f, 0.45f,0.080f, 0.20f, 0.85f, 0.30f, 0.090f, 0.20f, 0.80f },
   /* 6  Ensemble         */ { 1.0f, 0.6f, 0.060f, 0.20f, 0.85f, 0.30f, 0.070f, 0.20f, 0.80f },
   /* 7  Brass            */ { 1.0f, 1.3f, 0.030f, 0.10f, 0.80f, 0.15f, 0.050f, 0.12f, 0.70f },
   /* 8  Reed             */ { 1.0f, 1.0f, 0.020f, 0.10f, 0.85f, 0.12f, 0.030f, 0.10f, 0.75f },
   /* 9  Pipe             */ { 1.0f, 0.35f,0.030f, 0.08f, 0.90f, 0.10f, 0.040f, 0.10f, 0.85f },
   /* 10 Synth Lead       */ { 1.0f, 1.5f, 0.005f, 0.10f, 0.85f, 0.10f, 0.010f, 0.12f, 0.70f },
   /* 11 Synth Pad        */ { 1.0f, 0.5f, 0.200f, 0.30f, 0.80f, 0.50f, 0.220f, 0.30f, 0.75f },
   /* 12 Synth Effects    */ { 1.5f, 1.2f, 0.050f, 0.30f, 0.60f, 0.40f, 0.060f, 0.30f, 0.55f },
   /* 13 Ethnic           */ { 1.0f, 1.0f, 0.002f, 0.40f, 0.10f, 0.25f, 0.001f, 0.25f, 0.15f },
   /* 14 Percussive       */ { 2.0f, 2.0f, 0.001f, 0.30f, 0.00f, 0.15f, 0.001f, 0.15f, 0.05f },
   /* 15 Sound Effects    */ { 1.0f, 0.8f, 0.050f, 0.40f, 0.50f, 0.40f, 0.050f, 0.30f, 0.50f }
};

/* Resolve a GM program number (0..127) to an FM patch. Per-program overrides
 * for iconic instruments go here as a switch before the family fallback. */
static const fmsynth_patch_t *fmsynth_patch_for_program(unsigned program)
{
   return &fmsynth_family[(program & 0x7F) >> 3];
}

/* ---- GM percussion (MIDI channel 10) -----------------------------------
 * Channel-10 notes are sounds, not pitches. kind: 0 = unused (fall back to
 * a short noise burst), 1 = pitched FM hit, 2 = filtered-noise hit. 'pitch'
 * is a fixed Hz for FM hits. A representative subset of the GM drum map is
 * filled; the rest default to a neutral noise tick.
 * --------------------------------------------------------------------- */
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

#endif /* FMSYNTH_PATCHES_H */
