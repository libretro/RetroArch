/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018 The RetroArch team
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

#include <windows.h>

#include <libretro.h>
#include <lists/string_list.h>
#include <verbosity.h>
#include <string/stdstring.h>

#include "../midi_driver.h"

#define WINMM_MIDI_BUF_CNT 3
#define WINMM_MIDI_BUF_LEN 1024

typedef struct
{
   MIDIHDR header;
   DWORD data[WINMM_MIDI_BUF_LEN];
} winmm_midi_buffer_t;

typedef struct
{
   uint8_t data[WINMM_MIDI_BUF_LEN * 4];
   midi_event_t events[WINMM_MIDI_BUF_LEN];
   int rd_idx;
   int wr_idx;
} winmm_midi_queue_t;

typedef struct
{
   HMIDIIN in_dev;
   HMIDISTRM out_dev;
   winmm_midi_queue_t in_queue;
   winmm_midi_buffer_t out_bufs[WINMM_MIDI_BUF_CNT];
   int out_buf_idx;
   double tick_dur;
} winmm_midi_t;

static void winmm_midi_free(void *p);

static bool winmm_midi_queue_read(winmm_midi_queue_t *q, midi_event_t *ev)
{
   unsigned i;
   midi_event_t *src_ev = NULL;

   if (q->rd_idx == q->wr_idx)
      return false;

   if (ev->data_size < q->events[q->rd_idx].data_size)
   {
#ifdef DEBUG
      RARCH_ERR("[MIDI]: Input queue read failed (event data too small).\n");
#endif
      return false;
   }

   src_ev = &q->events[q->rd_idx];

   for (i = 0; i < src_ev->data_size; ++i)
      ev->data[i] = src_ev->data[i];

   ev->data_size = src_ev->data_size;
   ev->delta_time = src_ev->delta_time;

   if (q->rd_idx + 1 == WINMM_MIDI_BUF_LEN)
      q->rd_idx = 0;
   else
      ++q->rd_idx;

   return true;
}

static bool winmm_midi_queue_write(winmm_midi_queue_t *q, const midi_event_t *ev)
{
   int wr_avail;
   unsigned i;
   int rd_idx            = q->rd_idx;
   midi_event_t *dest_ev = NULL;

   if (q->wr_idx >= rd_idx)
      wr_avail = WINMM_MIDI_BUF_LEN - q->wr_idx + rd_idx;
   else
      wr_avail = rd_idx - q->wr_idx - 1;

   if (wr_avail < 1)
   {
#ifdef DEBUG
      RARCH_ERR("[MIDI]: Input queue overflow.\n");
#endif
      return false;
   }

   dest_ev = &q->events[q->wr_idx];
   if (ev->data_size > 4)
   {
#ifdef DEBUG
      RARCH_ERR("[MIDI]: Input queue write failed (event too big).\n");
#endif
      return false;
   }

   for (i = 0; i < ev->data_size; ++i)
      dest_ev->data[i] = ev->data[i];

   dest_ev->data_size = ev->data_size;
   dest_ev->delta_time = ev->delta_time;

   if (q->wr_idx + 1 == WINMM_MIDI_BUF_LEN)
      q->wr_idx = 0;
   else
      ++q->wr_idx;

   return true;
}

static void winmm_midi_queue_init(winmm_midi_queue_t *q)
{
   unsigned i, j;

   for (i = j = 0; i < WINMM_MIDI_BUF_LEN; ++i, j += 4)
   {
      q->events[i].data       = &q->data[j];
      q->events[i].delta_time = 0;
   }

   q->rd_idx = 0;
   q->wr_idx = 0;
}

static void CALLBACK winmm_midi_input_callback(HMIDIIN dev, UINT msg,
      DWORD_PTR q, DWORD_PTR par1, DWORD_PTR par2)
{
   uint8_t data[3];
   midi_event_t event;
   winmm_midi_queue_t *queue = (winmm_midi_queue_t*)q;

   (void)dev;

   if (msg == MIM_OPEN)
      winmm_midi_queue_init(queue);
   else if (msg == MIM_DATA)
   {
      data[0] = (uint8_t)(par1 & 0xFF);
      data[1] = (uint8_t)((par1 >> 8) & 0xFF);
      data[2] = (uint8_t)((par1 >> 16) & 0xFF);

      event.data       = data;
      event.data_size  = midi_driver_get_event_size(data[0]);
      event.delta_time = 0;

      if (!winmm_midi_queue_write(queue, &event))
      {
#ifdef DEBUG
         RARCH_ERR("[MIDI]: Input event dropped.\n");
#endif
      }
   }
   else if(msg == MIM_LONGDATA)
   {
#ifdef DEBUG
      RARCH_WARN("[MIDI]: SysEx input not implemented, event dropped.\n");
#endif
   }
}

static HMIDIIN winmm_midi_open_input_device(const char *dev_name,
      winmm_midi_queue_t *queue)
{
   unsigned i;
   UINT dev_count = midiInGetNumDevs();
   HMIDIIN dev    = NULL;

   for (i = 0; i < dev_count; ++i)
   {
      MIDIINCAPSA caps;
      MMRESULT mmr = midiInGetDevCapsA((UINT)i, &caps, sizeof(caps));
      if (mmr == MMSYSERR_NOERROR)
      {
         if (string_is_equal(caps.szPname, dev_name))
         {
            mmr = midiInOpen(&dev, (UINT)i, (DWORD_PTR)winmm_midi_input_callback,
                  (DWORD_PTR)queue, CALLBACK_FUNCTION);
            if (mmr != MMSYSERR_NOERROR)
               RARCH_ERR("[MIDI]: midiInOpen failed with error %d.\n", mmr);
            break;
         }
      }
      else
         RARCH_WARN("[MIDI]: midiInGetDevCapsA failed with error %d.\n", mmr);
   }

   return dev;
}

static HMIDISTRM winmm_midi_open_output_device(const char *dev_name)
{
   unsigned i;
   UINT dev_count = midiOutGetNumDevs();
   HMIDISTRM dev  = NULL;

   for (i = 0; i < dev_count; ++i)
   {
      MIDIOUTCAPSA caps;
      MMRESULT mmr = midiOutGetDevCapsA(i, &caps, sizeof(caps));
      if (mmr == MMSYSERR_NOERROR)
      {
         if (string_is_equal(caps.szPname, dev_name))
         {
            mmr = midiStreamOpen(&dev, &i, 1, 0, 0, CALLBACK_NULL);
            if (mmr != MMSYSERR_NOERROR)
               RARCH_ERR("[MIDI]: midiStreamOpen failed with error %d.\n", mmr);
            break;
         }
      }
      else
         RARCH_WARN("[MIDI]: midiOutGetDevCapsA failed with error %d.\n", mmr);
   }

   return dev;
}

static bool winmm_midi_init_clock(HMIDISTRM out_dev, double *tick_dur)
{
   MIDIPROPTIMEDIV division;
   MIDIPROPTEMPO tempo;
   MMRESULT mmr;

   tempo.cbStruct = sizeof(tempo);
   mmr = midiStreamProperty(out_dev, (LPBYTE)&tempo,
         MIDIPROP_GET | MIDIPROP_TEMPO);
   if (mmr != MMSYSERR_NOERROR)
   {
      RARCH_ERR("[MIDI]: Current tempo unavailable (error %d).\n", mmr);
      return false;
   }

   division.dwTimeDiv = 3;
   while (tempo.dwTempo / division.dwTimeDiv > 320)
      division.dwTimeDiv *= 2;

   division.cbStruct = sizeof(division);
   mmr = midiStreamProperty(out_dev, (LPBYTE)&division,
         MIDIPROP_SET | MIDIPROP_TIMEDIV);
   if (mmr != MMSYSERR_NOERROR)
   {
      RARCH_ERR("[MIDI]: Time division change failed (error %d).\n", mmr);
      return false;
   }

   *tick_dur = (double)tempo.dwTempo / (double)division.dwTimeDiv;
#ifdef DEBUG
   RARCH_LOG("[MIDI]: Tick duration %f us.\n", *tick_dur);
#endif

   return true;
}

static bool winmm_midi_init_output_buffers(HMIDISTRM dev,
      winmm_midi_buffer_t *bufs)
{
   unsigned i;

   for (i = 0; i < WINMM_MIDI_BUF_CNT; ++i)
   {
      MMRESULT mmr;

      bufs[i].header.dwBufferLength  = sizeof(DWORD) * WINMM_MIDI_BUF_LEN;
      bufs[i].header.dwBytesRecorded = 0;
      bufs[i].header.dwFlags         = 0;
      bufs[i].header.lpData          = (LPSTR)bufs[i].data;

      mmr = midiOutPrepareHeader((HMIDIOUT)dev, &bufs[i].header, sizeof(MIDIHDR));
      if (mmr != MMSYSERR_NOERROR)
      {
         RARCH_ERR("[MIDI]: midiOutPrepareHeader failed with error %d.\n", mmr);

         while (--i <= 0)
            midiOutUnprepareHeader((HMIDIOUT)dev, &bufs[i].header, sizeof(MIDIHDR));

         return false;
      }
   }

   return true;
}

static void winmm_midi_free_output_buffers(HMIDISTRM dev, winmm_midi_buffer_t *bufs)
{
   unsigned i;

   for (i = 0; i < WINMM_MIDI_BUF_CNT; ++i)
   {
      MMRESULT mmr = midiOutUnprepareHeader(
            (HMIDIOUT)dev, &bufs[i].header, sizeof(MIDIHDR));
      if (mmr != MMSYSERR_NOERROR)
         RARCH_ERR("[MIDI]: midiOutUnprepareHeader failed with error %d.\n", mmr);
   }
}

static bool winmm_midi_write_short_event(winmm_midi_buffer_t *buf,
      const uint8_t *data, size_t data_size, DWORD delta_time)
{
   DWORD i = buf->header.dwBytesRecorded / sizeof(DWORD);

   if (buf->header.dwBytesRecorded + sizeof(DWORD) * 3 >
         sizeof(DWORD) * WINMM_MIDI_BUF_LEN)
      return false;

   buf->data[i++] = delta_time;
   buf->data[i++] = 0;
   buf->data[i] = MEVT_F_SHORT << 24;
   if (data_size == 0)
      buf->data[i] |= MEVT_NOP;
   else
   {
      buf->data[i] |= MEVT_SHORTMSG << 24 | data[0];
      if (data_size > 1)
         buf->data[i] |= data[1] << 8;
      if (data_size > 2)
         buf->data[i] |= data[2] << 16;
   }

   buf->header.dwBytesRecorded += sizeof(DWORD) * 3;

   return true;
}

static bool winmm_midi_write_long_event(winmm_midi_buffer_t *buf,
      const uint8_t *data, size_t data_size, DWORD delta_time)
{
   DWORD i = buf->header.dwBytesRecorded / sizeof(DWORD);

   if (buf->header.dwBytesRecorded + sizeof(DWORD) * 3 + data_size >
         sizeof(DWORD) * WINMM_MIDI_BUF_LEN)
      return false;

   buf->data[i++] = delta_time;
   buf->data[i++] = 0;
   buf->data[i++] = MEVT_F_LONG << 24 | MEVT_LONGMSG << 24 | data_size;

   memcpy(&buf->data[i], data, data_size);
   buf->header.dwBytesRecorded += sizeof(DWORD) * 3 + data_size;

   return true;
}

static bool winmm_midi_get_avail_inputs(struct string_list *inputs)
{
   unsigned i;
   union string_list_elem_attr attr = {0};
   UINT dev_count                   = midiInGetNumDevs();

   for (i = 0; i < dev_count; ++i)
   {
      MIDIINCAPSA caps;
      MMRESULT mmr = midiInGetDevCapsA((UINT)i, &caps, sizeof(caps));
      if (mmr != MMSYSERR_NOERROR)
      {
         RARCH_ERR("[MIDI]: midiInGetDevCapsA failed with error %d.\n", mmr);
         return false;
      }

      if (!string_list_append(inputs, caps.szPname, attr))
      {
         RARCH_ERR("[MIDI]: string_list_append failed.\n");
         return false;
      }
   }

   return true;
}

static bool winmm_midi_get_avail_outputs(struct string_list *outputs)
{
   unsigned i;
   union string_list_elem_attr attr = {0};
   UINT dev_count                   = midiOutGetNumDevs();

   for (i = 0; i < dev_count; ++i)
   {
      MIDIOUTCAPSA caps;
      MMRESULT mmr = midiOutGetDevCapsA((UINT)i, &caps, sizeof(caps));
      if (mmr != MMSYSERR_NOERROR)
      {
         RARCH_ERR("[MIDI]: midiOutGetDevCapsA failed with error %d.\n", mmr);
         return false;
      }

      if (!string_list_append(outputs, caps.szPname, attr))
      {
         RARCH_ERR("[MIDI]: string_list_append failed.\n");
         return false;
      }
   }

   return true;
}

static void *winmm_midi_init(const char *input, const char *output)
{
   MMRESULT mmr;
   bool err        = false;
   winmm_midi_t *d = (winmm_midi_t*)calloc(sizeof(winmm_midi_t), 1);

   if (!d)
   {
      RARCH_ERR("[MIDI]: Out of memory.\n");
      return NULL;
   }

   if (input)
   {
      d->in_dev = winmm_midi_open_input_device(input, &d->in_queue);
      if (!d->in_dev)
         err = true;
      else
      {
         mmr = midiInStart(d->in_dev);
         if (mmr != MMSYSERR_NOERROR)
         {
            RARCH_ERR("[MIDI]: midiInStart failed with error %d.\n", mmr);
            err = true;
         }
      }
   }

   if (output)
   {
      d->out_dev = winmm_midi_open_output_device(output);
      if (!d->out_dev)
         err = true;
      else if (!winmm_midi_init_clock(d->out_dev, &d->tick_dur))
         err = true;
      else if (!winmm_midi_init_output_buffers(d->out_dev, d->out_bufs))
         err = true;
      else
      {
         mmr = midiStreamRestart(d->out_dev);
         if (mmr != MMSYSERR_NOERROR)
         {
            RARCH_ERR("[MIDI]: midiStreamRestart failed with error %d.\n", mmr);
            err = true;
         }
      }
   }

   if (err)
   {
      winmm_midi_free(d);
      return NULL;
   }

   return d;
}

static void winmm_midi_free(void *p)
{
   winmm_midi_t *d = (winmm_midi_t*)p;

   if (!d)
      return;

   if (d->in_dev)
   {
      midiInStop(d->in_dev);
      midiInClose(d->in_dev);
   }

   if (d->out_dev)
   {
      midiStreamStop(d->out_dev);
      winmm_midi_free_output_buffers(d->out_dev, d->out_bufs);
      midiStreamClose(d->out_dev);
   }

   free(d);
}

static bool winmm_midi_set_input(void *p, const char *input)
{
   winmm_midi_t *d = (winmm_midi_t*)p;
   HMIDIIN new_dev = NULL;

   if (input)
   {
      new_dev = winmm_midi_open_input_device(input, &d->in_queue);
      if (!new_dev)
         return false;
   }

   if (d->in_dev)
   {
      midiInStop(d->in_dev);
      midiInClose(d->in_dev);
   }

   d->in_dev = new_dev;
   if (d->in_dev)
   {
      MMRESULT mmr = midiInStart(d->in_dev);
      if (mmr != MMSYSERR_NOERROR)
      {
         RARCH_ERR("[MIDI]: midiInStart failed with error %d.\n", mmr);
         return false;
      }
   }

   return true;
}

static bool winmm_midi_set_output(void *p, const char *output)
{
   winmm_midi_t *d   = (winmm_midi_t*)p;
   HMIDISTRM new_dev = NULL;

   if (output)
   {
      new_dev = winmm_midi_open_output_device(output);
      if (!new_dev)
         return false;
   }

   if (d->out_dev)
   {
      midiStreamStop(d->out_dev);
      winmm_midi_free_output_buffers(d->out_dev, d->out_bufs);
      midiStreamClose(d->out_dev);
   }

   d->out_dev = new_dev;
   if (d->out_dev)
   {
      MMRESULT mmr;
      if (!winmm_midi_init_output_buffers(d->out_dev, d->out_bufs))
         return false;

      d->out_buf_idx = 0;

      mmr = midiStreamRestart(d->out_dev);
      if (mmr != MMSYSERR_NOERROR)
      {
         RARCH_ERR("[MIDI]: midiStreamRestart failed with error %d.\n", mmr);
         return false;
      }
   }

   return true;
}

static bool winmm_midi_read(void *p, midi_event_t *event)
{
   winmm_midi_t *d = (winmm_midi_t*)p;

   return winmm_midi_queue_read(&d->in_queue, event);
}

static bool winmm_midi_write(void *p, const midi_event_t *event)
{
   winmm_midi_t *d          = (winmm_midi_t*)p;
   winmm_midi_buffer_t *buf = &d->out_bufs[d->out_buf_idx];
   DWORD delta_time;

   if (buf->header.dwFlags & MHDR_INQUEUE)
      return false;

   if (buf->header.dwFlags & MHDR_DONE)
   {
      buf->header.dwBytesRecorded = 0;
      buf->header.dwFlags ^= MHDR_DONE;
   }

   delta_time = (DWORD)((double)event->delta_time / d->tick_dur);
   if (event->data_size < 4)
      return winmm_midi_write_short_event(buf, event->data,
            event->data_size, delta_time);

   return winmm_midi_write_long_event(buf, event->data,
         event->data_size, delta_time);
}

static bool winmm_midi_flush(void *p)
{
   winmm_midi_t *d          = (winmm_midi_t*)p;
   winmm_midi_buffer_t *buf = &d->out_bufs[d->out_buf_idx];

   if (buf->header.dwBytesRecorded)
   {
      MMRESULT mmr = midiStreamOut(
            d->out_dev, &buf->header, sizeof(buf->header));

      if (mmr != MMSYSERR_NOERROR)
      {
#ifdef DEBUG
         RARCH_ERR("[MIDI]: midiStreamOut failed with error %d.\n", mmr);
#endif
         return false;
      }

      if (++d->out_buf_idx == WINMM_MIDI_BUF_CNT)
         d->out_buf_idx = 0;
   }

   return true;
}

midi_driver_t midi_winmm = {
   "winmm",
   winmm_midi_get_avail_inputs,
   winmm_midi_get_avail_outputs,
   winmm_midi_init,
   winmm_midi_free,
   winmm_midi_set_input,
   winmm_midi_set_output,
   winmm_midi_read,
   winmm_midi_write,
   winmm_midi_flush
};
