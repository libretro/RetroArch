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

#include <alsa/asoundlib.h>

#include <libretro.h>
#include <verbosity.h>
#include <lists/string_list.h>
#include <string/stdstring.h>

#include "../midi_driver.h"

typedef struct
{
   snd_seq_t *seq;
   snd_seq_addr_t in;
   snd_seq_addr_t in_dest;
   snd_seq_addr_t out;
   snd_seq_addr_t out_src;
   int out_queue;
   snd_seq_real_time_t out_ev_time;  /* time of the last output event */
} alsa_midi_t;

static const snd_seq_event_type_t alsa_midi_ev_map[8] =
{
   SND_SEQ_EVENT_NOTEOFF,
   SND_SEQ_EVENT_NOTEON,
   SND_SEQ_EVENT_KEYPRESS,
   SND_SEQ_EVENT_CONTROLLER,
   SND_SEQ_EVENT_PGMCHANGE,
   SND_SEQ_EVENT_CHANPRESS,
   SND_SEQ_EVENT_PITCHBEND,
   SND_SEQ_EVENT_SYSEX
};

static bool alsa_midi_get_avail_ports(struct string_list *ports, unsigned caps)
{
   int r;
   snd_seq_t *seq;
   snd_seq_client_info_t *client_info;
   snd_seq_port_info_t *port_info;
   union string_list_elem_attr attr = {0};

   snd_seq_client_info_alloca(&client_info);
   snd_seq_port_info_alloca(&port_info);

   r = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK);
   if (r < 0)
   {
      RARCH_ERR("[MIDI]: snd_seq_open failed with error %d.\n", r);
      return false;
   }

   snd_seq_client_info_set_client(client_info, -1);

   while (snd_seq_query_next_client(seq, client_info) == 0)
   {
      int client = snd_seq_client_info_get_client(client_info);

      snd_seq_port_info_set_client(port_info, client);
      snd_seq_port_info_set_port(port_info, -1);

      while (snd_seq_query_next_port(seq, port_info) == 0)
      {
         unsigned port_caps = snd_seq_port_info_get_capability(port_info);
         unsigned port_type = snd_seq_port_info_get_type(port_info);

         if ((port_type & SND_SEQ_PORT_TYPE_MIDI_GENERIC) &&
               (port_caps & caps) == caps)
         {
            const char *port_name = snd_seq_port_info_get_name(port_info);

            if (!string_list_append(ports, port_name, attr))
            {
               RARCH_ERR("[MIDI]: string_list_append failed.\n");
               snd_seq_close(seq);

               return false;
            }
         }
      }
   }

   snd_seq_close(seq);

   return true;
}

static bool alsa_midi_get_port(snd_seq_t *seq, const char *name, unsigned caps,
      snd_seq_addr_t *addr)
{
   snd_seq_client_info_t *client_info;
   snd_seq_port_info_t *port_info;

   snd_seq_client_info_alloca(&client_info);
   snd_seq_port_info_alloca(&port_info);

   snd_seq_client_info_set_client(client_info, -1);
   while (snd_seq_query_next_client(seq, client_info) == 0)
   {
      int client_id = snd_seq_client_info_get_client(client_info);

      snd_seq_port_info_set_client(port_info, client_id);
      snd_seq_port_info_set_port(port_info, -1);

      while (snd_seq_query_next_port(seq, port_info) == 0)
      {
         unsigned port_caps = snd_seq_port_info_get_capability(port_info);
         unsigned type      = snd_seq_port_info_get_type(port_info);

         if ((type & SND_SEQ_PORT_TYPE_MIDI_GENERIC) && (port_caps & caps) == caps)
         {
            const char *port_name = snd_seq_port_info_get_name(port_info);

            if (string_is_equal(port_name, name))
            {
               addr->client = client_id;
               addr->port   = snd_seq_port_info_get_port(port_info);

               return true;
            }
         }
      }
   }

   return false;
}

static bool alsa_midi_get_avail_inputs(struct string_list *inputs)
{
   return alsa_midi_get_avail_ports(inputs, SND_SEQ_PORT_CAP_READ |
         SND_SEQ_PORT_CAP_SUBS_READ);
}

static bool alsa_midi_get_avail_outputs(struct string_list *outputs)
{
   return alsa_midi_get_avail_ports(outputs, SND_SEQ_PORT_CAP_WRITE |
         SND_SEQ_PORT_CAP_SUBS_WRITE);
}

static void alsa_midi_free(void *p)
{
   alsa_midi_t *d = (alsa_midi_t*)p;

   if (d)
   {
      if (d->seq)
         snd_seq_close(d->seq);
      free(d);
   }
}

static bool alsa_midi_set_input(void *p, const char *input)
{
   int r;
   snd_seq_port_subscribe_t *sub;
   alsa_midi_t *d = (alsa_midi_t*)p;

   if (!input)
   {
      if (d->in_dest.port >= 0)
      {
         snd_seq_delete_simple_port(d->seq, d->in_dest.port);
         d->in_dest.port = -1;
      }

      return true;
   }

   if (!alsa_midi_get_port(d->seq, input, SND_SEQ_PORT_CAP_READ |
         SND_SEQ_PORT_CAP_SUBS_READ, &d->in))
      return false;

   r = snd_seq_create_simple_port(d->seq, "in", SND_SEQ_PORT_CAP_WRITE |
         SND_SEQ_PORT_CAP_SUBS_WRITE, SND_SEQ_PORT_TYPE_APPLICATION);
   if (r < 0)
   {
      RARCH_ERR("[MIDI]: snd_seq_create_simple_port failed with error %d.\n", r);
      return false;
   }

   d->in_dest.client = snd_seq_client_id(d->seq);
   d->in_dest.port   = r;

   snd_seq_port_subscribe_alloca(&sub);
   snd_seq_port_subscribe_set_sender(sub, &d->in);
   snd_seq_port_subscribe_set_dest(sub, &d->in_dest);
   r = snd_seq_subscribe_port(d->seq, sub);
   if (r < 0)
      RARCH_ERR("[MIDI]: snd_seq_subscribe_port failed with error %d.\n", r);

   return r >= 0;
}

static bool alsa_midi_set_output(void *p, const char *output)
{
   int r;
   alsa_midi_t *d = (alsa_midi_t*)p;

   if (!output)
   {
      if (d->out_queue >= 0)
      {
         snd_seq_stop_queue(d->seq, d->out_queue, NULL);
         snd_seq_free_queue(d->seq, d->out_queue);
         d->out_queue = -1;
      }
      if (d->out_src.port >= 0)
      {
         snd_seq_delete_simple_port(d->seq, d->out_src.port);
         d->out_src.port = -1;
      }

      return true;
   }

   if (!alsa_midi_get_port(d->seq, output, SND_SEQ_PORT_CAP_WRITE |
         SND_SEQ_PORT_CAP_SUBS_WRITE, &d->out))
      return false;

   r = snd_seq_create_simple_port(d->seq, "out", SND_SEQ_PORT_CAP_READ |
         SND_SEQ_PORT_CAP_SUBS_READ, SND_SEQ_PORT_TYPE_APPLICATION);
   if (r < 0)
   {
      RARCH_ERR("[MIDI]: snd_seq_create_simple_port failed with error %d.\n", r);
      return false;
   }

   d->out_src.client = snd_seq_client_id(d->seq);
   d->out_src.port   = r;

   r = snd_seq_connect_to(d->seq, d->out_src.port, d->out.client, d->out.port);
   if (r < 0)
   {
      RARCH_ERR("[MIDI]: snd_seq_connect_to failed with error %d.\n", r);
      return false;
   }

   d->out_queue = snd_seq_alloc_queue(d->seq);
   if (d->out_queue < 0)
   {
      RARCH_ERR("[MIDI]: snd_seq_alloc_queue failed with error %d.\n", d->out_queue);
      return false;
   }

   r = snd_seq_start_queue(d->seq, d->out_queue, NULL);
   if (r < 0)
   {
       RARCH_ERR("[MIDI]: snd_seq_start_queue failed with error %d.\n", r);
       return false;
   }

   return true;
}

static void *alsa_midi_init(const char *input, const char *output)
{
   int r;
   bool err       = false;
   alsa_midi_t *d = (alsa_midi_t*)calloc(sizeof(alsa_midi_t), 1);

   if (!d)
   {
      RARCH_ERR("[MIDI]: Out of memory.\n");
      return NULL;
   }

   d->in_dest.port = -1;
   d->out_src.port = -1;
   d->out_queue    = -1;

   r = snd_seq_open(&d->seq, "default", SND_SEQ_OPEN_DUPLEX, SND_SEQ_NONBLOCK);
   if (r < 0)
   {
      RARCH_ERR("[MIDI]: snd_seq_open failed with error %d.\n", r);
      err = true;
   }
   else if (!alsa_midi_set_input(d, input))
      err = true;
   else if (!alsa_midi_set_output(d, output))
      err = true;

   if (err)
   {
      alsa_midi_free(d);
      d = NULL;
   }

   return d;
}

static bool alsa_midi_read(void *p, midi_event_t *event)
{
   int r;
   snd_seq_event_t *ev;
   alsa_midi_t *d = (alsa_midi_t*)p;

   r = snd_seq_event_input(d->seq, &ev);
   if (r < 0)
   {
#ifdef DEBUG
      if (r != -EAGAIN)
         RARCH_ERR("[MIDI]: snd_seq_event_input failed with error %d.\n", r);
#endif
      return false;
   }

   if (ev->type == SND_SEQ_EVENT_NOTEOFF)
   {
      event->data[0]   = 0x80 | ev->data.note.channel;
      event->data[1]   = ev->data.note.note;
      event->data[2]   = ev->data.note.velocity;
      event->data_size = 3;
   }
   else if (ev->type == SND_SEQ_EVENT_NOTEON)
   {
      event->data[0]   = 0x90 | ev->data.note.channel;
      event->data[1]   = ev->data.note.note;
      event->data[2]   = ev->data.note.velocity;
      event->data_size = 3;
   }
   else if (ev->type == SND_SEQ_EVENT_KEYPRESS)
   {
      event->data[0]   = 0xA0 | ev->data.note.channel;
      event->data[1]   = ev->data.note.note;
      event->data[2]   = ev->data.note.velocity;
      event->data_size = 3;
   }
   else if (ev->type == SND_SEQ_EVENT_CONTROLLER)
   {
      event->data[0]   = 0xB0 | ev->data.control.channel;
      event->data[1]   = ev->data.control.param;
      event->data[2]   = ev->data.control.value;
      event->data_size = 3;
   }
   else if (ev->type == SND_SEQ_EVENT_PGMCHANGE)
   {
      event->data[0]   = 0xC0 | ev->data.control.channel;
      event->data[1]   = ev->data.control.value;
      event->data_size = 2;
   }
   else if (ev->type == SND_SEQ_EVENT_CHANPRESS)
   {
      event->data[0]   = 0xD0 | ev->data.control.channel;
      event->data[1]   = ev->data.control.value;
      event->data_size = 2;
   }
   else if (ev->type == SND_SEQ_EVENT_PITCHBEND)
   {
      event->data[0]   = 0xE0 | ev->data.control.channel;
      event->data[1]   = ev->data.control.value & 127;
      event->data[2]   = ev->data.control.value >> 7;
      event->data_size = 3;
   }
   else if (ev->type == SND_SEQ_EVENT_SYSEX)
   {
      if (ev->data.ext.len <= event->data_size)
      {
         size_t i;
         uint8_t *ev_data = (uint8_t*)ev->data.ext.ptr;

         for (i = 0; i < ev->data.ext.len; ++i)
            event->data[i] = ev_data[i];

         event->data_size = ev->data.ext.len;
      }
#ifdef DEBUG
      else
      {
         RARCH_ERR("[MIDI]: SysEx event too big.\n");
         r = -1;
      }
#endif
   }
   else
      r = -1;

   event->delta_time = 0;
   snd_seq_free_event(ev);

   return r >= 0;
}

static bool alsa_midi_write(void *p, const midi_event_t *event)
{
   int r;
   snd_seq_event_t ev;
   alsa_midi_t *d = (alsa_midi_t*)p;

   ev.type  = alsa_midi_ev_map[(event->data[0] >> 4) & 7];
   ev.flags = SND_SEQ_TIME_STAMP_REAL | SND_SEQ_TIME_MODE_ABS;
   ev.queue = d->out_queue;
   ev.time.time.tv_sec  = d->out_ev_time.tv_sec + event->delta_time / 1000000;
   ev.time.time.tv_nsec = d->out_ev_time.tv_nsec +
         (event->delta_time % 1000000) * 1000;
   if(ev.time.time.tv_nsec >= 1000000000)
   {
       ev.time.time.tv_sec  += 1;
       ev.time.time.tv_nsec -= 1000000000;
   }
   ev.source.port = d->out_src.port;
   ev.dest.client = SND_SEQ_ADDRESS_SUBSCRIBERS;

   if (ev.type == SND_SEQ_EVENT_NOTEOFF || ev.type == SND_SEQ_EVENT_NOTEON ||
         ev.type == SND_SEQ_EVENT_KEYPRESS)
   {
      ev.data.note.channel  = event->data[0] & 0x0F;
      ev.data.note.note     = event->data[1];
      ev.data.note.velocity = event->data[2];
   }
   else if (ev.type == SND_SEQ_EVENT_CONTROLLER)
   {
      ev.data.control.channel = event->data[0] & 0x0F;
      ev.data.control.param   = event->data[1];
      ev.data.control.value   = event->data[2];
   }
   else if (ev.type == SND_SEQ_EVENT_PGMCHANGE ||
         ev.type == SND_SEQ_EVENT_CHANPRESS)
   {
      ev.data.control.channel = event->data[0] & 0x0F;
      ev.data.control.value   = event->data[1];
   }
   else if (ev.type == SND_SEQ_EVENT_PITCHBEND)
   {
      ev.data.control.channel = event->data[0] & 0x0F;
      ev.data.control.value   = (event->data[1] | (event->data[2] << 7)) - 0x2000;
   }
   else if (ev.type == SND_SEQ_EVENT_SYSEX)
   {
      ev.flags |= SND_SEQ_EVENT_LENGTH_VARIABLE;
      ev.data.ext.ptr = event->data;
      ev.data.ext.len = event->data_size;
   }

   r = snd_seq_event_output(d->seq, &ev);
#ifdef DEBUG
   if (r < 0)
      RARCH_ERR("[MIDI]: snd_seq_event_output failed with error %d.\n", r);
#endif

   d->out_ev_time.tv_sec  = ev.time.time.tv_sec;
   d->out_ev_time.tv_nsec = ev.time.time.tv_nsec;

   return r >= 0;
}

static bool alsa_midi_flush(void *p)
{
   int r;
   alsa_midi_t *d = (alsa_midi_t*)p;

   r = snd_seq_drain_output(d->seq);
#ifdef DEBUG
   if (r < 0)
      RARCH_ERR("[MIDI]: snd_seq_drain_output failed with error %d.\n", r);
#endif

   return r == 0;
}

midi_driver_t midi_alsa = {
   "alsa",
   alsa_midi_get_avail_inputs,
   alsa_midi_get_avail_outputs,
   alsa_midi_init,
   alsa_midi_free,
   alsa_midi_set_input,
   alsa_midi_set_output,
   alsa_midi_read,
   alsa_midi_write,
   alsa_midi_flush
};
