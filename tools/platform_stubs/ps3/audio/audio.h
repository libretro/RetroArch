/* Compile-only stand-in for PSL1GHT's <audio/audio.h>. */
#ifndef PS3STUB_AUDIO_AUDIO_H
#define PS3STUB_AUDIO_AUDIO_H

#include <stdint.h>
#include <sys/event_queue.h>

#define AUDIO_BLOCK_SAMPLES 256

/* ps3_defines.h maps param_attrib onto attrib on this path, and leaves
 * numChannels/numBlocks as PSL1GHT spells them. */
typedef struct
{
   uint32_t numChannels;
   uint32_t numBlocks;
   uint64_t attrib;
} audioPortParam;

int audioInit(void);
int audioQuit(void);
int audioPortOpen(audioPortParam *param, uint32_t *portNum);
int audioPortClose(uint32_t portNum);
int audioPortStart(uint32_t portNum);
int audioPortStop(uint32_t portNum);
int audioAddData(uint32_t portNum, float *data, uint32_t frames, float volume);
int audioCreateNotifyEventQueue(sys_event_queue_t *id, sys_ipc_key_t *key);
int audioSetNotifyEventQueue(sys_ipc_key_t key);
int audioRemoveNotifyEventQueue(sys_ipc_key_t key);

#endif
