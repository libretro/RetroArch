/* The format harness's stubs: its audio_thread_parked() follows
 * pipeline_consumer_running. */
#define audio_mixer_init unused_audio_mixer_init
#define audio_mixer_done unused_audio_mixer_done
#include "../pipeline_format/stubs_retroarch.c"
#undef audio_mixer_init
#undef audio_mixer_done

unsigned pipeline_mixer_inits, pipeline_mixer_dones;

void audio_mixer_init(unsigned rate)
{
   pipeline_mixer_inits++;
   unused_audio_mixer_init(rate);
}

void audio_mixer_done(void)
{
   pipeline_mixer_dones++;
   unused_audio_mixer_done();
}
