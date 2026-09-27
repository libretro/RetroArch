/* Compile-only PSP audio stub; see pspkernel.h. */
#ifndef STUB_PSPAUDIO_H
#define STUB_PSPAUDIO_H
#define PSP_AUDIO_VOLUME_MAX 0x8000
int sceAudioSRCChReserve(int samples, int freq, int channels);
int sceAudioSRCChRelease(void);
int sceAudioSRCOutputBlocking(int vol, void *buf);
#endif
