/* Compile-only Orbis stub, for the psp_ring suite and any ORBIS
 * syntax lane. Never linked against the real library. */
#ifndef STUB_LIBSCEAUDIOOUT_H
#define STUB_LIBSCEAUDIOOUT_H
int sceAudioOutInit(void);
int sceAudioOutOpen(unsigned user, int type, int index, unsigned len,
      unsigned freq, unsigned mode);
int sceAudioOutOutput(int port, const void *buf);
int sceAudioOutClose(int port);
#endif
