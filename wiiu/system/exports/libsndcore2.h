
EXPORT_BEGIN(sndcore2.rpl);

#include "../rpl/libsndcore2/exports.h"

EXPORT(AXRegisterFrameCallback);

EXPORT(AXAcquireMultiVoice);
EXPORT(AXSetMultiVoiceDeviceMix);
EXPORT(AXSetMultiVoiceOffsets);
EXPORT(AXSetMultiVoiceState);
EXPORT(AXSetMultiVoiceVe);
EXPORT(AXSetMultiVoiceSrcType);
EXPORT(AXSetMultiVoiceSrcRatio);
EXPORT(AXIsMultiVoiceRunning);
EXPORT(AXFreeMultiVoice);

EXPORT_END();
