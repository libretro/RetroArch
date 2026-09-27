/* Compile-only OpenAL stub for the matrix's openal lane: the shapes
 * audio/drivers/openal.c compiles against, so the lane needs no
 * system libopenal-dev and behaves the same on every runner.  Never
 * linked; see tools/platform_stubs/ for the pattern. */
#ifndef STUB_AL_AL_H
#define STUB_AL_AL_H

#define AL_APIENTRY

typedef char           ALchar;
typedef char           ALboolean;
typedef int            ALenum;
typedef int            ALint;
typedef int            ALsizei;
typedef unsigned int   ALuint;
typedef float          ALfloat;
typedef void           ALvoid;

#define AL_FALSE                 0
#define AL_TRUE                  1
#define AL_NO_ERROR              0
#define AL_SOURCE_STATE          0x1010
#define AL_INITIAL               0x1011
#define AL_PLAYING               0x1012
#define AL_PAUSED                0x1013
#define AL_STOPPED               0x1014
#define AL_LOOPING               0x1007
#define AL_SAMPLE_OFFSET         0x1025
#define AL_BUFFERS_PROCESSED     0x1016
#define AL_FORMAT_STEREO16       0x1103

void alBufferData(ALuint buf, ALenum fmt, const ALvoid *data, ALsizei size, ALsizei rate);
void alDeleteBuffers(ALsizei n, const ALuint *bufs);
void alDeleteSources(ALsizei n, const ALuint *srcs);
void alGenBuffers(ALsizei n, ALuint *bufs);
void alGenSources(ALsizei n, ALuint *srcs);
ALenum alGetEnumValue(const ALchar *name);
ALenum alGetError(void);
void *alGetProcAddress(const ALchar *name);
void alGetSourcei(ALuint src, ALenum param, ALint *value);
ALboolean alIsExtensionPresent(const ALchar *name);
void alSourcePlay(ALuint src);
void alSourceQueueBuffers(ALuint src, ALsizei n, const ALuint *bufs);
void alSourceStop(ALuint src);
void alSourceUnqueueBuffers(ALuint src, ALsizei n, ALuint *bufs);
void alSourcei(ALuint src, ALenum param, ALint value);

#endif
