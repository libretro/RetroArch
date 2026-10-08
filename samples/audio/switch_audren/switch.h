/* A stand-in <switch.h>: the libnx audren/audrv surface
 * audio/drivers/switch_libnx_audren_audio.c uses, so the driver builds
 * and runs on the host. audren_mock.c is the renderer: a voice whose
 * queued wave buffers a renderer thread plays out one per frame, the
 * states reaching the driver only on an update, as on the console. */

#ifndef SWITCH_AUDREN_MOCK_SWITCH_H
#define SWITCH_AUDREN_MOCK_SWITCH_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef uint8_t  u8;
typedef uint32_t u32;
typedef int32_t  s32;
typedef uint64_t u64;
typedef int64_t  s64;
typedef u32      Result;

#define R_FAILED(rc) ((rc) != 0)

#define AUDREN_MEMPOOL_ALIGNMENT   0x1000
#define AUDREN_DEFAULT_DEVICE_NAME "MainAudioOut"
#define AUDREN_FINAL_MIX_ID        0

typedef enum
{
   AudioRendererOutputRate_32kHz,
   AudioRendererOutputRate_48kHz
} AudioRendererOutputRate;

typedef enum
{
   PcmFormat_Invalid,
   PcmFormat_Int8,
   PcmFormat_Int16,
   PcmFormat_Int24,
   PcmFormat_Int32,
   PcmFormat_Float,
   PcmFormat_Adpcm
} PcmFormat;

typedef struct
{
   AudioRendererOutputRate output_rate;
   int num_voices;
   int num_effects;
   int num_sinks;
   int num_mix_objs;
   int num_mix_buffers;
} AudioRendererConfig;

typedef enum
{
   AudioDriverWaveBufState_Free,
   AudioDriverWaveBufState_Waiting,
   AudioDriverWaveBufState_Queued,
   AudioDriverWaveBufState_Playing,
   AudioDriverWaveBufState_Done
} AudioDriverWaveBufState;

typedef struct AudioDriverWaveBuf AudioDriverWaveBuf;
struct AudioDriverWaveBuf
{
   void *data_raw;
   u64   size;
   s32   start_sample_offset;
   s32   end_sample_offset;
   AudioDriverWaveBufState state;
   AudioDriverWaveBuf *next;
};

typedef struct
{
   int voice_playing;
   unsigned channels;
} AudioDriver;

typedef struct
{
   int id;
} Event;

Result audrenInitialize(const AudioRendererConfig *config);
void   audrenExit(void);
Result audrenStartAudioRenderer(void);
Event *audrenGetFrameEvent(void);
Result eventWait(Event *e, u64 timeout_ns);

Result audrvCreate(AudioDriver *d, const AudioRendererConfig *config, int num_final_mix_channels);
void   audrvClose(AudioDriver *d);
Result audrvUpdate(AudioDriver *d);
int    audrvMemPoolAdd(AudioDriver *d, void *buffer, size_t size);
bool   audrvMemPoolAttach(AudioDriver *d, int id);
int    audrvDeviceSinkAdd(AudioDriver *d, const char *device_name, int num_channels, const u8 *channel_ids);
bool   audrvVoiceInit(AudioDriver *d, int id, int num_channels, PcmFormat format, u32 sample_rate);
void   audrvVoiceSetDestinationMix(AudioDriver *d, int id, int mix_id);
void   audrvVoiceSetMixFactor(AudioDriver *d, int id, float factor, int src_channel_id, int dest_channel_id);
bool   audrvVoiceAddWaveBuf(AudioDriver *d, int id, AudioDriverWaveBuf *wavebuf);
void   audrvVoiceStart(AudioDriver *d, int id);
void   audrvVoiceStop(AudioDriver *d, int id);
bool   audrvVoiceIsPlaying(AudioDriver *d, int id);

void   armDCacheFlush(void *addr, size_t size);

#endif
