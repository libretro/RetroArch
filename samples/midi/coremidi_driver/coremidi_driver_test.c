/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (coremidi_driver_test.c).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* The CoreMIDI driver over a stand-in for CoreMIDI (stub/): reads and
 * writes look for their endpoints only after a device was removed; a
 * removal - an input with events queued, an output, a device neither
 * uses - is found by each instance on its next call, however many
 * there are; and removals notified from another thread while reads
 * go on touch no instance. Built under ThreadSanitizer too. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

#include <rthreads/rthreads.h>

#include "../../../midi/drivers/coremidi.c"

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }
bool string_list_append(struct string_list *list, const char *elem,
      union string_list_elem_attr attr)
{ (void)list; (void)elem; (void)attr; return true; }
size_t midi_driver_get_event_size(uint8_t status)
{
   if (status < 0x80)
      return 0;
   switch (status & 0xF0)
   {
      case 0xC0:
      case 0xD0:
         return 2;
      default:
         return 3;
   }
}

static unsigned failures;
#define CHECK(cond, ...) \
   do { if (!(cond)) { printf("FAIL line %d: ", __LINE__); \
        printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

/* ---- the stand-in ---- */
#define T_MAX 16

struct t_ep
{
   const char     *name;
   MIDIEndpointRef ref;
};

static struct t_ep    t_src[T_MAX], t_dst[T_MAX];
static unsigned       t_nsrc, t_ndst;
static unsigned       t_enumerations;
static unsigned       t_sends;
static MIDINotifyProc t_notify;
static MIDIReadProc   t_read;
static void          *t_read_ref[T_MAX + 1];
static MIDIPortRef    t_ports;

static void t_add(struct t_ep *eps, unsigned *n, MIDIEndpointRef ref,
      const char *name)
{
   eps[*n].ref    = ref;
   eps[*n].name   = name;
   (*n)++;
}

static void t_drop(struct t_ep *eps, unsigned *n, MIDIEndpointRef ref)
{
   unsigned i;
   for (i = 0; i < *n; i++)
      if (eps[i].ref == ref)
      {
         eps[i] = eps[--(*n)];
         return;
      }
}

OSStatus MIDIClientCreate(CFStringRef name, MIDINotifyProc notify,
      void *refCon, MIDIClientRef *out)
{
   (void)name; (void)refCon;
   t_notify = notify;
   *out     = 1;
   return noErr;
}
OSStatus MIDIInputPortCreate(MIDIClientRef client, CFStringRef name,
      MIDIReadProc read, void *refCon, MIDIPortRef *out)
{
   (void)client; (void)name;
   if (t_ports >= T_MAX)
      return -1;
   t_read               = read;
   *out                 = ++t_ports;
   t_read_ref[t_ports]  = refCon;
   return noErr;
}
OSStatus MIDIOutputPortCreate(MIDIClientRef client, CFStringRef name,
      MIDIPortRef *out)
{
   (void)client; (void)name;
   *out = 1000 + (++t_ports);
   return noErr;
}
OSStatus MIDIPortDispose(MIDIPortRef port) { (void)port; return noErr; }
OSStatus MIDIPortConnectSource(MIDIPortRef port, MIDIEndpointRef source,
      void *connRefCon)
{ (void)port; (void)source; (void)connRefCon; return noErr; }
OSStatus MIDIPortDisconnectSource(MIDIPortRef port, MIDIEndpointRef source)
{ (void)port; (void)source; return noErr; }
ItemCount MIDIGetNumberOfSources(void) { t_enumerations++; return t_nsrc; }
MIDIEndpointRef MIDIGetSource(ItemCount i) { return t_src[i].ref; }
ItemCount MIDIGetNumberOfDestinations(void) { t_enumerations++; return t_ndst; }
MIDIEndpointRef MIDIGetDestination(ItemCount i) { return t_dst[i].ref; }
OSStatus MIDIObjectGetStringProperty(MIDIObjectRef obj, CFStringRef prop,
      CFStringRef *out)
{
   unsigned i;
   (void)prop;
   for (i = 0; i < t_nsrc; i++)
      if (t_src[i].ref == obj)
         return (*out = t_src[i].name), noErr;
   for (i = 0; i < t_ndst; i++)
      if (t_dst[i].ref == obj)
         return (*out = t_dst[i].name), noErr;
   return -1;
}
int CFStringGetCString(CFStringRef s, char *buf, long size, int encoding)
{
   (void)encoding;
   if (!s || (long)strlen(s) >= size)
      return 0;
   strcpy(buf, s);
   return 1;
}
void CFRelease(const void *obj) { (void)obj; }
MIDIPacket *MIDIPacketListInit(MIDIPacketList *list)
{
   list->numPackets = 0;
   return &list->packet[0];
}
MIDIPacket *MIDIPacketListAdd(MIDIPacketList *list, unsigned long size,
      MIDIPacket *cur, MIDITimeStamp time, unsigned long n, const Byte *data)
{
   (void)size;
   if (n > sizeof(cur->data))
      return NULL;
   cur->timeStamp = time;
   cur->length    = (UInt16)n;
   memcpy(cur->data, data, n);
   list->numPackets++;
   return cur;
}
const MIDIPacket *MIDIPacketNext(const MIDIPacket *pkt) { return pkt + 1; }
OSStatus MIDISend(MIDIPortRef port, MIDIEndpointRef dest,
      const MIDIPacketList *list)
{
   unsigned i;
   (void)port; (void)list;
   for (i = 0; i < t_ndst; i++)
      if (t_dst[i].ref == dest)
         return t_sends++, noErr;
   return -1;
}

/* CoreMIDI's notification of an object gone, as its own thread sends it. */
static void t_removed(MIDIObjectRef child)
{
   MIDIObjectAddRemoveNotification n;
   memset(&n, 0, sizeof(n));
   n.messageID   = kMIDIMsgObjectRemoved;
   n.messageSize = sizeof(n);
   n.child       = child;
   t_notify((const MIDINotification*)&n, NULL);
}

static MIDIObjectRef t_removed_child;
static void t_removed_thread(void *arg)
{
   (void)arg;
   t_removed(t_removed_child);
}
static void t_removed_elsewhere(MIDIObjectRef child)
{
   sthread_t *t;
   t_removed_child = child;
   if ((t = sthread_create(t_removed_thread, NULL)))
      sthread_join(t);
}

/* A note from the device, through the instance's read proc. */
static void t_note(coremidi_t *d, uint8_t key)
{
   MIDIPacketList list;
   const Byte note[3] = { 0x90, 0, 100 };
   unsigned port;
   MIDIPacketListInit(&list);
   MIDIPacketListAdd(&list, sizeof(list), &list.packet[0], 0, 3, note);
   list.packet[0].data[1] = key;
   for (port = 1; port <= t_ports && port <= T_MAX; port++)
      if (t_read_ref[port] == d)
         t_read(&list, d, NULL);
}

static retro_atomic_int_t t_spam_done;
static void t_spam_thread(void *arg)
{
   unsigned i;
   (void)arg;
   for (i = 0; i < 2000; i++)
      t_removed(0x999);
   retro_atomic_store_release_int(&t_spam_done, 1);
}

int main(void)
{
   midi_event_t ev;
   coremidi_t *a, *c, *many[10];
   unsigned i, e, reads;
   sthread_t *spam;

   t_add(t_src, &t_nsrc, 0x101, "Keys");
   t_add(t_src, &t_nsrc, 0x102, "Pads");
   t_add(t_dst, &t_ndst, 0x201, "Synth");

   a = (coremidi_t*)coremidi_init("Keys", "Synth");
   CHECK(a && coremidi_set_input(a, "Keys") && coremidi_set_output(a, "Synth"),
         "instance");
   if (!a)
      return 1;

   /* No device gone: no device looked for, read or write. */
   e = t_enumerations;
   for (i = 0; i < 100; i++)
   {
      t_note(a, (uint8_t)(i & 0x7f));
      CHECK(coremidi_read(a, &ev) && ev.data[1] == (i & 0x7f),
            "read %u", i);
      CHECK(coremidi_write(a, &ev), "write %u", i);
   }
   CHECK(t_enumerations == e, "%u device lists for 100 reads and writes",
         t_enumerations - e);

   /* A device neither uses, removed: each side looks once, and keeps
    * its endpoint. */
   t_drop(t_src, &t_nsrc, 0x102);
   t_removed_elsewhere(0x102);
   t_note(a, 1);
   CHECK(coremidi_read(a, &ev) && coremidi_write(a, &ev),
         "an unrelated removal broke the endpoints");
   t_note(a, 2);
   CHECK(coremidi_read(a, &ev) && coremidi_write(a, &ev), "after it");
   CHECK(t_enumerations == e + 2, "%u device lists for one removal, want 2",
         t_enumerations - e);

   /* The input removed with notes queued: the next read finds it gone
    * and none of them come through, then or from a new device. */
   t_note(a, 3);
   t_note(a, 4);
   t_drop(t_src, &t_nsrc, 0x101);
   t_removed_elsewhere(0x101);
   CHECK(!coremidi_read(a, &ev) && !a->input_endpoint,
         "a read from a removed input");
   t_add(t_src, &t_nsrc, 0x103, "Keys");
   CHECK(coremidi_set_input(a, "Keys"), "the input again");
   CHECK(!coremidi_read(a, &ev), "a note from before the removal came through");
   t_note(a, 5);
   CHECK(coremidi_read(a, &ev) && ev.data[1] == 5, "the new input's note");

   /* The output removed: the next write finds it gone. */
   t_drop(t_dst, &t_ndst, 0x201);
   t_removed_elsewhere(0x201);
   CHECK(!coremidi_write(a, &ev) && !a->output_endpoint,
         "a write to a removed output");

   /* More instances than a fixed registry held, each finding a
    * removal on its own call. */
   for (i = 0; i < 10; i++)
   {
      many[i] = (coremidi_t*)coremidi_init("Keys", NULL);
      CHECK(many[i] && coremidi_set_input(many[i], "Keys"), "instance %u", i);
   }
   t_drop(t_src, &t_nsrc, 0x103);
   t_removed_elsewhere(0x103);
   for (i = 0; i < 10; i++)
      if (many[i])
      {
         CHECK(!coremidi_read(many[i], &ev) && !many[i]->input_endpoint,
               "instance %u read from a removed input", i);
         coremidi_free(many[i]);
      }
   coremidi_free(a);

   /* Removals notified from another thread while reads go on. */
   t_add(t_src, &t_nsrc, 0x104, "Strings");
   c = (coremidi_t*)coremidi_init("Strings", NULL);
   CHECK(c && coremidi_set_input(c, "Strings"), "instance");
   retro_atomic_store_release_int(&t_spam_done, 0);
   spam  = sthread_create(t_spam_thread, NULL);
   reads = 0;
   while (spam && !retro_atomic_load_acquire_int(&t_spam_done))
   {
      t_note(c, 7);
      if (coremidi_read(c, &ev) && ev.data[1] == 7)
         reads++;
      else
         failures++;
   }
   if (spam)
      sthread_join(spam);
   CHECK(c->input_endpoint == 0x104, "a removal of another device dropped the input");
   coremidi_free(c);

   if (failures)
   {
      printf("FAIL  %u checks\n", failures);
      return 1;
   }
   printf("ok    reads and writes list no devices until one is removed; "
         "removed inputs and outputs found by every instance; %u reads "
         "beside removals on another thread\n", reads);
   return 0;
}
