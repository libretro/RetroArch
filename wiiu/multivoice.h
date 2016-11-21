
#ifndef _MULTIVOICE_H_
#define _MULTIVOICE_H_

typedef struct
{
	bool running;
	uint32_t channels;
	AXVoice *v[];
} AXMVoice;

void AXAcquireMultiVoice(uint32_t prio, void *cb, uint32_t cbarg, void *setup, AXMVoice **mvoice);
void AXSetMultiVoiceState(AXMVoice *mvoice, AXVoiceState state);
void AXSetMultiVoiceVe(AXMVoice *mvoice, AXVoiceVeData *veData);
void AXSetMultiVoiceSrcType(AXMVoice *mvoice, AXVoiceSrcType type);
void AXSetMultiVoiceSrcRatio(AXMVoice *mvoice, float ratio);
bool AXIsMultiVoiceRunning(AXMVoice *mvoice);
void AXFreeMultiVoice(AXMVoice *mvoice);

#endif
