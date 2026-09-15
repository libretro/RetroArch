/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2023-2025 - Jesse Talavera-Greenberg
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 *  NOTE ON write_raw: This driver does NOT implement write_raw.
 *  RetroArch's audio rate control system works by dynamically adjusting
 *  the sinc resampler ratio each frame to keep the driver's audio buffer
 *  at ~50% saturation.  The write_raw fast path bypasses the software
 *  resampler and passes the rate_adjust parameter to the driver, which
 *  must apply it internally.  WASAPI's IAudioClient locks its stream
 *  format and internal resampling configuration at Initialize() time —
 *  there is no API to dynamically adjust the conversion ratio during
 *  streaming.  Without dynamic rate adjustment, the audio buffer would
 *  slowly drift until it underruns or overruns, breaking A/V sync
 *  within minutes.  The software sinc resampler remains the only
 *  viable path.  (CoreAudio is the only driver that implements
 *  write_raw because AudioConverter supports dynamic rate changes
 *  via AudioConverterSetProperty during playback.)
 */

#include <stdlib.h>

#include <lists/string_list.h>
#include <queues/fifo_queue.h>
#include <formats/rac3.h>
#include <formats/iec61937.h>

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#include <retro_atomic.h>
#endif

#include "fake_wasapi.h"



#include "../../../audio/audio_driver.h"
#ifdef HAVE_MICROPHONE
#include "../microphone_driver.h"
#endif
#include "../../../verbosity.h"
#include "../../../configuration.h"

#define WASAPI_TIMEOUT 256

#ifndef AUDCLNT_E_ENGINE_PERIODICITY_LOCKED
#define AUDCLNT_E_ENGINE_PERIODICITY_LOCKED AUDCLNT_ERR(0x028)
#endif
#ifndef AUDCLNT_E_ENGINE_FORMAT_LOCKED
#define AUDCLNT_E_ENGINE_FORMAT_LOCKED AUDCLNT_ERR(0x029)
#endif

enum wasapi_flags
{
   WASAPI_FLG_EXCLUSIVE = (1 << 0),
   WASAPI_FLG_NONBLOCK  = (1 << 1),
   WASAPI_FLG_RUNNING   = (1 << 2),
   /* Shared client initialised via IAudioClient3 at the engine's minimum
    * period rather than its default. The client FIFO keeps its
    * audio_latency size; only the release cadence is finer. */
   WASAPI_FLG_LOWLAT    = (1 << 3),
   /* wasapi_init brought COM up on its thread; wasapi_free releases
    * it after the last COM object.  See mmdevice_com_init(). */
   WASAPI_FLG_COM       = (1 << 4)
};

typedef struct
{
   HANDLE write_event;
#ifdef HAVE_THREADS
   sthread_t *imm_thread;
#else
   HANDLE imm_thread;
#endif
   IMMDevice          *device;
   IAudioClient       *client;
   IAudioRenderClient *renderer;
   fifo_buffer_t      *buffer;
   size_t engine_buffer_size;
#ifdef HAVE_THREADS
   /* Exclusive mode: the thread that owns the device's event. Each
    * period it takes one from the fifo and releases it, or releases
    * silence and counts an underrun. The writer touches only the fifo,
    * under fifo_lock; room_cond is signalled each time the pump frees
    * a period. See wasapi_pump_thread(). */
   sthread_t          *pump;
   slock_t            *fifo_lock;
   scond_t            *room_cond;
   retro_atomic_int_t  pump_run;
   /* Periods the pump filled with silence for want of audio: one
    * atomic add on that path, read by the frontend's overlay. */
   retro_atomic_size_t underruns;
   /* Frames the device has consumed, kept by the pump under fifo_lock:
    * exclusive, the periods it released, each taken by the device a
    * period after; shared, the frames released less the engine's
    * padding. For the frontend's sink rate estimate. */
   uint64_t            consumed;
   uint64_t            released;   /* shared: frames given to the engine */
   unsigned            sh_period_frames; /* shared: frames the engine takes per event */
   /* Read by the pump thread instead of the EXCLUSIVE bit in flags:
    * flags is one byte that start(), stop() and set_nonblock_state()
    * write from other threads while the pump runs, and a read of one
    * bit races with a write of another. Set at init, never changed. */
   bool                pump_exclusive;
#endif
   /* Bytes per frame: the channels at the sample size - 4 or 8 for
    * stereo, up to 32 for 7.1 float. The byte-to-frame conversions
    * on the write path divide by it; a shift was possible only while
    * every frame size was a power of two. */
   unsigned char frame_size;
   /* Float samples, whatever the channel count. */
   bool          float_format;
   /* AC-3 over IEC 61937: the frontend writes float frames of the
    * layout, the encoder takes them 1536 at a time, and what reaches
    * the fifo and the device is the 6144-byte burst - 1536 frames of
    * the 2-channel 16-bit carrier, so a layout frame and a device
    * frame are the same count and only the bytes per frame differ.
    * ac3_frame_size is the frontend's view; frame_size the device's.
    * The sizes reported to the frontend are in its own frames. */
   rac3_encoder_t *ac3;
   float         *ac3_in;         /* 1536 frames of the layout */
   unsigned       ac3_in_frames;  /* collected so far */
   /* Frames the frontend has written that the device has not taken:
    * the fifo's and the encoder's together, kept under fifo_lock so
    * write_avail() reads one figure - read as two, from two threads,
    * the fifo's room and the encoder's count could be from either
    * side of a burst's push, half the buffer apart. */
   size_t         ac3_inflight;
   /* Whether the fifo has ever been fed: a period of silence before
    * the first audio is not an underrun. On the AC-3 path the first
    * burst is 32 ms of input and an encode away from the first write,
    * and the frontend, told of underruns meanwhile, discarded the
    * audio it had primed the pipe with, then filled the fifo at rate
    * control's pace: a quarter of a minute low. Under fifo_lock. */
   bool           fed;
   unsigned       ac3_frame_size;
   uint8_t        ac3_frame[RAC3_MAX_FRAME_BYTES];
   uint8_t        ac3_burst[IEC61937_AC3_BURST_BYTES];
   uint8_t flags;
   /* The speaker layout the stream opened with, as the frontend's
    * mask (WAVEFORMATEXTENSIBLE's bits): what the device took, which
    * for a shared-mode stream is the engine's mix format's mask. */
   uint32_t layout;
} wasapi_t;

static void wasapi_imm_stop_thread(wasapi_t *w)
{
   if (!w->imm_thread)
      return;

   PostThreadMessage(IMMNotificationThreadId, WM_QUIT, 0, 0);

#ifdef HAVE_THREADS
   sthread_join(w->imm_thread);
#else
   WaitForSingleObject(w->imm_thread, WASAPI_TIMEOUT);
   CloseHandle(w->imm_thread);
#endif

   IMMNotificationThreadId = 0;
   w->imm_thread = NULL;
}

static bool wasapi_imm_start_thread(wasapi_t *w)
{
   if (!w->imm_thread)
   {
#ifdef HAVE_THREADS
      w->imm_thread = sthread_create(mmdevice_thread, w);
#else
      w->imm_thread = CreateThread(NULL, 0, mmdevice_thread, w, 0, NULL);
#endif
      if (!w->imm_thread)
         return false;
   }
   return true;
}

static const char *wasapi_wave_subtype_name(const GUID *guid)
{
   if (!memcmp(guid, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(GUID)))
      return "KSDATAFORMAT_SUBTYPE_IEEE_FLOAT";
   return "<unknown sub-format>";
}

static const char *wasapi_wave_format_name(const WAVEFORMATEXTENSIBLE *format)
{
   switch (format->Format.wFormatTag)
   {
      case WAVE_FORMAT_PCM:
         return "WAVE_FORMAT_PCM";
      case WAVE_FORMAT_EXTENSIBLE:
         return wasapi_wave_subtype_name(&format->SubFormat);
      default:
         break;
   }
   return "<unknown>";
}

/* bytes of the carrier left the fifo for the device; under fifo_lock
 * where there is one */
static INLINE void wasapi_ac3_drained(wasapi_t *w, size_t bytes)
{
   if (w->ac3)
   {
      size_t frames = bytes / 4;
      w->ac3_inflight = w->ac3_inflight > frames ? w->ac3_inflight - frames : 0;
   }
}

static const char* wasapi_error(DWORD error)
{
   /* One buffer per thread via __declspec(thread) would be ideal,
    * but that requires MSVC or GCC extensions beyond C89.
    * Use a single static buffer protected by the knowledge that
    * all callers are on the audio thread; document the limitation. */
   static char s[256];
   DWORD ret = FormatMessageA(
           FORMAT_MESSAGE_IGNORE_INSERTS
         | FORMAT_MESSAGE_FROM_SYSTEM,
         NULL, error,
         MAKELANGID(LANG_ENGLISH, SUBLANG_DEFAULT),
         s, (DWORD)(sizeof(s) - 1), NULL);
   if (!ret)
      s[0] = '\0';
   /* Strip trailing CR/LF that FormatMessage appends */
   else
   {
      char *p = s + ret - 1;
      while (p >= s && (*p == '\r' || *p == '\n' || *p == ' '))
         *p-- = '\0';
   }
   return s;
}

/* The speaker mask: mono, or the frontend's layout, whose bits are
 * WAVEFORMATEXTENSIBLE's own. */
static DWORD wasapi_channel_mask(unsigned channels, uint32_t layout)
{
   if (channels == 1)
      return KSAUDIO_SPEAKER_MONO;
   if (channels == 2)
      return KSAUDIO_SPEAKER_STEREO;
   return (DWORD)layout;
}

static void wasapi_set_format(WAVEFORMATEXTENSIBLE *wf,
      bool float_fmt, unsigned rate, unsigned channels, uint32_t layout)
{
   WORD wBitsPerSample        = float_fmt ? 32 : 16;
   WORD nBlockAlign           = (channels * wBitsPerSample) / 8;
   DWORD nAvgBytesPerSec      = rate * nBlockAlign;

   wf->Format.nChannels       = channels;
   wf->Format.nSamplesPerSec  = rate;
   wf->Format.nAvgBytesPerSec = nAvgBytesPerSec;
   wf->Format.nBlockAlign     = nBlockAlign;
   wf->Format.wBitsPerSample  = wBitsPerSample;

   /* Float, or more than stereo, is WAVE_FORMAT_EXTENSIBLE with the
    * speaker mask; stereo int16 stays plain PCM, as it always was. */
   if (float_fmt || channels > 2)
   {
      wf->Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE;
      wf->Format.cbSize               = sizeof(WORD) + sizeof(DWORD) + sizeof(GUID);
      wf->Samples.wValidBitsPerSample = wBitsPerSample;
      wf->dwChannelMask               = wasapi_channel_mask(channels, layout);
      wf->SubFormat                   = float_fmt
            ? KSDATAFORMAT_SUBTYPE_IEEE_FLOAT : KSDATAFORMAT_SUBTYPE_PCM;
   }
   else
   {
      wf->Format.wFormatTag           = WAVE_FORMAT_PCM;
      wf->Format.cbSize               = 0;
      wf->Samples.wValidBitsPerSample = 0;
      wf->dwChannelMask               = 0;
      memset(&wf->SubFormat, 0, sizeof(wf->SubFormat));
   }
}

/* The layout a format opened with: the mask a wider EXTENSIBLE format
 * carries, stereo for anything else. */
static uint32_t wasapi_format_layout(const WAVEFORMATEXTENSIBLE *wf)
{
   if (wf->Format.nChannels > 2 && wf->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE)
      return (uint32_t)wf->dwChannelMask;
   return AUDIO_LAYOUT_STEREO;
}

/**
 * @param[in] format The format to check.
 * @return \c true if \c format is suitable for RetroArch.
 */
static bool wasapi_is_format_suitable(const WAVEFORMATEXTENSIBLE *format)
{
   /* Mono mic input, stereo speaker output, or a wider speaker layout
    * the frontend can fill - and for that the format must say its
    * positions: a wider format without a speaker mask is not one. */
   if (!format || format->Format.nChannels == 0)
      return false;
   if (format->Format.nChannels > 2)
   {
      if (format->Format.wFormatTag != WAVE_FORMAT_EXTENSIBLE)
         return false;
      if (!audio_layout_supported((uint32_t)format->dwChannelMask))
         return false;
      if (audio_layout_channels((uint32_t)format->dwChannelMask) != format->Format.nChannels)
         return false;
   }

   switch (format->Format.wFormatTag)
   {
      case WAVE_FORMAT_PCM:
         if (format->Format.wBitsPerSample != 16)
            /* Integer samples must be 16-bit */
            return false;
         break;
      case WAVE_FORMAT_EXTENSIBLE:
         if (memcmp(&format->SubFormat, &KSDATAFORMAT_SUBTYPE_IEEE_FLOAT, sizeof(GUID)) == 0)
         {
            if (format->Format.wBitsPerSample != 32)
               /* floating-point samples must be 32-bit */
               return false;
         }
         else if (memcmp(&format->SubFormat, &KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID)) == 0)
         {
            if (format->Format.wBitsPerSample != 16)
               return false;
         }
         else
            /* RetroArch doesn't support any other subformat */
            return false;
         break;
      default:
         /* Other formats are unsupported */
         return false;
   }
   return true;
}

/**
 * Selects a sample format suitable for the given device.
 * @param[in,out] format The place where the chosen format will be written,
 * as well as the first format checked.
 * @param[in] client The audio client (i.e. device handle) for which a format will be selected.
 * @param[in] mode The device mode (shared or exclusive) that \c client will use.
 * @param[in] channels The number of channels that will constitute one audio frame.
 * @return \c true if successful, \c false if a suitable format wasn't found or there was an error.
 * If \c true, the selected format will be written to \c format.
 * If \c false, the value referred by \c format will be unchanged.
 */
/* The other layout of the same count: 5.1 with the rear pair at the
 * sides for 5.1 with it at the back and back again. Windows' own
 * endpoints are usually the sides one (KSAUDIO_SPEAKER_5POINT1_SURROUND
 * is what a 5.1 endpoint reports), and exclusive mode takes exactly
 * the mask or nothing, so a request for one is tried as the other
 * before it is given up on - and whichever the device took is what is
 * reported, never the request. 0 when there is no sibling. */
static uint32_t wasapi_sibling_layout(uint32_t layout)
{
   if (layout == AUDIO_LAYOUT_5POINT1)
      return AUDIO_LAYOUT_5POINT1_SURROUND;
   if (layout == AUDIO_LAYOUT_5POINT1_SURROUND)
      return AUDIO_LAYOUT_5POINT1;
   return 0;
}

/* What the endpoint would take, logged once when a wide layout is
 * refused: the engine's mix format, and for six and eight channels
 * under each mask whether 16-bit, 24-in-32-bit and float are accepted
 * in the mode. The frontend writes 16-bit or float; a device that
 * takes multichannel only as 24-in-32 says so here, which is the
 * fact the next step needs. */
static void wasapi_log_endpoint_formats(IAudioClient *client, AUDCLNT_SHAREMODE mode)
{
   WAVEFORMATEXTENSIBLE *mix = NULL;
   static const struct { unsigned ch; uint32_t mask; const char *name; } lays[] = {
      { 6, AUDIO_LAYOUT_5POINT1,          "5.1 back"  },
      { 6, AUDIO_LAYOUT_5POINT1_SURROUND, "5.1 sides" },
      { 8, AUDIO_LAYOUT_7POINT1,          "7.1"       },
   };
   size_t i;

   if (SUCCEEDED(_IAudioClient_GetMixFormat(client, (WAVEFORMATEX **)&mix)) && mix)
   {
      RARCH_LOG("[WASAPI] Endpoint mix format: %s, %u channels, mask 0x%03x, %u bits (%u valid), %u Hz.\n",
            wasapi_wave_format_name(mix), mix->Format.nChannels,
            mix->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE ? (unsigned)mix->dwChannelMask : 0u,
            mix->Format.wBitsPerSample,
            mix->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE ? mix->Samples.wValidBitsPerSample : mix->Format.wBitsPerSample,
            (unsigned)mix->Format.nSamplesPerSec);
      /* The mix format is the endpoint's speaker setup as Windows has
       * it, and exclusive mode opens nothing wider than that setup:
       * a stereo mix format means the endpoint is configured stereo,
       * and no six-channel format will be taken until it is set to
       * 5.1 or 7.1 in Windows' sound settings - which an HDMI
       * television often does not offer, taking multichannel only as
       * a bitstream, not PCM. */
      if (mix->Format.nChannels <= 2)
         RARCH_WARN("[WASAPI] The endpoint is configured as %u-channel in Windows; a wider layout needs its speaker setup set to 5.1 or 7.1 in the sound settings, and an HDMI display may not offer that for PCM.\n",
               mix->Format.nChannels);
      CoTaskMemFree(mix);
   }
   for (i = 0; i < ARRAY_SIZE(lays); i++)
   {
      WAVEFORMATEXTENSIBLE wf;
      HRESULT h16, h24, hf;
      wasapi_set_format(&wf, false, 48000, lays[i].ch, lays[i].mask);
      h16 = _IAudioClient_IsFormatSupported(client, mode, (const WAVEFORMATEX *)&wf, NULL);
      /* 24 valid bits in a 32-bit container, the common HDMI form. */
      wf.Format.wBitsPerSample        = 32;
      wf.Format.nBlockAlign           = (WORD)(lays[i].ch * 4);
      wf.Format.nAvgBytesPerSec       = 48000 * wf.Format.nBlockAlign;
      wf.Samples.wValidBitsPerSample  = 24;
      h24 = _IAudioClient_IsFormatSupported(client, mode, (const WAVEFORMATEX *)&wf, NULL);
      wasapi_set_format(&wf, true, 48000, lays[i].ch, lays[i].mask);
      hf  = _IAudioClient_IsFormatSupported(client, mode, (const WAVEFORMATEX *)&wf, NULL);
      RARCH_LOG("[WASAPI] %s mode, %s (mask 0x%03x) at 48000 Hz: 16-bit %s, 24-in-32 %s, float %s.\n",
            mode == AUDCLNT_SHAREMODE_EXCLUSIVE ? "Exclusive" : "Shared",
            lays[i].name, lays[i].mask,
            h16 == S_OK ? "yes" : "no", h24 == S_OK ? "yes" : "no", hf == S_OK ? "yes" : "no");
   }
}

static bool wasapi_select_device_format(WAVEFORMATEXTENSIBLE *format, IAudioClient *client, AUDCLNT_SHAREMODE mode, unsigned channels, uint32_t layout)
{
   /* Try the requested sample format first, then try the other one. */
   WAVEFORMATEXTENSIBLE *suggested_format  = NULL;
   /* IAudioClient::IsFormatSupported allocates a format object. */
   /* The Windows docs say that casting these arguments to WAVEFORMATEX* is okay. */
   HRESULT hr                              = _IAudioClient_IsFormatSupported(
         client, mode,
         (const WAVEFORMATEX *)format,
         (WAVEFORMATEX **)&suggested_format);

   switch (hr)
   {
      case S_OK:
         /* The requested format is okay without any changes. */
         CoTaskMemFree(suggested_format);
         return true;
      case S_FALSE:
         /* The requested format is unsupported, but Windows has suggested a similar one. */
         RARCH_DBG("[WASAPI] Windows suggests a format of (%s, %u-channel, %uHz).\n",
               wasapi_wave_format_name(suggested_format),
               suggested_format->Format.nChannels,
               suggested_format->Format.nSamplesPerSec);

         /* A suggestion with fewer channels than asked is not this
          * request granted with a different sample format; it is the
          * layout refused, and the caller decides what stereo costs. */
         if (suggested_format->Format.nChannels != channels)
         {
            RARCH_WARN("[WASAPI] Windows offers %u channels for the %u requested; layout 0x%03x refused.\n",
                  suggested_format->Format.nChannels, channels, layout);
            break;
         }
         if (wasapi_is_format_suitable(suggested_format))
         {
            *format = *suggested_format;
            CoTaskMemFree(suggested_format);
            return true;
         }
         RARCH_ERR("[WASAPI] Windows suggested a format, but RetroArch can't use it.\n");
         break;
      case AUDCLNT_E_UNSUPPORTED_FORMAT:
      {
         /* The requested format is unsupported
          * and Windows was unable to suggest another.
          * Usually happens with exclusive mode.
          * RetroArch will try selecting a format. */
         size_t i, j;
         bool preferred_formats[2];
         preferred_formats[0] = (format->Format.wFormatTag == WAVE_FORMAT_EXTENSIBLE);
         preferred_formats[1] = (format->Format.wFormatTag != WAVE_FORMAT_EXTENSIBLE);
         RARCH_WARN("[WASAPI] Requested format not supported, and Windows could not suggest one. RetroArch will do so.\n");
         /* The layout as asked, then its sibling: the same speakers
          * under the mask the device may prefer. */
         {
            uint32_t layouts[2];
            size_t   nl = 1, k;
            layouts[0] = layout;
            layouts[1] = wasapi_sibling_layout(layout);
            if (layouts[1])
               nl = 2;
         for (k = 0; k < nl; k++)
         for (i = 0; i < ARRAY_SIZE(preferred_formats); ++i)
         {
            static const unsigned preferred_rates[] = { 48000, 44100, 96000, 192000, 32000 };
            for (j = 0; j < ARRAY_SIZE(preferred_rates); ++j)
            {
               HRESULT format_check_hr;
               WAVEFORMATEXTENSIBLE possible_format;
               wasapi_set_format(&possible_format, preferred_formats[i], preferred_rates[j], channels, layouts[k]);
               format_check_hr = _IAudioClient_IsFormatSupported(client, mode, (const WAVEFORMATEX *) &possible_format, NULL);
               if (SUCCEEDED(format_check_hr))
               {
                  *format = possible_format;
                  RARCH_DBG("[WASAPI] RetroArch suggests a format of (%s, %u-channel, %uHz, mask 0x%03x).\n",
                        wasapi_wave_format_name(format),
                        format->Format.nChannels,
                        format->Format.nSamplesPerSec,
                        (unsigned)format->dwChannelMask);
                  if (k)
                     RARCH_LOG("[WASAPI] The device took layout 0x%03x in place of the requested 0x%03x: the rear pair is at the %s.\n",
                           layouts[k], layout,
                           (layouts[k] & AUDIO_SPEAKER_SIDE_LEFT) ? "sides" : "back");
                  CoTaskMemFree(suggested_format);
                  return true;
               }
            }
         }
         }
         RARCH_ERR("[WASAPI] Failed to select client format: No suitable format available.\n");
         if (channels > 2)
            wasapi_log_endpoint_formats(client, mode);
         break;
      }
      default:
         /* Something else went wrong. */
         RARCH_ERR("[WASAPI] Failed to select client format: %s.\n",
               mmdevice_hresult_name(hr));
         break;
   }

   if (suggested_format)
      CoTaskMemFree(suggested_format);
   return false;
}

/* IAudioClient::Initialize for an exclusive event-driven stream on
 * client with format wf, with the period policy and the retries every
 * exclusive stream needs: the period is a quarter of the latency
 * setting, never above it, floored at the device's minimum; an
 * unaligned buffer is retried at the size the device wants, an
 * already-initialised client and a refused period are retried on a
 * fresh client with the whole setting. On failure the client is
 * released and NULL is left in *client. */
static HRESULT wasapi_initialize_exclusive(IMMDevice *device, IAudioClient **client,
      const WAVEFORMATEX *wf, unsigned latency, unsigned rate)
{
   REFERENCE_TIME minimum_period  = 0;
   REFERENCE_TIME buffer_duration = 0;
   HRESULT hr;

   hr = _IAudioClient_GetDevicePeriod(*client, NULL, &minimum_period);
   if (FAILED(hr))
      RARCH_ERR("[WASAPI] Failed to get minimum device period of exclusive client: %s.\n",
            mmdevice_hresult_name(hr));

   /* The exclusive period, in 100 ns units. In exclusive event mode the
    * duration and the periodicity given to Initialize are one figure:
    * the WaveRT period, the device stage. It was the whole latency
    * setting - a 64 ms setting made a 64 ms hardware period, with a
    * period-sized fifo in front of it. As the ASIO driver picks its
    * period: a quarter of the setting, never above it, floored at the
    * device's minimum period as it always was. The fifo, sized in
    * wasapi_init(), holds the setting; the pump thread feeds the
    * device from it. A device that refuses the smaller period gets the
    * whole setting again, below. */
   buffer_duration = latency * 10000.0 / 4.0;
   if (buffer_duration > latency * 10000.0)
      buffer_duration = latency * 10000.0;
   if (buffer_duration < minimum_period)
      buffer_duration = minimum_period;

   hr = _IAudioClient_Initialize(*client, AUDCLNT_SHAREMODE_EXCLUSIVE,
         AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
         buffer_duration, buffer_duration, (WAVEFORMATEX*)wf, NULL);

   if (hr == AUDCLNT_E_BUFFER_SIZE_NOT_ALIGNED)
   {
      UINT32 buffer_length = 0;
      RARCH_WARN("[WASAPI] Unaligned buffer size: %s.\n",
            mmdevice_hresult_name(hr));
      hr = _IAudioClient_GetBufferSize(*client, &buffer_length);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI] Failed to get buffer size of client: %s.\n",
               mmdevice_hresult_name(hr));
         goto error;
      }

      RELEASE((*client));

      hr     = _IMMDevice_Activate(device,
            IID_IAudioClient,
            CLSCTX_ALL, NULL, (void**)client);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI] IMMDevice::Activate failed: %s.\n",
               mmdevice_hresult_name(hr));
         *client = NULL;
         return hr;
      }

      buffer_duration = 10000.0 * 1000.0 / rate * buffer_length + 0.5;
      hr = _IAudioClient_Initialize(*client, AUDCLNT_SHAREMODE_EXCLUSIVE,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
            buffer_duration, buffer_duration, (WAVEFORMATEX*)wf, NULL);
   }
   if (hr == AUDCLNT_E_ALREADY_INITIALIZED)
   {
      RELEASE((*client));

      hr     = _IMMDevice_Activate(device,
            IID_IAudioClient,
            CLSCTX_ALL, NULL, (void**)client);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI] IMMDevice::Activate failed: %s.\n",
               mmdevice_hresult_name(hr));
         *client = NULL;
         return hr;
      }

      hr = _IAudioClient_Initialize(*client, AUDCLNT_SHAREMODE_EXCLUSIVE,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
            buffer_duration, buffer_duration, (WAVEFORMATEX*)wf, NULL);
   }
   if (     hr == AUDCLNT_E_DEVICE_IN_USE
         || hr == AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED)
      goto error;

   /* A period the device will not take - some pins reject anything
    * under their own idea of a minimum, alignment aside - is retried
    * as the whole setting, which is what every device took before. */
   if (FAILED(hr) && buffer_duration < (REFERENCE_TIME)(latency * 10000.0)
         && (REFERENCE_TIME)(latency * 10000.0) >= minimum_period)
   {
      RARCH_WARN("[WASAPI] Exclusive period of %.1f ms refused (%s); retrying with the whole %u ms setting.\n",
            (float)buffer_duration / 10000.0f, mmdevice_hresult_name(hr), latency);
      RELEASE((*client));
      hr = _IMMDevice_Activate(device, IID_IAudioClient, CLSCTX_ALL, NULL, (void**)client);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI] IMMDevice::Activate failed: %s.\n",
               mmdevice_hresult_name(hr));
         *client = NULL;
         return hr;
      }
      buffer_duration = latency * 10000.0;
      hr = _IAudioClient_Initialize(*client, AUDCLNT_SHAREMODE_EXCLUSIVE,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_NOPERSIST,
            buffer_duration, buffer_duration, (WAVEFORMATEX*)wf, NULL);
      if (hr == AUDCLNT_E_DEVICE_IN_USE || hr == AUDCLNT_E_EXCLUSIVE_MODE_NOT_ALLOWED)
         goto error;
   }

   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI] IAudioClient::Initialize failed: %s.\n",
            mmdevice_hresult_name(hr));
      goto error;
   }
   return hr;

error:
   RELEASE((*client));
   *client = NULL;
   return hr;
}

static IAudioClient *wasapi_init_client_ex(IMMDevice *device,
      bool *float_fmt, unsigned *rate, unsigned latency, unsigned channels,
      uint32_t layout, uint32_t *layout_out)
{
   WAVEFORMATEXTENSIBLE wf;
   IAudioClient *client           = NULL;
   HRESULT hr                     = _IMMDevice_Activate(device,
         IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&client);

   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI] IMMDevice::Activate failed: %s.\n",
            mmdevice_hresult_name(hr));
      return NULL;
   }

   wasapi_set_format(&wf, *float_fmt, *rate, channels, layout);
   RARCH_DBG("[WASAPI] Requesting exclusive %u-bit %u-channel client with %s samples at %uHz %ums.\n",
         wf.Format.wBitsPerSample,
         wf.Format.nChannels,
         wasapi_wave_format_name(&wf),
         wf.Format.nSamplesPerSec,
         latency);

   if (!wasapi_select_device_format(&wf, client, AUDCLNT_SHAREMODE_EXCLUSIVE, channels, layout))
   {
      RARCH_ERR("[WASAPI] Failed to select a suitable device format.\n");
      RELEASE(client);
      return NULL;
   }

   if (FAILED(wasapi_initialize_exclusive(device, &client, (const WAVEFORMATEX*)&wf, latency, *rate)))
      return NULL;

   *float_fmt = wf.Format.wFormatTag != WAVE_FORMAT_PCM
         && memcmp(&wf.SubFormat, &KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID)) != 0;
   *rate      = wf.Format.nSamplesPerSec;
   if (layout_out)
      *layout_out = wasapi_format_layout(&wf);
   return client;
}

/* An exclusive stream carrying AC-3 in IEC 61937 bursts, for a
 * device - a TV or receiver on HDMI or S/PDIF - that takes no more
 * than stereo PCM but decodes Dolby Digital: what the frontend's
 * wider layout becomes when the PCM form of it is refused. The
 * stream the device sees is 2-channel 16-bit at the encoder's rate;
 * *rate is forced to one the encoder takes (48 kHz unless it already
 * is 44.1 or 32). NULL, quietly, when the device does not take the
 * format: the caller has stereo PCM to fall back on. */
static IAudioClient *wasapi_init_client_ac3(IMMDevice *device,
      unsigned *rate, unsigned latency, unsigned channels, unsigned kbps)
{
   mmdevice_iec61937_format_t wf;
   IAudioClient *client = NULL;
   HRESULT hr;

   if (*rate != 48000 && *rate != 44100 && *rate != 32000)
      *rate = 48000;

   hr = _IMMDevice_Activate(device, IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&client);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI] IMMDevice::Activate failed: %s.\n",
            mmdevice_hresult_name(hr));
      return NULL;
   }

   memset(&wf, 0, sizeof(wf));
   wf.FormatExt.Format.wFormatTag           = WAVE_FORMAT_EXTENSIBLE;
   wf.FormatExt.Format.nChannels            = 2;
   wf.FormatExt.Format.nSamplesPerSec       = *rate;
   wf.FormatExt.Format.wBitsPerSample       = 16;
   wf.FormatExt.Format.nBlockAlign          = 4;
   wf.FormatExt.Format.nAvgBytesPerSec      = *rate * 4;
   wf.FormatExt.Format.cbSize               = sizeof(wf) - sizeof(WAVEFORMATEX);
   wf.FormatExt.Samples.wValidBitsPerSample = 16;
   wf.FormatExt.dwChannelMask               = 0x3;   /* FL FR: the carrier */
   wf.FormatExt.SubFormat                   = mmdevice_SUBTYPE_IEC61937_DOLBY_DIGITAL;
   wf.dwEncodedSamplesPerSec                = *rate;
   wf.dwEncodedChannelCount                 = channels;
   wf.dwAverageBytesPerSec                  = kbps * 1000 / 8;

   hr = _IAudioClient_IsFormatSupported(client, AUDCLNT_SHAREMODE_EXCLUSIVE,
         (const WAVEFORMATEX*)&wf, NULL);
   if (hr != S_OK)
   {
      RARCH_DBG("[WASAPI] Device does not take AC-3 over IEC 61937 in exclusive mode (%s).\n",
            mmdevice_hresult_name(hr));
      RELEASE(client);
      return NULL;
   }
   RARCH_LOG("[WASAPI] Device takes AC-3 over IEC 61937: encoding %u channels at %u kbit/s, %u Hz.\n",
         channels, kbps, *rate);

   if (FAILED(wasapi_initialize_exclusive(device, &client, (const WAVEFORMATEX*)&wf, latency, *rate)))
      return NULL;
   return client;
}

/* The engine period the shared stream was opened at by the
 * IAudioClient3 path, in frames; 0 when the legacy path was taken and
 * the engine runs at its default period. Read by wasapi_init() for the
 * pump's count of what the device consumes per event. */
static unsigned wasapi_sh_engine_period = 0;

static IAudioClient *wasapi_init_client_sh(IMMDevice *device,
      bool *float_fmt, unsigned *rate, unsigned latency, unsigned channels,
      uint32_t layout, uint32_t *layout_out, bool *low_latency)
{
   wasapi_sh_engine_period = 0;
   WAVEFORMATEXTENSIBLE wf;
   IAudioClient *client           = NULL;
   settings_t *settings           = config_get_ptr();
   unsigned sh_buffer_length      = settings->uints.audio_wasapi_sh_buffer_length;
   REFERENCE_TIME default_period  = 0;
   REFERENCE_TIME buffer_duration = 0;
   HRESULT hr                     = _IMMDevice_Activate(device,
         IID_IAudioClient, CLSCTX_ALL, NULL, (void**)&client);

   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI] IMMDevice::Activate failed: %s.\n",
            mmdevice_hresult_name(hr));
      return NULL;
   }

   hr = _IAudioClient_GetDevicePeriod(client, &default_period, NULL);
   if (FAILED(hr))
      RARCH_ERR("[WASAPI] Failed to get default device period of shared client: %s.\n",
            mmdevice_hresult_name(hr));

   /* The engine buffer on the legacy path, in 100 ns units. It was the
    * whole latency setting, with a fifo the same size in front of it -
    * twice the setting in all, 128 ms for 64 - because the writer fed
    * the engine only when the fifo was full and needed the engine deep
    * to cover its frame-sized cadence. The pump feeds the engine every
    * period now; two periods of engine buffer cover it, and the fifo,
    * sized in wasapi_init(), holds the setting. */
   if (     (sh_buffer_length < WASAPI_SH_BUFFER_DEVICE_PERIOD)
         || (sh_buffer_length > WASAPI_SH_BUFFER_CLIENT_BUFFER))
      buffer_duration = default_period * 2;

   wasapi_set_format(&wf, *float_fmt, *rate, channels, layout);
   RARCH_DBG("[WASAPI] Requesting shared %u-bit %u-channel client with %s samples at %uHz %ums.\n",
         wf.Format.wBitsPerSample,
         wf.Format.nChannels,
         wasapi_wave_format_name(&wf),
         wf.Format.nSamplesPerSec,
         latency);

   if (!wasapi_select_device_format(&wf, client, AUDCLNT_SHAREMODE_SHARED, channels, layout))
   {
      RARCH_ERR("[WASAPI] Failed to select a suitable device format.\n");
      goto error;
   }

   if (low_latency)
      *low_latency = false;

#ifdef __IAudioClient3_INTERFACE_DEFINED__
   /* Windows 10 1607+: ask the engine for its minimum shared-mode
    * period instead of the default 10 ms. The write event then fires
    * at that period, and a client blocking on it is released on a
    * grid several times finer - which, with audio_sync on, is the grid
    * the whole frame loop is released on. Any failure here falls
    * through to the IAudioClient path below on a fresh client, so a
    * system that cannot do this behaves exactly as before. */
   {
      IAudioClient3 *client3 = NULL;
      hr = _IAudioClient_QueryInterface(client,
            &mmdevice_IID_IAudioClient3, (void**)&client3);
      if (SUCCEEDED(hr) && client3)
      {
         UINT32 p_default = 0, p_fundamental = 0, p_min = 0, p_max = 0;
         hr = _IAudioClient3_GetSharedModeEnginePeriod(client3,
               (WAVEFORMATEX*)&wf, &p_default, &p_fundamental, &p_min, &p_max);
         /* Every way out of this path is said at INFO: a user comparing
          * the engine buffer in the statistics overlay against what the
          * device can do needs to know why it is 10 ms and not 3. */
         if (FAILED(hr))
            RARCH_LOG("[WASAPI] IAudioClient3::GetSharedModeEnginePeriod failed: %s; using the engine's default period.\n",
                  mmdevice_hresult_name(hr));
         else if (!(p_min > 0 && p_min < p_default))
            RARCH_LOG("[WASAPI] Engine periods: default %u, fundamental %u, min %u, max %u frames; the minimum is not below the default, so the default period it is.\n",
                  p_default, p_fundamental, p_min, p_max);
         else
         {
            UINT32 period = p_min;
            RARCH_LOG("[WASAPI] Engine periods: default %u, fundamental %u, min %u, max %u frames; asking for %u.\n",
                  p_default, p_fundamental, p_min, p_max, period);
            hr = _IAudioClient3_InitializeSharedAudioStream(client3,
                  AUDCLNT_STREAMFLAGS_EVENTCALLBACK, period,
                  (WAVEFORMATEX*)&wf, NULL);
            if (hr == AUDCLNT_E_ENGINE_PERIODICITY_LOCKED)
            {
               /* Another stream holds the engine at its period: join it.
                * It is whatever it is - often the 10 ms default - but a
                * stream on the engine's actual cadence beats the legacy
                * path's assumption of one. */
               WAVEFORMATEX *cur_fmt = NULL;
               UINT32        cur     = 0;
               if (SUCCEEDED(_IAudioClient3_GetCurrentSharedModeEnginePeriod(client3, &cur_fmt, &cur)) && cur > 0)
               {
                  RARCH_LOG("[WASAPI] Engine period is locked by another stream at %u frames; joining it.\n", cur);
                  period = cur;
                  hr = _IAudioClient3_InitializeSharedAudioStream(client3,
                        AUDCLNT_STREAMFLAGS_EVENTCALLBACK, period,
                        (WAVEFORMATEX*)&wf, NULL);
               }
               if (cur_fmt)
                  CoTaskMemFree(cur_fmt);
            }
            if (SUCCEEDED(hr))
            {
               if (low_latency)
                  *low_latency = true;
               wasapi_sh_engine_period = period;
               RARCH_LOG("[WASAPI] Shared client at engine period %u frames (%.2f ms; default %u).\n",
                     period, (double)period * 1000.0 / (double)wf.Format.nSamplesPerSec,
                     p_default);
            }
            else
               RARCH_LOG("[WASAPI] IAudioClient3::InitializeSharedAudioStream at %u frames failed: %s; using the engine's default period.\n",
                     period, mmdevice_hresult_name(hr));
         }
         _IAudioClient3_Release(client3);
      }
      else
         RARCH_LOG("[WASAPI] IAudioClient3 not offered by this endpoint (%s); using the engine's default period.\n",
               mmdevice_hresult_name(hr));

      if (low_latency && *low_latency)
         goto initialized;

      /* Not taken, or failed partway: start again from a fresh client
       * so the legacy path sees exactly the object it always has. */
      RELEASE(client);
      hr = _IMMDevice_Activate(device, IID_IAudioClient, CLSCTX_ALL, NULL,
            (void**)&client);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI] IMMDevice::Activate failed: %s.\n",
               mmdevice_hresult_name(hr));
         return NULL;
      }
   }
#else
   RARCH_LOG("[WASAPI] Built without IAudioClient3 (the SDK's audioclient.h predates it); shared mode uses the engine's default period.\n");
#endif

   hr = _IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED,
         AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
         buffer_duration, 0, (WAVEFORMATEX*)&wf, NULL);

   if (hr == AUDCLNT_E_ALREADY_INITIALIZED)
   {
      RELEASE(client);

      hr     = _IMMDevice_Activate(device,
            IID_IAudioClient,
            CLSCTX_ALL, NULL, (void**)&client);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI] IMMDevice::Activate failed: %s.\n",
               mmdevice_hresult_name(hr));
         return NULL;
      }

      hr = _IAudioClient_Initialize(client, AUDCLNT_SHAREMODE_SHARED,
            AUDCLNT_STREAMFLAGS_EVENTCALLBACK,
            buffer_duration, 0, (WAVEFORMATEX*)&wf, NULL);
   }

   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI] IAudioClient::Initialize failed: %s.\n",
            mmdevice_hresult_name(hr));
      goto error;
   }

#ifdef __IAudioClient3_INTERFACE_DEFINED__
initialized:
#endif
   *float_fmt = wf.Format.wFormatTag != WAVE_FORMAT_PCM
         && memcmp(&wf.SubFormat, &KSDATAFORMAT_SUBTYPE_PCM, sizeof(GUID)) != 0;
   *rate      = wf.Format.nSamplesPerSec;
   if (layout_out)
      *layout_out = wasapi_format_layout(&wf);
   return client;

error:
   RELEASE(client);
   return NULL;
}


static IAudioClient *wasapi_init_client(IMMDevice *device, bool *exclusive,
      bool *float_fmt, unsigned *rate, unsigned latency, unsigned channels,
      uint32_t layout, uint32_t *layout_out, bool *low_latency)
{
   HRESULT hr;
   IAudioClient *client;
   float latency_res;
   REFERENCE_TIME device_period     = 0;
   REFERENCE_TIME device_period_min = 0;
   REFERENCE_TIME stream_latency    = 0;
   UINT32 buffer_length             = 0;

   if (*exclusive)
   {
      client = wasapi_init_client_ex(device, float_fmt, rate, latency, channels, layout, layout_out);
      if (!client)
      {
         RARCH_WARN("[WASAPI] Failed to initialize exclusive client, attempting shared client.\n");
         client = wasapi_init_client_sh(device, float_fmt, rate, latency, channels, layout, layout_out, low_latency);
         if (client)
            *exclusive = false;
      }
   }
   else
   {
      client = wasapi_init_client_sh(device, float_fmt, rate, latency, channels, layout, layout_out, low_latency);
      if (!client)
      {
         RARCH_WARN("[WASAPI] Failed to initialize shared client, attempting exclusive client.\n");
         client = wasapi_init_client_ex(device, float_fmt, rate, latency, channels, layout, layout_out);
         if (client)
            *exclusive = true;
      }
   }

   if (!client)
      return NULL;

   /* Remaining calls are for logging purposes. */

   hr = _IAudioClient_GetDevicePeriod(client, &device_period, &device_period_min);
   if (SUCCEEDED(hr))
   {
      RARCH_DBG("[WASAPI] Default device period is %.1fms.\n", (float)device_period * 100 / 1e6);
      RARCH_DBG("[WASAPI] Minimum device period is %.1fms.\n", (float)device_period_min * 100 / 1e6);
   }
   else
      RARCH_WARN("[WASAPI] IAudioClient::GetDevicePeriod failed: %s.\n",
            mmdevice_hresult_name(hr));

   if (!*exclusive)
   {
      hr = _IAudioClient_GetStreamLatency(client, &stream_latency);
      if (SUCCEEDED(hr))
         RARCH_DBG("[WASAPI] Shared stream latency is %.1fms.\n", (float)stream_latency * 100 / 1e6);
      else
         RARCH_WARN("[WASAPI] IAudioClient::GetStreamLatency failed: %s.\n",
               mmdevice_hresult_name(hr));
   }

   hr = _IAudioClient_GetBufferSize(client, &buffer_length);
   if (SUCCEEDED(hr))
   {
      size_t num_samples = buffer_length * channels;
      size_t num_bytes = num_samples * (*float_fmt ? sizeof(float) : sizeof(int16_t));
      RARCH_DBG("[WASAPI] Endpoint buffer size is %u frames (%u samples, %u bytes, %.1f ms).\n",
            buffer_length, num_samples, num_bytes, (float)buffer_length * 1000.0 / *rate);
   }
   else
      RARCH_WARN("[WASAPI] IAudioClient::GetBufferSize failed: %s.\n",
            mmdevice_hresult_name(hr));

   if (*exclusive)
      latency_res = (float)buffer_length * 1000.0 / (*rate);
   else
   {
      settings_t *settings      = config_get_ptr();
      unsigned sh_buffer_length = settings->uints.audio_wasapi_sh_buffer_length;

      switch (sh_buffer_length)
      {
         case WASAPI_SH_BUFFER_AUDIO_LATENCY:
         case WASAPI_SH_BUFFER_CLIENT_BUFFER:
            latency_res = (float)buffer_length * 1000.0 / (*rate);
            break;
         case WASAPI_SH_BUFFER_DEVICE_PERIOD:
            latency_res = (float)(stream_latency + device_period) / 10000.0;
            break;
         default:
            latency_res = (float)sh_buffer_length * 1000.0 / (*rate);
            break;
      }
   }

   RARCH_LOG("[WASAPI] Client initialized (%s, %s, %uHz, %.1fms).\n",
         *exclusive ? "exclusive" : "shared",
         *float_fmt ? "FLOAT" : "PCM",
         *rate, latency_res);
   return client;
}

#ifdef HAVE_MICROPHONE
typedef struct
{
   HANDLE              read_event;
   IMMDevice           *device;
   char                *device_name;
   IAudioClient        *client;
   IAudioCaptureClient *capture;

   /**
    * The buffer in which samples from the microphone will be read and stored
    * until the frontend fetches them.
    */
   fifo_buffer_t       *buffer;

   /**
    * The size of an audio frame, in bytes.
    * Mic input is in one channel with either 16-bit ints or 32-bit floats,
    * so this will be 2 or 4.
    */
   size_t frame_size;
   size_t engine_buffer_size;
   bool exclusive;
   bool running;
   /* open_mic brought COM up on its thread; close_mic releases it
    * after the last COM object.  See mmdevice_com_init(). */
   bool com_init;
} wasapi_microphone_handle_t;

typedef struct wasapi_microphone
{
   bool nonblock;
} wasapi_microphone_t;

static void wasapi_microphone_close_mic(void *driver_context, void *mic_context)
{
   DWORD ir;
   HANDLE write_event;
   wasapi_microphone_t     *wasapi = (wasapi_microphone_t*)driver_context;
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t*)mic_context;

   if (!wasapi || !mic)
      return;

   write_event = mic->read_event;

   if (mic->client)
      _IAudioClient_Stop(mic->client);

   RELEASE(mic->capture);
   RELEASE(mic->client);
   RELEASE(mic->device);
   mmdevice_com_uninit(mic->com_init);

   if (mic->buffer)
      fifo_free(mic->buffer);
   if (mic->device_name)
      free(mic->device_name);
   free(mic);

   ir = WaitForSingleObject(write_event, WASAPI_TIMEOUT);
   if (ir == WAIT_FAILED)
   {
      RARCH_ERR("[WASAPI mic] WaitForSingleObject failed: %s.\n",
            wasapi_error(GetLastError()));
   }

   /* If event isn't signaled log and leak */
   if (ir == WAIT_OBJECT_0)
      CloseHandle(write_event);
}

static void *wasapi_microphone_init(void)
{
   wasapi_microphone_t *wasapi = (wasapi_microphone_t*)calloc(1, sizeof(wasapi_microphone_t));
   if (!wasapi)
   {
      RARCH_ERR("[WASAPI mic] Failed to allocate microphone driver context.\n");
      return NULL;
   }
   wasapi->nonblock = !config_get_ptr()->bools.audio_sync;
   return wasapi;
}

static void wasapi_microphone_free(void *driver_context)
{
   wasapi_microphone_t *wasapi = (wasapi_microphone_t*)driver_context;
   if (wasapi)
      free(wasapi);
}

/**
 * Flushes microphone's most recent input to the provided context's FIFO queue.
 * WASAPI requires that fetched input be consumed in its entirety,
 * so the returned value may be less than the queue's size
 * if the next packet won't fit in it.
 * @param microphone Pointer to the microphone context.
 * @return The number of bytes in the queue after fetching input,
 * or -1 if there was an error.
 */
static int wasapi_microphone_fetch_fifo(wasapi_microphone_handle_t *mic)
{
   UINT32 next_packet_size = 0;
   /* Shared-mode capture streams split their input buffer into multiple packets,
    * while exclusive-mode capture streams just use the one.
    *
    * The following loop will run at least once;
    * for exclusive-mode streams, that's all that we'll need.
    */

   do
   {
      BYTE *mic_input           = NULL;
      UINT32 frames_read        = 0;
      UINT32 bytes_read         = 0;
      DWORD buffer_status_flags = 0;
      HRESULT hr = _IAudioCaptureClient_GetBuffer(mic->capture,
            &mic_input, &frames_read, &buffer_status_flags, NULL, NULL);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI] Failed to get capture device \"%s\"'s buffer: %s.\n",
            mic->device_name, mmdevice_hresult_name(hr));
         return -1;
      }
      bytes_read = frames_read * mic->frame_size;

      /* If the queue has room for the packets we just got,
       * then enqueue the bytes directly from the mic's buffer */
      if (FIFO_WRITE_AVAIL(mic->buffer) >= bytes_read && bytes_read > 0)
         fifo_write(mic->buffer, mic_input, bytes_read);
      else /* Not enough space for new frames, so we can't consume this packet right now */
         frames_read = 0;
      /* If there's insufficient room in the queue, then we can't read the packet.
       * In that case, we leave the packet for next time. */

      hr = _IAudioCaptureClient_ReleaseBuffer(mic->capture, frames_read);
      if (FAILED(hr))
      {
         RARCH_ERR("[WASAPI] Failed to release capture device \"%s\"'s buffer after consuming %u frames: %s.\n",
            mic->device_name, frames_read, mmdevice_hresult_name(hr));
         return -1;
      }

      /* If this is a shared-mode stream and
       * we didn't run out of room in the sample queue... */
      if (!mic->exclusive && frames_read > 0)
      {
         hr = _IAudioCaptureClient_GetNextPacketSize(mic->capture, &next_packet_size);
         /* Get the number of frames that the mic has for us. */
         if (FAILED(hr))
         {
            RARCH_ERR("[WASAPI] Failed to get capture device \"%s\"'s next packet size: %s.\n",
                      mic->device_name, mmdevice_hresult_name(hr));
            return -1;
         }
      }
      /* Exclusive-mode streams only deliver one packet at a time, though it's bigger. */
      else
         next_packet_size = 0;
   }
   while (next_packet_size != 0);
   return FIFO_READ_AVAIL(mic->buffer);
}

/**
 * Blocks until the provided microphone's capture event is signalled.
 *
 * @param microphone The microphone to wait on.
 * @param timeout The amount of time to wait, in milliseconds.
 * @return \c true if the event was signalled,
 * \c false if it timed out or there was an error.
 */
static bool wasapi_microphone_wait_for_capture_event(wasapi_microphone_handle_t *mic, DWORD timeout)
{
   /*...then let's wait for the mic to tell us that samples are ready. */
   switch (WaitForSingleObject(mic->read_event, timeout))
   {
      case WAIT_OBJECT_0:
         /* Okay, there's data available. */
         return true;
      case WAIT_TIMEOUT:
         /* Time out; there's nothing here for us. */
         RARCH_ERR("[WASAPI] Failed to wait for capture device \"%s\" event: Timeout after %ums.\n",
               mic->device_name, timeout);
         break;
      default:
         RARCH_ERR("[WASAPI] Failed to wait for capture device \"%s\" event: %s.\n",
               mic->device_name, wasapi_error(GetLastError()));
         break;
   }
   return false;
}

/**
 * Reads samples from a microphone,
 * fetching more from it if necessary.
 * Works for exclusive and shared-mode streams.
 *
 * @param microphone Pointer to the context of the microphone
 * from which samples will be read.
 * @param buffer The buffer in which the fetched samples will be stored.
 * @param buffer_size The size of buffer, in bytes.
 * @param timeout Timeout for new samples, in milliseconds.
 * 0 means that this function won't wait for new samples,
 * \c INFINITE means that this function will wait indefinitely.
 * @return The number of samples that were retrieved,
 * or -1 if there was an error (including timeout).
 */
static int wasapi_microphone_read_buffered(
   wasapi_microphone_handle_t *mic, void *s, size_t len,
   DWORD timeout)
{
   int bytes_read      = 0; /* Number of bytes sent to the core */
   int bytes_available = FIFO_READ_AVAIL(mic->buffer);

   /* If we don't have any queued samples to give to the core... */
   if (!bytes_available)
   {
      /* If we couldn't wait for the microphone to signal a capture event... */
      if (!wasapi_microphone_wait_for_capture_event(mic, timeout))
         return -1;

      bytes_available = wasapi_microphone_fetch_fifo(mic);
      /* If we couldn't fetch samples from the microphone... */
      if (bytes_available < 0)
         return -1;
   }

   /* Now that we have samples available, let's give them to the core */
   bytes_read = MIN((int)len, bytes_available);
   fifo_read(mic->buffer, s, bytes_read);
   /* Read data from the sample queue and store it in the provided buffer */
   return bytes_read;
}

/* Sleeps until the capture queue holds len bytes, then says how many it
 * holds.
 *
 * The same two steps wasapi_microphone_read() takes - wait on the
 * device's capture event, then drain what it delivered into the fifo -
 * without the copy out. Bounded by WASAPI_TIMEOUT for the reason given
 * below: an unbounded wait parks the caller with no way out if the
 * device stops signalling, and here the caller is the capture worker.
 * Exclusive and shared differ only inside fetch_fifo, as they do for
 * the read. */
static size_t wasapi_microphone_wait_readable(void *driver_context,
      void *mic_context, size_t len)
{
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t*)mic_context;
   int avail;

   if (!mic || !mic->buffer)
      return 0;

   if ((avail = (int)FIFO_READ_AVAIL(mic->buffer)) >= (int)len)
      return (size_t)avail;

   if (!wasapi_microphone_wait_for_capture_event(mic, WASAPI_TIMEOUT))
      return (avail > 0) ? (size_t)avail : 0;

   if ((avail = wasapi_microphone_fetch_fifo(mic)) < 0)
      return 0;

   return (size_t)avail;
}

static int wasapi_microphone_read(void *driver_context, void *mic_context, void *s, size_t len)
{
   int read;
   int bytes_read = 0;
   wasapi_microphone_t     *wasapi = (wasapi_microphone_t *)driver_context;
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t*)mic_context;

   if (!wasapi || !mic || !s)
      return -1;

   /* If microphones shouldn't block... */
   if (wasapi->nonblock)
      return wasapi_microphone_read_buffered(mic, s, len, 0);

   /* Both exclusive and shared modes use the same blocking read loop;
    * the distinction is handled inside wasapi_microphone_read_buffered.
    *
    * WASAPI_TIMEOUT rather than INFINITE: every other wait in this
    * driver is bounded, and an unbounded one here parks the thread the
    * core runs on with no way out if the device stops signalling its
    * capture event - an unplug, a driver reset, a format
    * renegotiation.  It also made the timeout branch of
    * wasapi_microphone_wait_for_capture_event, message and all,
    * unreachable.
    *
    * Stopping on a zero-length read matters just as much.
    * wasapi_microphone_fetch_fifo returns the queue's fill level, and
    * that is legitimately still zero when GetBuffer hands back no
    * frames, or when the packet is larger than the whole queue and
    * gets dropped - which is reachable whenever the shared-buffer
    * length setting is smaller than one device packet.  The old loop
    * only ever left on error or on a full read, so either case span
    * forever on an event that kept firing while no data ever arrived.
    *
    * A short count is not a failure the caller has to guess at:
    * microphone_driver_flush treats <= 0 as "no frames this time" and
    * microphone_driver_read counts the stalls, substituting silence
    * once they pile up.  Looping here defeated that.  Errors are still
    * reported as errors, but only when nothing was read at all -
    * otherwise the bytes already collected are worth more to the core
    * than the error code. */
   while ((size_t)bytes_read < len)
   {
      read = wasapi_microphone_read_buffered(mic,
            (char *)s   + bytes_read,
            len         - bytes_read,
            WASAPI_TIMEOUT);

      if (read < 0)
         return (bytes_read > 0) ? bytes_read : -1;
      if (read == 0)
         break;

      bytes_read += read;
   }
   return bytes_read;
}

static void wasapi_microphone_set_nonblock_state(void *driver_context, bool nonblock)
{
   wasapi_microphone_t *wasapi = (wasapi_microphone_t*)driver_context;
   wasapi->nonblock = nonblock;
}

static void *wasapi_microphone_open_mic(void *driver_context, const char *device, unsigned rate,
      unsigned latency, unsigned *new_rate)
{
   HRESULT hr;
   settings_t *settings            = config_get_ptr();
   DWORD flags                     = 0;
   UINT32 frame_count              = 0;
   REFERENCE_TIME dev_period       = 0;
   BYTE *dest                      = NULL;
   bool float_format               = settings->bools.microphone_wasapi_float_format;
   bool exclusive_mode             = settings->bools.microphone_wasapi_exclusive_mode;
   unsigned sh_buffer_length       = settings->uints.microphone_wasapi_sh_buffer_length;
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t*)calloc(
         1, sizeof(wasapi_microphone_handle_t));

   if (!mic)
      return NULL;

   mic->exclusive         = exclusive_mode;
   mic->com_init          = mmdevice_com_init();
   mic->device            = (IMMDevice*)mmdevice_init_device(device, 1 /* eCapture */);

   /* If we requested a particular capture device, but couldn't open it... */
   if (device && !mic->device)
   {
      RARCH_WARN("[WASAPI] Failed to open requested capture device \"%s\", attempting to open default device.\n",
            device);
      mic->device = (IMMDevice*)mmdevice_init_device(NULL, 1 /* eCapture */);
   }

   if (!mic->device)
   {
      RARCH_ERR("[WASAPI] Failed to open capture device.\n");
      goto error;
   }

   if (!(mic->device_name = mmdevice_name(mic->device)))
   {
      RARCH_ERR("[WASAPI] Failed to get friendly name of capture device.\n");
      goto error;
   }

   mic->client = wasapi_init_client(mic->device,
      &mic->exclusive, &float_format, &rate, latency, 1,
      AUDIO_LAYOUT_STEREO, NULL, NULL);
   if (!mic->client)
   {
      RARCH_ERR("[WASAPI] Failed to open client for capture device \"%s\".\n", mic->device_name);
      goto error;
   }

   hr = _IAudioClient_GetBufferSize(mic->client, &frame_count);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI] Failed to get buffer size of IAudioClient for capture device \"%s\": %s.\n",
          mic->device_name, mmdevice_hresult_name(hr));
      goto error;
   }

   mic->frame_size         = float_format ? sizeof(float) : sizeof(int16_t);
   mic->engine_buffer_size = frame_count * mic->frame_size;

   /* If this mic should be used *exclusively* by RetroArch... */
   if (mic->exclusive)
   {
      mic->buffer = fifo_new(mic->engine_buffer_size);
      if (!mic->buffer)
      {
         RARCH_ERR("[WASAPI] Failed to initialize FIFO queue for capture device.\n");
         goto error;
      }

      RARCH_LOG("[WASAPI] Intermediate exclusive-mode capture buffer length is %u frames (%.1fms, %u bytes).\n",
                frame_count, (double)frame_count * 1000.0 / rate, mic->engine_buffer_size);
   }
   else
   {
      /* If the user selected the "default" shared buffer length... */
      if (sh_buffer_length <= 0)
      {
         hr = _IAudioClient_GetDevicePeriod(mic->client, &dev_period, NULL);
         if (FAILED(hr))
            goto error;

         sh_buffer_length = (dev_period * rate / 10000000) * 2;
         /* Default buffer seems to be too small, resulting in slowdown.
          * Doubling it seems to work okay. Dunno why. */
      }

      mic->buffer = fifo_new(sh_buffer_length * mic->frame_size);
      if (!mic->buffer)
         goto error;

      RARCH_LOG("[WASAPI] Intermediate shared-mode capture buffer length is %u frames (%.1fms, %u bytes).\n",
                sh_buffer_length, (double)sh_buffer_length * 1000.0 / rate,
                sh_buffer_length * mic->frame_size);
   }

   if (!(mic->read_event = CreateEventA(NULL, FALSE, FALSE, NULL)))
   {
      RARCH_ERR("[WASAPI] Failed to allocate capture device's event handle.\n");
      goto error;
   }

   hr = _IAudioClient_SetEventHandle(mic->client, mic->read_event);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI] Failed to set capture device's event handle: %s.\n",
            mmdevice_hresult_name(hr));
      goto error;
   }

   hr = _IAudioClient_GetService(mic->client,
         IID_IAudioCaptureClient, (void**)&mic->capture);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI] Failed to get capture device's IAudioCaptureClient service: %s.\n",
            mmdevice_hresult_name(hr));
      goto error;
   }

   /* Get and release the buffer, just to ensure that we can. */
   hr = _IAudioCaptureClient_GetBuffer(mic->capture, &dest, &frame_count, &flags, NULL, NULL);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI] Failed to get capture client buffer: %s.\n",
            mmdevice_hresult_name(hr));
      goto error;
   }

   hr = _IAudioCaptureClient_ReleaseBuffer(mic->capture, 0);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI] Failed to release capture client buffer: %s.\n",
            mmdevice_hresult_name(hr));
      goto error;
   }

   /* The rate was (possibly) modified when we initialized the client */
   if (new_rate)
      *new_rate = rate;
   return mic;

error:
   RELEASE(mic->capture);
   RELEASE(mic->client);
   RELEASE(mic->device);
   mmdevice_com_uninit(mic->com_init);

   if (mic->read_event)
      CloseHandle(mic->read_event);
   if (mic->buffer)
      fifo_free(mic->buffer);
   if (mic->device_name)
      free(mic->device_name);
   free(mic);
   return NULL;
}

static bool wasapi_microphone_start_mic(void *driver_context, void *mic_context)
{
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t*)mic_context;
   HRESULT hr;
   if (!mic)
      return false;
   hr = _IAudioClient_Start(mic->client);

   /* Starting an already-active microphone is not an error */
   if (SUCCEEDED(hr) || hr == AUDCLNT_E_NOT_STOPPED)
      mic->running = true;
   else
   {
      RARCH_ERR("[WASAPI mic] Failed to start capture device \"%s\"'s IAudioClient: %s.\n",
         mic->device_name, mmdevice_hresult_name(hr));
      mic->running = false;
   }
   return mic->running;
}

static bool wasapi_microphone_stop_mic(void *driver_context, void *mic_context)
{
   HRESULT hr;
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t*)mic_context;
   if (!mic)
      return false;
   hr = _IAudioClient_Stop(mic->client);
   if (FAILED(hr))
   {
      RARCH_ERR("[WASAPI mic] Failed to stop capture device \"%s\"'s IAudioClient: %s.\n",
         mic->device_name, mmdevice_hresult_name(hr));
      return false;
   }
   RARCH_LOG("[WASAPI mic] Stopped capture device \"%s\".\n", mic->device_name);
   mic->running = false;
   return true;
}

static bool wasapi_microphone_mic_alive(const void *driver_context, const void *mic_context)
{
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t *)mic_context;
   return mic && mic->running;
}

static struct string_list *wasapi_microphone_device_list_new(const void *driver_context)
{
   return (struct string_list*)mmdevice_list_new(driver_context, 1 /* eCapture */);
}

static void wasapi_microphone_device_list_free(const void *driver_context, struct string_list *devices)
{
   struct string_list *sl = (struct string_list*)devices;
   if (sl)
      string_list_free(sl);
}

static bool wasapi_microphone_use_float(const void *driver_context, const void *mic_context)
{
   wasapi_microphone_handle_t *mic = (wasapi_microphone_handle_t *)mic_context;
   return (mic && (mic->frame_size == sizeof(float)));
}

microphone_driver_t microphone_wasapi = {
      wasapi_microphone_init,
      wasapi_microphone_free,
      wasapi_microphone_read,
      wasapi_microphone_set_nonblock_state,
      "wasapi",
      wasapi_microphone_device_list_new,
      wasapi_microphone_device_list_free,
      wasapi_microphone_open_mic,
      wasapi_microphone_close_mic,
      wasapi_microphone_mic_alive,
      wasapi_microphone_start_mic,
      wasapi_microphone_stop_mic,
      wasapi_microphone_use_float,
      wasapi_microphone_wait_readable
};
#endif

#ifdef HAVE_THREADS
static bool wasapi_pump_start(wasapi_t *w);
static void wasapi_pump_stop(wasapi_t *w);
#endif

static void *wasapi_init(const char *dev_id, unsigned rate, unsigned latency,
      unsigned *new_rate)
{
   HRESULT hr;
   UINT32 frame_count        = 0;
   REFERENCE_TIME dev_period = 0;
   BYTE *dest                = NULL;
   settings_t *settings      = config_get_ptr();
   bool float_format         = (settings->uints.audio_format_negotiation
         == AUDIO_FORMAT_NEGOTIATION_FLOAT);
   bool exclusive_mode       = settings->bools.audio_wasapi_exclusive_mode;
   bool audio_sync           = settings->bools.audio_sync;
   unsigned sh_buffer_length = settings->uints.audio_wasapi_sh_buffer_length;
   bool low_latency          = false;
   uint32_t layout           = AUDIO_LAYOUT_STEREO;
   unsigned req_rate         = rate;
   wasapi_t *w               = (wasapi_t*)calloc(1, sizeof(wasapi_t));

   if (!w)
      return NULL;

   retro_atomic_size_init(&w->underruns, 0);
   if (mmdevice_com_init())
      w->flags              |= WASAPI_FLG_COM;
   w->device                 = (IMMDevice*)mmdevice_init_device(dev_id, 0 /* eRender */);
   if (!w->device && dev_id)
      w->device              = (IMMDevice*)mmdevice_init_device(NULL, 0 /* eRender */);
   if (!w->device)
      goto error;

   /* The layout the frontend asked for; a device that will not open
    * with it gets stereo, and the frontend learns which from the
    * layout hook. */
   layout    = audio_driver_requested_layout();
   w->layout = AUDIO_LAYOUT_STEREO;
   if (layout == AUDIO_LAYOUT_STEREO)
      w->client = wasapi_init_client(w->device,
            &exclusive_mode, &float_format, &rate, latency, 2,
            AUDIO_LAYOUT_STEREO, NULL, &low_latency);
   else
   {
      /* A wider layout, in the preferred mode first; a device that
       * will not open with it in that mode gets stereo in that mode
       * before the other mode is tried with either, so a refused
       * layout costs the layout and never the mode - exclusive mode
       * is the latency, and the layout is not worth it. */
      bool     want_ex  = exclusive_mode;
      unsigned ch       = audio_layout_channels(layout);
      bool     f0       = float_format;
      unsigned pass;
      for (pass = 0; pass < 4 && !w->client; pass++)
      {
         bool     ex   = (pass < 2) ? want_ex : !want_ex;
         uint32_t lay  = (pass & 1) ? AUDIO_LAYOUT_STEREO : layout;
         unsigned chs  = (pass & 1) ? 2 : ch;
         uint32_t *lo  = (pass & 1) ? NULL : &w->layout;
         float_format  = f0;
         rate          = req_rate;
         w->layout     = AUDIO_LAYOUT_STEREO;
         w->client     = ex
               ? wasapi_init_client_ex(w->device, &float_format, &rate, latency, chs, lay, lo)
               : wasapi_init_client_sh(w->device, &float_format, &rate, latency, chs, lay, lo, &low_latency);
         /* A wider layout refused as PCM in exclusive mode is tried
          * as AC-3 over IEC 61937 before stereo: a TV or receiver that
          * advertises stereo PCM only, on HDMI or S/PDIF, usually
          * decodes Dolby Digital, and the layout survives as a
          * bit stream. A/52 takes 1.0 to 5.1; a 7.1 request goes as
          * 5.1, its back pair folded into the surround pair by the
          * upmix to what the layout hook reports. */
         if (!w->client && ex && !(pass & 1))
         {
            uint32_t enc_layout = layout;
            unsigned enc_ch;
            if (enc_layout & 0x030u)   /* BL BR: fold onto SL SR */
               enc_layout = (enc_layout & ~0x030u) | 0x600u;
            if (rac3_layout_acmod(enc_layout) < 0)
               enc_layout = (enc_layout & 0x008u) ? 0x60Fu : 0x607u;
            enc_ch = audio_layout_channels(enc_layout);
            w->client = wasapi_init_client_ac3(w->device, &rate, latency, enc_ch,
                  enc_ch >= 5 ? 640 : enc_ch >= 3 ? 448 : 256);
            if (w->client)
            {
               w->ac3 = rac3_encoder_new(rate, enc_layout, enc_ch >= 5 ? 640 : enc_ch >= 3 ? 448 : 256);
               w->ac3_in = (float*)calloc((size_t)1536 * enc_ch, sizeof(float));
               if (!w->ac3 || !w->ac3_in)
               {
                  RELEASE(w->client);
                  w->client = NULL;
               }
               else
               {
                  w->layout       = enc_layout;
                  w->ac3_frame_size = enc_ch * sizeof(float);
                  float_format    = false;   /* the carrier: 16-bit stereo */
                  RARCH_LOG("[WASAPI] Layout 0x%03x goes to the device as AC-3 over IEC 61937 (the device takes stereo PCM only).\n",
                        w->layout);
               }
            }
         }
         if (w->client)
         {
            exclusive_mode = ex;
            if (pass & 1)
               RARCH_WARN("[WASAPI] Device would not open with layout 0x%03x in %s mode; opened stereo in it instead.\n",
                     layout, ex ? "exclusive" : "shared");
            else if (pass)
               RARCH_WARN("[WASAPI] Device would not open in %s mode; opened layout 0x%03x in %s mode instead.\n",
                     want_ex ? "exclusive" : "shared", w->layout, ex ? "exclusive" : "shared");
         }
      }
      if (w->client)
         RARCH_LOG("[WASAPI] Client initialized (%s).\n", exclusive_mode ? "exclusive" : "shared");
   }
   if (!w->client)
      goto error;
   if (exclusive_mode)
      w->flags              |= WASAPI_FLG_EXCLUSIVE;
   w->pump_exclusive = exclusive_mode;
   if (low_latency)
      w->flags              |= WASAPI_FLG_LOWLAT;

   /* Shared: frames the engine takes per event, for the pump's count of
    * what the device consumed - the IAudioClient3 period when that path
    * opened the stream, the default period otherwise. */
   if (!(w->flags & WASAPI_FLG_EXCLUSIVE))
   {
      REFERENCE_TIME def_period = 0;
      w->sh_period_frames = wasapi_sh_engine_period;
      if (!w->sh_period_frames
            && SUCCEEDED(_IAudioClient_GetDevicePeriod(w->client, &def_period, NULL))
            && def_period > 0)
         w->sh_period_frames = (unsigned)(def_period * rate / 10000000);
   }

   hr = _IAudioClient_GetBufferSize(w->client, &frame_count);
   if (FAILED(hr))
      goto error;

   w->float_format           = float_format;
   w->frame_size             = (unsigned char)(audio_layout_channels(w->layout)
         * (float_format ? sizeof(float) : sizeof(int16_t)));
   /* AC-3: the device's frame is the carrier's, 2 channels of 16 bits,
    * whatever layout the encoder takes */
   if (w->ac3)
      w->frame_size          = 4;
   w->engine_buffer_size     = frame_count * w->frame_size;

   if (w->flags & WASAPI_FLG_EXCLUSIVE)
   {
      /* The fifo holds the latency setting, as the other drivers'
       * buffers do, and never less than two engine buffers. It was one
       * engine buffer, when the engine buffer was the whole setting;
       * now the period is a quarter of it, this is where the setting
       * lives, what buffer_size() reports, and what the pump thread
       * feeds the device from. */
      size_t fifo_bytes = ((size_t)rate * latency / 1000) * w->frame_size;
      if (fifo_bytes < (size_t)w->engine_buffer_size * 2)
         fifo_bytes     = (size_t)w->engine_buffer_size * 2;
      /* AC-3 arrives a whole burst - 32 ms - at a time, encoded on the
       * writer's thread when its 1536 frames are in: the fifo holds
       * four, so a burst's encode and the writer's own pace never find
       * it empty, and rate control sees the fill in steps of a burst
       * against a span of four. A TV or receiver decoding the stream
       * adds its own latency to this in any case. */
      if (w->ac3 && fifo_bytes < (size_t)IEC61937_AC3_BURST_BYTES * 4 + 1)
         fifo_bytes     = (size_t)IEC61937_AC3_BURST_BYTES * 4 + 1;
      if (!(w->buffer = fifo_new(fifo_bytes)))
         goto error;
      RARCH_LOG("[WASAPI] Exclusive: %u ms setting as a %u-frame fifo (%u ms, rate control holds it about half full) in front of a %u-frame device period (%.1f ms); about %u ms from write to the device.\n",
            latency,
            (unsigned)(fifo_bytes / w->frame_size),
            (unsigned)(fifo_bytes / w->frame_size * 1000 / rate),
            frame_count,
            (float)frame_count * 1000.0f / rate,
            (unsigned)(fifo_bytes / w->frame_size * 1000 / rate / 2
               + frame_count * 1000 / rate));
      /* With audio sync off the frame-synchronous writer does not
       * wait: the fifo must take a whole frame of core audio at once -
       * 16.7 ms at 60 fps - or the remainder is dropped, every frame.
       * The threaded pipeline's writer hands over at most half the
       * buffer a pass and keeps the rest in its ring, so it drops
       * nothing at any size. */
      if (     !settings->bools.audio_sync
            && !settings->bools.audio_threaded_pipeline
            && fifo_bytes / w->frame_size * 1000 / rate < 20)
         RARCH_WARN("[WASAPI] Audio sync is off and the %u ms buffer holds less than one frame of audio at 60 fps; a latency setting of 20 ms or more avoids dropping the remainder each frame.\n",
               (unsigned)(fifo_bytes / w->frame_size * 1000 / rate));
   }
   else
   {
      switch (sh_buffer_length)
      {
         case WASAPI_SH_BUFFER_AUDIO_LATENCY:
         case WASAPI_SH_BUFFER_CLIENT_BUFFER:
            /* Under IAudioClient3 the engine sizes its own buffer to the
             * small period, so frame_count is a few ms and no longer
             * reflects audio_latency. Keep the FIFO at the latency the
             * user asked for; the engine buffer is only the drain step. */
            /* The setting less the engine buffer in front of it, never
             * under two engine buffers: the writer's burst - a frame of
             * core audio, with audio sync off - must fit. On the legacy
             * path the engine is two 10 ms periods, so a 64 ms setting
             * is a 44 ms fifo and reads as 64; under IAudioClient3 the
             * engine is a few ms and the fifo is nearly the setting. */
            {
               /* The floor: what the writer's burst needs. The writer
                * hands the fifo a whole frame of core audio at a time
                * and the pump only frees room once per period.
                *
                * A writer that blocks inside retro_run() - audio sync
                * on, the frame-synchronous pipeline - gets a frame at
                * 50 fps plus a period, the room the pump frees while
                * the frame is written, and never under the engine
                * buffer: fifo_new() keeps a byte and rate control
                * stretches the chunk, so a fifo of exactly one frame
                * would make it wait a period on every frame.
                *
                * A writer that cannot stall a frame - the threaded
                * pipeline's audio thread, or any writer with audio sync
                * off - needs only the frame to fit: one at 50 fps, plus
                * the 2% rate control can stretch it, plus a frame of
                * slack, never under two periods.
                *
                * Periods, not engine buffers: the engine buffer is two
                * periods on the legacy path, and a floor of two of
                * those would swallow every setting below 44 ms. */
               unsigned period_frames = 0;
               unsigned floor_frames;
               bool writer_blocks_frame = audio_sync
                     && !settings->bools.audio_threaded_pipeline;
               /* The period the engine actually runs at - the one its
                * events follow - is what the pump counts consumed by.
                * Under IAudioClient3 that may be below the default, and
                * it is recorded in wasapi_sh_engine_period when that
                * path opened the stream; GetDevicePeriod() only knows
                * the default. */
               if (wasapi_sh_engine_period)
                  period_frames = wasapi_sh_engine_period;
               else
               {
                  hr = _IAudioClient_GetDevicePeriod(w->client, &dev_period, NULL);
                  if (SUCCEEDED(hr) && dev_period > 0)
                     period_frames = (unsigned)(dev_period * rate / 10000000);
               }
               if (writer_blocks_frame)
               {
                  floor_frames = rate / 50 + period_frames;
                  if (floor_frames < frame_count)
                     floor_frames = frame_count;
               }
               else
                  floor_frames = rate / 50 + rate / 2000 + 1;
               if (floor_frames < period_frames * 2)
                  floor_frames = period_frames * 2;
               sh_buffer_length = (unsigned)(((uint64_t)latency * rate) / 1000);
               if (sh_buffer_length > frame_count + floor_frames)
                  sh_buffer_length -= frame_count;
               else
                  sh_buffer_length = floor_frames;
            }
            break;
         case WASAPI_SH_BUFFER_DEVICE_PERIOD:
            hr = _IAudioClient_GetDevicePeriod(w->client, &dev_period, NULL);

            if (FAILED(hr))
               goto error;

            sh_buffer_length = dev_period * rate / 10000000;
            break;
         default:
            break;
      }

      if (!(w->buffer = fifo_new(sh_buffer_length * w->frame_size)))
         goto error;
      RARCH_LOG("[WASAPI] Shared: %u ms setting as a %u-frame fifo (%u ms) in front of a %u-frame engine buffer (%u ms) fed a %s-frame period at a time; %u ms in all, rate control holds the fifo about half full.\n",
            latency, sh_buffer_length, (unsigned)((uint64_t)sh_buffer_length * 1000 / rate),
            frame_count, (unsigned)((uint64_t)frame_count * 1000 / rate),
            (w->flags & WASAPI_FLG_LOWLAT) ? "small" : "480",
            (unsigned)(((uint64_t)sh_buffer_length + frame_count) * 1000 / rate));
   }

#ifdef HAVE_THREADS
   /* The fifo is shared between the writer and the pump, in both
    * modes. */
   w->fifo_lock = slock_new();
   w->room_cond = scond_new();
   if (!w->fifo_lock || !w->room_cond)
      goto error;
#endif

   if (!(w->write_event = CreateEventA(NULL, FALSE, FALSE, NULL)))
      goto error;

   hr = _IAudioClient_SetEventHandle(w->client, w->write_event);
   if (FAILED(hr))
      goto error;

   hr = _IAudioClient_GetService(w->client,
         IID_IAudioRenderClient, (void**)&w->renderer);
   if (FAILED(hr))
      goto error;

   hr = _IAudioRenderClient_GetBuffer(w->renderer, frame_count, &dest);
   if (FAILED(hr))
      goto error;

   hr = _IAudioRenderClient_ReleaseBuffer(
         w->renderer, frame_count,
         AUDCLNT_BUFFERFLAGS_SILENT);
   if (FAILED(hr))
      goto error;

#ifdef HAVE_THREADS
   /* The pump beside the client, as start() brings them up together.
    * The synchronous path never calls start(): audio_driver_init()
    * leaves that to the runloop, which only issues it around pause,
    * menu and thread-wait transitions. With the client running and no
    * pump the fifo filled and nothing drained - silence, with the
    * threaded pipeline off, since the pump replaced the writer feeding
    * the device itself. */
   if (!wasapi_pump_start(w))
   {
      RARCH_ERR("[WASAPI] Failed to start the pump thread.\n");
      goto error;
   }
#endif
   hr = _IAudioClient_Start(w->client);
   if (FAILED(hr))
      goto error;

   w->flags    |=   WASAPI_FLG_RUNNING;
   if (audio_sync)
      w->flags &= ~WASAPI_FLG_NONBLOCK;
   else
      w->flags |=  (WASAPI_FLG_NONBLOCK);

   if (new_rate)
      *new_rate = rate;

   /* The device stage behind what buffer_size() reports, for the
    * statistics overlay. Exclusive: buffer_size() is the fifo, and the
    * endpoint buffer - one period, event-driven - is the device's own.
    * Shared: buffer_size() spans fifo and engine buffer already, so
    * what remains is the engine's own latency between that buffer and
    * the endpoint, which GetStreamLatency gives. */
   {
      size_t device_frames = 0;
      if (w->flags & WASAPI_FLG_EXCLUSIVE)
         device_frames = frame_count;
      else
      {
         REFERENCE_TIME stream_latency = 0;
         if (SUCCEEDED(_IAudioClient_GetStreamLatency(w->client, &stream_latency)))
            device_frames = (size_t)((uint64_t)stream_latency * rate / 10000000);
      }
      audio_driver_set_device_latency(device_frames);
   }

   if (!wasapi_imm_start_thread(w))
      goto error;

   return w;

error:
#ifdef HAVE_THREADS
   /* A pump that came up before Start failed must be joined before
    * the event and the client it waits on go away. */
   wasapi_pump_stop(w);
#endif
   RELEASE(w->renderer);
   RELEASE(w->client);
   RELEASE(w->device);
   mmdevice_com_uninit((w->flags & WASAPI_FLG_COM) != 0);

   if (w->write_event)
      CloseHandle(w->write_event);
   if (w->buffer)
      fifo_free(w->buffer);
   if (w->ac3)
      rac3_encoder_free(w->ac3);
   if (w->ac3_in)
      free(w->ac3_in);
#ifdef HAVE_THREADS
   if (w->room_cond)
      scond_free(w->room_cond);
   if (w->fifo_lock)
      slock_free(w->fifo_lock);
#endif
   free(w);
   return NULL;
}

#ifdef HAVE_THREADS
/* Exclusive mode, event-driven: the endpoint buffer is one period, the
 * device raises write_event when it wants the next, and an event
 * nobody answers is a period of silence. The write path used to
 * answer it, and only when the fifo was full - which was every period
 * while the fifo was one engine buffer, and one period in four once
 * it held the setting; with audio sync off the writer does not wait at
 * all, and between its calls nothing answered. This thread answers
 * every one: a period from the fifo when one is there, silence and an
 * underrun counted when not. */
static bool wasapi_push_sh(wasapi_t *w);

static void wasapi_pump_thread(void *data)
{
   wasapi_t *w = (wasapi_t*)data;
   /* A period late is a period of silence; above normal priority, as
    * the wrapper's audio thread runs. */
   SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_TIME_CRITICAL);
   while (retro_atomic_load_acquire_int(&w->pump_run))
   {
      if (WaitForSingleObject(w->write_event, WASAPI_TIMEOUT) != WAIT_OBJECT_0)
         continue;
      if (!retro_atomic_load_acquire_int(&w->pump_run))
         break;
      if (w->pump_exclusive)
      {
         BYTE *dest         = NULL;
         UINT32 frame_count = (UINT32)(w->engine_buffer_size / w->frame_size);
         DWORD  flags       = 0;
         if (FAILED(_IAudioRenderClient_GetBuffer(w->renderer, frame_count, &dest)))
            continue;
         slock_lock(w->fifo_lock);
         if (FIFO_READ_AVAIL(w->buffer) >= w->engine_buffer_size)
         {
            fifo_read(w->buffer, dest, w->engine_buffer_size);
            wasapi_ac3_drained(w, w->engine_buffer_size);
         }
         else
         {
            memset(dest, 0, w->engine_buffer_size);
            flags = AUDCLNT_BUFFERFLAGS_SILENT;
            if (w->fed)
               retro_atomic_fetch_add_size(&w->underruns, 1);
         }
         /* Each period released is one the device takes; silence
          * counts too, the device's clock does not stop for it. */
         w->consumed += frame_count;
         scond_signal(w->room_cond);
         slock_unlock(w->fifo_lock);
         _IAudioRenderClient_ReleaseBuffer(w->renderer, frame_count, flags);
      }
      else
      {
         /* Shared: the engine signals each period; move what it has
          * room for. Under the legacy 10 ms engine with a buffer the
          * size of the setting the writer alone kept it fed; under the
          * IAudioClient3 engine at 3 ms the buffer is three periods and
          * the writer's frame-sized cadence starved it - 46% of periods,
          * in the harness - so the pump feeds it here too. */
         slock_lock(w->fifo_lock);
         wasapi_push_sh(w);
         /* The engine signals once per period and takes one period per
          * signal, our audio or its own silence: the event is the
          * device's clock. Released less padding stopped counting
          * during an underrun, when the engine plays silence of its
          * own, and read the same pin 400 to 1000 ppm slow that
          * exclusive read at +11. */
         w->consumed += w->sh_period_frames;
         scond_signal(w->room_cond);
         slock_unlock(w->fifo_lock);
      }
   }
}

static bool wasapi_pump_start(wasapi_t *w)
{
   if (w->pump)
      return true;
   retro_atomic_store_release_int(&w->pump_run, 1);
   w->pump     = sthread_create(wasapi_pump_thread, w);
   if (!w->pump)
   {
      retro_atomic_store_release_int(&w->pump_run, 0);
      return false;
   }
   return true;
}

static void wasapi_pump_stop(wasapi_t *w)
{
   if (!w->pump)
      return;
   retro_atomic_store_release_int(&w->pump_run, 0);
   /* The pump waits on the device's event with a timeout, so it sees
    * the flag within WASAPI_TIMEOUT even if the device has gone quiet;
    * a raised event ends the wait sooner. */
   SetEvent(w->write_event);
   sthread_join(w->pump);
   w->pump = NULL;
}
#endif

/* Shared mode: move what the engine has room for from the fifo into
 * it. The writer used to do this only when the fifo was full, so a
 * fifo bigger than the writer's burst - the latency setting, against
 * a frame of core audio - filled for several frames while the engine
 * ran dry; a fifo of exactly one frame was the only size that worked
 * with audio sync off, which is what a reporter found by hand. Now on
 * every write. Returns false on a render-client failure. */
static bool wasapi_push_sh(wasapi_t *w)
{
   UINT32 padding    = 0;
   size_t read_avail, engine_free, n;
   if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
      return false;
   read_avail  = FIFO_READ_AVAIL(w->buffer);
   engine_free = w->engine_buffer_size - padding * w->frame_size;
   n           = read_avail < engine_free ? read_avail : engine_free;
   n           = (n / w->frame_size) * w->frame_size;
   if (n)
   {
      BYTE *dest         = NULL;
      UINT32 frame_count = (UINT32)(n / w->frame_size);
      if (FAILED(_IAudioRenderClient_GetBuffer(w->renderer, frame_count, &dest)))
         return false;
      fifo_read(w->buffer, dest, n);
      if (FAILED(_IAudioRenderClient_ReleaseBuffer(w->renderer, frame_count, 0)))
         return false;
#ifdef HAVE_THREADS
      w->released += frame_count;
#endif
   }
   return true;
}

static ssize_t wasapi_write_raw(wasapi_t *w, const void *data, size_t len);


/* AC-3: float frames of the layout in, bursts to the fifo. Frames are
 * collected until a syncframe's worth, encoded, wrapped, and the
 * burst written to the fifo through the raw path, which blocks or
 * not as the mode says; a burst that does not fit in non-blocking
 * mode is the frames that are dropped. Returns the input bytes
 * taken, which is all of them unless a burst was dropped. */
static ssize_t wasapi_write_ac3(wasapi_t *w, const void *data, size_t len)
{
   const uint8_t *src = (const uint8_t*)data;
   size_t frames      = len / w->ac3_frame_size, done = 0;
   while (done < frames)
   {
      size_t take = 1536 - w->ac3_in_frames;
      if (take > frames - done)
         take = frames - done;
      /* A burst goes whole or not at all: part of one in the stream
       * puts the receiver out of sync until the next preamble. Without
       * blocking, the frames that would complete a burst are not taken
       * until the fifo has room for it; the caller offers them again. */
      if ((w->flags & WASAPI_FLG_NONBLOCK) && w->ac3_in_frames + take == 1536)
      {
         size_t room;
#ifdef HAVE_THREADS
         slock_lock(w->fifo_lock);
         room = FIFO_WRITE_AVAIL(w->buffer);
         slock_unlock(w->fifo_lock);
#else
         room = FIFO_WRITE_AVAIL(w->buffer);
#endif
         if (room < IEC61937_AC3_BURST_BYTES)
            break;
      }
      memcpy((uint8_t*)w->ac3_in + (size_t)w->ac3_in_frames * w->ac3_frame_size,
            src + done * w->ac3_frame_size, take * w->ac3_frame_size);
      w->ac3_in_frames += (unsigned)take;
      done             += take;
#ifdef HAVE_THREADS
      slock_lock(w->fifo_lock);
      w->ac3_inflight  += take;
      slock_unlock(w->fifo_lock);
#else
      w->ac3_inflight  += take;
#endif
      if (w->ac3_in_frames == 1536)
      {
         size_t n = rac3_encode_frame(w->ac3, w->ac3_in, w->ac3_frame, sizeof(w->ac3_frame));
         size_t b = n ? iec61937_wrap_ac3(w->ac3_frame, n, 0, w->ac3_burst, sizeof(w->ac3_burst)) : 0;
         ssize_t written = b ? wasapi_write_raw(w, w->ac3_burst, b) : -1;
         w->ac3_in_frames = 0;
         if (written < 0)
            return -1;
         if ((size_t)written < b)
         {
            /* Cut short: only the pump takes room and the room was
             * checked, so this is a blocking write that gave up (the
             * pump stopped, or the bounded wait ran out). The stream
             * has part of a burst; the receiver resyncs at the next
             * preamble. Counted with the underruns for the overlay. */
#ifdef HAVE_THREADS
            retro_atomic_fetch_add_size(&w->underruns, 1);
#endif
            return (ssize_t)(done * w->ac3_frame_size);
         }
      }
   }
   return (ssize_t)(done * w->ac3_frame_size);
}

static ssize_t wasapi_write(void *wh, const void *data, size_t len)
{
   wasapi_t *w = (wasapi_t*)wh;
   if (!(w->flags & WASAPI_FLG_RUNNING))
      return -1;
   if (w->ac3)
      return wasapi_write_ac3(w, data, len);
   return wasapi_write_raw(w, data, len);
}

static ssize_t wasapi_write_raw(wasapi_t *w, const void *data, size_t len)
{
   size_t _len = 0;
   uint8_t flg = w->flags;

#ifdef HAVE_THREADS
   {
      /* Both modes: the fifo only; the pump feeds the device.
       * Non-blocking: what fits. Blocking: wait for the pump to free
       * room, bounded as every wait here is. */
      int laps = 0;
      slock_lock(w->fifo_lock);
      while (_len < len)
      {
         size_t room = FIFO_WRITE_AVAIL(w->buffer);
         size_t ir   = len - _len;
         if (!room)
         {
            if (flg & WASAPI_FLG_NONBLOCK)
               break;
            if (!retro_atomic_load_acquire_int(&w->pump_run) || ++laps > WASAPI_TIMEOUT)
               break;
            scond_wait_timeout(w->room_cond, w->fifo_lock, 1000);
            continue;
         }
         if (ir > room)
            ir = room;
         fifo_write(w->buffer, (const char*)data + _len, ir);
         w->fed = true;
         _len += ir;
      }
      slock_unlock(w->fifo_lock);
   }
#else
   if (flg & WASAPI_FLG_EXCLUSIVE)
   {
      /* Without threads there is no pump: the old, writer-paced path. */
      while (_len < len)
      {
         size_t ir;
         size_t write_avail = FIFO_WRITE_AVAIL(w->buffer);
         if (!write_avail)
         {
            BYTE *dest         = NULL;
            UINT32 frame_count = w->engine_buffer_size / w->frame_size;
            if (flg & WASAPI_FLG_NONBLOCK)
               break;
            if (WaitForSingleObject(w->write_event, WASAPI_TIMEOUT) != WAIT_OBJECT_0)
               break;
            if (FAILED(_IAudioRenderClient_GetBuffer(w->renderer, frame_count, &dest)))
               return -1;
            fifo_read(w->buffer, dest, w->engine_buffer_size);
            wasapi_ac3_drained(w, w->engine_buffer_size);
            if (FAILED(_IAudioRenderClient_ReleaseBuffer(w->renderer, frame_count, 0)))
               return -1;
            write_avail = w->engine_buffer_size;
         }
         ir = (len - _len < write_avail) ? len - _len : write_avail;
         fifo_write(w->buffer, (const char*)data + _len, ir);
         _len += ir;
      }
   }
   else
   {
      if (flg & WASAPI_FLG_NONBLOCK)
      {
         size_t write_avail;
         if (!wasapi_push_sh(w))
            return -1;
         write_avail = FIFO_WRITE_AVAIL(w->buffer);
         _len = len < write_avail ? len : write_avail;
         if (_len)
            fifo_write(w->buffer, data, _len);
         /* The room the write made use of may let more reach the
          * engine now, so it does not wait for the next call. */
         if (!wasapi_push_sh(w))
            return -1;
      }
      else
      {
         while (_len < len)
         {
            size_t ir;
            size_t __len       = len - _len;
            size_t write_avail;
            if (!wasapi_push_sh(w))
               return -1;
            write_avail        = FIFO_WRITE_AVAIL(w->buffer);
            if (!write_avail)
            {
               /* The fifo and the engine are both full: wait a period
                * for the engine to take some. */
               if (!(WaitForSingleObject(w->write_event, WASAPI_TIMEOUT) == WAIT_OBJECT_0))
                  break;
               continue;
            }
            ir = (__len < write_avail) ? __len : write_avail;
            {
               const void *_data = (char*)data + _len;
               fifo_write(w->buffer, _data, ir);
               _len += ir;
            }
         }
      }
   }
#endif
   return _len;
}

static bool wasapi_stop(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;

#ifdef HAVE_THREADS
   wasapi_pump_stop(w);
#endif
   if (FAILED(_IAudioClient_Stop(w->client)))
      return (!(w->flags & WASAPI_FLG_RUNNING));

   w->flags &= ~WASAPI_FLG_RUNNING;
   return true;
}

static bool wasapi_start(void *wh, bool u)
{
   wasapi_t *w = (wasapi_t*)wh;
   HRESULT  hr;
#ifdef HAVE_THREADS
   /* The pump first, so it is waiting on the event before the device
    * raises the first one; the other way round, the first periods
    * went unanswered while the thread came up. */
   if (!wasapi_pump_start(w))
   {
      RARCH_ERR("[WASAPI] Failed to start the exclusive-mode pump thread.\n");
      return false;
   }
#endif
   hr = _IAudioClient_Start(w->client);
   if (hr != AUDCLNT_E_NOT_STOPPED)
   {
      if (FAILED(hr))
      {
#ifdef HAVE_THREADS
         wasapi_pump_stop(w);
#endif
         return (w->flags & WASAPI_FLG_RUNNING) != 0;
      }
      w->flags  |= WASAPI_FLG_RUNNING;
   }
   return true;
}

static bool wasapi_alive(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;
   return (w->flags & WASAPI_FLG_RUNNING) != 0;
}

static void wasapi_set_nonblock_state(void *wh, bool nonblock)
{
   wasapi_t *w = (wasapi_t*)wh;

   if (nonblock)
      w->flags |=  WASAPI_FLG_NONBLOCK;
   else
      w->flags &= ~WASAPI_FLG_NONBLOCK;
}

static void wasapi_free(void *wh)
{
   wasapi_t *w        = (wasapi_t*)wh;
   HANDLE write_event = w->write_event;

   if (w)
      wasapi_imm_stop_thread(w);
#ifdef HAVE_THREADS
   wasapi_pump_stop(w);
#endif
   if (w->client)
      _IAudioClient_Stop(w->client);

   RELEASE(w->renderer);
   RELEASE(w->client);
   RELEASE(w->device);
   mmdevice_com_uninit((w->flags & WASAPI_FLG_COM) != 0);

   if (w->buffer)
      fifo_free(w->buffer);
   if (w->ac3)
      rac3_encoder_free(w->ac3);
   if (w->ac3_in)
      free(w->ac3_in);
#ifdef HAVE_THREADS
   if (w->room_cond)
      scond_free(w->room_cond);
   if (w->fifo_lock)
      slock_free(w->fifo_lock);
#endif
   free(w);

   /* Nothing waits on the event once the pump is joined and the client
    * stopped: it closes unconditionally. It used to close only when a
    * wait found it signalled, and leaked otherwise. */
   if (write_event)
      CloseHandle(write_event);
}

static bool wasapi_use_float(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;
   /* The encoder takes float whatever the carrier is */
   return w->float_format || w->ac3;
}

static uint32_t wasapi_layout(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;
   return w ? w->layout : AUDIO_LAYOUT_STEREO;
}

static void *wasapi_device_list_new(void *u)
{
   return mmdevice_list_new(u, 0 /* eRender */);
}

static void wasapi_device_list_free(void *u, void *slp)
{
   struct string_list *sl = (struct string_list*)slp;

   if (sl)
      string_list_free(sl);
}

static size_t wasapi_write_avail(void *wh)
{
   wasapi_t *w    = (wasapi_t*)wh;
   UINT32 padding = 0;

   if (w->flags & WASAPI_FLG_EXCLUSIVE)
   {
      size_t room;
#ifdef HAVE_THREADS
      slock_lock(w->fifo_lock);
      room = FIFO_WRITE_AVAIL(w->buffer);
      slock_unlock(w->fifo_lock);
#else
      room = FIFO_WRITE_AVAIL(w->buffer);
#endif
      if (w->ac3)
      {
         /* In the frontend's frames: the buffer's frames less those in
          * flight - the fifo's and the encoder's, one figure kept under
          * the lock (a burst is 1536 carrier frames for 1536 layout
          * frames, so the two counts add). */
         size_t cap = (w->buffer->size - 1) / w->frame_size;
         size_t inflight;
#ifdef HAVE_THREADS
         slock_lock(w->fifo_lock);
         inflight = w->ac3_inflight;
         slock_unlock(w->fifo_lock);
#else
         inflight = w->ac3_inflight;
#endif
         return (cap > inflight ? cap - inflight : 0) * w->ac3_frame_size;
      }
      return room;
   }
#ifdef HAVE_THREADS
   {
      /* Shared, with the pump: the fifo's room under its lock, plus
       * the engine's, which only the pump fills. */
      size_t room;
      slock_lock(w->fifo_lock);
      room = FIFO_WRITE_AVAIL(w->buffer);
      slock_unlock(w->fifo_lock);
      if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
         return room;
      return room + (w->engine_buffer_size - padding * w->frame_size);
   }
#endif
   if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
      return 0;
   /* Free space across the whole pipeline the writer can fill:
    * free fifo bytes plus free engine-buffer bytes.  The previous
    * form added GetCurrentPadding() directly - a frame count
    * (1/8 scale for float frames) added to a byte count, and a
    * measure of *queued* data rather than free space, so a fuller
    * engine reported more room.  Rate control consumed that as a
    * mis-scaled, inverted, period-rate noise term in its occupancy
    * measurement. */
   return FIFO_WRITE_AVAIL(w->buffer)
         + (w->engine_buffer_size - padding * w->frame_size);
}

static size_t wasapi_buffer_size(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;

   /* Must bound what write_avail reports: exclusive mode's avail spans
    * the fifo only (the engine double buffer is the device's own), and
    * shared mode's spans fifo + engine.  Reporting only the fifo in
    * shared mode let avail exceed the "buffer size", pushing the rate
    * controller's direction term past its intended +-1 range. */
   if (w->flags & WASAPI_FLG_EXCLUSIVE)
   {
      if (w->ac3)
         return (w->buffer->size - 1) / w->frame_size * w->ac3_frame_size;
      return w->buffer->size - 1;
   }
   /* The fifo's capacity is one less than its slot count. */
   return (w->buffer->size - 1) + w->engine_buffer_size;
}

/* Sleep on the engine's event until the fifo in front of it has room.
 * The fifo only frees when a period is moved into the engine, and that
 * move is the same one wasapi_write() makes when it finds the fifo
 * full: in exclusive mode the engine takes the whole fifo, in shared
 * mode as much as its padding allows. Returns the free space
 * write_avail() would report, or 0 when the stream is not running or
 * the event never came. len is a lower bound the caller would like;
 * what is returned may be less than that when the fifo is only partly
 * free, and the write's own loop then blocks at most one period for the
 * remainder, after the fill rate control needs has been read. */
static size_t wasapi_wait_writable(void *wh, size_t len)
{
   wasapi_t *w = (wasapi_t*)wh;

   if (!(w->flags & WASAPI_FLG_RUNNING))
      return 0;

#ifdef HAVE_THREADS
   {
      /* Room comes from the pump, in either mode; wait for it, capped
       * at half the fifo so the wait always ends, bounded in time as
       * every wait here. */
      size_t room;
      int laps = 0;
      if (len > (w->buffer->size - 1) / 2)
         len = (w->buffer->size - 1) / 2;
      slock_lock(w->fifo_lock);
      while ((room = FIFO_WRITE_AVAIL(w->buffer)) < len
            && retro_atomic_load_acquire_int(&w->pump_run) && ++laps <= WASAPI_TIMEOUT)
         scond_wait_timeout(w->room_cond, w->fifo_lock, 1000);
      slock_unlock(w->fifo_lock);
      return room;
   }
#endif

   while (FIFO_WRITE_AVAIL(w->buffer) == 0)
   {
      BYTE *dest         = NULL;
      UINT32 frame_count = 0;

      if (WaitForSingleObject(w->write_event, WASAPI_TIMEOUT) != WAIT_OBJECT_0)
         return 0;

      if (w->flags & WASAPI_FLG_EXCLUSIVE)
      {
         frame_count = w->engine_buffer_size / w->frame_size;
         if (FAILED(_IAudioRenderClient_GetBuffer(
                     w->renderer, frame_count, &dest)))
            return 0;
         fifo_read(w->buffer, dest, w->engine_buffer_size);
         wasapi_ac3_drained(w, w->engine_buffer_size);
      }
      else
      {
         UINT32 padding     = 0;
         size_t read_avail;
         size_t engine_free;
         size_t ir;
         if (FAILED(_IAudioClient_GetCurrentPadding(w->client, &padding)))
            return 0;
         read_avail  = FIFO_READ_AVAIL(w->buffer);
         engine_free = w->engine_buffer_size - padding * w->frame_size;
         ir          = read_avail < engine_free ? read_avail : engine_free;
         if (!ir)
            continue;
         frame_count = ir / w->frame_size;
         if (FAILED(_IAudioRenderClient_GetBuffer(
                     w->renderer, frame_count, &dest)))
            return 0;
         fifo_read(w->buffer, dest, ir);
      }
      if (FAILED(_IAudioRenderClient_ReleaseBuffer(
                  w->renderer, frame_count, 0)))
         return 0;
   }

   (void)len;
   return wasapi_write_avail(w);
}

static size_t wasapi_underruns(void *wh)
{
   wasapi_t *w = (wasapi_t*)wh;
   return w ? retro_atomic_load_acquire_size(&w->underruns) : 0;
}

static size_t wasapi_frames_consumed(void *wh)
{
#ifdef HAVE_THREADS
   wasapi_t *w = (wasapi_t*)wh;
   size_t n;
   if (!w || !w->fifo_lock)
      return 0;
   slock_lock(w->fifo_lock);
   n = (size_t)w->consumed;
   slock_unlock(w->fifo_lock);
   return n;
#else
   (void)wh;
   return 0;
#endif
}

audio_driver_t audio_wasapi = {
   wasapi_init,
   wasapi_write,
   wasapi_stop,
   wasapi_start,
   wasapi_alive,
   wasapi_set_nonblock_state,
   wasapi_free,
   wasapi_use_float,
   "wasapi",
   wasapi_device_list_new,
   wasapi_device_list_free,
   wasapi_write_avail,
   wasapi_buffer_size,
   NULL, /* write_raw */
   wasapi_wait_writable,
   wasapi_frames_consumed,
   wasapi_underruns,
   wasapi_layout
};
