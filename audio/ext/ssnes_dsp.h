/////
// API header for external RetroArch DSP plugins.
//
//

#ifndef __SSNES_DSP_PLUGIN_H
#define __SSNES_DSP_PLUGIN_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _WIN32
#ifdef SSNES_DLL_IMPORT
#define SSNES_API_EXPORT __declspec(dllimport) 
#else
#define SSNES_API_EXPORT __declspec(dllexport) 
#endif
#define SSNES_API_CALLTYPE __cdecl
#else
#define SSNES_API_EXPORT
#define SSNES_API_CALLTYPE
#endif

#ifndef SSNES_FALSE
#define SSNES_FALSE 0
#endif

#ifndef SSNES_TRUE
#define SSNES_TRUE 1
#endif

#define SSNES_DSP_API_VERSION 3

typedef struct ssnes_dsp_info
{
   // Input sample rate that the DSP plugin receives. This is generally ~32kHz.
   // Some small variance is allowed due to syncing behavior.
   float input_rate;

   // RetroArch requests that the DSP plugin resamples the 
   // input to a certain frequency.
   //
   // However, the plugin might ignore this
   // using the resample field in ssnes_dsp_output_t (see below).
   float output_rate;
} ssnes_dsp_info_t;

typedef struct ssnes_dsp_output
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

   // If true, the DSP plugin did not resample the input audio, 
   // and requests resampling to the proper frequency to be 
   // performed outside the plugin.
   // If false, 
   // it is assumed that the output has the same sample rate as given
   // in output_rate.
   int should_resample;
} ssnes_dsp_output_t;

typedef struct ssnes_dsp_input
{
   // Input data for the DSP. The samples are interleaved in order: LRLRLRLR
   const float *samples;

   // Number of frames for input data.
   // One frame is here defined as a combined sample of 
   // left and right channels. 
   // (I.e. 44.1kHz, 16bit stereo will have 
   // 88.2k samples/sec and 44.1k frames/sec.)
   unsigned frames;
} ssnes_dsp_input_t;

typedef struct ssnes_dsp_plugin
{
   // Creates a handle of the plugin. Returns NULL if failed.
   void *(*init)(const ssnes_dsp_info_t *info);

   // Processes input data. 
   // The plugin is allowed to return variable sizes for output data.
   void (*process)(void *data, ssnes_dsp_output_t *output, 
         const ssnes_dsp_input_t *input);

   // Frees the handle.
   void (*free)(void *data);

   // API version used to compile the plugin.
   // Used to detect mismatches in API.
   // Must be set to SSNES_DSP_API_VERSION on compile.
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
} ssnes_dsp_plugin_t;

// Called by RetroArch at startup to get the callback struct.
// This is NOT dynamically allocated!
SSNES_API_EXPORT const ssnes_dsp_plugin_t* SSNES_API_CALLTYPE 
   ssnes_dsp_plugin_init(void);

#ifdef __cplusplus
}
#endif

#endif
