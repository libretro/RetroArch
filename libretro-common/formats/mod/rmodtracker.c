/* rmodtracker -- ProTracker MOD / Scream Tracker 3 S3M / FastTracker 2 XM
 * replayer for libretro-common, with twin output pipelines: the native
 * fixed-point mixer (integer end to end, bit-identical output across
 * compilers, flags and architectures) and a float mixer sharing the
 * same integer sequencer and resampler positions, so musical content
 * is deterministic in both modes.
 *
 * Replay engine based on ibxm/ac 20191214 (c) Martin Cameron,
 * https://github.com/martincameron/micromod -- BSD licence retained at
 * the end of this file.
 *
 * What it implements: ProTracker MOD (including multi-channel
 * variants, the OKTA/OCTA/CD81/FA08 8-channel tags, Startrekker
 * FLT8/EXO8 paired patterns, Startrekker AM EXO4 and untagged
 * 15-instrument Soundtracker modules),
 * Scream Tracker 3 S3M and FastTracker 2 XM modules with
 * their effect sets, instrument envelopes and 8/16-bit samples --
 * mono everywhere, stereo in S3M -- plus ModPlug 4-bit ADPCM packed
 * samples in XM and S3M, played to interleaved stereo s16 or float at
 * the engine rate, with duration calculation and rewind.
 *
 * What it does not implement: Impulse Tracker (IT) and other later
 * formats including IT's sample compression, and MIDI-style external
 * control. Seeking is forward-walking (a tracker has no seek table)
 * but snapshot-accelerated after the first seek.
 */
#include <limits.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <formats/rmodtracker.h>

#if defined(__GNUC__) || defined(__clang__)
#define RMT_RESTRICT __restrict__
#elif defined(_MSC_VER)
#define RMT_RESTRICT __restrict
#else
#define RMT_RESTRICT
#endif

struct data {
	char *buffer;
	int length;
};

struct sample {
	char name[ 32 ];
	int loop_start, loop_length;
	/* When stereo is set, data holds interleaved L,R pairs and every
	   frame-counted field (loop points, indices) stays in frames; only
	   the fetch multiplies by two. */
	/* glob_vol carries IT's always-applied sample x instrument global
	   volume, stored +1 so calloc's zero means the 64 every other
	   format implies; the volume column overrides the default volume
	   but never this. */
	short volume, panning, rel_note, fine_tune, stereo, glob_vol, *data;
};

struct envelope {
	char enabled, sustain, looped, num_points;
	short sustain_tick, loop_start_tick, loop_end_tick;
	short points_tick[ 16 ], points_ampl[ 16 ];
};

struct instrument {
	int num_samples, vol_fadeout;
	char name[ 32 ], key_to_sample[ 97 ];
	/* IT new-note action: 0 cut (every other format's behaviour and
	   calloc's default), 1 continue, 2 note off, 3 fade. dct/dca are
	   the duplicate check type and action. */
	char nna, dct, dca;
	/* IT initial filter cutoff/resonance (bit 7 = set) and the third
	   envelope, which flag bit 7 repurposes as a filter envelope. */
	char ifc, ifr, pitch_is_filter;
	struct envelope pitch_env;
	char vib_type, vib_sweep, vib_depth, vib_rate;
	struct envelope vol_env, pan_env;
	struct sample *samples;
};

struct pattern {
	int num_channels, num_rows;
	char *data;
};

struct module {
	char name[ 32 ];
	int num_channels, num_instruments;
	int num_patterns, sequence_len, restart_pos;
	/* IT: per-channel default volumes (0..64), NULL elsewhere, and a
	   flag for the effect encodings whose parameter conventions
	   diverge between ST3 and IT (Xxx panning). */
	unsigned char *default_chan_vol;
	char it_effects;
	int default_gvol, default_speed, default_tempo, c2_rate, gain;
	int linear_periods, fast_vol_slides;
	unsigned char *default_panning, *sequence;
	struct pattern *patterns;
	struct instrument *instruments;
};






const char *IBXM_VERSION = "ibxm/ac mod/xm/s3m replay 20191214 (c)mumart@gmail.com";

static const int FP_SHIFT = 15, FP_ONE = 32768, FP_MASK = 32767;

static const int exp2_table[] = {
	32768, 32946, 33125, 33305, 33486, 33667, 33850, 34034,
	34219, 34405, 34591, 34779, 34968, 35158, 35349, 35541,
	35734, 35928, 36123, 36319, 36516, 36715, 36914, 37114,
	37316, 37518, 37722, 37927, 38133, 38340, 38548, 38757,
	38968, 39180, 39392, 39606, 39821, 40037, 40255, 40473,
	40693, 40914, 41136, 41360, 41584, 41810, 42037, 42265,
	42495, 42726, 42958, 43191, 43425, 43661, 43898, 44137,
	44376, 44617, 44859, 45103, 45348, 45594, 45842, 46091,
	46341, 46593, 46846, 47100, 47356, 47613, 47871, 48131,
	48393, 48655, 48920, 49185, 49452, 49721, 49991, 50262,
	50535, 50810, 51085, 51363, 51642, 51922, 52204, 52488,
	52773, 53059, 53347, 53637, 53928, 54221, 54515, 54811,
	55109, 55408, 55709, 56012, 56316, 56622, 56929, 57238,
	57549, 57861, 58176, 58491, 58809, 59128, 59449, 59772,
	60097, 60423, 60751, 61081, 61413, 61746, 62081, 62419,
	62757, 63098, 63441, 63785, 64132, 64480, 64830, 65182,
	65536
};

static const short sine_table[] = {
	  0,  24,  49,  74,  97, 120, 141, 161, 180, 197, 212, 224, 235, 244, 250, 253,
	255, 253, 250, 244, 235, 224, 212, 197, 180, 161, 141, 120,  97,  74,  49,  24
};

struct note {
	unsigned char key, instrument, volume, effect, param;
};

struct channel {
	struct replay *replay;
	struct instrument *instrument;
	struct sample *sample;
	struct note note;
	int id, key_on, random_seed, pl_row;
	int sample_off, sample_idx, sample_fra, freq, ampl, pann;
	int volume, panning, fadeout_vol, vol_env_tick, pan_env_tick;
	int chan_vol, cvol_slide_param, tempo_slide_param, high_offset;
	/* IT resonant lowpass: parameters, cached coefficient key, Q14
	   coefficients as IT2 computes them, and left/right filter memory
	   (the filter is linear, so filtering after panning with one
	   coefficient set equals IT2's pre-pan mono filtering). */
	int flt_cutoff, flt_q, flt_env, flt_key, flt_on;
	int flt_a, flt_b, flt_c;
	int flt_y1l, flt_y2l, flt_y1r, flt_y2r;
	int flt_errl, flt_errr;
	int pitch_env_tick;
	int period, porta_period, retrig_count, fx_count, av_count;
	int porta_up_param, porta_down_param, tone_porta_param, offset_param;
	int fine_porta_up_param, fine_porta_down_param, xfine_porta_param;
	int arpeggio_param, vol_slide_param, gvol_slide_param, pan_slide_param;
	int fine_vslide_up_param, fine_vslide_down_param;
	int retrig_volume, retrig_ticks, tremor_on_ticks, tremor_off_ticks;
	int vibrato_type, vibrato_phase, vibrato_speed, vibrato_depth;
	int tremolo_type, tremolo_phase, tremolo_speed, tremolo_depth;
	int tremolo_add, vibrato_add, arpeggio_add;
};

/* Background voices for IT new-note actions: a note whose
   instrument's NNA is not "cut" keeps ringing here when its host
   channel takes a new note. A ghost is an ordinary struct channel
   that runs only the envelope, fade and mixing subset - never
   effects - so the amplitude model and both resamplers apply to it
   unchanged. The pool is global and the quietest voice is stolen
   when it fills. */
#define RMT_NUM_GHOSTS 32

struct replay {
	int sample_rate, interpolation, global_vol;
	int seq_pos, break_pos, row, next_row, tick;
	int speed, tempo, pl_count, pl_chan;
	/* One block holding ramp_buf, channels, ghosts and the two filter
	   scratch buffers; the pointers below are views into it. */
	unsigned char *arena;
	int *ramp_buf;
	int *flt_buf;
	float *flt_buf_f;
	char **play_count;
	struct channel *channels;
	struct channel *ghosts;
	struct module *module;
};

static int exp_2( int x ) {
	int c, m, y;
	int x0 = ( x & FP_MASK ) >> ( FP_SHIFT - 7 );
	c = exp2_table[ x0 ];
	m = exp2_table[ x0 + 1 ] - c;
	y = ( m * ( x & ( FP_MASK >> 7 ) ) >> 8 ) + c;
	return ( y << FP_SHIFT ) >> ( FP_SHIFT - ( x >> FP_SHIFT ) );
}

static int log_2( int x ) {
	int step;
	int y = 16 << FP_SHIFT;
	for( step = y; step > 0; step >>= 1 ) {
		if( exp_2( y - step ) >= x ) {
			y -= step;
		}
	}
	return y;
}

/* Every accessor below clamps the offset at both ends.
 *
 * The upper bound was always checked; the lower was not, and a negative
 * offset sailed straight through "offset < data->length" into a read
 * before the buffer. Offsets are computed from the file: the XM loader
 * advances by a self-declared header size, so a module declaring
 * 0x7FFFFFFF overflows the int and lands somewhere negative. Reject it
 * here rather than at each of the places one can be produced. */
static char* data_ascii( struct data *data, int offset, int length, char *dest ) {
	int idx, chr;
	memset( dest, 32, length );
	if( offset < 0 || offset > data->length ) {
		offset = data->length;
	}
	if( ( unsigned int ) offset + length > ( unsigned int ) data->length ) {
		length = data->length - offset;
	}
	for( idx = 0; idx < length; idx++ ) {
		chr = data->buffer[ offset + idx ] & 0xFF;
		if( chr > 32 ) {
			dest[ idx ] = chr;
		}
	}
	return dest;
}

static int data_s8( struct data *data, int offset ) {
	int value = 0;
	if( offset >= 0 && offset < data->length ) {
		value = data->buffer[ offset ];
		value = ( value & 0x7F ) - ( value & 0x80 );
	}
	return value;
}

static int data_u8( struct data *data, int offset ) {
	int value = 0;
	if( offset >= 0 && offset < data->length ) {
		value = data->buffer[ offset ] & 0xFF;
	}
	return value;
}

static int data_u16be( struct data *data, int offset ) {
	int value = 0;
	if( offset >= 0 && offset + 1 < data->length ) {
		value = ( ( data->buffer[ offset ] & 0xFF ) << 8 )
			| ( data->buffer[ offset + 1 ] & 0xFF );
	}
	return value;
}

static int data_u16le( struct data *data, int offset ) {
	int value = 0;
	if( offset >= 0 && offset + 1 < data->length ) {
		value = ( data->buffer[ offset ] & 0xFF )
			| ( ( data->buffer[ offset + 1 ] & 0xFF ) << 8 );
	}
	return value;
}

static unsigned int data_u32le( struct data *data, int offset ) {
	unsigned int value = 0;
	if( offset >= 0 && offset + 3 < data->length ) {
		value = ( unsigned int ) ( data->buffer[ offset ] & 0xFF )
			| ( ( unsigned int ) ( data->buffer[ offset + 1 ] & 0xFF ) << 8 )
			| ( ( unsigned int ) ( data->buffer[ offset + 2 ] & 0xFF ) << 16 )
			| ( ( unsigned int ) ( data->buffer[ offset + 3 ] & 0xFF ) << 24 );
	}
	return value;
}

static void data_sam_s8( struct data *data, int offset, int count, short *dest ) {
	int idx, amp, length = data->length;
	char *buffer = data->buffer;
	if( offset < 0 || offset > length ) {
		offset = length;
	}
	if( offset + count > length ) {
		count = length - offset;
	}
	for( idx = 0; idx < count; idx++ ) {
		amp = ( buffer[ offset + idx ] & 0xFF ) << 8;
		dest[ idx ] = ( amp & 0x7FFF ) - ( amp & 0x8000 );
	}
}

static void data_sam_s16le( struct data *data, int offset, int count, short *dest ) {
	int idx, amp, length = data->length;
	char *buffer = data->buffer;
	if( offset < 0 || offset > length ) {
		offset = length;
	}
	if( offset + count * 2 > length ) {
		count = ( length - offset ) / 2;
	}
	for( idx = 0; idx < count; idx++ ) {
		amp = ( buffer[ offset + idx * 2 ] & 0xFF )
			| ( ( buffer[ offset + idx * 2 + 1 ] & 0xFF ) << 8 );
		dest[ idx ] = ( amp & 0x7FFF ) - ( amp & 0x8000 );
	}
}

/* ModPlug 4-bit ADPCM: a 16-entry signed delta table followed by a
   nibble stream, low nibble first, accumulated with 8-bit wrap into
   signed 8-bit PCM. The output is absolute, so callers skip their
   delta or unsigned conversions. Compressed size on disk is
   16 + ( count + 1 ) / 2 bytes for count output samples. */
static void data_sam_adpcm4( struct data *data, int offset, int count, short *dest ) {
	int idx, byt, amp, acc = 0;
	int tab[ 16 ];
	for( idx = 0; idx < 16; idx++ ) {
		byt = data_u8( data, offset + idx );
		tab[ idx ] = ( byt & 0x7F ) - ( byt & 0x80 );
	}
	offset += 16;
	for( idx = 0; idx < count; idx++ ) {
		byt = data_u8( data, offset + ( idx >> 1 ) );
		if( idx & 1 ) {
			byt = byt >> 4;
		}
		acc = ( acc + tab[ byt & 0xF ] ) & 0xFF;
		amp = acc << 8;
		amp = ( amp & 0x7FFF ) - ( amp & 0x8000 );
		dest[ idx ] = ( short ) amp;
	}
}

static int envelope_next_tick( struct envelope *envelope, int tick, int key_on ) {
	tick++;
	if( envelope->looped && tick >= envelope->loop_end_tick ) {
		tick = envelope->loop_start_tick;
	}
	if( envelope->sustain && key_on && tick >= envelope->sustain_tick ) {
		tick = envelope->sustain_tick;
	}
	return tick;
}

static int envelope_calculate_ampl( struct envelope *envelope, int tick ) {
	int idx, point, dt, da;
	int ampl = envelope->points_ampl[ envelope->num_points - 1 ];
	if( tick < envelope->points_tick[ envelope->num_points - 1 ] ) {
		point = 0;
		for( idx = 1; idx < envelope->num_points; idx++ ) {
			if( envelope->points_tick[ idx ] <= tick ) {
				point = idx;
			}
		}
		dt = envelope->points_tick[ point + 1 ] - envelope->points_tick[ point ];
		ampl = envelope->points_ampl[ point ];
		if( dt > 0 ) {
			/* Guard against a malformed envelope whose adjacent points share
			 * a tick: the division below would divide by zero. */
			da = envelope->points_ampl[ point + 1 ] - envelope->points_ampl[ point ];
			ampl += ( ( da * ( 1 << 24 ) ) / dt ) * ( tick - envelope->points_tick[ point ] ) >> 24;
		}
	}
	return ampl;
}

static void sample_ping_pong( struct sample *sample ) {
	int idx;
	int loop_start = sample->loop_start;
	int loop_length = sample->loop_length;
	int loop_end = loop_start + loop_length;
	short *sample_data = sample->data;
	short *new_data = calloc( loop_end + loop_length + 1, sizeof( short ) );
	if( new_data ) {
		memcpy( new_data, sample_data, loop_end * sizeof( short ) );
		for( idx = 0; idx < loop_length; idx++ ) {
			new_data[ loop_end + idx ] = sample_data[ loop_end - idx - 1 ];
		}
		free( sample->data );
		sample->data = new_data;
		sample->loop_length *= 2;
		sample->data[ loop_start + sample->loop_length ] = sample->data[ loop_start ];
	}
}

/* Deallocate the specified module. */
static void dispose_module( struct module *module ) {
	int idx, sam;
	struct instrument *instrument;
	free( module->default_panning );
	free( module->default_chan_vol );
	free( module->sequence );
	if( module->patterns ) {
		for( idx = 0; idx < module->num_patterns; idx++ ) {
			free( module->patterns[ idx ].data );
		}
		free( module->patterns );
	}
	if( module->instruments ) {
		for( idx = 0; idx <= module->num_instruments; idx++ ) {
			instrument = &module->instruments[ idx ];
			if( instrument->samples ) {
				for( sam = 0; sam < instrument->num_samples; sam++ ) {
					free( instrument->samples[ sam ].data );
				}
				free( instrument->samples );
			}
		}
		free( module->instruments );
	}
	free( module );
}

static struct module* module_load_xm( struct data *data, char *message ) {
	int delta_env, offset, next_offset, idx, entry;
	int num_rows, num_notes, pat_data_len, pat_data_offset;
	int sam, sam_head_offset, sam_data_bytes, sam_data_samples;
	int num_samples, sam_loop_start, sam_loop_length, amp;
	int note, flags, key, ins, vol, fxc, fxp;
	int point, point_tick, point_offset;
	int looped, ping_pong, sixteen_bit, adpcm;
	char ascii[ 16 ], *pattern_data;
	struct instrument *instrument;
	struct sample *sample;
	struct module *module = calloc( 1, sizeof( struct module ) );
	if( module ) {
		if( data_u16le( data, 58 ) != 0x0104 ) {
			strcpy( message, "XM format version must be 0x0104!" );
			dispose_module( module );
			return NULL;
		}
		data_ascii( data, 17, 20, module->name );
		delta_env = !memcmp( data_ascii( data, 38, 15, ascii ), "DigiBooster Pro", 15 );
		offset = 60 + data_u32le( data, 60 );
		module->sequence_len = data_u16le( data, 64 );
		if( module->sequence_len < 1 ) {
			/* A zero-length sequence (usually a truncated file whose
			   header bytes read as zero) would have the replay index
			   an empty play-count table; entry 0 of the zeroed
			   sequence is pattern 0, a safe degenerate module. */
			module->sequence_len = 1;
		}
		module->restart_pos = data_u16le( data, 66 );
		module->num_channels = data_u16le( data, 68 );
		module->num_patterns = data_u16le( data, 70 );
		if( module->num_patterns < 1 ) {
			/* Zero patterns (a truncated header) leaves every sequence
			   entry unmappable and the walk to the next playable
			   position spinning; the loop below synthesises an empty
			   pattern from out-of-range data, which is a playable
			   degenerate module. */
			module->num_patterns = 1;
		}
		module->num_instruments = data_u16le( data, 72 );
		module->linear_periods = data_u16le( data, 74 ) & 0x1;
		module->default_gvol = 64;
		module->default_speed = data_u16le( data, 76 );
		module->default_tempo = data_u16le( data, 78 );
		module->c2_rate = 8363;
		module->gain = 64;
		module->default_panning = calloc( module->num_channels, sizeof( unsigned char ) );
		if( !module->default_panning ) {
			dispose_module( module );
			return NULL;
		}
		for( idx = 0; idx < module->num_channels; idx++ ) {
			module->default_panning[ idx ] = 128;
		}
		module->sequence = calloc( module->sequence_len, sizeof( unsigned char ) );
		if( !module->sequence ) {
			dispose_module( module );
			return NULL;
		}
		for( idx = 0; idx < module->sequence_len; idx++ ) {
			entry = data_u8( data, 80 + idx );
			module->sequence[ idx ] = entry < module->num_patterns ? entry : 0;
		}
		module->patterns = calloc( module->num_patterns, sizeof( struct pattern ) );
		if( !module->patterns ) {
			dispose_module( module );
			return NULL;
		}
		for( idx = 0; idx < module->num_patterns; idx++ ) {
			if( data_u8( data, offset + 4 ) ) {
				strcpy( message, "Unknown pattern packing type!" );
				dispose_module( module );
				return NULL;
			}
			num_rows = data_u16le( data, offset + 5 );
			if( num_rows < 1 ) {
				num_rows = 1;
			}
			pat_data_len = data_u16le( data, offset + 7 );
			offset += data_u32le( data, offset );
			next_offset = offset + pat_data_len;
			num_notes = num_rows * module->num_channels;
			pattern_data = calloc( num_notes, 5 );
			if( !pattern_data ) {
				dispose_module( module );
				return NULL;
			}
			module->patterns[ idx ].num_channels = module->num_channels;
			module->patterns[ idx ].num_rows = num_rows;
			module->patterns[ idx ].data = pattern_data;
			if( pat_data_len > 0 ) {
				pat_data_offset = 0;
				for( note = 0; note < num_notes; note++ ) {
					flags = data_u8( data, offset );
					if( ( flags & 0x80 ) == 0 ) {
						flags = 0x1F;
					} else {
						offset++;
					}
					key = ( flags & 0x01 ) > 0 ? data_u8( data, offset++ ) : 0;
					pattern_data[ pat_data_offset++ ] = key;
					ins = ( flags & 0x02 ) > 0 ? data_u8( data, offset++ ) : 0;
					pattern_data[ pat_data_offset++ ] = ins;
					vol = ( flags & 0x04 ) > 0 ? data_u8( data, offset++ ) : 0;
					pattern_data[ pat_data_offset++ ] = vol;
					fxc = ( flags & 0x08 ) > 0 ? data_u8( data, offset++ ) : 0;
					fxp = ( flags & 0x10 ) > 0 ? data_u8( data, offset++ ) : 0;
					if( fxc >= 0x40 ) {
						fxc = fxp = 0;
					}
					pattern_data[ pat_data_offset++ ] = fxc;
					pattern_data[ pat_data_offset++ ] = fxp;
				}
			}
			offset = next_offset;
		}
		module->instruments = calloc( module->num_instruments + 1, sizeof( struct instrument ) );
		if( !module->instruments ) {
			dispose_module( module );
			return NULL;
		}
		instrument = &module->instruments[ 0 ];
		instrument->samples = calloc( 1, sizeof( struct sample ) );
		if( !instrument->samples ) {
			dispose_module( module );
			return NULL;
		}
		for( ins = 1; ins <= module->num_instruments; ins++ ) {
			instrument = &module->instruments[ ins ];
			data_ascii( data, offset + 4, 22, instrument->name );
			num_samples = data_u16le( data, offset + 27 );
			instrument->num_samples = ( num_samples > 0 ) ? num_samples : 1;
			instrument->samples = calloc( instrument->num_samples, sizeof( struct sample ) );
			if( !instrument->samples ) {
				dispose_module( module );
				return NULL;
			}
			if( num_samples > 0 ) {
				for( key = 0; key < 96; key++ ) {
					/* Clamp the file-supplied sample index; an out-of-range
					 * value would index past the instrument's sample array
					 * during playback. */
					int sample_idx = data_u8( data, offset + 33 + key );
					instrument->key_to_sample[ key + 1 ] =
							sample_idx < num_samples ? sample_idx : 0;
				}
				point_tick = 0;
				for( point = 0; point < 12; point++ ) {
					point_offset = offset + 129 + ( point * 4 );
					point_tick = ( delta_env ? point_tick : 0 ) + data_u16le( data, point_offset );
					instrument->vol_env.points_tick[ point ] = point_tick;
					instrument->vol_env.points_ampl[ point ] = data_u16le( data, point_offset + 2 );
				}
				point_tick = 0;
				for( point = 0; point < 12; point++ ) {
					point_offset = offset + 177 + ( point * 4 );
					point_tick = ( delta_env ? point_tick : 0 ) + data_u16le( data, point_offset );
					instrument->pan_env.points_tick[ point ] = point_tick;
					instrument->pan_env.points_ampl[ point ] = data_u16le( data, point_offset + 2 );
				}
				instrument->vol_env.num_points = data_u8( data, offset + 225 );
				if( instrument->vol_env.num_points > 12 ) {
					instrument->vol_env.num_points = 0;
				}
				instrument->pan_env.num_points = data_u8( data, offset + 226 );
				if( instrument->pan_env.num_points > 12 ) {
					instrument->pan_env.num_points = 0;
				}
				instrument->vol_env.sustain_tick = instrument->vol_env.points_tick[ data_u8( data, offset + 227 ) & 0xF ];
				instrument->vol_env.loop_start_tick = instrument->vol_env.points_tick[ data_u8( data, offset + 228 ) & 0xF ];
				instrument->vol_env.loop_end_tick = instrument->vol_env.points_tick[ data_u8( data, offset + 229 ) & 0xF ];
				instrument->pan_env.sustain_tick = instrument->pan_env.points_tick[ data_u8( data, offset + 230 ) & 0xF ];
				instrument->pan_env.loop_start_tick = instrument->pan_env.points_tick[ data_u8( data, offset + 231 ) & 0xF ];
				instrument->pan_env.loop_end_tick = instrument->pan_env.points_tick[ data_u8( data, offset + 232 ) & 0xF ];
				instrument->vol_env.enabled = instrument->vol_env.num_points > 0 && ( data_u8( data, offset + 233 ) & 0x1 );
				instrument->vol_env.sustain = ( data_u8( data, offset + 233 ) & 0x2 ) > 0;
				instrument->vol_env.looped = ( data_u8( data, offset + 233 ) & 0x4 ) > 0;
				instrument->pan_env.enabled = instrument->pan_env.num_points > 0 && ( data_u8( data, offset + 234 ) & 0x1 );
				instrument->pan_env.sustain = ( data_u8( data, offset + 234 ) & 0x2 ) > 0;
				instrument->pan_env.looped = ( data_u8( data, offset + 234 ) & 0x4 ) > 0;
				instrument->vib_type = data_u8( data, offset + 235 );
				instrument->vib_sweep = data_u8( data, offset + 236 );
				instrument->vib_depth = data_u8( data, offset + 237 );
				instrument->vib_rate = data_u8( data, offset + 238 );
				instrument->vol_fadeout = data_u16le( data, offset + 239 );
			}
			offset += data_u32le( data, offset );
			sam_head_offset = offset;
			offset += num_samples * 40;
			for( sam = 0; sam < num_samples; sam++ ) {
				sample = &instrument->samples[ sam ];
				sam_data_bytes = data_u32le( data, sam_head_offset );
				sam_loop_start = data_u32le( data, sam_head_offset + 4 );
				sam_loop_length = data_u32le( data, sam_head_offset + 8 );
				sample->volume = data_u8( data, sam_head_offset + 12 );
				sample->fine_tune = data_s8( data, sam_head_offset + 13 );
				looped = ( data_u8( data, sam_head_offset + 14 ) & 0x3 ) > 0;
				ping_pong = ( data_u8( data, sam_head_offset + 14 ) & 0x2 ) > 0;
				sixteen_bit = ( data_u8( data, sam_head_offset + 14 ) & 0x10 ) > 0;
				sample->panning = data_u8( data, sam_head_offset + 15 ) + 1;
				sample->rel_note = data_s8( data, sam_head_offset + 16 );
				/* ModPlug marks 4-bit ADPCM packing in the reserved
				   byte. It was only ever written for 8-bit samples;
				   the marker on a 16-bit sample is ignored. */
				adpcm = data_u8( data, sam_head_offset + 17 ) == 0xAD && !sixteen_bit;
				data_ascii( data, sam_head_offset + 18, 22, sample->name );
				sam_head_offset += 40;
				sam_data_samples = sam_data_bytes;
				if( sixteen_bit ) {
					sam_data_samples = sam_data_samples >> 1;
					sam_loop_start = sam_loop_start >> 1;
					sam_loop_length = sam_loop_length >> 1;
				}
				if( !looped || ( sam_loop_start + sam_loop_length ) > sam_data_samples ) {
					sam_loop_start = sam_data_samples;
					sam_loop_length = 0;
				}
				sample->loop_start = sam_loop_start;
				sample->loop_length = sam_loop_length;
				sample->data = calloc( sam_data_samples + 1, sizeof( short ) );
				if( sample->data ) {
					if( adpcm ) {
						/* ADPCM output is absolute PCM; the delta
						   pass below belongs to raw samples only. */
						data_sam_adpcm4( data, offset, sam_data_samples, sample->data );
					} else {
						if( sixteen_bit ) {
							data_sam_s16le( data, offset, sam_data_samples, sample->data );
						} else {
							data_sam_s8( data, offset, sam_data_samples, sample->data );
						}
						amp = 0;
						for( idx = 0; idx < sam_data_samples; idx++ ) {
							amp = amp + sample->data[ idx ];
							amp = ( amp & 0x7FFF ) - ( amp & 0x8000 );
							sample->data[ idx ] = amp;
						}
					}
					sample->data[ sam_loop_start + sam_loop_length ] = sample->data[ sam_loop_start ];
					if( ping_pong ) {
						sample_ping_pong( sample );
					}
				} else {
					dispose_module( module );
					return NULL;
				}
				offset += adpcm ? 16 + ( ( sam_data_bytes + 1 ) >> 1 ) : sam_data_bytes;
			}
		}
	}
	return module;
}

static struct module* module_load_s3m( struct data *data, char *message ) {
	int idx, module_data_idx, inst_offset, flags;
	int version, sixteen_bit, tune, signed_samples;
	int stereo, adpcm, pack, frames;
	short *scratch;
	int stereo_mode, default_pan, channel_map[ 32 ];
	int sample_offset, sample_length, loop_start, loop_length;
	int pat_offset, note_offset, row, chan, token;
	int key, ins, volume, effect, param, panning;
	char *pattern_data;
	struct instrument *instrument;
	struct sample *sample;
	struct module *module = calloc( 1, sizeof( struct module ) );
	if( module ) {
		data_ascii( data, 0, 28, module->name );
		module->sequence_len = data_u16le( data, 32 );
		if( module->sequence_len < 1 ) {
			module->sequence_len = 1; /* see the XM loader note */
		}
		module->num_instruments = data_u16le( data, 34 );
		module->num_patterns = data_u16le( data, 36 );
		if( module->num_patterns < 1 ) {
			module->num_patterns = 1; /* see the XM loader note */
		}
		flags = data_u16le( data, 38 );
		version = data_u16le( data, 40 );
		module->fast_vol_slides = ( ( flags & 0x40 ) == 0x40 ) || version == 0x1300;
		signed_samples = data_u16le( data, 42 ) == 1;
		if( data_u32le( data, 44 ) != 0x4d524353 ) {
			strcpy( message, "Not an S3M file!" );
			dispose_module( module );
			return NULL;
		}
		module->default_gvol = data_u8( data, 48 );
		module->default_speed = data_u8( data, 49 );
		module->default_tempo = data_u8( data, 50 );
		module->c2_rate = 8363;
		module->gain = data_u8( data, 51 ) & 0x7F;
		stereo_mode = ( data_u8( data, 51 ) & 0x80 ) == 0x80;
		default_pan = data_u8( data, 53 ) == 0xFC;
		for( idx = 0; idx < 32; idx++ ) {
			channel_map[ idx ] = -1;
			if( data_u8( data, 64 + idx ) < 16 ) {
				channel_map[ idx ] = module->num_channels++;
			}
		}
		module->sequence = calloc( module->sequence_len, sizeof( unsigned char ) );
		if( !module->sequence ){
			dispose_module( module );
			return NULL;
		}
		for( idx = 0; idx < module->sequence_len; idx++ ) {
			module->sequence[ idx ] = data_u8( data, 96 + idx );
		}
		/* The parapointer table follows the full, on-disk order list; compute
		 * the read offset from the original length before trimming below. */
		module_data_idx = 96 + module->sequence_len;
		/* Drop trailing orders that point past the pattern table; otherwise
		 * the sequencer would fetch a pattern out of bounds. */
		while( module->sequence_len > 0
				&& module->sequence[ module->sequence_len - 1 ] >= module->num_patterns ) {
			module->sequence_len--;
		}
		module->instruments = calloc( module->num_instruments + 1, sizeof( struct instrument ) );
		if( !module->instruments ) {
			dispose_module( module );
			return NULL;
		}
		instrument = &module->instruments[ 0 ];
		instrument->num_samples = 1;
		instrument->samples = calloc( 1, sizeof( struct sample ) );
		if( !instrument->samples ) {
			dispose_module( module );
			return NULL;
		}
		for( ins = 1; ins <= module->num_instruments; ins++ ) {
			instrument = &module->instruments[ ins ];
			instrument->num_samples = 1;
			instrument->samples = calloc( 1, sizeof( struct sample ) );
			if( !instrument->samples ) {
				dispose_module( module );
				return NULL;
			}
			sample = &instrument->samples[ 0 ];
			inst_offset = data_u16le( data, module_data_idx ) << 4;
			module_data_idx += 2;
			data_ascii( data, inst_offset + 48, 28, instrument->name );
			if( data_u8( data, inst_offset ) == 1 && data_u16le( data, inst_offset + 76 ) == 0x4353 ) {
				sample_offset = ( data_u8( data, inst_offset + 13 ) << 20 )
					+ ( data_u16le( data, inst_offset + 14 ) << 4 );
				sample_length = data_u32le( data, inst_offset + 16 );
				loop_start = data_u32le( data, inst_offset + 20 );
				loop_length = data_u32le( data, inst_offset + 24 ) - loop_start;
				sample->volume = data_u8( data, inst_offset + 28 );
				pack = data_u8( data, inst_offset + 30 );
				adpcm = pack == 4;
				if( pack != 0 && !adpcm ) {
					strcpy( message, "Packed samples not supported!" );
					dispose_module( module );
					return NULL;
				}
				if( loop_start + loop_length > sample_length ) {
					loop_length = sample_length - loop_start;
				}
				if( loop_length < 1 || !( data_u8( data, inst_offset + 31 ) & 0x1 ) ) {
					loop_start = sample_length;
					loop_length = 0;
				}
				sample->loop_start = loop_start;
				sample->loop_length = loop_length;
				stereo = ( data_u8( data, inst_offset + 31 ) & 0x2 ) > 0;
				sixteen_bit = data_u8( data, inst_offset + 31 ) & 0x4;
				sample->stereo = ( short ) stereo;
				tune = ( log_2( data_u32le( data, inst_offset + 32 ) ) - log_2( module->c2_rate ) ) * 12;
				sample->rel_note = tune >> FP_SHIFT;
				sample->fine_tune = ( tune & FP_MASK ) >> ( FP_SHIFT - 7 );
				frames = sample_length;
				sample->data = calloc( ( frames + 1 ) * ( stereo ? 2 : 1 ), sizeof( short ) );
				if( sample->data ) {
					if( stereo ) {
						/* Stored as the whole left block then the
						   whole right block; interleave to frames.
						   ADPCM packs both blocks in one stream
						   after a single table. */
						scratch = calloc( frames * 2, sizeof( short ) );
						if( !scratch ) {
							dispose_module( module );
							return NULL;
						}
						if( adpcm ) {
							data_sam_adpcm4( data, sample_offset, frames * 2, scratch );
						} else if( sixteen_bit ) {
							data_sam_s16le( data, sample_offset, frames * 2, scratch );
						} else {
							data_sam_s8( data, sample_offset, frames * 2, scratch );
						}
						if( !signed_samples && !adpcm ) {
							for( idx = 0; idx < frames * 2; idx++ ) {
								scratch[ idx ] = ( scratch[ idx ] & 0xFFFF ) - 32768;
							}
						}
						for( idx = 0; idx < frames; idx++ ) {
							sample->data[ idx * 2 ] = scratch[ idx ];
							sample->data[ idx * 2 + 1 ] = scratch[ frames + idx ];
						}
						free( scratch );
						sample->data[ ( loop_start + loop_length ) * 2 ] = sample->data[ loop_start * 2 ];
						sample->data[ ( loop_start + loop_length ) * 2 + 1 ] = sample->data[ loop_start * 2 + 1 ];
					} else {
						if( adpcm ) {
							data_sam_adpcm4( data, sample_offset, frames, sample->data );
						} else if( sixteen_bit ) {
							data_sam_s16le( data, sample_offset, frames, sample->data );
						} else {
							data_sam_s8( data, sample_offset, frames, sample->data );
						}
						if( !signed_samples && !adpcm ) {
							for( idx = 0; idx < frames; idx++ ) {
								sample->data[ idx ] = ( sample->data[ idx ] & 0xFFFF ) - 32768;
							}
						}
						sample->data[ loop_start + loop_length ] = sample->data[ loop_start ];
					}
				} else {
					dispose_module( module );
					return NULL;
				}
			}
		}
		module->patterns = calloc( module->num_patterns, sizeof( struct pattern ) );
		if( !module->patterns ) {
			dispose_module( module );
			return NULL;
		}
		for( idx = 0; idx < module->num_patterns; idx++ ) {
			module->patterns[ idx ].num_channels = module->num_channels;
			module->patterns[ idx ].num_rows = 64;
			pattern_data = calloc( module->num_channels * 64, 5 );
			if( !pattern_data ) {
				dispose_module( module );
				return NULL;
			}
			module->patterns[ idx ].data = pattern_data;
			pat_offset = ( data_u16le( data, module_data_idx ) << 4 ) + 2;
			row = 0;
			while( row < 64 ) {
				token = data_u8( data, pat_offset++ );
				if( token ) {
					key = ins = 0;
					if( ( token & 0x20 ) == 0x20 ) {
						/* Key + Instrument.*/
						key = data_u8( data, pat_offset++ );
						ins = data_u8( data, pat_offset++ );
						if( key < 0xFE ) {
							key = ( key >> 4 ) * 12 + ( key & 0xF ) + 1;
						} else if( key == 0xFF ) {
							key = 0;
						}
					}
					volume = 0;
					if( ( token & 0x40 ) == 0x40 ) {
						/* Volume Column.*/
						volume = ( data_u8( data, pat_offset++ ) & 0x7F ) + 0x10;
						if( volume > 0x50 ) {
							volume = 0;
						}
					}
					effect = param = 0;
					if( ( token & 0x80 ) == 0x80 ) {
						/* Effect + Param.*/
						effect = data_u8( data, pat_offset++ );
						param = data_u8( data, pat_offset++ );
						if( effect < 1 || effect >= 0x40 ) {
							effect = param = 0;
						} else if( effect > 0 ) {
							effect += 0x80;
						}
					}
					chan = channel_map[ token & 0x1F ];
					if( chan >= 0 ) {
						note_offset = ( row * module->num_channels + chan ) * 5;
						pattern_data[ note_offset     ] = key;
						pattern_data[ note_offset + 1 ] = ins;
						pattern_data[ note_offset + 2 ] = volume;
						pattern_data[ note_offset + 3 ] = effect;
						pattern_data[ note_offset + 4 ] = param;
					}
				} else {
					row++;
				}
			}
			module_data_idx += 2;
		}
		module->default_panning = calloc( module->num_channels, sizeof( unsigned char ) );
		if( module->default_panning ) {
			for( chan = 0; chan < 32; chan++ ) {
				if( channel_map[ chan ] >= 0 ) {
					panning = 7;
					if( stereo_mode ) {
						panning = 12;
						if( data_u8( data, 64 + chan ) < 8 ) {
							panning = 3;
						}
					}
					if( default_pan ) {
						flags = data_u8( data, module_data_idx + chan );
						if( ( flags & 0x20 ) == 0x20 ) {
							panning = flags & 0xF;
						}
					}
					module->default_panning[ channel_map[ chan ] ] = panning * 17;
				}
			}
		} else {
			dispose_module( module );
			return NULL;
		}
	}
	return module;
}


/* --------------------------------------------------------------------
 * Impulse Tracker loader, stage 1.
 *
 * What this stage covers: the IMPM header, orders with skip and end
 * markers, uncompressed 8/16-bit signed/unsigned samples including
 * stereo, sample and instrument mode, the note-to-sample keyboard,
 * volume envelopes (clamped to the engine's sixteen nodes), fadeout,
 * default channel pans and the packed pattern format with its
 * per-channel mask and last-value memory. Effects reuse the S3M
 * mapping: IT kept Scream Tracker's command letters, so the letters
 * route to the same handlers, with IT's divergent sub-behaviours a
 * later stage.
 *
 * IT214/215 sample decompression is implemented, including IT215's
 * extra delta stage and stereo's twin streams.
 *
 * New-note actions run on a pool of background voices: continue, off
 * and fade move the old note aside instead of cutting it, with fade
 * approximated as a release. Note cut (254) is immediate silence.
 *
 * Known divergences, deliberate at this stage: duplicate-check
 * actions (DCT/DCA) are not implemented; linear
 * slide mode is approximated with Amiga periods, so portamento depth
 * can differ; sustain loops are ignored in favour of the normal loop;
 * panbrello (Yxy), the S9x sound-control set and Zxx macros are not
 * applied, and the volume column's tone porta uses the engine's rate
 * scale rather than IT's table. Panning, pitch and filter envelopes,
 * the resonant lowpass (initial cutoff/resonance and the filter
 * envelope, coefficients as IT2 computes them) are applied. Channel
 * volumes (initial table, Mxx, Nxy), Wxy global volume slide, Xxx
 * panning, Pxy panning slide, Txx tempo slides, SAx high offset,
 * volume-column volume effects and duplicate checks (DCT/DCA, sample
 * check approximated as instrument check) are applied.
 * ------------------------------------------------------------------ */


/* --- IT214/IT215 sample decompression ----------------------------
 * The stream is a run of blocks, each a 16-bit packed byte length
 * followed by that many bytes, decompressing to at most 0x8000 output
 * bytes; the bit width and every accumulator reset per block. Values
 * arrive as variable-width deltas with three escape encodings that
 * change the width, one per width band. IT215 adds a second
 * delta-to-PCM pass, also per block. Behaviour follows Impulse
 * Tracker's own replayer as preserved in it2play (BSD-3-Clause),
 * reimplemented on the bounds-checked readers here; a stereo sample
 * is two complete streams, left then right, so the unpackers return
 * the offset just past what they consumed. */

static int it_read_bits( struct data *data, int base, int *bitpos, int count ) {
	int byte_idx = base + ( *bitpos >> 3 );
	int shift = *bitpos & 7;
	unsigned int v = ( unsigned int ) data_u8( data, byte_idx )
		| ( ( unsigned int ) data_u8( data, byte_idx + 1 ) << 8 )
		| ( ( unsigned int ) data_u8( data, byte_idx + 2 ) << 16 )
		| ( ( unsigned int ) data_u8( data, byte_idx + 3 ) << 24 );
	*bitpos += count;
	return ( int ) ( ( v >> shift ) & ( ( 1u << count ) - 1u ) );
}

/* Decompress 'count' 8-bit samples into dest at 'stride' shorts per
 * frame, scaled to the engine's 16-bit domain. Returns the offset
 * just past the consumed input. */
static int it_unpack8( struct data *data, int src, short *dest,
		int stride, int count, int it215 ) {
	int done = 0;
	while( done < count ) {
		int block = count - done;
		int packed, bitpos, width, acc, d215, n, v, w, sp, lim;
		if( block > 0x8000 ) {
			block = 0x8000;
		}
		packed = data_u16le( data, src );
		src += 2;
		bitpos = 0;
		width = 9;
		acc = 0;
		d215 = 0;
		n = 0;
		lim = packed * 8;
		while( n < block && bitpos < lim ) {
			v = it_read_bits( data, src, &bitpos, width );
			if( width < 7 ) {
				if( v == 1 << ( width - 1 ) ) {
					w = it_read_bits( data, src, &bitpos, 3 ) + 1;
					width = ( w < width ) ? w : w + 1;
					continue;
				}
				sp = 1 << ( width - 1 );
				v = ( v & ( sp - 1 ) ) - ( v & sp );
			} else if( width < 9 ) {
				int border = ( 0xFF >> ( 9 - width ) ) - 4;
				if( v > border && v <= border + 8 ) {
					w = v - border;
					width = ( w < width ) ? w : w + 1;
					continue;
				}
				sp = 1 << ( width - 1 );
				v = ( v & ( sp - 1 ) ) - ( v & sp );
			} else {
				if( v & 0x100 ) {
					width = ( v & 0xFF ) + 1;
					if( width > 9 ) {
						break; /* malformed; drop the rest of the block */
					}
					continue;
				}
				v = ( v & 0x7F ) - ( v & 0x80 );
			}
			acc = ( acc + v ) & 0xFF;
			w = acc;
			if( it215 ) {
				d215 = ( d215 + acc ) & 0xFF;
				w = d215;
			}
			w = w << 8;
			w = ( w & 0x7FFF ) - ( w & 0x8000 );
			dest[ ( ( size_t ) done + n ) * stride ] = ( short ) w;
			n++;
		}
		done += block;
		src += packed;
	}
	return src;
}

/* The 16-bit twin: widths to 17, a 4-bit escape in the low band, and
 * a block of at most 0x4000 samples (0x8000 bytes). */
static int it_unpack16( struct data *data, int src, short *dest,
		int stride, int count, int it215 ) {
	int done = 0;
	while( done < count ) {
		int block = count - done;
		int packed, bitpos, width, acc, d215, n, v, w, sp, lim;
		if( block > 0x4000 ) {
			block = 0x4000;
		}
		packed = data_u16le( data, src );
		src += 2;
		bitpos = 0;
		width = 17;
		acc = 0;
		d215 = 0;
		n = 0;
		lim = packed * 8;
		while( n < block && bitpos < lim ) {
			v = it_read_bits( data, src, &bitpos, width );
			if( width < 7 ) {
				if( v == 1 << ( width - 1 ) ) {
					w = it_read_bits( data, src, &bitpos, 4 ) + 1;
					width = ( w < width ) ? w : w + 1;
					continue;
				}
				sp = 1 << ( width - 1 );
				v = ( v & ( sp - 1 ) ) - ( v & sp );
			} else if( width < 17 ) {
				int border = ( 0xFFFF >> ( 17 - width ) ) - 8;
				if( v > border && v <= border + 16 ) {
					w = v - border;
					width = ( w < width ) ? w : w + 1;
					continue;
				}
				sp = 1 << ( width - 1 );
				v = ( v & ( sp - 1 ) ) - ( v & sp );
			} else {
				if( v & 0x10000 ) {
					width = ( v & 0xFFFF ) + 1;
					if( width > 17 ) {
						break; /* malformed; drop the rest of the block */
					}
					continue;
				}
				v = ( v & 0x7FFF ) - ( v & 0x8000 );
			}
			acc = ( acc + v ) & 0xFFFF;
			w = acc;
			if( it215 ) {
				d215 = ( d215 + acc ) & 0xFFFF;
				w = d215;
			}
			w = ( w & 0x7FFF ) - ( w & 0x8000 );
			dest[ ( ( size_t ) done + n ) * stride ] = ( short ) w;
			n++;
		}
		done += block;
		src += packed;
	}
	return src;
}

/* Parse one IMPS header and its data into an engine sample. Returns
   the sample's frame count through *out_frames for the deep copies
   instrument mode makes, or -1 on allocation failure. Compressed
   samples (flag bit 3) become silence of their stated length. */
static int it_load_sample( struct data *data, int offset,
		struct sample *sample, int *out_frames ) {
	int flg, cvt, vol, gvs, dfp, frames, loop_start, loop_end, tune;
	int sixteen_bit, stereo, has_data, compressed, signed_samples;
	int sample_offset, idx, c5speed;
	short *scratch;
	*out_frames = 0;
	gvs = data_u8( data, offset + 0x11 );
	flg = data_u8( data, offset + 0x12 );
	vol = data_u8( data, offset + 0x13 );
	data_ascii( data, offset + 0x14, 26, sample->name );
	cvt = data_u8( data, offset + 0x2E );
	dfp = data_u8( data, offset + 0x2F );
	frames = data_u32le( data, offset + 0x30 );
	loop_start = data_u32le( data, offset + 0x34 );
	loop_end = data_u32le( data, offset + 0x38 );
	c5speed = data_u32le( data, offset + 0x3C );
	sample_offset = data_u32le( data, offset + 0x48 );
	has_data = flg & 0x01;
	sixteen_bit = flg & 0x02;
	stereo = ( flg & 0x04 ) > 0;
	compressed = flg & 0x08;
	signed_samples = cvt & 0x01;
	if( frames < 0 || frames > 0x1000000 ) {
		frames = 0;
	}
	if( !has_data ) {
		frames = 0;
	}
	sample->volume = ( short ) ( vol > 64 ? 64 : vol );
	sample->glob_vol = ( short ) ( ( gvs > 64 ? 64 : gvs ) + 1 );
	if( dfp & 0x80 ) {
		idx = ( dfp & 0x7F ) * 4;
		sample->panning = ( short ) ( ( idx > 255 ? 255 : idx ) + 1 );
	}
	if( c5speed < 1 ) {
		c5speed = 8363;
	}
	tune = ( log_2( c5speed ) - log_2( 8363 ) ) * 12;
	sample->rel_note = ( short ) ( tune >> FP_SHIFT );
	sample->fine_tune = ( short ) ( ( tune & FP_MASK ) >> ( FP_SHIFT - 7 ) );
	if( !( flg & 0x10 ) || loop_end <= loop_start || loop_end > frames ) {
		loop_start = frames;
		loop_end = frames;
	}
	sample->loop_start = loop_start;
	sample->loop_length = loop_end - loop_start;
	sample->stereo = ( short ) stereo;
	sample->data = calloc( ( ( size_t ) frames + 1 ) * ( stereo ? 2 : 1 ),
		sizeof( short ) );
	if( !sample->data ) {
		return -1;
	}
	if( frames > 0 && compressed ) {
		/* cvt bit 2 marks IT215's extra delta stage. Compressed data
		   is stored signed whatever cvt bit 0 says. */
		int it215 = ( cvt & 0x04 ) > 0;
		if( stereo ) {
			int next = sixteen_bit
				? it_unpack16( data, sample_offset, sample->data, 2, frames, it215 )
				: it_unpack8( data, sample_offset, sample->data, 2, frames, it215 );
			if( sixteen_bit ) {
				it_unpack16( data, next, sample->data + 1, 2, frames, it215 );
			} else {
				it_unpack8( data, next, sample->data + 1, 2, frames, it215 );
			}
		} else if( sixteen_bit ) {
			it_unpack16( data, sample_offset, sample->data, 1, frames, it215 );
		} else {
			it_unpack8( data, sample_offset, sample->data, 1, frames, it215 );
		}
	} else if( frames > 0 ) {
		if( stereo ) {
			scratch = calloc( ( size_t ) frames * 2, sizeof( short ) );
			if( !scratch ) {
				return -1;
			}
			if( sixteen_bit ) {
				data_sam_s16le( data, sample_offset, frames * 2, scratch );
			} else {
				data_sam_s8( data, sample_offset, frames * 2, scratch );
			}
			if( !signed_samples ) {
				for( idx = 0; idx < frames * 2; idx++ ) {
					scratch[ idx ] = ( scratch[ idx ] & 0xFFFF ) - 32768;
				}
			}
			for( idx = 0; idx < frames; idx++ ) {
				sample->data[ idx * 2 ] = scratch[ idx ];
				sample->data[ idx * 2 + 1 ] = scratch[ frames + idx ];
			}
			free( scratch );
		} else {
			if( sixteen_bit ) {
				data_sam_s16le( data, sample_offset, frames, sample->data );
			} else {
				data_sam_s8( data, sample_offset, frames, sample->data );
			}
			if( !signed_samples ) {
				for( idx = 0; idx < frames; idx++ ) {
					sample->data[ idx ] = ( sample->data[ idx ] & 0xFFFF ) - 32768;
				}
			}
		}
	}
	if( stereo ) {
		sample->data[ ( sample->loop_start + sample->loop_length ) * 2 ]
			= sample->data[ sample->loop_start * 2 ];
		sample->data[ ( sample->loop_start + sample->loop_length ) * 2 + 1 ]
			= sample->data[ sample->loop_start * 2 + 1 ];
	} else {
		sample->data[ sample->loop_start + sample->loop_length ]
			= sample->data[ sample->loop_start ];
	}
	if( ( flg & 0x40 ) && sample->loop_length > 1 && !stereo ) {
		/* Unrolling reallocates the data to loop_start + doubled
		   loop_length + 1 and drops any tail past the loop, so the
		   frame count instrument-mode copies use must follow it.
		   On allocation failure the loop stays undoubled and the
		   original count still describes the data. */
		idx = sample->loop_length;
		sample_ping_pong( sample );
		if( sample->loop_length != idx ) {
			frames = sample->loop_start + sample->loop_length;
		}
	}
	*out_frames = frames;
	return 0;
}

/* Deep copy for instrument mode, where several instruments may map
   notes onto the same sample: the engine's instruments own their
   samples, so each gets its own data. */
static int it_sample_copy( struct sample *dst, struct sample *src, int frames ) {
	size_t words = ( ( size_t ) frames + 1 ) * ( src->stereo ? 2 : 1 );
	*dst = *src;
	dst->data = calloc( words, sizeof( short ) );
	if( !dst->data ) {
		return -1;
	}
	memcpy( dst->data, src->data, words * sizeof( short ) );
	return 0;
}

/* Convert an IT node envelope (up to 25 nodes, value 0..64, tick
   u16) to the engine's 16-point form. */
/* bias 0 reads unsigned 0..64 values (volume, panning); bias 32
   reads the pitch block's signed -32..32 values into the same 0..64
   domain, with the application subtracting the bias back out. */
static void it_load_envelope( struct data *data, int offset,
		struct envelope *env, int bias ) {
	int flg, num, lpb, lpe, slb, sle, idx;
	flg = data_u8( data, offset );
	num = data_u8( data, offset + 1 );
	lpb = data_u8( data, offset + 2 );
	lpe = data_u8( data, offset + 3 );
	slb = data_u8( data, offset + 4 );
	sle = data_u8( data, offset + 5 );
	if( num > 16 ) {
		num = 16;
	}
	if( num > 0 ) {
		env->num_points = ( char ) num;
		for( idx = 0; idx < num; idx++ ) {
			int val = data_u8( data, offset + 6 + idx * 3 );
			if( bias ) {
				val = ( ( val & 0x7F ) - ( val & 0x80 ) ) + bias;
			}
			if( val < 0 ) {
				val = 0;
			}
			env->points_ampl[ idx ] = ( short ) ( val > 64 ? 64 : val );
			env->points_tick[ idx ] = ( short ) data_u16le( data,
				offset + 6 + idx * 3 + 1 );
		}
		env->enabled = ( flg & 0x01 ) > 0;
		env->looped = ( flg & 0x02 ) > 0 && lpe < num;
		env->sustain = ( flg & 0x04 ) > 0 && sle < num;
		if( env->looped ) {
			env->loop_start_tick = env->points_tick[ lpb < num ? lpb : 0 ];
			env->loop_end_tick = env->points_tick[ lpe ];
		}
		if( env->sustain ) {
			/* The engine has one sustain tick; IT sustains a loop.
			   Holding at the loop's end point is the closest fit. */
			env->sustain_tick = env->points_tick[ sle ];
			( void ) slb;
		}
	}
}

static struct module* module_load_it( struct data *data, char *message ) {
	int ord_num, ins_num, smp_num, pat_num, flags, use_instruments;
	int idx, sub, ofs, key, ins, volume, effect, param, chan;
	int row, num_rows, pat_len, pat_ofs_idx, max_chan, tok, mask;
	int gv, mv, tick_speed, tempo, entry, seq_end;
	int *smp_frames = NULL;
	struct sample *temp_samples = NULL;
	struct instrument *instrument;
	char *pattern_data;
	unsigned char last_mask[ 64 ], last_note[ 64 ], last_ins[ 64 ];
	unsigned char last_vol[ 64 ], last_fx[ 64 ], last_pm[ 64 ];
	struct module *module = calloc( 1, sizeof( struct module ) );
	if( !module ) {
		return NULL;
	}
	data_ascii( data, 4, 26, module->name );
	ord_num = data_u16le( data, 0x20 );
	ins_num = data_u16le( data, 0x22 );
	smp_num = data_u16le( data, 0x24 );
	pat_num = data_u16le( data, 0x26 );
	flags = data_u16le( data, 0x2C );
	gv = data_u8( data, 0x30 );
	mv = data_u8( data, 0x31 );
	tick_speed = data_u8( data, 0x32 );
	tempo = data_u8( data, 0x33 );
	use_instruments = ( flags & 0x04 ) > 0 && ins_num > 0;
	if( ord_num < 0 || ord_num > 256 ) {
		ord_num = 256;
	}
	if( ins_num < 0 || ins_num > 255 ) {
		ins_num = 255;
	}
	if( smp_num < 0 || smp_num > 255 ) {
		smp_num = 255;
	}
	if( pat_num < 0 || pat_num > 255 ) {
		pat_num = 255;
	}
	module->num_patterns = pat_num > 0 ? pat_num : 1;
	module->default_gvol = ( gv > 128 ? 128 : gv ) >> 1;
	module->default_speed = tick_speed > 0 ? tick_speed : 6;
	module->default_tempo = tempo > 31 ? tempo : 125;
	module->c2_rate = 8363;
	/* The S3M-style mapping of the mix-volume byte ran about 2 dB
	   under libxmp's IT levels (geometric mean 0.77 over the
	   real-world A/B corpus); scale by 4/3 to sit on it. */
	module->gain = ( mv & 0x7F ) > 0 ? ( ( mv & 0x7F ) * 4 ) / 3 : 64;
	module->sequence_len = 0;
	module->sequence = calloc( ord_num > 0 ? ord_num : 1,
		sizeof( unsigned char ) );
	if( !module->sequence ) {
		dispose_module( module );
		return NULL;
	}
	seq_end = 0;
	for( idx = 0; idx < ord_num && !seq_end; idx++ ) {
		entry = data_u8( data, 0xC0 + idx );
		if( entry == 255 ) {
			seq_end = 1;
		} else {
			/* 254 is the skip marker; anything at or past the
			   pattern count is skipped by the sequencer walk, which
			   is exactly its meaning. */
			module->sequence[ module->sequence_len++ ]
				= ( unsigned char ) ( entry > 254 ? 254 : entry );
		}
	}
	if( module->sequence_len < 1 ) {
		module->sequence_len = 1;
	}
	/* The three parapointer tables follow the orders. */
	ofs = 0xC0 + ord_num;
	/* Pre-scan the patterns for the highest channel in use, so the
	   engine mixes only the voices the file plays. The scan walks the
	   same packed stream as the decode below, tracking the per-channel
	   mask memory, because the mask decides how many bytes follow. */
	max_chan = 0;
	for( idx = 0; idx < pat_num; idx++ ) {
		int pofs = data_u32le( data, ofs + ins_num * 4 + smp_num * 4 + idx * 4 );
		if( pofs == 0 ) {
			continue;
		}
		pat_len = data_u16le( data, pofs );
		num_rows = data_u16le( data, pofs + 2 );
		memset( last_mask, 0, sizeof( last_mask ) );
		sub = pofs + 8;
		row = 0;
		while( row < num_rows && sub < pofs + 8 + pat_len ) {
			tok = data_u8( data, sub++ );
			if( tok == 0 ) {
				row++;
				continue;
			}
			chan = ( tok - 1 ) & 63;
			if( chan > max_chan ) {
				max_chan = chan;
			}
			if( tok & 0x80 ) {
				last_mask[ chan ] = ( unsigned char ) data_u8( data, sub++ );
			}
			mask = last_mask[ chan ];
			if( mask & 0x01 ) sub++;
			if( mask & 0x02 ) sub++;
			if( mask & 0x04 ) sub++;
			if( mask & 0x08 ) sub += 2;
		}
	}
	module->num_channels = max_chan + 1;
	module->default_panning = calloc( module->num_channels,
		sizeof( unsigned char ) );
	if( !module->default_panning ) {
		dispose_module( module );
		return NULL;
	}
	module->it_effects = 1;
	module->default_chan_vol = calloc( module->num_channels,
		sizeof( unsigned char ) );
	for( idx = 0; idx < module->num_channels; idx++ ) {
		int pan = data_u8( data, 0x40 + idx ) & 0x7F;
		int cvol = data_u8( data, 0x80 + idx ) & 0x7F;
		if( pan > 64 ) {
			pan = 32; /* surround and out-of-range play centred */
		}
		pan = pan * 4;
		module->default_panning[ idx ]
			= ( unsigned char ) ( pan > 255 ? 255 : pan );
		if( module->default_chan_vol ) {
			module->default_chan_vol[ idx ]
				= ( unsigned char ) ( cvol > 64 ? 64 : cvol );
		}
	}
	/* Samples first: instrument mode copies from them. */
	if( smp_num > 0 ) {
		temp_samples = calloc( smp_num, sizeof( struct sample ) );
		smp_frames = calloc( smp_num, sizeof( int ) );
		if( !temp_samples || !smp_frames ) {
			goto it_oom;
		}
		for( idx = 0; idx < smp_num; idx++ ) {
			int sofs = data_u32le( data, ofs + ins_num * 4 + idx * 4 );
			if( it_load_sample( data, sofs, &temp_samples[ idx ],
					&smp_frames[ idx ] ) ) {
				goto it_oom;
			}
		}
	}
	if( use_instruments ) {
		module->num_instruments = ins_num;
	} else {
		module->num_instruments = smp_num > 0 ? smp_num : 1;
	}
	module->instruments = calloc( module->num_instruments + 1,
		sizeof( struct instrument ) );
	if( !module->instruments ) {
		goto it_oom;
	}
	for( ins = 0; ins <= module->num_instruments; ins++ ) {
		instrument = &module->instruments[ ins ];
		instrument->num_samples = 1;
		instrument->samples = calloc( 1, sizeof( struct sample ) );
		if( !instrument->samples ) {
			goto it_oom;
		}
	}
	if( use_instruments ) {
		for( ins = 1; ins <= ins_num; ins++ ) {
			int iofs = data_u32le( data, ofs + ( ins - 1 ) * 4 );
			int fade, gbv, nos, local, want, kb_note, kb_smp;
			int local_of[ 100 ];
			instrument = &module->instruments[ ins ];
			data_ascii( data, iofs + 0x20, 26, instrument->name );
			instrument->nna = ( char ) ( data_u8( data, iofs + 0x11 ) & 3 );
			instrument->dct = ( char ) ( data_u8( data, iofs + 0x12 ) & 3 );
			instrument->dca = ( char ) ( data_u8( data, iofs + 0x13 ) & 3 );
			fade = data_u16le( data, iofs + 0x14 );
			gbv = data_u8( data, iofs + 0x18 );
			fade = fade * 64;
			instrument->vol_fadeout = fade > 32768 ? 32768 : fade;
			/* Gather the samples this instrument's keyboard uses and
			   give the instrument private copies of them. */
			nos = 0;
			for( idx = 0; idx < 120 && nos < 100; idx++ ) {
				kb_smp = data_u8( data, iofs + 0x40 + idx * 2 + 1 );
				if( kb_smp >= 1 && kb_smp <= smp_num ) {
					for( local = 0; local < nos; local++ ) {
						if( local_of[ local ] == kb_smp ) {
							break;
						}
					}
					if( local == nos ) {
						local_of[ nos++ ] = kb_smp;
					}
				}
			}
			if( nos > 0 ) {
				free( instrument->samples );
				instrument->samples = calloc( nos, sizeof( struct sample ) );
				if( !instrument->samples ) {
					instrument->num_samples = 0;
					goto it_oom;
				}
				instrument->num_samples = nos;
				for( local = 0; local < nos; local++ ) {
					struct sample *src = &temp_samples[ local_of[ local ] - 1 ];
					if( it_sample_copy( &instrument->samples[ local ], src,
							smp_frames[ local_of[ local ] - 1 ] ) ) {
						goto it_oom;
					}
					{
						int g = instrument->samples[ local ].glob_vol;
						g = g ? g - 1 : 64;
						g = ( g * ( gbv > 128 ? 128 : gbv ) ) >> 7;
						instrument->samples[ local ].glob_vol
							= ( short ) ( g + 1 );
					}
				}
				for( idx = 0; idx < 120; idx++ ) {
					kb_note = idx - 11;
					if( kb_note >= 1 && kb_note <= 96 ) {
						kb_smp = data_u8( data, iofs + 0x40 + idx * 2 + 1 );
						want = 0;
						for( local = 0; local < nos; local++ ) {
							if( local_of[ local ] == kb_smp ) {
								want = local;
								break;
							}
						}
						instrument->key_to_sample[ kb_note ] = ( char ) want;
					}
				}
			}
			it_load_envelope( data, iofs + 0x130, &instrument->vol_env, 0 );
			it_load_envelope( data, iofs + 0x182, &instrument->pan_env, 0 );
			it_load_envelope( data, iofs + 0x1D4, &instrument->pitch_env, 32 );
			instrument->pitch_is_filter
				= ( char ) ( ( data_u8( data, iofs + 0x1D4 ) & 0x80 ) >> 7 );
			instrument->ifc = ( char ) data_u8( data, iofs + 0x3A );
			instrument->ifr = ( char ) data_u8( data, iofs + 0x3B );
		}
	} else {
		for( ins = 1; ins <= module->num_instruments; ins++ ) {
			instrument = &module->instruments[ ins ];
			if( temp_samples && ins <= smp_num ) {
				/* Sample mode: transfer ownership, no copy. */
				instrument->samples[ 0 ] = temp_samples[ ins - 1 ];
				memcpy( instrument->name, temp_samples[ ins - 1 ].name, 26 );
				temp_samples[ ins - 1 ].data = NULL;
			}
		}
	}
	/* Patterns. */
	module->patterns = calloc( module->num_patterns, sizeof( struct pattern ) );
	if( !module->patterns ) {
		goto it_oom;
	}
	for( pat_ofs_idx = 0; pat_ofs_idx < module->num_patterns; pat_ofs_idx++ ) {
		int pofs = pat_num > 0 ? data_u32le( data,
			ofs + ins_num * 4 + smp_num * 4 + pat_ofs_idx * 4 ) : 0;
		num_rows = 64;
		pat_len = 0;
		if( pofs > 0 ) {
			pat_len = data_u16le( data, pofs );
			num_rows = data_u16le( data, pofs + 2 );
			if( num_rows < 1 || num_rows > 200 ) {
				num_rows = 64;
			}
		}
		module->patterns[ pat_ofs_idx ].num_channels = module->num_channels;
		module->patterns[ pat_ofs_idx ].num_rows = num_rows;
		pattern_data = calloc( ( size_t ) module->num_channels * num_rows, 5 );
		if( !pattern_data ) {
			goto it_oom;
		}
		module->patterns[ pat_ofs_idx ].data = pattern_data;
		if( pofs == 0 ) {
			continue;
		}
		memset( last_mask, 0, sizeof( last_mask ) );
		memset( last_note, 0, sizeof( last_note ) );
		memset( last_ins, 0, sizeof( last_ins ) );
		memset( last_vol, 0, sizeof( last_vol ) );
		memset( last_fx, 0, sizeof( last_fx ) );
		memset( last_pm, 0, sizeof( last_pm ) );
		sub = pofs + 8;
		row = 0;
		while( row < num_rows && sub < pofs + 8 + pat_len ) {
			tok = data_u8( data, sub++ );
			if( tok == 0 ) {
				row++;
				continue;
			}
			chan = ( tok - 1 ) & 63;
			if( tok & 0x80 ) {
				last_mask[ chan ] = ( unsigned char ) data_u8( data, sub++ );
			}
			mask = last_mask[ chan ];
			key = ins = volume = effect = param = 0;
			if( mask & 0x01 ) {
				last_note[ chan ] = ( unsigned char ) data_u8( data, sub++ );
			}
			if( mask & 0x11 ) {
				entry = last_note[ chan ];
				if( entry >= 254 ) {
					/* 255 is note off (97); 254 is note cut, carried
					   as 98 for its immediate-silence semantics. */
					key = entry == 254 ? 98 : 97;
				} else if( entry <= 119 ) {
					key = entry - 11;
					if( key < 1 || key > 96 ) {
						key = 0;
					}
				}
			}
			if( mask & 0x02 ) {
				last_ins[ chan ] = ( unsigned char ) data_u8( data, sub++ );
			}
			if( mask & 0x22 ) {
				ins = last_ins[ chan ];
				if( ins > module->num_instruments ) {
					ins = 0;
				}
			}
			if( mask & 0x04 ) {
				last_vol[ chan ] = ( unsigned char ) data_u8( data, sub++ );
			}
			if( mask & 0x44 ) {
				entry = last_vol[ chan ];
				if( entry <= 64 ) {
					volume = entry + 0x10;
				} else if( entry <= 74 ) {
					volume = 0x90 | ( entry - 65 );  /* fine vol up */
				} else if( entry <= 84 ) {
					volume = 0x80 | ( entry - 75 );  /* fine vol down */
				} else if( entry <= 94 ) {
					volume = 0x70 | ( entry - 85 );  /* vol slide up */
				} else if( entry <= 104 ) {
					volume = 0x60 | ( entry - 95 );  /* vol slide down */
				} else if( entry >= 128 && entry <= 192 ) {
					entry = ( entry - 128 ) >> 2;
					volume = 0xC0 | ( entry > 15 ? 15 : entry );
				} else if( entry >= 193 && entry <= 202 ) {
					/* Tone porta; IT's rate table is coarser than the
					   nibble this passes, an approximation. */
					volume = 0xF0 | ( entry - 193 );
				} else if( entry >= 203 && entry <= 212 ) {
					volume = 0xB0 | ( entry - 203 );  /* vibrato depth */
				}
			}
			if( mask & 0x08 ) {
				last_fx[ chan ] = ( unsigned char ) data_u8( data, sub++ );
				last_pm[ chan ] = ( unsigned char ) data_u8( data, sub++ );
			}
			if( mask & 0x88 ) {
				effect = last_fx[ chan ];
				param = last_pm[ chan ];
				if( effect < 1 || effect >= 0x40 ) {
					effect = param = 0;
				} else {
					effect += 0x80;
				}
			}
			if( chan < module->num_channels && row < num_rows ) {
				idx = ( row * module->num_channels + chan ) * 5;
				pattern_data[ idx     ] = ( char ) key;
				pattern_data[ idx + 1 ] = ( char ) ins;
				pattern_data[ idx + 2 ] = ( char ) volume;
				pattern_data[ idx + 3 ] = ( char ) effect;
				pattern_data[ idx + 4 ] = ( char ) param;
			}
		}
	}
	if( temp_samples ) {
		for( idx = 0; idx < smp_num; idx++ ) {
			free( temp_samples[ idx ].data );
		}
		free( temp_samples );
	}
	free( smp_frames );
	( void ) message;
	return module;
it_oom:
	if( temp_samples ) {
		for( idx = 0; idx < smp_num; idx++ ) {
			free( temp_samples[ idx ].data );
		}
		free( temp_samples );
	}
	free( smp_frames );
	dispose_module( module );
	return NULL;
}

/* Untagged 15-instrument Soundtracker modules carry no format tag to
 * key on, so detection is a plausibility check on the fixed-position
 * header fields: a printable title and sample names, a sane sequence
 * length and sequence entries within the 7-bit pattern range. This is
 * the acceptance test pocketmod applied (title and names), tightened
 * with the sequence checks. A 31-instrument module with a tag this
 * loader does not know can in principle slip past it - its bytes at
 * these offsets are also sample names - but such a file would not have
 * played either way, and the sequence checks reject most of them. */
static int mod_st15_check( struct data *data ) {
	int idx, sub, chr, seq_len;
	if( data->length < 600 ) {
		return 0;
	}
	for( idx = 0; idx < 20; idx++ ) {
		chr = data_u8( data, idx );
		if( chr != 0 && ( chr < 32 || chr > 126 ) ) {
			return 0;
		}
	}
	for( idx = 0; idx < 15; idx++ ) {
		for( sub = 0; sub < 22; sub++ ) {
			chr = data_u8( data, 20 + idx * 30 + sub );
			if( chr != 0 && ( chr < 32 || chr > 126 ) ) {
				return 0;
			}
		}
	}
	seq_len = data_u8( data, 470 );
	if( seq_len < 1 || seq_len > 128 ) {
		return 0;
	}
	for( idx = 0; idx < 128; idx++ ) {
		if( data_u8( data, 472 + idx ) > 127 ) {
			return 0;
		}
	}
	return 1;
}

static struct module* module_load_mod( struct data *data, char *message ) {
	int idx, pat, module_data_idx, pat_data_len, pat_data_idx;
	int period, key, ins, effect, param, fine_tune;
	int sample_length, loop_start, loop_length;
	int tag, pre, flt8, seq_offset, src_idx, cell, row, chan;
	char *pattern_data;
	struct instrument *instrument;
	struct sample *sample;
	struct module *module = calloc( 1, sizeof( struct module ) );
	if( module ) {
		data_ascii( data, 0, 20, module->name );
		flt8 = 0;
		seq_offset = 950;
		module_data_idx = 1084;
		module->num_instruments = 31;
		tag = data_u16be( data, 1082 );
		pre = data_u16be( data, 1080 );
		switch( tag ) {
			case 0x4b2e: /* M.K. */
			case 0x4b21: /* M!K! */
			case 0x5434: /* FLT4 */
				module->num_channels = 4;
				module->c2_rate = 8287;
				module->gain = 64;
				break;
			case 0x5438: /* FLT8 */
			case 0x4f34: /* EXO4 */
			case 0x4f38: /* EXO8 */
			case 0x5441: /* OKTA / OCTA */
			case 0x3831: /* CD81 */
			case 0x3038: /* FA08 */
				/* The switch keys on the last two tag bytes, so
				   confirm the leading two before accepting; a miss
				   falls through to the untagged heuristic. */
				if( tag == 0x4f34 && pre == 0x4558 ) {
					/* Startrekker AM 4-channel: FLT4 layout. The AM
					   synth instruments live in a companion file and
					   are not synthesised; their sample data is
					   absent so they play silent, as in players
					   without the companion. */
					module->num_channels = 4;
					module->c2_rate = 8287;
					module->gain = 64;
					break;
				}
				if( ( tag == 0x5438 && pre == 0x464c )
				 || ( tag == 0x4f38 && pre == 0x4558 )
				 || ( tag == 0x5441 && ( pre == 0x4f4b || pre == 0x4f43 ) )
				 || ( tag == 0x3831 && pre == 0x4344 )
				 || ( tag == 0x3038 && pre == 0x4641 ) ) {
					module->num_channels = 8;
					module->c2_rate = 8287;
					module->gain = 32;
					if( tag == 0x5438 || tag == 0x4f38 ) {
						/* EXO8 is Startrekker AM's FLT8: the same
						   paired 4-channel pattern storage. */
						flt8 = 1;
					}
					break;
				}
				/* fall through */
			default:
				if( mod_st15_check( data ) ) {
					/* Untagged 15-instrument Soundtracker module:
					   the sequence sits at 470 and pattern data at
					   600 rather than 950 and 1084. */
					module->num_instruments = 15;
					module->num_channels = 4;
					module->c2_rate = 8287;
					module->gain = 64;
					seq_offset = 470;
					module_data_idx = 600;
					break;
				}
				strcpy( message, "MOD Format not recognised!" );
				dispose_module( module );
				return NULL;
			case 0x484e: /* xCHN */
				module->num_channels = data_u8( data, 1080 ) - 48;
				module->c2_rate = 8363;
				module->gain = 32;
				break;
			case 0x4348: /* xxCH */
				module->num_channels = ( data_u8( data, 1080 ) - 48 ) * 10;
				module->num_channels += data_u8( data, 1081 ) - 48;
				module->c2_rate = 8363;
				module->gain = 32;
				break;
		}
		module->sequence_len = data_u8( data, seq_offset ) & 0x7F;
		if( module->sequence_len < 1 ) {
			module->sequence_len = 1; /* see the XM loader note */
		}
		module->restart_pos = data_u8( data, seq_offset + 1 ) & 0x7F;
		if( module->restart_pos >= module->sequence_len ) {
			module->restart_pos = 0;
		}
		module->sequence = calloc( 128, sizeof( unsigned char ) );
		if( !module->sequence ){
			dispose_module( module );
			return NULL;
		}
		for( idx = 0; idx < 128; idx++ ) {
			pat = data_u8( data, seq_offset + 2 + idx ) & 0x7F;
			if( flt8 ) {
				/* An FLT8 sequence addresses the stored 4-channel
				   patterns, which come in pairs; halve to index the
				   combined 8-channel patterns. */
				pat = pat >> 1;
			}
			module->sequence[ idx ] = pat;
			if( pat >= module->num_patterns ) {
				module->num_patterns = pat + 1;
			}
		}
		module->default_gvol = 64;
		module->default_speed = 6;
		module->default_tempo = 125;
		module->default_panning = calloc( module->num_channels, sizeof( unsigned char ) );
		if( !module->default_panning ) {
			dispose_module( module );
			return NULL;
		}
		for( idx = 0; idx < module->num_channels; idx++ ) {
			module->default_panning[ idx ] = 51;
			if( ( idx & 3 ) == 1 || ( idx & 3 ) == 2 ) {
				module->default_panning[ idx ] = 204;
			}
		}
		module->patterns = calloc( module->num_patterns, sizeof( struct pattern ) );
		if( !module->patterns ) {
			dispose_module( module );
			return NULL;
		}
		pat_data_len = module->num_channels * 64 * 5;
		for( pat = 0; pat < module->num_patterns; pat++ ) {
			module->patterns[ pat ].num_channels = module->num_channels;
			module->patterns[ pat ].num_rows = 64;
			pattern_data = calloc( 1, pat_data_len );
			if( !pattern_data ) {
				dispose_module( module );
				return NULL;
			}
			module->patterns[ pat ].data = pattern_data;
			for( pat_data_idx = 0; pat_data_idx < pat_data_len; pat_data_idx += 5 ) {
				src_idx = module_data_idx;
				chan = 0;
				if( flt8 ) {
					/* An 8-channel FLT8 pattern is stored as two
					   consecutive 4-channel patterns: the first
					   holds channels 0-3, the second channels 4-7. */
					cell = pat_data_idx / 5;
					row = cell >> 3;
					chan = cell & 7;
					src_idx = 1084 + ( pat << 11 )
						+ ( ( chan >> 2 ) << 10 )
						+ ( row << 4 ) + ( ( chan & 3 ) << 2 );
				}
				period = ( data_u8( data, src_idx ) & 0xF ) << 8;
				period = ( period | data_u8( data, src_idx + 1 ) ) * 4;
				if( period >= 112 && period <= 6848 ) {
					key = -12 * log_2( ( period << FP_SHIFT ) / 29021 );
					key = ( key + ( key & ( FP_ONE >> 1 ) ) ) >> FP_SHIFT;
					pattern_data[ pat_data_idx ] = key;
				}
				ins = ( data_u8( data, src_idx + 2 ) & 0xF0 ) >> 4;
				ins = ins | ( data_u8( data, src_idx ) & 0x10 );
				pattern_data[ pat_data_idx + 1 ] = ins;
				effect = data_u8( data, src_idx + 2 ) & 0x0F;
				param  = data_u8( data, src_idx + 3 );
				if( flt8 && chan >= 4 && effect == 0xE ) {
					/* Startrekker uses Exx on the second pattern of
					   a pair for AM synth macros, not effects. */
					effect = param = 0;
				}
				if( param == 0 && ( effect < 3 || effect == 0xA ) ) {
					effect = 0;
				}
				if( param == 0 && ( effect == 5 || effect == 6 ) ) {
					effect -= 2;
				}
				if( effect == 8 ) {
					if( module->num_channels == 4 ) {
						effect = param = 0;
					} else if( param > 128 ) {
						param = 128;
					} else {
						param = ( param * 255 ) >> 7;
					}
				}
				pattern_data[ pat_data_idx + 3 ] = effect;
				pattern_data[ pat_data_idx + 4 ] = param;
				module_data_idx += 4;
			}
		}
		module->instruments = calloc( module->num_instruments + 1, sizeof( struct instrument ) );
		if( !module->instruments ) {
			dispose_module( module );
			return NULL;
		}
		instrument = &module->instruments[ 0 ];
		instrument->num_samples = 1;
		instrument->samples = calloc( 1, sizeof( struct sample ) );
		if( !instrument->samples ) {
			dispose_module( module );
			return NULL;
		}
		for( ins = 1; ins <= module->num_instruments; ins++ ) {
			instrument = &module->instruments[ ins ];
			instrument->num_samples = 1;
			instrument->samples = calloc( 1, sizeof( struct sample ) );
			if( !instrument->samples ) {
				dispose_module( module );
				return NULL;
			}
			sample = &instrument->samples[ 0 ];
			data_ascii( data, ins * 30 - 10, 22, instrument->name );
			sample_length = data_u16be( data, ins * 30 + 12 ) * 2;
			fine_tune = ( data_u8( data, ins * 30 + 14 ) & 0xF ) << 4;
			sample->fine_tune = ( fine_tune & 0x7F ) - ( fine_tune & 0x80 );
			sample->volume = data_u8( data, ins * 30 + 15 ) & 0x7F;
			if( sample->volume > 64 ) {
				sample->volume = 64;
			}
			loop_start = data_u16be( data, ins * 30 + 16 ) * 2;
			loop_length = data_u16be( data, ins * 30 + 18 ) * 2;
			if( loop_start + loop_length > sample_length ) {
				loop_length = sample_length - loop_start;
			}
			if( loop_length < 4 ) {
				loop_start = sample_length;
				loop_length = 0;
			}
			sample->loop_start = loop_start;
			sample->loop_length = loop_length;
			sample->data = calloc( sample_length + 1, sizeof( short ) );
			if( sample->data ) {
				data_sam_s8( data, module_data_idx, sample_length, sample->data );
				sample->data[ loop_start + loop_length ] = sample->data[ loop_start ];
			} else {
				dispose_module( module );
				return NULL;
			}
			module_data_idx += sample_length;
		}
	}
	return module;
}

/* Allocate and initialize a module from the specified data, returns NULL on error.
   Message must point to a 64-character buffer to receive error messages. */
static struct module* module_load( struct data *data, char *message ) {
	char ascii[ 16 ];
	struct module* module;
	if( !memcmp( data_ascii( data, 0, 16, ascii ), "Extended Module:", 16 ) ) {
		module = module_load_xm( data, message );
	} else if( !memcmp( data_ascii( data, 0, 4, ascii ), "IMPM", 4 ) ) {
		module = module_load_it( data, message );
	} else if( !memcmp( data_ascii( data, 44, 4, ascii ), "SCRM", 4 ) ) {
		module = module_load_s3m( data, message );
	} else {
		module = module_load_mod( data, message );
	}
	return module;
}

static void pattern_get_note( struct pattern *pattern, int row, int chan, struct note *dest ) {
	int offset = ( row * pattern->num_channels + chan ) * 5;
	if( offset >= 0 && row < pattern->num_rows && chan < pattern->num_channels ) {
		dest->key = pattern->data[ offset ];
		dest->instrument = pattern->data[ offset + 1 ];
		dest->volume = pattern->data[ offset + 2 ];
		dest->effect = pattern->data[ offset + 3 ];
		dest->param = pattern->data[ offset + 4 ];
	} else {
		memset( dest, 0, sizeof( struct note ) );
	}
}

static void channel_init( struct channel *channel, struct replay *replay, int idx ) {
	memset( channel, 0, sizeof( struct channel ) );
	channel->replay = replay;
	channel->id = idx;
	channel->panning = replay->module->default_panning[ idx ];
	channel->chan_vol = replay->module->default_chan_vol
		? replay->module->default_chan_vol[ idx ] : 64;
	channel->flt_cutoff = 127;
	channel->flt_env = 255;
	channel->flt_key = -1;
	channel->instrument = &replay->module->instruments[ 0 ];
	channel->sample = &channel->instrument->samples[ 0 ];
	/* Unsigned: the channel count comes from the file and is not
	 * clamped, and this overflows a signed int from 191 channels up.
	 * It is a seed, so wrapping is fine - being undefined is not. */
	channel->random_seed = ( int ) ( ( ( unsigned int ) idx + 1u ) * 0xABCDEFu );
}

/* IT Nxy: the volume-slide grammar applied to the channel volume,
   fine variants included. */
static void channel_cvol_slide( struct channel *channel ) {
	int up = channel->cvol_slide_param >> 4;
	int down = channel->cvol_slide_param & 0xF;
	if( down == 0xF && up > 0 ) {
		if( channel->fx_count == 0 ) {
			channel->chan_vol += up;
		}
	} else if( up == 0xF && down > 0 ) {
		if( channel->fx_count == 0 ) {
			channel->chan_vol -= down;
		}
	} else if( channel->fx_count > 0 ) {
		channel->chan_vol += up - down;
	}
	if( channel->chan_vol > 64 ) {
		channel->chan_vol = 64;
	}
	if( channel->chan_vol < 0 ) {
		channel->chan_vol = 0;
	}
}

static void channel_volume_slide( struct channel *channel ) {
	int up = channel->vol_slide_param >> 4;
	int down = channel->vol_slide_param & 0xF;
	if( down == 0xF && up > 0 ) {
		/* Fine slide up.*/
		if( channel->fx_count == 0 ) {
			channel->volume += up;
		}
	} else if( up == 0xF && down > 0 ) {
		/* Fine slide down.*/
		if( channel->fx_count == 0 ) {
			channel->volume -= down;
		}
	} else if( channel->fx_count > 0 || channel->replay->module->fast_vol_slides ) {
		/* Normal.*/
		channel->volume += up - down;
	}
	if( channel->volume > 64 ) {
		channel->volume = 64;
	}
	if( channel->volume < 0 ) {
		channel->volume = 0;
	}
}

static void channel_porta_up( struct channel *channel, int param ) {
	switch( param & 0xF0 ) {
		case 0xE0: /* Extra-fine porta.*/
			if( channel->fx_count == 0 ) {
				channel->period -= param & 0xF;
			}
			break;
		case 0xF0: /* Fine porta.*/
			if( channel->fx_count == 0 ) {
				channel->period -= ( param & 0xF ) << 2;
			}
			break;
		default:/* Normal porta.*/
			if( channel->fx_count > 0 ) {
				channel->period -= param << 2;
			}
			break;
	}
	if( channel->period < 0 ) {
		channel->period = 0;
	}
}

static void channel_porta_down( struct channel *channel, int param ) {
	if( channel->period > 0 ) {
		switch( param & 0xF0 ) {
			case 0xE0: /* Extra-fine porta.*/
				if( channel->fx_count == 0 ) {
					channel->period += param & 0xF;
				}
				break;
			case 0xF0: /* Fine porta.*/
				if( channel->fx_count == 0 ) {
					channel->period += ( param & 0xF ) << 2;
				}
				break;
			default:/* Normal porta.*/
				if( channel->fx_count > 0 ) {
					channel->period += param << 2;
				}
				break;
		}
		if( channel->period > 65535 ) {
			channel->period = 65535;
		}
	}
}

static void channel_tone_porta( struct channel *channel ) {
	if( channel->period > 0 ) {
		if( channel->period < channel->porta_period ) {
			channel->period += channel->tone_porta_param << 2;
			if( channel->period > channel->porta_period ) {
				channel->period = channel->porta_period;
			}
		} else {
			channel->period -= channel->tone_porta_param << 2;
			if( channel->period < channel->porta_period ) {
				channel->period = channel->porta_period;
			}
		}
	}
}

static int channel_waveform( struct channel *channel, int phase, int type ) {
	int amplitude = 0;
	switch( type ) {
		default: /* Sine. */
			amplitude = sine_table[ phase & 0x1F ];
			if( ( phase & 0x20 ) > 0 ) {
				amplitude = -amplitude;
			}
			break;
		case 6: /* Saw Up.*/
			amplitude = ( ( ( phase + 0x20 ) & 0x3F ) << 3 ) - 255;
			break;
		case 1: case 7: /* Saw Down. */
			amplitude = 255 - ( ( ( phase + 0x20 ) & 0x3F ) << 3 );
			break;
		case 2: case 5: /* Square. */
			amplitude = ( phase & 0x20 ) > 0 ? 255 : -255;
			break;
		case 3: case 8: /* Random. */
			amplitude = ( channel->random_seed >> 20 ) - 255;
			/* Masked to 0x1FFFFFFF, so the multiply below overflows a
			 * signed int before the mask ever runs. Wrapping is the
			 * intent of an LCG; undefined behaviour is not. */
			channel->random_seed = ( int ) ( ( ( unsigned int )
					channel->random_seed * 65u + 17u ) & 0x1FFFFFFFu );
			break;
	}
	return amplitude;
}

static void channel_vibrato( struct channel *channel, int fine ) {
	int wave = channel_waveform( channel, channel->vibrato_phase, channel->vibrato_type & 0x3 );
	channel->vibrato_add = wave * channel->vibrato_depth >> ( fine ? 7 : 5 );
}

static void channel_tremolo( struct channel *channel ) {
	int wave = channel_waveform( channel, channel->tremolo_phase, channel->tremolo_type & 0x3 );
	channel->tremolo_add = wave * channel->tremolo_depth >> 6;
}

static void channel_tremor( struct channel *channel ) {
	if( channel->retrig_count >= channel->tremor_on_ticks ) {
		channel->tremolo_add = -64;
	}
	if( channel->retrig_count >= ( channel->tremor_on_ticks + channel->tremor_off_ticks ) ) {
		channel->tremolo_add = channel->retrig_count = 0;
	}
}

static void channel_retrig_vol_slide( struct channel *channel ) {
	if( channel->retrig_count >= channel->retrig_ticks ) {
		channel->retrig_count = channel->sample_idx = channel->sample_fra = 0;
		switch( channel->retrig_volume ) {
			case 0x1: channel->volume = channel->volume -  1; break;
			case 0x2: channel->volume = channel->volume -  2; break;
			case 0x3: channel->volume = channel->volume -  4; break;
			case 0x4: channel->volume = channel->volume -  8; break;
			case 0x5: channel->volume = channel->volume - 16; break;
			case 0x6: channel->volume = channel->volume * 2 / 3; break;
			case 0x7: channel->volume = channel->volume >> 1; break;
			case 0x8: /* ? */ break;
			case 0x9: channel->volume = channel->volume +  1; break;
			case 0xA: channel->volume = channel->volume +  2; break;
			case 0xB: channel->volume = channel->volume +  4; break;
			case 0xC: channel->volume = channel->volume +  8; break;
			case 0xD: channel->volume = channel->volume + 16; break;
			case 0xE: channel->volume = channel->volume * 3 / 2; break;
			case 0xF: channel->volume = channel->volume << 1; break;
		}
		if( channel->volume <  0 ) {
			channel->volume = 0;
		}
		if( channel->volume > 64 ) {
			channel->volume = 64;
		}
	}
}

/* Move the channel's current voice into a background slot when the
   arriving note's instrument asks for a new-note action other than
   cut. Runs at trigger entry, before the trigger overwrites the old
   instrument and envelope state. */
static void channel_capture_ghost( struct channel *channel ) {
	struct replay *replay = channel->replay;
	struct channel *ghost;
	int idx, best, best_ampl, nna, porta;
	if( !replay->ghosts || !channel->sample || channel->ampl <= 0 ) {
		return;
	}
	if( channel->note.key < 1 || channel->note.key > 96 ) {
		return;
	}
	porta = ( channel->note.volume & 0xF0 ) == 0xF0 ||
		channel->note.effect == 0x03 || channel->note.effect == 0x05 ||
		channel->note.effect == 0x87 || channel->note.effect == 0x8C;
	if( porta ) {
		return;
	}
	nna = channel->instrument->nna;
	if( nna < 1 || nna > 3 ) {
		return;
	}
	best = 0;
	best_ampl = INT_MAX;
	for( idx = 0; idx < RMT_NUM_GHOSTS; idx++ ) {
		ghost = &replay->ghosts[ idx ];
		if( !ghost->sample ) {
			best = idx;
			break;
		}
		if( ghost->ampl < best_ampl ) {
			best_ampl = ghost->ampl;
			best = idx;
		}
	}
	ghost = &replay->ghosts[ best ];
	*ghost = *channel;
	ghost->note.effect = ghost->note.param = ghost->note.volume = 0;
	ghost->tremolo_add = ghost->vibrato_add = ghost->arpeggio_add = 0;
	if( nna >= 2 ) {
		/* Note off releases the envelope and starts the fade; fade
		   proper would hold the sustain point while fading, which
		   this approximates as a release. Continue leaves the voice
		   exactly as it was. */
		ghost->key_on = 0;
	}
	/* Duplicate check: the incoming note's instrument can ask for
	   its own duplicates on this channel - including the voice just
	   moved aside - to be cut, released or faded, which is what
	   stops repeated NNA notes stacking without bound. Runs after
	   the move, as in IT. The sample check (2) is approximated as
	   an instrument check, since instrument-mode sample copies make
	   pointer identity per-instrument. */
	idx = channel->note.instrument;
	if( idx > 0 && idx <= replay->module->num_instruments ) {
		struct instrument *newins = &replay->module->instruments[ idx ];
		int dct = newins->dct & 3;
		if( dct ) {
			for( idx = 0; idx < RMT_NUM_GHOSTS; idx++ ) {
				int match;
				ghost = &replay->ghosts[ idx ];
				if( !ghost->sample || ghost->id != channel->id
				 || ghost->instrument != newins ) {
					continue;
				}
				match = dct != 1
					|| ghost->note.key == channel->note.key;
				if( match ) {
					if( ( newins->dca & 3 ) == 0 ) {
						ghost->sample = NULL;
					} else {
						ghost->key_on = 0;
					}
				}
			}
		}
	}
}

static void channel_trigger( struct channel *channel ) {
	int key, sam, porta, period, fine_tune, ins = channel->note.instrument;
	struct sample *sample;
	channel_capture_ghost( channel );
	if( ins > 0 && ins <= channel->replay->module->num_instruments ) {
		channel->instrument = &channel->replay->module->instruments[ ins ];
		key = channel->note.key < 97 ? channel->note.key : 0;
		sam = channel->instrument->key_to_sample[ key ];
		sample = &channel->instrument->samples[ sam ];
		channel->volume = sample->volume >= 64 ? 64 : sample->volume & 0x3F;
		if( sample->panning > 0 ) {
			channel->panning = ( sample->panning - 1 ) & 0xFF;
		}
		if( channel->period > 0 && sample->loop_length > 1 ) {
			/* Amiga trigger.*/
			channel->sample = sample;
		}
		channel->sample_off = 0;
		channel->vol_env_tick = channel->pan_env_tick = 0;
		channel->pitch_env_tick = 0;
		channel->fadeout_vol = 32768;
		channel->key_on = 1;
		/* IT filter reset on an instrument-carrying note: the
		   envelope value opens, initial cutoff/resonance apply when
		   their set bits say so, and the filter memory clears. */
		channel->flt_env = 255;
		if( channel->instrument->ifc & 0x80 ) {
			channel->flt_cutoff = channel->instrument->ifc & 0x7F;
		}
		if( channel->instrument->ifr & 0x80 ) {
			channel->flt_q = channel->instrument->ifr & 0x7F;
		}
		channel->flt_y1l = channel->flt_y2l = 0;
		channel->flt_y1r = channel->flt_y2r = 0;
		channel->flt_errl = channel->flt_errr = 0;
	}
	if( channel->note.effect == 0x09 || channel->note.effect == 0x8F ) {
		/* Set Sample Offset. */
		if( channel->note.param > 0 ) {
			channel->offset_param = channel->note.param;
		}
		channel->sample_off = ( channel->offset_param << 8 )
			+ channel->high_offset;
	}
	if( channel->note.volume >= 0x10 && channel->note.volume < 0x60 ) {
		channel->volume = channel->note.volume < 0x50 ? channel->note.volume - 0x10 : 64;
	}
	switch( channel->note.volume & 0xF0 ) {
		case 0x80: /* Fine Vol Down.*/
			channel->volume -= channel->note.volume & 0xF;
			if( channel->volume < 0 ) {
				channel->volume = 0;
			}
			break;
		case 0x90: /* Fine Vol Up.*/
			channel->volume += channel->note.volume & 0xF;
			if( channel->volume > 64 ) {
				channel->volume = 64;
			}
			break;
		case 0xA0: /* Set Vibrato Speed.*/
			if( ( channel->note.volume & 0xF ) > 0 ) {
				channel->vibrato_speed = channel->note.volume & 0xF;
			}
			break;
		case 0xB0: /* Vibrato.*/
			if( ( channel->note.volume & 0xF ) > 0 ) {
				channel->vibrato_depth = channel->note.volume & 0xF;
			}
			channel_vibrato( channel, 0 );
			break;
		case 0xC0: /* Set Panning.*/
			channel->panning = ( channel->note.volume & 0xF ) * 17;
			break;
		case 0xF0: /* Tone Porta.*/
			if( ( channel->note.volume & 0xF ) > 0 ) {
				channel->tone_porta_param = channel->note.volume & 0xF;
			}
			break;
	}
	if( channel->note.key > 0 ) {
		if( channel->note.key > 96 ) {
			channel->key_on = 0;
			if( channel->note.key == 98 ) {
				/* IT note cut: immediate silence rather than an
				   envelope release. */
				channel->volume = 0;
			}
		} else {
			porta = ( channel->note.volume & 0xF0 ) == 0xF0 ||
				channel->note.effect == 0x03 || channel->note.effect == 0x05 ||
				channel->note.effect == 0x87 || channel->note.effect == 0x8C;
			if( !porta ) {
				ins = channel->instrument->key_to_sample[ channel->note.key ];
				channel->sample = &channel->instrument->samples[ ins ];
			}
			fine_tune = channel->sample->fine_tune;
			if( channel->note.effect == 0x75 || channel->note.effect == 0xF2 ) {
				/* Set Fine Tune. */
				fine_tune = ( ( channel->note.param & 0xF ) << 4 ) - 128;
			}
			key = channel->note.key + channel->sample->rel_note;
			if( key < 1 ) {
				key = 1;
			}
			if( key > 120 ) {
				key = 120;
			}
			period = ( key << 6 ) + ( fine_tune >> 1 );
			if( channel->replay->module->linear_periods ) {
				channel->porta_period = 7744 - period;
			} else {
				channel->porta_period = 29021 * exp_2( ( period << FP_SHIFT ) / -768 ) >> FP_SHIFT;
			}
			if( !porta ) {
				channel->period = channel->porta_period;
				channel->sample_idx = channel->sample_off;
				channel->sample_fra = 0;
				if( channel->vibrato_type < 4 ) {
					channel->vibrato_phase = 0;
				}
				if( channel->tremolo_type < 4 ) {
					channel->tremolo_phase = 0;
				}
				channel->retrig_count = channel->av_count = 0;
			}
		}
	}
}

/* Recompute the IT filter coefficients when cutoff, resonance or the
   filter-envelope value changed. The formulas are IT2's own as
   preserved in it2play: r tracks the cutoff as a power of two scaled
   to the mixing rate (the 2x oversampled stage here), p is the
   resonance table, and a/b/c land in Q14 integers. The double math
   runs at event rate only; the per-sample filter is pure integer, so
   the deterministic-rendering guarantee is untouched. */
static int calculate_tick_len( int tempo, int sample_rate );

static void channel_filter_coeffs( struct channel *channel ) {
	int ffv = channel->flt_env * channel->flt_cutoff;
	int key = ( ffv << 7 ) | channel->flt_q;
	double r, p, d, e, a, b, c;
	if( key == channel->flt_key ) {
		return;
	}
	channel->flt_key = key;
	if( ffv == 127 * 255 && channel->flt_q == 0 ) {
		channel->flt_on = 0;
		return;
	}
	r = pow( 2.0, ffv * ( -1.0 / 6144.0 ) )
		* ( ( ( double ) channel->replay->sample_rate * 2.0 )
			/ ( 2.0 * 3.14159265358979324 * 110.0 * 1.18920711500272107 ) );
	p = pow( 10.0, channel->flt_q * ( -24.0 / 2560.0 ) );
	d = p * r + ( p - 1.0 );
	e = r * r;
	a = 16384.0 / ( 1.0 + d + e );
	b = ( d + e + e ) * a;
	c = -( e * a );
	channel->flt_a = ( int ) ( a + 0.5 );
	channel->flt_b = ( int ) ( b + ( b >= 0.0 ? 0.5 : -0.5 ) );
	channel->flt_c = ( int ) ( c - 0.5 );
	channel->flt_on = 1;
}

/* The per-tick work for the third envelope: as a pitch envelope it is
   a cumulative linear slide of ( value * 32 ) / 768 octaves per tick,
   folded into the period the engine recomputes pitch from; as a
   filter envelope it scales the cutoff through flt_env ( value * 4,
   0..255, 255 with the envelope off ). Values are stored biased +32.
   Ghosts keep their filter envelopes running but their frequency is
   frozen by design, so the pitch branch skips them. */
static void channel_update_pitch_filter( struct channel *channel ) {
	struct envelope *env = &channel->instrument->pitch_env;
	int val, slide;
	if( env->enabled ) {
		val = envelope_calculate_ampl( env, channel->pitch_env_tick );
		if( channel->instrument->pitch_is_filter ) {
			val = val * 4;
			channel->flt_env = val > 255 ? 255 : val;
		} else if( channel->sample ) {
			slide = ( val - 32 ) * 32;
			if( slide != 0 && channel->period > 0 ) {
				channel->period = ( channel->period * FP_ONE )
					/ exp_2( ( slide * FP_ONE ) / 768 );
				if( channel->period < 1 ) {
					channel->period = 1;
				}
			}
		}
		channel->pitch_env_tick = envelope_next_tick( env,
			channel->pitch_env_tick, channel->key_on );
	}
	channel_filter_coeffs( channel );
}

static void channel_update_envelopes( struct channel *channel ) {
	channel_update_pitch_filter( channel );
	if( channel->instrument->vol_env.enabled ) {
		struct envelope *env = &channel->instrument->vol_env;
		int fade = !channel->key_on;
		/* IT starts the fadeout as soon as a non-looping volume
		   envelope reaches its final node, key on or not - a
		   sustained envelope never reaches it while the key is held.
		   XM holds the last node forever, so this is gated. */
		if( !fade && channel->replay->module->it_effects
				&& !env->looped && env->num_points > 0
				&& channel->vol_env_tick
					>= env->points_tick[ ( int ) env->num_points - 1 ] ) {
			fade = 1;
		}
		if( fade ) {
			channel->fadeout_vol -= channel->instrument->vol_fadeout;
			if( channel->fadeout_vol < 0 ) {
				channel->fadeout_vol = 0;
			}
		}
		channel->vol_env_tick = envelope_next_tick( env,
			channel->vol_env_tick, channel->key_on );
	}
	if( channel->instrument->pan_env.enabled ) {
		channel->pan_env_tick = envelope_next_tick( &channel->instrument->pan_env,
			channel->pan_env_tick, channel->key_on );
	}
}

static void channel_auto_vibrato( struct channel *channel ) {
	int sweep, rate, type, wave;
	int depth = channel->instrument->vib_depth & 0x7F;
	if( depth > 0 ) {
		sweep = channel->instrument->vib_sweep & 0x7F;
		rate = channel->instrument->vib_rate & 0x7F;
		type = channel->instrument->vib_type;
		if( channel->av_count < sweep ) {
			depth = depth * channel->av_count / sweep;
		}
		wave = channel_waveform( channel, channel->av_count * rate >> 2, type + 4 );
		channel->vibrato_add += wave * depth >> 8;
		channel->av_count++;
	}
}

static void channel_calculate_freq( struct channel *channel ) {
	int per = channel->period + channel->vibrato_add;
	if( channel->replay->module->linear_periods ) {
		per = per - ( channel->arpeggio_add << 6 );
		if( per < 28 || per > 7680 ) {
			per = 7680;
		}
		/* FP_ONE is 1 << FP_SHIFT, so this is the same value for a
		 * non-negative operand and defined for a negative one - and
		 * 4608 - per is negative for any period above 4608. */
		channel->freq = ( ( channel->replay->module->c2_rate >> 4 )
			* exp_2( ( ( 4608 - per ) * FP_ONE ) / 768 ) ) >> ( FP_SHIFT - 4 );
	} else {
		if( per > 29021 ) {
			per = 29021;
		}
		/* per is only clamped from above here; vibrato can carry it
		 * below zero, and the "per < 28" guard underneath runs after
		 * this line rather than before it. */
		per = ( per * FP_ONE ) / exp_2( ( channel->arpeggio_add * FP_ONE ) / 12 );
		if( per < 28 ) {
			per = 29021;
		}
		channel->freq = channel->replay->module->c2_rate * 1712 / per;
	}
}

static void channel_calculate_ampl( struct channel *channel ) {
	int vol, range, env_pan = 32, env_vol = channel->key_on ? 64 : 0;
	if( channel->instrument->vol_env.enabled ) {
		env_vol = envelope_calculate_ampl( &channel->instrument->vol_env, channel->vol_env_tick );
	}
	vol = channel->volume + channel->tremolo_add;
	if( vol > 64 ) {
		vol = 64;
	}
	if( vol < 0 ) {
		vol = 0;
	}
	/* At the default of 64 this is vol * 64 >> 6 == vol exactly, so
	   formats without channel volumes are bit-identical; the same
	   holds for the sample global volume below. */
	vol = ( vol * channel->chan_vol ) >> 6;
	vol = ( vol * ( channel->sample->glob_vol
		? channel->sample->glob_vol - 1 : 64 ) ) >> 6;
	vol = ( vol * channel->replay->module->gain * FP_ONE ) >> 13;
	vol = ( vol * channel->fadeout_vol ) >> 15;
	channel->ampl = ( vol * channel->replay->global_vol * env_vol ) >> 12;
	if( channel->instrument->pan_env.enabled ) {
		env_pan = envelope_calculate_ampl( &channel->instrument->pan_env, channel->pan_env_tick );
	}
	range = ( channel->panning < 128 ) ? channel->panning : ( 255 - channel->panning );
	channel->pann = channel->panning + ( range * ( env_pan - 32 ) >> 5 );
}

static void channel_tick( struct channel *channel ) {
	channel->vibrato_add = 0;
	channel->fx_count++;
	channel->retrig_count++;
	if( !( channel->note.effect == 0x7D && channel->fx_count <= channel->note.param ) ) {
		switch( channel->note.volume & 0xF0 ) {
			case 0x60: /* Vol Slide Down.*/
				channel->volume -= channel->note.volume & 0xF;
				if( channel->volume < 0 ) {
					channel->volume = 0;
				}
				break;
			case 0x70: /* Vol Slide Up.*/
				channel->volume += channel->note.volume & 0xF;
				if( channel->volume > 64 ) {
					channel->volume = 64;
				}
				break;
			case 0xB0: /* Vibrato.*/
				channel->vibrato_phase += channel->vibrato_speed;
				channel_vibrato( channel, 0 );
				break;
			case 0xD0: /* Pan Slide Left.*/
				channel->panning -= channel->note.volume & 0xF;
				if( channel->panning < 0 ) {
					channel->panning = 0;
				}
				break;
			case 0xE0: /* Pan Slide Right.*/
				channel->panning += channel->note.volume & 0xF;
				if( channel->panning > 255 ) {
					channel->panning = 255;
				}
				break;
			case 0xF0: /* Tone Porta.*/
				channel_tone_porta( channel );
				break;
		}
	}
	switch( channel->note.effect ) {
		case 0x01: case 0x86: /* Porta Up. */
			channel_porta_up( channel, channel->porta_up_param );
			break;
		case 0x02: case 0x85: /* Porta Down. */
			channel_porta_down( channel, channel->porta_down_param );
			break;
		case 0x03: case 0x87: /* Tone Porta. */
			channel_tone_porta( channel );
			break;
		case 0x04: case 0x88: /* Vibrato. */
			channel->vibrato_phase += channel->vibrato_speed;
			channel_vibrato( channel, 0 );
			break;
		case 0x05: case 0x8C: /* Tone Porta + Vol Slide. */
			channel_tone_porta( channel );
			channel_volume_slide( channel );
			break;
		case 0x06: case 0x8B: /* Vibrato + Vol Slide. */
			channel->vibrato_phase += channel->vibrato_speed;
			channel_vibrato( channel, 0 );
			channel_volume_slide( channel );
			break;
		case 0x07: case 0x92: /* Tremolo. */
			channel->tremolo_phase += channel->tremolo_speed;
			channel_tremolo( channel );
			break;
		case 0x0A: case 0x84: /* Vol Slide. */
			channel_volume_slide( channel );
			break;
		case 0x8E: /* IT Channel Volume Slide. */
			channel_cvol_slide( channel );
			break;
		case 0x94: /* IT Tempo Slide, on the non-row ticks. */
			if( channel->note.param < 0x20
					&& channel->tempo_slide_param > 0 ) {
				int t = channel->replay->tempo;
				if( channel->tempo_slide_param < 0x10 ) {
					t -= channel->tempo_slide_param;
				} else {
					t += channel->tempo_slide_param & 0xF;
				}
				if( t < 32 ) {
					t = 32;
				}
				if( t > 255 ) {
					t = 255;
				}
				channel->replay->tempo = t;
			}
			break;
		case 0x11: case 0x97: /* Global Volume Slide. */
			channel->replay->global_vol = channel->replay->global_vol
				+ ( channel->gvol_slide_param >> 4 )
				- ( channel->gvol_slide_param & 0xF );
			if( channel->replay->global_vol < 0 ) {
				channel->replay->global_vol = 0;
			}
			if( channel->replay->global_vol > 64 ) {
				channel->replay->global_vol = 64;
			}
			break;
		case 0x19: case 0x90: /* Panning Slide. */
			channel->panning = channel->panning
				+ ( channel->pan_slide_param >> 4 )
				- ( channel->pan_slide_param & 0xF );
			if( channel->panning < 0 ) {
				channel->panning = 0;
			}
			if( channel->panning > 255 ) {
				channel->panning = 255;
			}
			break;
		case 0x1B: case 0x91: /* Retrig + Vol Slide. */
			channel_retrig_vol_slide( channel );
			break;
		case 0x1D: case 0x89: /* Tremor. */
			channel_tremor( channel );
			break;
		case 0x79: /* Retrig. */
			if( channel->fx_count >= channel->note.param ) {
				channel->fx_count = 0;
				channel->sample_idx = channel->sample_fra = 0;
			}
			break;
		case 0x7C: case 0xFC: /* Note Cut. */
			if( channel->note.param == channel->fx_count ) {
				channel->volume = 0;
			}
			break;
		case 0x7D: case 0xFD: /* Note Delay. */
			if( channel->note.param == channel->fx_count ) {
				channel_trigger( channel );
			}
			break;
		case 0x8A: /* Arpeggio. */
			if( channel->fx_count == 1 ) {
				channel->arpeggio_add = channel->arpeggio_param >> 4;
			} else if( channel->fx_count == 2 ) {
				channel->arpeggio_add = channel->arpeggio_param & 0xF;
			} else {
				channel->arpeggio_add = channel->fx_count = 0;
			}
			break;
		case 0x95: /* Fine Vibrato. */
			channel->vibrato_phase += channel->vibrato_speed;
			channel_vibrato( channel, 1 );
			break;
	}
	channel_auto_vibrato( channel );
	channel_calculate_freq( channel );
	channel_calculate_ampl( channel );
	channel_update_envelopes( channel );
}

static void channel_row( struct channel *channel, struct note *note ) {
	channel->note = *note;
	channel->retrig_count++;
	channel->vibrato_add = channel->tremolo_add = channel->arpeggio_add = channel->fx_count = 0;
	if( !( ( note->effect == 0x7D || note->effect == 0xFD ) && note->param > 0 ) ) {
		/* Not note delay.*/
		channel_trigger( channel );
	}
	switch( channel->note.effect ) {
		case 0x01: case 0x86: /* Porta Up. */
			if( channel->note.param > 0 ) {
				channel->porta_up_param = channel->note.param;
			}
			channel_porta_up( channel, channel->porta_up_param );
			break;
		case 0x02: case 0x85: /* Porta Down. */
			if( channel->note.param > 0 ) {
				channel->porta_down_param = channel->note.param;
			}
			channel_porta_down( channel, channel->porta_down_param );
			break;
		case 0x03: case 0x87: /* Tone Porta. */
			if( channel->note.param > 0 ) {
				channel->tone_porta_param = channel->note.param;
			}
			break;
		case 0x04: case 0x88: /* Vibrato. */
			if( ( channel->note.param >> 4 ) > 0 ) {
				channel->vibrato_speed = channel->note.param >> 4;
			}
			if( ( channel->note.param & 0xF ) > 0 ) {
				channel->vibrato_depth = channel->note.param & 0xF;
			}
			channel_vibrato( channel, 0 );
			break;
		case 0x05: case 0x8C: /* Tone Porta + Vol Slide. */
			if( channel->note.param > 0 ) {
				channel->vol_slide_param = channel->note.param;
			}
			channel_volume_slide( channel );
			break;
		case 0x06: case 0x8B: /* Vibrato + Vol Slide. */
			if( channel->note.param > 0 ) {
				channel->vol_slide_param = channel->note.param;
			}
			channel_vibrato( channel, 0 );
			channel_volume_slide( channel );
			break;
		case 0x07: case 0x92: /* Tremolo. */
			if( ( channel->note.param >> 4 ) > 0 ) {
				channel->tremolo_speed = channel->note.param >> 4;
			}
			if( ( channel->note.param & 0xF ) > 0 ) {
				channel->tremolo_depth = channel->note.param & 0xF;
			}
			channel_tremolo( channel );
			break;
		case 0x08: /* Set Panning.*/
			channel->panning = channel->note.param & 0xFF;
			break;
		case 0x0A: case 0x84: /* Vol Slide. */
			if( channel->note.param > 0 ) {
				channel->vol_slide_param = channel->note.param;
			}
			channel_volume_slide( channel );
			break;
		case 0x0C: /* Set Volume. */
			channel->volume = channel->note.param >= 64 ? 64 : channel->note.param & 0x3F;
			break;
		case 0x10: case 0x96: /* Set Global Volume. */
			channel->replay->global_vol = channel->note.param >= 64 ? 64 : channel->note.param & 0x3F;
			break;
		case 0x8D: /* IT Set Channel Volume. */
			if( channel->note.param <= 64 ) {
				channel->chan_vol = channel->note.param;
			}
			break;
		case 0x8E: /* IT Channel Volume Slide. */
			if( channel->note.param > 0 ) {
				channel->cvol_slide_param = channel->note.param;
			}
			channel_cvol_slide( channel );
			break;
		case 0x98: /* Set Panning (IT Xxx full range, ST3 halved). */
			if( channel->replay->module->it_effects ) {
				channel->panning = channel->note.param;
			} else if( channel->note.param <= 0x80 ) {
				channel->panning = ( channel->note.param * 255 ) >> 7;
			}
			break;
		case 0x11: case 0x97: /* Global Volume Slide. */
			if( channel->note.param > 0 ) {
				channel->gvol_slide_param = channel->note.param;
			}
			break;
		case 0x14: /* Key Off. */
			channel->key_on = 0;
			break;
		case 0x15: /* Set Envelope Tick. */
			channel->vol_env_tick = channel->pan_env_tick = channel->note.param & 0xFF;
			break;
		case 0x19: case 0x90: /* Panning Slide. */
			if( channel->note.param > 0 ) {
				channel->pan_slide_param = channel->note.param;
			}
			break;
		case 0x1B: case 0x91: /* Retrig + Vol Slide. */
			if( ( channel->note.param >> 4 ) > 0 ) {
				channel->retrig_volume = channel->note.param >> 4;
			}
			if( ( channel->note.param & 0xF ) > 0 ) {
				channel->retrig_ticks = channel->note.param & 0xF;
			}
			channel_retrig_vol_slide( channel );
			break;
		case 0x1D: case 0x89: /* Tremor. */
			if( ( channel->note.param >> 4 ) > 0 ) {
				channel->tremor_on_ticks = channel->note.param >> 4;
			}
			if( ( channel->note.param & 0xF ) > 0 ) {
				channel->tremor_off_ticks = channel->note.param & 0xF;
			}
			channel_tremor( channel );
			break;
		case 0x21: /* Extra Fine Porta. */
			if( channel->note.param > 0 ) {
				channel->xfine_porta_param = channel->note.param;
			}
			switch( channel->xfine_porta_param & 0xF0 ) {
				case 0x10:
					channel_porta_up( channel, 0xE0 | ( channel->xfine_porta_param & 0xF ) );
					break;
				case 0x20:
					channel_porta_down( channel, 0xE0 | ( channel->xfine_porta_param & 0xF ) );
					break;
			}
			break;
		case 0x71: /* Fine Porta Up. */
			if( channel->note.param > 0 ) {
				channel->fine_porta_up_param = channel->note.param;
			}
			channel_porta_up( channel, 0xF0 | ( channel->fine_porta_up_param & 0xF ) );
			break;
		case 0x72: /* Fine Porta Down. */
			if( channel->note.param > 0 ) {
				channel->fine_porta_down_param = channel->note.param;
			}
			channel_porta_down( channel, 0xF0 | ( channel->fine_porta_down_param & 0xF ) );
			break;
		case 0x74: case 0xF3: /* Set Vibrato Waveform. */
			if( channel->note.param < 8 ) {
				channel->vibrato_type = channel->note.param;
			}
			break;
		case 0x77: case 0xF4: /* Set Tremolo Waveform. */
			if( channel->note.param < 8 ) {
				channel->tremolo_type = channel->note.param;
			}
			break;
		case 0x7A: /* Fine Vol Slide Up. */
			if( channel->note.param > 0 ) {
				channel->fine_vslide_up_param = channel->note.param;
			}
			channel->volume += channel->fine_vslide_up_param;
			if( channel->volume > 64 ) {
				channel->volume = 64;
			}
			break;
		case 0x7B: /* Fine Vol Slide Down. */
			if( channel->note.param > 0 ) {
				channel->fine_vslide_down_param = channel->note.param;
			}
			channel->volume -= channel->fine_vslide_down_param;
			if( channel->volume < 0 ) {
				channel->volume = 0;
			}
			break;
		case 0x7C: case 0xFC: /* Note Cut. */
			if( channel->note.param <= 0 ) {
				channel->volume = 0;
			}
			break;
		case 0x8A: /* Arpeggio. */
			if( channel->note.param > 0 ) {
				channel->arpeggio_param = channel->note.param;
			}
			break;
		case 0x95: /* Fine Vibrato.*/
			if( ( channel->note.param >> 4 ) > 0 ) {
				channel->vibrato_speed = channel->note.param >> 4;
			}
			if( ( channel->note.param & 0xF ) > 0 ) {
				channel->vibrato_depth = channel->note.param & 0xF;
			}
			channel_vibrato( channel, 1 );
			break;
		case 0xFA: /* IT SAx: high sample offset, in 65536s. */
			if( channel->replay->module->it_effects ) {
				channel->high_offset = channel->note.param << 16;
			}
			break;
		case 0xF8: /* Set Panning. */
			channel->panning = channel->note.param * 17;
			break;
	}
	channel_auto_vibrato( channel );
	channel_calculate_freq( channel );
	channel_calculate_ampl( channel );
	channel_update_envelopes( channel );
}

static void channel_resample( struct channel *channel, int *mix_buf,
		int offset, int count, int sample_rate, int interpolate ) {
	struct sample *sample = channel->sample;
	int l_gain, r_gain, sam_idx, sam_fra, step;
	int loop_len, loop_end, out_idx, out_end, y, m, c, y2, m2, c2;
	short *sample_data = channel->sample->data;
	if( channel->ampl > 0 ) {
		l_gain = channel->ampl * ( 255 - channel->pann ) >> 8;
		r_gain = channel->ampl * channel->pann >> 8;
		sam_idx = channel->sample_idx;
		sam_fra = channel->sample_fra;
		step = ( channel->freq << ( FP_SHIFT - 3 ) ) / ( sample_rate >> 3 );
		loop_len = sample->loop_length;
		loop_end = sample->loop_start + loop_len;
		out_idx = offset * 2;
		out_end = ( offset + count ) * 2;
		if( sample->stereo ) {
			/* Interleaved frames: the left sample feeds the left
			   gain and the right the right, so panning acts as
			   balance. Kept out of the mono loops so those stay
			   instruction-identical. */
			while( out_idx < out_end ) {
				if( sam_idx >= loop_end ) {
					if( loop_len > 1 ) {
						while( sam_idx >= loop_end ) {
							sam_idx -= loop_len;
						}
					} else {
						break;
					}
				}
				if( interpolate ) {
					c = sample_data[ sam_idx * 2 ];
					m = sample_data[ sam_idx * 2 + 2 ] - c;
					y = ( ( m * sam_fra ) >> FP_SHIFT ) + c;
					c2 = sample_data[ sam_idx * 2 + 1 ];
					m2 = sample_data[ sam_idx * 2 + 3 ] - c2;
					y2 = ( ( m2 * sam_fra ) >> FP_SHIFT ) + c2;
				} else {
					y = sample_data[ sam_idx * 2 ];
					y2 = sample_data[ sam_idx * 2 + 1 ];
				}
				mix_buf[ out_idx++ ] += ( y * l_gain ) >> FP_SHIFT;
				mix_buf[ out_idx++ ] += ( y2 * r_gain ) >> FP_SHIFT;
				sam_fra += step;
				sam_idx += sam_fra >> FP_SHIFT;
				sam_fra &= FP_MASK;
			}
		} else if( interpolate ) {
			while( out_idx < out_end ) {
				if( sam_idx >= loop_end ) {
					if( loop_len > 1 ) {
						while( sam_idx >= loop_end ) {
							sam_idx -= loop_len;
						}
					} else {
						break;
					}
				}
				c = sample_data[ sam_idx ];
				m = sample_data[ sam_idx + 1 ] - c;
				y = ( ( m * sam_fra ) >> FP_SHIFT ) + c;
				mix_buf[ out_idx++ ] += ( y * l_gain ) >> FP_SHIFT;
				mix_buf[ out_idx++ ] += ( y * r_gain ) >> FP_SHIFT;
				sam_fra += step;
				sam_idx += sam_fra >> FP_SHIFT;
				sam_fra &= FP_MASK;
			}
		} else {
			while( out_idx < out_end ) {
				if( sam_idx >= loop_end ) {
					if( loop_len > 1 ) {
						while( sam_idx >= loop_end ) {
							sam_idx -= loop_len;
						}
					} else {
						break;
					}
				}
				y = sample_data[ sam_idx ];
				mix_buf[ out_idx++ ] += ( y * l_gain ) >> FP_SHIFT;
				mix_buf[ out_idx++ ] += ( y * r_gain ) >> FP_SHIFT;
				sam_fra += step;
				sam_idx += sam_fra >> FP_SHIFT;
				sam_fra &= FP_MASK;
			}
		}
	}
}

static void channel_update_sample_idx( struct channel *channel, int count, int sample_rate ) {
	struct sample *sample = channel->sample;
	int step = ( channel->freq << ( FP_SHIFT - 3 ) ) / ( sample_rate >> 3 );
	channel->sample_fra += step * count;
	channel->sample_idx += channel->sample_fra >> FP_SHIFT;
	if( channel->sample_idx > sample->loop_start ) {
		if( sample->loop_length > 1 ) {
			channel->sample_idx = sample->loop_start
				+ ( channel->sample_idx - sample->loop_start ) % sample->loop_length;
		} else {
			channel->sample_idx = sample->loop_start;
		}
	}
	channel->sample_fra &= FP_MASK;
}

static void replay_row( struct replay *replay ) {
	int idx, count;
	struct note note;
	struct pattern *pattern;
	struct channel *channel;
	struct module *module = replay->module;
	if( replay->next_row < 0 ) {
		replay->break_pos = replay->seq_pos + 1;
		replay->next_row = 0;
	}
	if( replay->break_pos >= 0 ) {
		if( replay->break_pos >= module->sequence_len ) {
			replay->break_pos = replay->next_row = 0;
		}
		while( module->sequence[ replay->break_pos ] >= module->num_patterns ) {
			replay->break_pos++;
			if( replay->break_pos >= module->sequence_len ) {
				replay->break_pos = replay->next_row = 0;
			}
		}
		replay->seq_pos = replay->break_pos;
		for( idx = 0; idx < module->num_channels; idx++ ) {
			replay->channels[ idx ].pl_row = 0;
		}
		replay->break_pos = -1;
	}
	pattern = &module->patterns[ module->sequence[ replay->seq_pos ] ];
	replay->row = replay->next_row;
	if( replay->row >= pattern->num_rows ) {
		replay->row = 0;
	}
	if( replay->play_count && replay->play_count[ 0 ] ) {
		count = replay->play_count[ replay->seq_pos ][ replay->row ];
		if( replay->pl_count < 0 && count < 127 ) {
			replay->play_count[ replay->seq_pos ][ replay->row ] = count + 1;
		}
	}
	replay->next_row = replay->row + 1;
	if( replay->next_row >= pattern->num_rows ) {
		replay->next_row = -1;
	}
	for( idx = 0; idx < module->num_channels; idx++ ) {
		channel = &replay->channels[ idx ];
		pattern_get_note( pattern, replay->row, idx, &note );
		if( note.effect == 0xE ) {
			note.effect = 0x70 | ( note.param >> 4 );
			note.param &= 0xF;
		}
		if( note.effect == 0x93 ) {
			note.effect = 0xF0 | ( note.param >> 4 );
			note.param &= 0xF;
		}
		if( note.effect == 0 && note.param > 0 ) {
			note.effect = 0x8A;
		}
		channel_row( channel, &note );
		switch( note.effect ) {
			case 0x81: /* Set Speed. */
				if( note.param > 0 ) {
					replay->tick = replay->speed = note.param;
				}
				break;
			case 0xB: case 0x82: /* Pattern Jump.*/
				if( replay->pl_count < 0 ) {
					replay->break_pos = note.param;
					replay->next_row = 0;
				}
				break;
			case 0xD: case 0x83: /* Pattern Break.*/
				if( replay->pl_count < 0 ) {
					if( replay->break_pos < 0 ) {
						replay->break_pos = replay->seq_pos + 1;
					}
					replay->next_row = ( note.param >> 4 ) * 10 + ( note.param & 0xF );
				}
				break;
			case 0xF: /* Set Speed/Tempo.*/
				if( note.param > 0 ) {
					if( note.param < 32 ) {
						replay->tick = replay->speed = note.param;
					} else {
						replay->tempo = note.param;
					}
				}
				break;
			case 0x94: /* Set Tempo / IT Tempo Slide. */
				if( note.param >= 0x20 ) {
					replay->tempo = note.param;
				} else if( note.param > 0 ) {
					channel->tempo_slide_param = note.param;
				}
				break;
			case 0x76: case 0xFB : /* Pattern Loop.*/
				if( note.param == 0 ) {
					/* Set loop marker on this channel. */
					channel->pl_row = replay->row;
				}
				if( channel->pl_row < replay->row && replay->break_pos < 0 ) {
					/* Marker valid. */
					if( replay->pl_count < 0 ) {
						/* Not already looping, begin. */
						replay->pl_count = note.param;
						replay->pl_chan = idx;
					}
					if( replay->pl_chan == idx ) {
						/* Next Loop.*/
						if( replay->pl_count == 0 ) {
							/* Loop finished. Invalidate current marker. */
							channel->pl_row = replay->row + 1;
						} else {
							/* Loop. */
							replay->next_row = channel->pl_row;
						}
						replay->pl_count--;
					}
				}
				break;
			case 0x7E: case 0xFE: /* Pattern Delay.*/
				replay->tick = replay->speed + replay->speed * note.param;
				break;
		}
	}
}

static void replay_update_ghosts( struct replay *replay ) {
	int idx;
	struct channel *ghost;
	if( !replay->ghosts ) {
		return;
	}
	for( idx = 0; idx < RMT_NUM_GHOSTS; idx++ ) {
		ghost = &replay->ghosts[ idx ];
		if( !ghost->sample ) {
			continue;
		}
		channel_calculate_ampl( ghost );
		channel_update_envelopes( ghost );
		/* A voice is done when it faded out, its one-shot sample
		   ended, or its release reached silence. A continuing voice
		   with a looped sample rings until stolen, as in IT. */
		if( ghost->fadeout_vol <= 0
		 || ( ghost->sample->loop_length <= 1
			&& ghost->sample_idx >= ghost->sample->loop_start )
		 || ( !ghost->key_on && ghost->ampl <= 0 ) ) {
			ghost->sample = NULL;
		}
	}
}

static int replay_tick( struct replay *replay ) {
	int idx, num_channels, count = 1;
	if( --replay->tick <= 0 ) {
		replay->tick = replay->speed;
		replay_row( replay );
	} else {
		num_channels = replay->module->num_channels;
		for( idx = 0; idx < num_channels; idx++ ) {
			channel_tick( &replay->channels[ idx ] );
		}
	}
	replay_update_ghosts( replay );
	if( replay->play_count && replay->play_count[ 0 ] ) {
		count = replay->play_count[ replay->seq_pos ][ replay->row ] - 1;
	}
	return count;
}

static int module_init_play_count( struct module *module, char **play_count ) {
	int idx, pat, rows, len = 0;
	for( idx = 0; idx < module->sequence_len; idx++ ) {
		pat = module->sequence[ idx ];
		rows = ( pat < module->num_patterns ) ? module->patterns[ pat ].num_rows : 0;
		if( play_count ) {
			play_count[ idx ] = play_count[ 0 ] ? &play_count[ 0 ][ len ] : NULL;
		}
		len += rows;
	}
	return len;
}

/* Set the pattern in the sequence to play. The tempo is reset to the default. */
static void replay_set_sequence_pos( struct replay *replay, int pos ) {
	int idx;
	struct module *module = replay->module;
	if( pos >= module->sequence_len ) {
		pos = 0;
	}
	replay->break_pos = pos;
	replay->next_row = 0;
	replay->tick = 1;
	replay->global_vol = module->default_gvol;
	replay->speed = module->default_speed > 0 ? module->default_speed : 6;
	replay->tempo = module->default_tempo > 0 ? module->default_tempo : 125;
	replay->pl_count = replay->pl_chan = -1;
	if( replay->play_count ) {
		free( replay->play_count[ 0 ] );
		free( replay->play_count );
	}
	replay->play_count = (char**)calloc( module->sequence_len, sizeof( char * ) );
	if( replay->play_count ) {
		replay->play_count[ 0 ] = calloc( module_init_play_count( module, NULL ), sizeof( char ) );
		module_init_play_count( module, replay->play_count );
	}
	for( idx = 0; idx < module->num_channels; idx++ ) {
		channel_init( &replay->channels[ idx ], replay, idx );
	}
	if( replay->ghosts ) {
		memset( replay->ghosts, 0, RMT_NUM_GHOSTS * sizeof( struct channel ) );
	}
	memset( replay->ramp_buf, 0, 128 * sizeof( int ) );
	replay_tick( replay );
}

/* Deallocate the specified replay. */
static void dispose_replay( struct replay *replay ) {
	if( replay->play_count ) {
		free( replay->play_count[ 0 ] );
		free( replay->play_count );
	}
	free( replay->arena );
	free( replay );
}

/* Region spacing inside the replay arena, in bytes: every buffer starts
   on a 64-byte boundary. */
#define REPLAY_ARENA_NEXT( cur, bytes ) \
	( ( ( ( cur ) + ( bytes ) + 63 ) / 64 ) * 64 )

/* Allocate and initialize a replay with the specified sampling rate and interpolation. */
static struct replay* new_replay( struct module *module, int sample_rate, int interpolation ) {
	size_t o_chan, o_ghost, o_flt, o_flt_f, len;
	struct replay *replay = calloc( 1, sizeof( struct replay ) );
	if( replay ) {
		replay->module = module;
		replay->sample_rate = sample_rate;
		replay->interpolation = interpolation;
		/* The mixer walks the channel and ghost arrays and the ramp
		   buffer every tick, so they come out of one zeroed block, each
		   region on a 64-byte boundary: the volume ramp, the channels,
		   the background-voice pool for IT new-note actions, and (for
		   IT modules) the per-voice filter scratch sized for the slowest
		   tempo the engine clamps to. */
		o_chan = REPLAY_ARENA_NEXT( 0, 128 * sizeof( int ) );
		o_ghost = REPLAY_ARENA_NEXT( o_chan,
			( size_t ) module->num_channels * sizeof( struct channel ) );
		o_flt = REPLAY_ARENA_NEXT( o_ghost,
			RMT_NUM_GHOSTS * sizeof( struct channel ) );
		o_flt_f = len = o_flt;
		if( module->it_effects ) {
			size_t flt_frames = ( calculate_tick_len( 32, sample_rate ) + 65 ) * 4;
			o_flt_f = REPLAY_ARENA_NEXT( o_flt, flt_frames * sizeof( int ) );
			len = REPLAY_ARENA_NEXT( o_flt_f, flt_frames * sizeof( float ) );
		}
		replay->arena = calloc( 1, len );
		if( replay->arena ) {
			replay->ramp_buf = ( int * ) replay->arena;
			replay->channels = ( struct channel * ) ( replay->arena + o_chan );
			replay->ghosts = ( struct channel * ) ( replay->arena + o_ghost );
			if( module->it_effects ) {
				replay->flt_buf = ( int * ) ( replay->arena + o_flt );
				replay->flt_buf_f = ( float * ) ( replay->arena + o_flt_f );
			}
		}
		if( replay->arena ) {
			replay_set_sequence_pos( replay, 0 );
		} else {
			dispose_replay( replay );
			replay = NULL;
		}
	}
	return replay;
}

static int calculate_tick_len( int tempo, int sample_rate ) {
	return ( sample_rate * 5 ) / ( tempo * 2 );
}

/* Returns the length of the output buffer required by replay_get_audio(). */
static int calculate_mix_buf_len( int sample_rate ) {
	return ( calculate_tick_len( 32, sample_rate ) + 65 ) * 4;
}

/* Returns the song duration in samples at the current sampling rate. */
static int replay_calculate_duration( struct replay *replay ) {
	int count = 0, duration = 0;
	replay_set_sequence_pos( replay, 0 );
	while( count < 1 ) {
		int tick = calculate_tick_len( replay->tempo, replay->sample_rate );
		/* An S3M order count is a u16, and order bytes that fall past the
		 * end of the file read back as 0 - a valid pattern index, so the
		 * trim in the loader does not shorten the sequence. A long enough
		 * one overflows this accumulator, which is undefined, and leaves
		 * rmodtracker_seek comparing frames against a negative duration.
		 * Saturate and stop: at INT_MAX frames the answer is already
		 * meaningless, and the walk is what costs. */
		if( duration > INT_MAX - tick ) {
			duration = INT_MAX;
			break;
		}
		duration += tick;
		count = replay_tick( replay );
	}
	replay_set_sequence_pos( replay, 0 );
	return duration;
}


static void replay_volume_ramp( struct replay *replay, int *mix_buf, int tick_len ) {
	int idx, a1, a2, ramp_rate = 256 * 2048 / replay->sample_rate;
	for( idx = 0, a1 = 0; a1 < 256; idx += 2, a1 += ramp_rate ) {
		a2 = 256 - a1;
		mix_buf[ idx     ] = ( mix_buf[ idx     ] * a1 + replay->ramp_buf[ idx     ] * a2 ) >> 8;
		mix_buf[ idx + 1 ] = ( mix_buf[ idx + 1 ] * a1 + replay->ramp_buf[ idx + 1 ] * a2 ) >> 8;
	}
	memcpy( replay->ramp_buf, &mix_buf[ tick_len * 2 ], 128 * sizeof( int ) );
}

/* 2:1 downsampling with simple but effective anti-aliasing. Buf must contain count * 2 + 1 stereo samples. */
static void downsample( int *buf, int count ) {
	int idx, out_idx, out_len = count * 2;
	for( idx = 0, out_idx = 0; out_idx < out_len; idx += 4, out_idx += 2 ) {
		buf[ out_idx     ] = ( buf[ idx     ] >> 2 ) + ( buf[ idx + 2 ] >> 1 ) + ( buf[ idx + 4 ] >> 2 );
		buf[ out_idx + 1 ] = ( buf[ idx + 1 ] >> 2 ) + ( buf[ idx + 3 ] >> 1 ) + ( buf[ idx + 5 ] >> 2 );
	}
}

/* Generates audio and returns the number of stereo samples written into mix_buf.
   Individual channels may be excluded using the mute bitmask. */
/* Run the Q14 lowpass over an interleaved stereo span. The span
   covers the tick plus the downsampler's 65-frame overlap tail that
   the next tick re-renders, so the state committed back to the
   channel is the state at the tick boundary; the tail is filtered
   with throwaway state. Products stay within 32 bits: inputs are
   clamped to 16 bits, a is at most 16384 and a stable filter keeps
   |b| under two in Q14, at the cost of seven low bits per term. */
static void channel_filter_run( struct channel *channel, int *buf,
		int commit, int total ) {
	int i, x, y, acc;
	int y1l = channel->flt_y1l, y2l = channel->flt_y2l;
	int y1r = channel->flt_y1r, y2r = channel->flt_y2r;
	int errl = channel->flt_errl, errr = channel->flt_errr;
	for( i = 0; i < total; i++ ) {
		/* One 14-bit shift per sample with first-order error
		   feedback: the truncated remainder is carried into the
		   next sample, so the shift is exactly bias-free. A plain
		   floor ( or round-half-up ) leaks a half-LSB of DC into
		   the feedback loop, and a lowpass with near-unity feedback
		   amplifies it by its DC gain - over a thousandfold at low
		   cutoffs - into a large standing offset per voice. The
		   voice scales into a 14-bit-ish domain ( oversampled
		   voices reach about 1.3x of 16 bits with a hot mix
		   volume ) so every product and the accumulator stay within
		   32 bits with the state clamped at 15 bits: worst case
		   0.18e9 + 1.08e9 + 0.54e9 + err < 2^31. */
		x = buf[ i * 2 ] >> 2;
		acc = x * channel->flt_a + y1l * channel->flt_b
			+ y2l * channel->flt_c + errl;
		y = acc >> 14;
		errl = acc - ( y << 14 );
		if( y > 32767 ) y = 32767;
		if( y < -32768 ) y = -32768;
		y2l = y1l;
		y1l = y;
		buf[ i * 2 ] = y << 2;
		x = buf[ i * 2 + 1 ] >> 2;
		acc = x * channel->flt_a + y1r * channel->flt_b
			+ y2r * channel->flt_c + errr;
		y = acc >> 14;
		errr = acc - ( y << 14 );
		if( y > 32767 ) y = 32767;
		if( y < -32768 ) y = -32768;
		y2r = y1r;
		y1r = y;
		buf[ i * 2 + 1 ] = y << 2;
		if( i == commit - 1 ) {
			channel->flt_y1l = y1l;
			channel->flt_y2l = y2l;
			channel->flt_y1r = y1r;
			channel->flt_y2r = y2r;
			channel->flt_errl = errl;
			channel->flt_errr = errr;
		}
	}
}

static void channel_filter_run_f( struct channel *channel, float *buf,
		int commit, int total ) {
	int i;
	float x, y;
	float fa = ( float ) channel->flt_a * ( 1.0f / 16384.0f );
	float fb = ( float ) channel->flt_b * ( 1.0f / 16384.0f );
	float fc = ( float ) channel->flt_c * ( 1.0f / 16384.0f );
	float y1l = channel->flt_y1l * ( 1.0f / 32768.0f );
	float y2l = channel->flt_y2l * ( 1.0f / 32768.0f );
	float y1r = channel->flt_y1r * ( 1.0f / 32768.0f );
	float y2r = channel->flt_y2r * ( 1.0f / 32768.0f );
	for( i = 0; i < total; i++ ) {
		x = buf[ i * 2 ];
		y = x * fa + y1l * fb + y2l * fc;
		y2l = y1l;
		y1l = y;
		buf[ i * 2 ] = y;
		x = buf[ i * 2 + 1 ];
		y = x * fa + y1r * fb + y2r * fc;
		y2r = y1r;
		y1r = y;
		buf[ i * 2 + 1 ] = y;
		if( i == commit - 1 ) {
			channel->flt_y1l = ( int ) ( y1l * 32768.0f );
			channel->flt_y2l = ( int ) ( y2l * 32768.0f );
			channel->flt_y1r = ( int ) ( y1r * 32768.0f );
			channel->flt_y2r = ( int ) ( y2r * 32768.0f );
		}
	}
}

static int replay_get_audio( struct replay *replay, int *mix_buf, int mute ) {
	struct channel *channel;
	int idx, num_channels, tick_len = calculate_tick_len( replay->tempo, replay->sample_rate );
	/* Clear output buffer. */
	memset( mix_buf, 0, ( tick_len + 65 ) * 4 * sizeof( int ) );
	/* Resample. */
	num_channels = replay->module->num_channels;
	for( idx = 0; idx < num_channels; idx++ ) {
		channel = &replay->channels[ idx ];
		if( !( mute & 1 ) ) {
			if( channel->flt_on && channel->ampl > 0 && replay->flt_buf ) {
				int i, span = ( tick_len + 65 ) * 2;
				memset( replay->flt_buf, 0, span * 2 * sizeof( int ) );
				channel_resample( channel, replay->flt_buf, 0, span,
					replay->sample_rate * 2, replay->interpolation );
				channel_filter_run( channel, replay->flt_buf,
					tick_len * 2, span );
				for( i = 0; i < span * 2; i++ ) {
					mix_buf[ i ] += replay->flt_buf[ i ];
				}
			} else {
				channel_resample( channel, mix_buf, 0, ( tick_len + 65 ) * 2,
					replay->sample_rate * 2, replay->interpolation );
			}
		}
		channel_update_sample_idx( channel, tick_len * 2, replay->sample_rate * 2 );
		mute >>= 1;
	}
	if( replay->ghosts ) {
		for( idx = 0; idx < RMT_NUM_GHOSTS; idx++ ) {
			channel = &replay->ghosts[ idx ];
			if( channel->sample ) {
				if( channel->flt_on && channel->ampl > 0 && replay->flt_buf ) {
					int i, span = ( tick_len + 65 ) * 2;
					memset( replay->flt_buf, 0, span * 2 * sizeof( int ) );
					channel_resample( channel, replay->flt_buf, 0, span,
						replay->sample_rate * 2, replay->interpolation );
					channel_filter_run( channel, replay->flt_buf,
						tick_len * 2, span );
					for( i = 0; i < span * 2; i++ ) {
						mix_buf[ i ] += replay->flt_buf[ i ];
					}
				} else {
					channel_resample( channel, mix_buf, 0, ( tick_len + 65 ) * 2,
						replay->sample_rate * 2, replay->interpolation );
				}
				channel_update_sample_idx( channel, tick_len * 2,
					replay->sample_rate * 2 );
			}
		}
	}
	downsample( mix_buf, tick_len + 64 );
	replay_volume_ramp( replay, mix_buf, tick_len );
	replay_tick( replay );
	return tick_len;
}



/* ---------------------------------------------------------------------
 * Float mixing pipeline.
 *
 * The sequencer, effects, envelopes and the resampler POSITION state
 * (sample_idx/sample_fra) are integer and shared with the s16 pipeline,
 * so the musical content -- note timing, pitch, effect behaviour -- is
 * bit-identical in both modes and deterministic everywhere.  Only the
 * sample-domain arithmetic differs: this pipeline fetches the (integer)
 * sample data once into float and performs interpolation, gain, the 2:1
 * anti-alias decimation and the volume ramp in float, accumulating at
 * the same nominal scale as the integer mixer (s16 units); the getter
 * normalises by 1/32768 on output.
 * ------------------------------------------------------------------ */
static void channel_resample_f( struct channel *channel, float *mix_buf,
		int offset, int count, int sample_rate, int interpolate ) {
	struct sample *sample = channel->sample;
	int l_gain, r_gain, sam_idx, sam_fra, step;
	int loop_len, loop_end, out_idx, out_end;
	float gl, gr, c, m, y, c2, m2, y2;
	short *sample_data = channel->sample->data;
	if( channel->ampl > 0 ) {
		l_gain = channel->ampl * ( 255 - channel->pann ) >> 8;
		r_gain = channel->ampl * channel->pann >> 8;
		gl = ( float ) l_gain * ( 1.0f / FP_ONE );
		gr = ( float ) r_gain * ( 1.0f / FP_ONE );
		sam_idx = channel->sample_idx;
		sam_fra = channel->sample_fra;
		step = ( channel->freq << ( FP_SHIFT - 3 ) ) / ( sample_rate >> 3 );
		loop_len = sample->loop_length;
		loop_end = sample->loop_start + loop_len;
		out_idx = offset * 2;
		out_end = ( offset + count ) * 2;
		if( sample->stereo ) {
			/* Same shape as the integer stereo path; see there. */
			while( out_idx < out_end ) {
				if( sam_idx >= loop_end ) {
					if( loop_len > 1 ) {
						while( sam_idx >= loop_end ) {
							sam_idx -= loop_len;
						}
					} else {
						break;
					}
				}
				if( interpolate ) {
					c = ( float ) sample_data[ sam_idx * 2 ];
					m = ( float ) sample_data[ sam_idx * 2 + 2 ] - c;
					y = m * ( ( float ) sam_fra * ( 1.0f / FP_ONE ) ) + c;
					c2 = ( float ) sample_data[ sam_idx * 2 + 1 ];
					m2 = ( float ) sample_data[ sam_idx * 2 + 3 ] - c2;
					y2 = m2 * ( ( float ) sam_fra * ( 1.0f / FP_ONE ) ) + c2;
				} else {
					y = ( float ) sample_data[ sam_idx * 2 ];
					y2 = ( float ) sample_data[ sam_idx * 2 + 1 ];
				}
				mix_buf[ out_idx++ ] += y * gl;
				mix_buf[ out_idx++ ] += y2 * gr;
				sam_fra += step;
				sam_idx += sam_fra >> FP_SHIFT;
				sam_fra &= FP_MASK;
			}
		} else if( interpolate ) {
			while( out_idx < out_end ) {
				if( sam_idx >= loop_end ) {
					if( loop_len > 1 ) {
						while( sam_idx >= loop_end ) {
							sam_idx -= loop_len;
						}
					} else {
						break;
					}
				}
				c = ( float ) sample_data[ sam_idx ];
				m = ( float ) sample_data[ sam_idx + 1 ] - c;
				y = m * ( ( float ) sam_fra * ( 1.0f / FP_ONE ) ) + c;
				mix_buf[ out_idx++ ] += y * gl;
				mix_buf[ out_idx++ ] += y * gr;
				sam_fra += step;
				sam_idx += sam_fra >> FP_SHIFT;
				sam_fra &= FP_MASK;
			}
		} else {
			while( out_idx < out_end ) {
				if( sam_idx >= loop_end ) {
					if( loop_len > 1 ) {
						while( sam_idx >= loop_end ) {
							sam_idx -= loop_len;
						}
					} else {
						break;
					}
				}
				y = ( float ) sample_data[ sam_idx ];
				mix_buf[ out_idx++ ] += y * gl;
				mix_buf[ out_idx++ ] += y * gr;
				sam_fra += step;
				sam_idx += sam_fra >> FP_SHIFT;
				sam_fra &= FP_MASK;
			}
		}
	}
}

static void downsample_f( float *buf, int count ) {
	int idx, out_idx, out_len = count * 2;
	for( idx = 0, out_idx = 0; out_idx < out_len; idx += 4, out_idx += 2 ) {
		buf[ out_idx     ] = buf[ idx     ] * 0.25f + buf[ idx + 2 ] * 0.5f + buf[ idx + 4 ] * 0.25f;
		buf[ out_idx + 1 ] = buf[ idx + 1 ] * 0.25f + buf[ idx + 3 ] * 0.5f + buf[ idx + 5 ] * 0.25f;
	}
}

static void replay_volume_ramp_f( struct replay *replay, float *mix_buf,
		float *ramp_buf_f, int tick_len ) {
	int idx, a1, ramp_rate = 256 * 2048 / replay->sample_rate;
	float f1, f2;
	for( idx = 0, a1 = 0; a1 < 256; idx += 2, a1 += ramp_rate ) {
		f1 = ( float ) a1 * ( 1.0f / 256.0f );
		f2 = 1.0f - f1;
		mix_buf[ idx     ] = mix_buf[ idx     ] * f1 + ramp_buf_f[ idx     ] * f2;
		mix_buf[ idx + 1 ] = mix_buf[ idx + 1 ] * f1 + ramp_buf_f[ idx + 1 ] * f2;
	}
	memcpy( ramp_buf_f, &mix_buf[ tick_len * 2 ], 128 * sizeof( float ) );
}

static int replay_get_audio_f( struct replay *replay, float *mix_buf,
		float *ramp_buf_f, int mute ) {
	struct channel *channel;
	int idx, num_channels, tick_len = calculate_tick_len( replay->tempo, replay->sample_rate );
	memset( mix_buf, 0, ( tick_len + 65 ) * 4 * sizeof( float ) );
	num_channels = replay->module->num_channels;
	for( idx = 0; idx < num_channels; idx++ ) {
		channel = &replay->channels[ idx ];
		if( !( mute & 1 ) ) {
			if( channel->flt_on && channel->ampl > 0 && replay->flt_buf_f ) {
				int i, span = ( tick_len + 65 ) * 2;
				memset( replay->flt_buf_f, 0, span * 2 * sizeof( float ) );
				channel_resample_f( channel, replay->flt_buf_f, 0, span,
					replay->sample_rate * 2, replay->interpolation );
				channel_filter_run_f( channel, replay->flt_buf_f,
					tick_len * 2, span );
				for( i = 0; i < span * 2; i++ ) {
					mix_buf[ i ] += replay->flt_buf_f[ i ];
				}
			} else {
				channel_resample_f( channel, mix_buf, 0, ( tick_len + 65 ) * 2,
					replay->sample_rate * 2, replay->interpolation );
			}
		}
		channel_update_sample_idx( channel, tick_len * 2, replay->sample_rate * 2 );
		mute >>= 1;
	}
	if( replay->ghosts ) {
		for( idx = 0; idx < RMT_NUM_GHOSTS; idx++ ) {
			channel = &replay->ghosts[ idx ];
			if( channel->sample ) {
				if( channel->flt_on && channel->ampl > 0 && replay->flt_buf_f ) {
					int i, span = ( tick_len + 65 ) * 2;
					memset( replay->flt_buf_f, 0, span * 2 * sizeof( float ) );
					channel_resample_f( channel, replay->flt_buf_f, 0, span,
						replay->sample_rate * 2, replay->interpolation );
					channel_filter_run_f( channel, replay->flt_buf_f,
						tick_len * 2, span );
					for( i = 0; i < span * 2; i++ ) {
						mix_buf[ i ] += replay->flt_buf_f[ i ];
					}
				} else {
					channel_resample_f( channel, mix_buf, 0, ( tick_len + 65 ) * 2,
						replay->sample_rate * 2, replay->interpolation );
					channel_update_sample_idx( channel, tick_len * 2,
						replay->sample_rate * 2 );
					continue;
				}
				channel_update_sample_idx( channel, tick_len * 2,
					replay->sample_rate * 2 );
			}
		}
	}
	downsample_f( mix_buf, tick_len + 64 );
	replay_volume_ramp_f( replay, mix_buf, ramp_buf_f, tick_len );
	replay_tick( replay );
	return tick_len;
}


/* Saturating int32 -> int16 copy.  SSE2's packssdw and NEON's
 * vqmovn_s32 perform exactly this saturation in hardware, so the
 * SIMD paths are byte-identical to the scalar loop by construction. */
#if defined(__SSE2__) || ( defined(_M_IX86_FP) && _M_IX86_FP >= 2 ) || defined(_M_X64)
#include <emmintrin.h>
#define RMT_CLAMP_SSE2 1
#else
#define RMT_CLAMP_SSE2 0
#endif
#if defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(__aarch64__)
#include <arm_neon.h>
#define RMT_CLAMP_NEON 1
#else
#define RMT_CLAMP_NEON 0
#endif
static void rmt_clamp_s16( int16_t * RMT_RESTRICT dst,
		const int * RMT_RESTRICT src, int n )
{
	int i = 0;
#if RMT_CLAMP_SSE2
	for( ; i + 8 <= n; i += 8 ) {
		__m128i a = _mm_loadu_si128( ( const __m128i * ) ( src + i ) );
		__m128i b = _mm_loadu_si128( ( const __m128i * ) ( src + i + 4 ) );
		_mm_storeu_si128( ( __m128i * ) ( dst + i ), _mm_packs_epi32( a, b ) );
	}
#elif RMT_CLAMP_NEON
	for( ; i + 8 <= n; i += 8 ) {
		int32x4_t a = vld1q_s32( src + i );
		int32x4_t b = vld1q_s32( src + i + 4 );
		vst1q_s16( dst + i, vcombine_s16( vqmovn_s32( a ), vqmovn_s32( b ) ) );
	}
#endif
	for( ; i < n; i++ ) {
		int v = src[ i ];
		if( v >  32767 ) v =  32767;
		if( v < -32768 ) v = -32768;
		dst[ i ] = ( int16_t ) v;
	}
}

/* ---------------------------------------------------------------------
 * Public rmodtracker API.
 * ------------------------------------------------------------------ */
struct rmodtracker {
	struct module *module;
	struct replay *replay;
	/* One tick of mixing, per domain. Allocated on first use rather
	 * than at open: a caller reaches for one getter and keeps using it,
	 * so the other domain's buffer was being paid for and never
	 * touched - about 60 KB at 48 kHz, for the life of every voice. */
	int   *mix_i;         /* one tick, integer pipeline                 */
	float *mix_f;         /* one tick, float pipeline                   */
	float *ramp_f;        /* float twin of replay->ramp_buf             */
	int    buf_len;       /* entries in mix_i / mix_f when allocated     */
	int    carry_len;     /* frames left over from the last tick        */
	int    carry_pos;
	int    carry_domain;  /* 0 = none, 1 = int, 2 = float               */
	int    ended;
	int    duration;      /* one pass, measured once at open            */
	int    rate;          /* mix rate chosen at open                    */
	/* Seek snapshots: full sequencer and voice state captured at tick
	 * boundaries during the first seek's walk, so later seeks restore
	 * the nearest one and walk at most one interval instead of the
	 * whole distance from the top. Built lazily - a caller that never
	 * seeks never pays for them - and bounded: at most RMT_SNAP_MAX
	 * records, at least two seconds apart. */
	char  *snaps;         /* slab of snapshot records                   */
	int    snap_stride;   /* bytes per record, 16-aligned               */
	int    snap_count;    /* captured so far                            */
	int    snap_interval; /* frames between capture thresholds          */
	int    snap_next;     /* next capture threshold, past all captured  */
	int    pc_bytes;      /* play-count blob size, fixed per module     */
};

#define RMT_SNAP_MAX 33

/* A record is this header, then the channels array, then the
 * play-count blob. The struct replay copy is bitwise: its pointer
 * fields are ignored on restore, since rewinding reallocates the
 * play-count table and the live pointers are the valid ones. */
struct rmt_snap {
	int frame;
	struct replay r;
};

static struct rmt_snap *rmt_snap_at( struct rmodtracker *rmt, int idx )
{
	return ( struct rmt_snap * ) ( rmt->snaps + ( size_t ) idx * rmt->snap_stride );
}

static int rmt_snaps_init( struct rmodtracker *rmt )
{
	int ch_bytes, stride, interval;
	if( rmt->snaps )
		return 1;
	rmt->pc_bytes = module_init_play_count( rmt->module, NULL );
	ch_bytes = rmt->module->num_channels * ( int ) sizeof( struct channel );
	stride = ( int ) sizeof( struct rmt_snap ) + ch_bytes + rmt->pc_bytes
		+ RMT_NUM_GHOSTS * ( int ) sizeof( struct channel );
	stride = ( stride + 15 ) & ~15;
	interval = rmt->duration / ( RMT_SNAP_MAX - 1 );
	if( interval < rmt->rate * 2 )
		interval = rmt->rate * 2;
	rmt->snap_stride = stride;
	rmt->snap_interval = interval;
	rmt->snap_next = interval;
	rmt->snap_count = 0;
	rmt->snaps = ( char * ) calloc( RMT_SNAP_MAX, ( size_t ) stride );
	/* On failure every seek simply walks from the top, as before. */
	return rmt->snaps != NULL;
}

static void rmt_snap_capture( struct rmodtracker *rmt, int frame )
{
	struct replay *replay = rmt->replay;
	struct rmt_snap *s = rmt_snap_at( rmt, rmt->snap_count );
	char *body = ( char * ) s + sizeof( struct rmt_snap );
	int ch_bytes = rmt->module->num_channels * ( int ) sizeof( struct channel );
	s->frame = frame;
	s->r = *replay;
	memcpy( body, replay->channels, ( size_t ) ch_bytes );
	if( rmt->pc_bytes > 0 && replay->play_count && replay->play_count[ 0 ] )
		memcpy( body + ch_bytes, replay->play_count[ 0 ], ( size_t ) rmt->pc_bytes );
	if( replay->ghosts )
		memcpy( body + ch_bytes + rmt->pc_bytes, replay->ghosts,
			RMT_NUM_GHOSTS * sizeof( struct channel ) );
	rmt->snap_count++;
	if( rmt->snap_next <= INT_MAX - rmt->snap_interval )
		rmt->snap_next += rmt->snap_interval;
	else
		rmt->snap_count = RMT_SNAP_MAX; /* threshold would overflow; stop */
}

static void rmt_snap_restore( struct rmodtracker *rmt, struct rmt_snap *s )
{
	struct replay *replay = rmt->replay;
	char *body = ( char * ) s + sizeof( struct rmt_snap );
	int ch_bytes = rmt->module->num_channels * ( int ) sizeof( struct channel );
	struct replay tmp = s->r;
	tmp.ramp_buf   = replay->ramp_buf;
	tmp.flt_buf    = replay->flt_buf;
	tmp.flt_buf_f  = replay->flt_buf_f;
	tmp.play_count = replay->play_count;
	tmp.channels   = replay->channels;
	tmp.ghosts     = replay->ghosts;
	tmp.module     = replay->module;
	*replay = tmp;
	memcpy( replay->channels, body, ( size_t ) ch_bytes );
	if( rmt->pc_bytes > 0 && replay->play_count && replay->play_count[ 0 ] )
		memcpy( replay->play_count[ 0 ], body + ch_bytes, ( size_t ) rmt->pc_bytes );
	if( replay->ghosts )
		memcpy( replay->ghosts, body + ch_bytes + rmt->pc_bytes,
			RMT_NUM_GHOSTS * sizeof( struct channel ) );
}

/* Advance the sequencer without mixing, from a tick boundary to the
 * last tick boundary at or before the target. A tracker has no seek
 * table: where it is at a given moment is the result of every row that
 * came before, so the only way to arrive somewhere is to go through
 * the song. What can be skipped is the mixing, which is nearly all of
 * the cost; each channel's sample position is carried forward by the
 * same helper the mixer uses for a channel it is not rendering.
 *
 * A snapshot is captured at the first boundary at or past each
 * interval threshold. Thresholds only sit beyond everything already
 * captured, and the walk from a restored snapshot reproduces the
 * from-the-top walk exactly, so the set of snapshot positions is the
 * same whichever seeks happen to build it. */
static int rmt_seek_walk( struct rmodtracker *rmt, int start_pos, int target )
{
	struct replay *replay = rmt->replay;
	int idx, current_pos = start_pos;
	int tick_len = calculate_tick_len( replay->tempo, replay->sample_rate );
	while( ( target - current_pos ) >= tick_len ) {
		for( idx = 0; idx < replay->module->num_channels; idx++ ) {
			channel_update_sample_idx( &replay->channels[ idx ],
				tick_len * 2, replay->sample_rate * 2 );
		}
		if( replay->ghosts ) {
			for( idx = 0; idx < RMT_NUM_GHOSTS; idx++ ) {
				if( replay->ghosts[ idx ].sample ) {
					channel_update_sample_idx( &replay->ghosts[ idx ],
						tick_len * 2, replay->sample_rate * 2 );
				}
			}
		}
		current_pos += tick_len;
		replay_tick( replay );
		tick_len = calculate_tick_len( replay->tempo, replay->sample_rate );
		if( rmt->snaps && rmt->snap_count < RMT_SNAP_MAX
				&& current_pos >= rmt->snap_next )
			rmt_snap_capture( rmt, current_pos );
	}
	return current_pos;
}

rmodtracker *rmodtracker_open_memory( const void *data, size_t size )
{
	return rmodtracker_open_memory_rate( data, size, RMODTRACKER_RATE );
}

rmodtracker *rmodtracker_open_memory_rate( const void *data, size_t size,
		int sample_rate )
{
	struct rmodtracker *rmt;
	struct data d;
	char msg[ 64 ];
	int buf_len;
	if( !data || size < 4 )
		return NULL;
	if( sample_rate < RMODTRACKER_RATE_MIN
		|| sample_rate > RMODTRACKER_RATE_MAX )
		return NULL;
	rmt = ( struct rmodtracker * ) calloc( 1, sizeof( struct rmodtracker ) );
	if( !rmt )
		return NULL;
	d.buffer = ( char * ) data;
	d.length = ( int ) size;
	msg[ 0 ] = 0;
	rmt->module = module_load( &d, msg );
	if( !rmt->module ) {
		free( rmt );
		return NULL;
	}
	rmt->rate = sample_rate;
	rmt->replay = new_replay( rmt->module, sample_rate, 1 );
	if( !rmt->replay ) {
		dispose_module( rmt->module );
		free( rmt );
		return NULL;
	}
	buf_len = calculate_mix_buf_len( sample_rate );
	if( buf_len <= 0 ) {
		rmodtracker_close( rmt );
		return NULL;
	}
	rmt->buf_len = buf_len;
	/* Measured here rather than on demand: the walk that measures it
	 * rewinds the sequencer, which would be a surprising thing for a
	 * query to do to a module that is already playing. */
	rmt->duration = replay_calculate_duration( rmt->replay );
	return rmt;
}

void rmodtracker_close( rmodtracker *rmt )
{
	if( !rmt )
		return;
	if( rmt->replay )
		dispose_replay( rmt->replay );
	if( rmt->module )
		dispose_module( rmt->module );
	free( rmt->mix_i );
	free( rmt->mix_f );
	free( rmt->ramp_f );
	free( rmt->snaps );
	free( rmt );
}

int rmodtracker_sample_rate( rmodtracker *rmt )
{
	return rmt ? rmt->rate : 0;
}

/* Voices the module itself has - four in a classic MOD, up to
 * thirty-two in an XM or S3M.
 *
 * Not the channel count a caller mixes at.  The replayer sums these
 * into interleaved stereo, so two is what comes out however many the
 * module writes for, and that is what the audio_transfer arm reports;
 * this is the module's own figure, for a caller that wants to say
 * something about the file rather than about its output. */
int rmodtracker_voices( rmodtracker *rmt )
{
	return ( rmt && rmt->module ) ? rmt->module->num_channels : 0;
}

/* Duration of one pass through the sequence, in frames at the mix rate. */
int rmodtracker_duration_frames( rmodtracker *rmt )
{
	return rmt ? rmt->duration : 0;
}

void rmodtracker_rewind( rmodtracker *rmt )
{
	replay_set_sequence_pos( rmt->replay, 0 );
	if( rmt->ramp_f )
		memset( rmt->ramp_f, 0, 128 * sizeof( float ) );
	rmt->carry_len = rmt->carry_pos = rmt->carry_domain = 0;
	rmt->ended = 0;
}

int rmodtracker_seek( rmodtracker *rmt, int frame )
{
	int16_t scratch[ 256 * 2 ];
	int reached, base_pos, idx;
	if( !rmt || frame < 0 )
		return 0;
	/* A module loops, so any frame at all is a position somewhere in the
	 * stream and the walk below would dutifully grind its way there.
	 * Stop at one pass: past that the caller has almost certainly asked
	 * for something it did not mean, and the alternative is a walk whose
	 * length it chose by accident, on this thread. */
	if( frame > rmt->duration )
		frame = rmt->duration;
	rmodtracker_rewind( rmt );
	if( frame == 0 )
		return 0;
	base_pos = 0;
	if( rmt_snaps_init( rmt ) ) {
		/* Newest snapshot at or before the target; the walk covers
		 * the rest, extending the snapshot set if the target lies
		 * beyond everything captured so far. */
		for( idx = rmt->snap_count - 1; idx >= 0; idx-- ) {
			struct rmt_snap *s = rmt_snap_at( rmt, idx );
			if( s->frame <= frame ) {
				rmt_snap_restore( rmt, s );
				base_pos = s->frame;
				break;
			}
		}
	}
	reached = rmt_seek_walk( rmt, base_pos, frame );
	/* The walk lands on a tick boundary, so render the rest of the way
	 * and throw it away - under one tick, some twenty milliseconds, and
	 * it puts the caller exactly where it asked to be rather than
	 * somewhere nearby. */
	while( reached < frame ) {
		int want = frame - reached;
		int got;
		if( want > 256 )
			want = 256;
		got = ( int ) rmodtracker_get_samples_s16_interleaved( rmt,
				scratch, ( size_t ) want );
		if( got <= 0 )
			break;          /* ran off the end of the song */
		reached += got;
	}
	return reached;
}

/* Make the buffers a domain needs, the first time that domain is asked
 * for. Returns 0 if the allocation fails, in which case the caller ends
 * the stream rather than spinning on a getter that can never produce
 * anything. */
static int rmt_need_i( struct rmodtracker *rmt )
{
	if( !rmt->mix_i )
		rmt->mix_i = ( int * ) calloc( ( size_t ) rmt->buf_len,
				sizeof( int ) );
	return rmt->mix_i != NULL;
}

static int rmt_need_f( struct rmodtracker *rmt )
{
	if( !rmt->mix_f )
		rmt->mix_f = ( float * ) calloc( ( size_t ) rmt->buf_len,
				sizeof( float ) );
	if( !rmt->ramp_f )
		rmt->ramp_f = ( float * ) calloc( 128, sizeof( float ) );
	return rmt->mix_f != NULL && rmt->ramp_f != NULL;
}

/* Switching getters mid-carry converts the few carried frames once;
 * steady-state use of either getter performs no conversions at all. */
static void rmt_carry_to_domain( struct rmodtracker *rmt, int domain )
{
	int i;
	/* Both buffers have to exist for a conversion to mean anything. With
	 * lazy allocation the source one may never have been made - which is
	 * the normal case, a caller that only ever uses one getter, and then
	 * there is nothing carried in the other domain to convert. */
	if( rmt->carry_domain && rmt->carry_domain != domain
			&& rmt->carry_pos < rmt->carry_len
			&& rmt->mix_i && rmt->mix_f ) {
		int n   = ( rmt->carry_len - rmt->carry_pos ) * 2;
		int off = rmt->carry_pos * 2;
		if( domain == 2 ) {
			for( i = 0; i < n; i++ )
				rmt->mix_f[ off + i ] = ( float ) rmt->mix_i[ off + i ];
		} else {
			for( i = 0; i < n; i++ )
				rmt->mix_i[ off + i ] = ( int ) rmt->mix_f[ off + i ];
		}
	}
	rmt->carry_domain = domain;
}

size_t rmodtracker_get_samples_s16_interleaved( rmodtracker *rmt,
		int16_t *out, size_t frames )
{
	size_t done = 0;
	if( !rmt || rmt->ended )
		return 0;
	/* Before carry_to_domain, which converts *into* this buffer. */
	if( !rmt_need_i( rmt ) ) {
		rmt->ended = 1;
		return 0;
	}
	rmt_carry_to_domain( rmt, 1 );
	{
		int *mix = rmt->mix_i;
		int carry_pos = rmt->carry_pos;
		int carry_len = rmt->carry_len;
		while( done < frames ) {
			int avail, take;
			if( carry_pos >= carry_len ) {
				int n = replay_get_audio( rmt->replay, mix, 0 );
				if( n <= 0 ) {
					rmt->ended = 1;
					break;
				}
				carry_len = n;
				carry_pos = 0;
			}
			avail = carry_len - carry_pos;
			take  = ( int ) ( frames - done );
			if( take > avail )
				take = avail;
			rmt_clamp_s16( out + done * 2, mix + carry_pos * 2, take * 2 );
			carry_pos += take;
			done += ( size_t ) take;
		}
		rmt->carry_pos = carry_pos;
		rmt->carry_len = carry_len;
	}
	return done;
}

size_t rmodtracker_get_samples_float_interleaved( rmodtracker *rmt,
		float *out, size_t frames )
{
	size_t done = 0;
	if( !rmt || rmt->ended )
		return 0;
	if( !rmt_need_f( rmt ) ) {
		rmt->ended = 1;
		return 0;
	}
	rmt_carry_to_domain( rmt, 2 );
	{
		float *mix = rmt->mix_f;
		int carry_pos = rmt->carry_pos;
		int carry_len = rmt->carry_len;
		while( done < frames ) {
			int avail, take, i;
			if( carry_pos >= carry_len ) {
				int n = replay_get_audio_f( rmt->replay, mix, rmt->ramp_f, 0 );
				if( n <= 0 ) {
					rmt->ended = 1;
					break;
				}
				carry_len = n;
				carry_pos = 0;
			}
			avail = carry_len - carry_pos;
			take  = ( int ) ( frames - done );
			if( take > avail )
				take = avail;
			{
				const float * RMT_RESTRICT srcp = mix + carry_pos * 2;
				float       * RMT_RESTRICT dstp = out + done * 2;
				for( i = 0; i < take * 2; i++ )
					dstp[ i ] = srcp[ i ] * ( 1.0f / 32768.0f );
			}
			carry_pos += take;
			done += ( size_t ) take;
		}
		rmt->carry_pos = carry_pos;
		rmt->carry_len = carry_len;
	}
	return done;
}

/*
---
Copyright (c) 2015, Martin Cameron
All rights reserved.

Redistribution and use in source and binary forms, with or
without modification, are permitted provided that the
following conditions are met:

 * Redistributions of source code must retain the above
   copyright notice, this list of conditions and the
   following disclaimer.

 * Redistributions in binary form must reproduce the
   above copyright notice, this list of conditions and the
   following disclaimer in the documentation and/or other
   materials provided with the distribution.
 
 * Neither the name of the organization nor the names of
   its contributors may be used to endorse or promote
   products derived from this software without specific
   prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND
CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES,
INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR
CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR
BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT
OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
POSSIBILITY OF SUCH DAMAGE.
---
*/
