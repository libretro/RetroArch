#pragma once
#include <wut.h>
#include <sndcore2/voice.h>
#include <sndcore2/ra_dsp.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct AXMVoice AXMVoice;


struct AXMVoice
{
	bool running;
	uint32_t channels;
	AXVoice *v[];
};


WUT_CHECK_OFFSET(AXMVoice, 0x0, running);
WUT_CHECK_OFFSET(AXMVoice, 0x4, channels);
WUT_CHECK_OFFSET(AXMVoice, 0x8, v);

typedef void (*AXVoiceCallbackEx)(void *p, uint32_t context, uint32_t reason);

void AXAcquireMultiVoice(uint32_t priority, AXVoiceCallbackEx callback, uint32_t userContext, DspConfig *config, AXMVoice **mvp);
void AXSetMultiVoiceDeviceMix(AXMVoice *mvoice, AXDeviceType type, uint32_t id, uint32_t bus, uint16_t vol, int16_t delta);
void AXSetMultiVoiceOffsets(AXMVoice *mvoice, AXVoiceOffsets *offsets);
void AXSetMultiVoiceCurrentOffset(AXMVoice *mvoice, uint32_t offset);
void AXSetMultiVoiceState(AXMVoice *mvoice, AXVoiceState state);
void AXSetMultiVoiceVe(AXMVoice *mvoice, AXVoiceVeData *veData);
void AXSetMultiVoiceSrcType(AXMVoice *mvoice, AXVoiceSrcType type);
void AXSetMultiVoiceSrcRatio(AXMVoice *mvoice, float ratio);
bool AXIsMultiVoiceRunning(AXMVoice *mvoice);
void AXFreeMultiVoice(AXMVoice *mvoice);

#ifdef __cplusplus
}
#endif
