/* Stand-in for ps2sdk's <audsrv.h>, implemented by ps2_mock.c. */
#ifndef PS2_AUDIO_MOCK_AUDSRV_H
#define PS2_AUDIO_MOCK_AUDSRV_H
#define MAX_VOLUME 0x3fff
struct audsrv_fmt_t { int freq, bits, channels; };
typedef int (*audsrv_callback_t)(void *arg);
int audsrv_set_format(struct audsrv_fmt_t *fmt);
int audsrv_set_volume(int volume);
int audsrv_available(void);
int audsrv_queued(void);
int audsrv_play_audio(const char *chunk, int bytes);
int audsrv_stop_audio(void);
int audsrv_on_fillbuf(int amount, audsrv_callback_t cb, void *arg);
#endif
