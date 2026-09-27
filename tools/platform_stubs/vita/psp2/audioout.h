/* Compile-only Vita stub for the matrix's vita psp2_audio lane. */
#ifndef STUB_PSP2_AUDIOOUT_H
#define STUB_PSP2_AUDIOOUT_H
#define SCE_AUDIO_OUT_PORT_TYPE_MAIN 0
#define SCE_AUDIO_OUT_MODE_STEREO    1
int sceAudioOutOpenPort(int type, int len, int freq, int mode);
int sceAudioOutReleasePort(int port);
int sceAudioOutOutput(int port, const void *buf);
#endif
