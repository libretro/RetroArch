/////
// API header for external RetroArch DSP plugins.
//
//

#ifndef __RARCH_DSP_PLUGIN_H
#define __RARCH_DSP_PLUGIN_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#ifdef RARCH_DLL_IMPORT
#define RARCH_API_EXPORT __declspec(dllimport) 
#else
#define RARCH_API_EXPORT __declspec(dllexport) 
#endif
#define RARCH_API_CALLTYPE __cdecl
#else
#define RARCH_API_EXPORT
#define RARCH_API_CALLTYPE
#endif

#ifndef RARCH_FALSE
#define RARCH_FALSE 0
#endif

#ifndef RARCH_TRUE
#define RARCH_TRUE 1
#endif

#define RARCH_DSP_API_VERSION 5

typedef struct rarch_dsp_info
{
   // Input sample rate that the DSP plugin receives.
   float input_rate;
} rarch_dsp_info_t;

typedef struct rarch_dsp_output
{
   // The DSP plugin has to provide the buffering for the output samples.
   // This is for performance reasons to avoid redundant copying of data.
   // The samples are laid out in interleaving order: LRLRLRLR
   // The range of the samples are [-1.0, 1.0]. 
   // This range cannot be exceeded without horrible audio glitches.
   const float *samples;

   // Frames which the DSP plugin outputted for the current process.
   // One frame is here defined as a combined sample of 
   // left and right channels. 
   // (I.e. 44.1kHz, 16bit stereo will have 
   // 88.2k samples/sec and 44.1k frames/sec.)
   unsigned frames;
} rarch_dsp_output_t;

typedef struct rarch_dsp_input
{
   // Input data for the DSP. The samples are interleaved in order: LRLRLRLR
   const float *samples;

   // Number of frames for input data.
   // One frame is here defined as a combined sample of 
   // left and right channels. 
   // (I.e. 44.1kHz, 16bit stereo will have 
   // 88.2k samples/sec and 44.1k frames/sec.)
   unsigned frames;
} rarch_dsp_input_t;

typedef struct rarch_dsp_plugin
{
   // Creates a handle of the plugin. Returns NULL if failed.
   void *(*init)(const rarch_dsp_info_t *info);

   // Processes input data. 
   // The plugin is allowed to return variable sizes for output data.
   void (*process)(void *data, rarch_dsp_output_t *output, 
         const rarch_dsp_input_t *input);

   // Frees the handle.
   void (*free)(void *data);

   // API version used to compile the plugin.
   // Used to detect mismatches in API.
   // Must be set to RARCH_DSP_API_VERSION on compile.
   int api_version;

   // Signal plugin that it may open a configuring window or
   // something similiar. The behavior of this function
   // is thus plugin dependent. Implementing this is optional,
   // and can be set to NULL.
   void (*config)(void *data);

   // Human readable identification string.
   const char *ident;

   // Called every frame, allows creating a GUI main loop in the main thread.
   // GUI events can be processed here in a non-blocking fashion.
   // Can be set to NULL to ignore it.
   void (*events)(void *data);
} rarch_dsp_plugin_t;

// Called by RetroArch at startup to get the callback struct.
// This is NOT dynamically allocated!
RARCH_API_EXPORT const rarch_dsp_plugin_t* RARCH_API_CALLTYPE 
   rarch_dsp_plugin_init(void);

#ifdef __cplusplus
}
#endif

#endif
