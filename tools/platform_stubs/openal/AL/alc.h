/* Compile-only stub; see AL/al.h in this directory. */
#ifndef STUB_AL_ALC_H
#define STUB_AL_ALC_H

#include <AL/al.h>

#define ALC_APIENTRY

typedef char ALCchar;
typedef char ALCboolean;
typedef int  ALCenum;
typedef int  ALCint;
typedef struct ALCdevice_  ALCdevice;
typedef struct ALCcontext_ ALCcontext;

#define ALC_DEVICE_SPECIFIER        0x1005
#define ALC_ALL_DEVICES_SPECIFIER   0x1013

ALCboolean alcCloseDevice(ALCdevice *dev);
ALCcontext *alcCreateContext(ALCdevice *dev, const ALCint *attrs);
void alcDestroyContext(ALCcontext *ctx);
void *alcGetProcAddress(ALCdevice *dev, const ALCchar *name);
const ALCchar *alcGetString(ALCdevice *dev, ALCenum param);
ALCboolean alcIsExtensionPresent(ALCdevice *dev, const ALCchar *name);
ALCboolean alcMakeContextCurrent(ALCcontext *ctx);
ALCdevice *alcOpenDevice(const ALCchar *name);

#endif
