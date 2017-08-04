
/* ibxm/ac mod/xm/s3m replay (c)mumart@gmail.com */

#ifndef __IBXM_H__
#define __IBXM_H__

extern const char *IBXM_VERSION;

struct data {
	char *buffer;
	int length;
};

struct sample {
	char name[ 32 ];
	int loop_start, loop_length;
	short volume, panning, rel_note, fine_tune, *data;
};

struct envelope {
	char enabled, sustain, looped, num_points;
	short sustain_tick, loop_start_tick, loop_end_tick;
	short points_tick[ 16 ], points_ampl[ 16 ];
};

struct instrument {
	int num_samples, vol_fadeout;
	char name[ 32 ], key_to_sample[ 97 ];
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
	int default_gvol, default_speed, default_tempo, c2_rate, gain;
	int linear_periods, fast_vol_slides;
	unsigned char *default_panning, *sequence;
	struct pattern *patterns;
	struct instrument *instruments;
};

/* Allocate and initialize a module from the specified data, returns NULL on error.
   Message should point to a 64-character buffer to receive error messages. */
struct module* module_load( struct data *data, char *message );
/* Deallocate the specified module. */
void dispose_module( struct module *module );
/* Allocate and initialize a replay with the specified module and sampling rate. */
struct replay* new_replay( struct module *module, int sample_rate, int interpolation );
/* Deallocate the specified replay. */
void dispose_replay( struct replay *replay );
/* Returns the song duration in samples at the current sampling rate. */
int replay_calculate_duration( struct replay *replay );
/* Seek to approximately the specified sample position.
   The actual sample position reached is returned. */
int replay_seek( struct replay *replay, int sample_pos );
/* Set the pattern in the sequence to play. The tempo is reset to the default. */
void replay_set_sequence_pos( struct replay *replay, int pos );
/* Generates audio and returns the number of stereo samples written into mix_buf. */
int replay_get_audio( struct replay *replay, int *mix_buf );
/* Returns the length of the output buffer required by replay_get_audio(). */
int calculate_mix_buf_len( int sample_rate );

#endif
