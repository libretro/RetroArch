/* Compile-only stand-in for <emscripten/webaudio.h>. Shapes follow
 * emscripten's, because the driver initialises the options structs
 * positionally and indexes the sample frames. */
#ifndef EMSTUB_EMSCRIPTEN_WEBAUDIO_H
#define EMSTUB_EMSCRIPTEN_WEBAUDIO_H

#include <stdint.h>

/* As emscripten's own does, by way of <emscripten/emscripten.h>: this
 * is where the EM_ASM_* macros reach the driver from. */
#include <emscripten.h>

#ifndef __cplusplus
#include <stdbool.h>
#endif

typedef int EMSCRIPTEN_WEBAUDIO_T;
typedef int EMSCRIPTEN_AUDIO_WORKLET_NODE_T;

typedef struct EmscriptenWebAudioCreateAttributes
{
   const char *latencyHint;
   uint32_t    sampleRate;
} EmscriptenWebAudioCreateAttributes;

EMSCRIPTEN_WEBAUDIO_T emscripten_create_audio_context(const EmscriptenWebAudioCreateAttributes *options);

typedef void (*EmscriptenStartWebAudioWorkletCallback)(EMSCRIPTEN_WEBAUDIO_T audioContext, bool success, void *userData2);
void emscripten_start_wasm_audio_worklet_thread_async(EMSCRIPTEN_WEBAUDIO_T audioContext, void *stackLowestAddress, uint32_t stackSize, EmscriptenStartWebAudioWorkletCallback callback, void *userData2);

typedef struct WebAudioParamDescriptor
{
   float defaultValue;
   float minValue;
   float maxValue;
   int   automationRate;
} WebAudioParamDescriptor;

typedef struct WebAudioWorkletProcessorCreateOptions
{
   const char *name;
   int         numAudioParams;
   const WebAudioParamDescriptor *audioParamDescriptors;
} WebAudioWorkletProcessorCreateOptions;

typedef void (*EmscriptenWorkletProcessorCreatedCallback)(EMSCRIPTEN_WEBAUDIO_T audioContext, bool success, void *userData3);
void emscripten_create_wasm_audio_worklet_processor_async(EMSCRIPTEN_WEBAUDIO_T audioContext, const WebAudioWorkletProcessorCreateOptions *options, EmscriptenWorkletProcessorCreatedCallback callback, void *userData3);

int emscripten_audio_context_quantum_size(EMSCRIPTEN_WEBAUDIO_T audioContext);
int emscripten_audio_context_sample_rate(EMSCRIPTEN_WEBAUDIO_T audioContext);

typedef struct AudioSampleFrame
{
   const int numberOfChannels;
   const int samplesPerChannel;
   float    *data;
} AudioSampleFrame;

typedef struct AudioParamFrame
{
   int    length;
   float *data;
} AudioParamFrame;

typedef bool (*EmscriptenWorkletNodeProcessCallback)(int numInputs, const AudioSampleFrame *inputs, int numOutputs, AudioSampleFrame *outputs, int numParams, const AudioParamFrame *params, void *userData4);

typedef enum {
   WEBAUDIO_CHANNEL_COUNT_MODE_MAX         = 0,
   WEBAUDIO_CHANNEL_COUNT_MODE_CLAMPED_MAX = 1,
   WEBAUDIO_CHANNEL_COUNT_MODE_EXPLICIT    = 2
} WEBAUDIO_CHANNEL_COUNT_MODE;

typedef enum {
   WEBAUDIO_CHANNEL_INTERPRETATION_SPEAKERS = 0,
   WEBAUDIO_CHANNEL_INTERPRETATION_DISCRETE = 1
} WEBAUDIO_CHANNEL_INTERPRETATION;

typedef struct EmscriptenAudioWorkletNodeCreateOptions
{
   int  numberOfInputs;
   int  numberOfOutputs;
   int *outputChannelCounts;
   unsigned long channelCount;
   WEBAUDIO_CHANNEL_COUNT_MODE    channelCountMode;
   WEBAUDIO_CHANNEL_INTERPRETATION channelInterpretation;
} EmscriptenAudioWorkletNodeCreateOptions;

EMSCRIPTEN_WEBAUDIO_T emscripten_create_wasm_audio_worklet_node(EMSCRIPTEN_WEBAUDIO_T audioContext, const char *name, const EmscriptenAudioWorkletNodeCreateOptions *options, EmscriptenWorkletNodeProcessCallback processCallback, void *userData4);
void emscripten_audio_node_connect(EMSCRIPTEN_WEBAUDIO_T source, EMSCRIPTEN_WEBAUDIO_T destination, int outputIndex, int inputIndex);

#endif
