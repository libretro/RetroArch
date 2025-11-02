/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2025 The RetroArch team
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

#include <CoreMIDI/CoreMIDI.h>
#include <libretro.h>
#include <lists/string_list.h>
#include <string/stdstring.h>
#include <rthreads/rthreads.h>

#include "../midi_driver.h"
#include "../../verbosity.h"

#define CORE_MIDI_QUEUE_SIZE 1024
#define CORE_MIDI_MAX_EVENT_SIZE 256

/* Persistent CoreMIDI client shared across all driver instances.
 * This avoids XPC race conditions from rapid client dispose/recreate cycles.
 * CoreMIDI clients are designed to be long-lived; ports are disposable. */
static MIDIClientRef shared_midi_client = 0;

typedef struct
{
    uint8_t data[CORE_MIDI_MAX_EVENT_SIZE]; /* Inline data buffer */
    size_t data_size;
    uint32_t delta_time;
} coremidi_event_t;

typedef struct
{
    coremidi_event_t events[CORE_MIDI_QUEUE_SIZE]; /* Event buffer */
    int read_index;         /* Current read position */
    int write_index;        /* Current write position */
} coremidi_queue_t;

typedef struct
{
    MIDIClientRef client;            /* CoreMIDI client */
    MIDIPortRef input_port;          /* Input port for receiving MIDI data */
    MIDIPortRef output_port;         /* Output port for sending MIDI data */
    MIDIEndpointRef input_endpoint;  /* Selected input endpoint */
    MIDIEndpointRef output_endpoint; /* Selected output endpoint */
    coremidi_queue_t input_queue;    /* Queue for incoming MIDI events */
    slock_t *queue_lock;             /* Mutex for queue synchronization */
    volatile bool is_shutting_down;  /* Flag to prevent callbacks during shutdown */
} coremidi_t;

/* Global list of active driver instances for notification handling */
#define MAX_MIDI_INSTANCES 8
static coremidi_t *active_instances[MAX_MIDI_INSTANCES];
static slock_t *instances_lock = NULL;

/* Clear the queue (must be called with lock held) */
static void coremidi_queue_clear(coremidi_queue_t *q)
{
    q->read_index = 0;
    q->write_index = 0;
}

/* Register a driver instance in the global list */
static bool register_instance(coremidi_t *d)
{
    int i;
    if (!instances_lock)
        return false;

    slock_lock(instances_lock);
    for (i = 0; i < MAX_MIDI_INSTANCES; i++)
    {
        if (!active_instances[i])
        {
            active_instances[i] = d;
            slock_unlock(instances_lock);
            return true;
        }
    }
    slock_unlock(instances_lock);

    RARCH_ERR("[MIDI] Too many MIDI instances (max %d).\n", MAX_MIDI_INSTANCES);
    return false;
}

/* Unregister a driver instance from the global list */
static void unregister_instance(coremidi_t *d)
{
    int i;
    if (!instances_lock || !d)
        return;

    slock_lock(instances_lock);
    for (i = 0; i < MAX_MIDI_INSTANCES; i++)
    {
        if (active_instances[i] == d)
        {
            active_instances[i] = NULL;
            break;
        }
    }
    slock_unlock(instances_lock);
}

/* MIDI notification callback for device changes */
static void midi_notify_callback(const MIDINotification *message, void *refCon)
{
    int i;

    if (!instances_lock)
        return;

    switch (message->messageID)
    {
        case kMIDIMsgObjectRemoved:
        {
            const MIDIObjectAddRemoveNotification *notify =
                (const MIDIObjectAddRemoveNotification *)message;

            /* Check all active instances for matching endpoints */
            slock_lock(instances_lock);
            for (i = 0; i < MAX_MIDI_INSTANCES; i++)
            {
                coremidi_t *d = active_instances[i];
                if (!d || d->is_shutting_down)
                    continue;

                /* Check if this instance's endpoints were removed */
                if (notify->child == d->input_endpoint)
                {
                    RARCH_LOG("[MIDI] Input endpoint removed, clearing.\n");
                    d->input_endpoint = 0;
                    if (d->queue_lock)
                    {
                        slock_lock(d->queue_lock);
                        coremidi_queue_clear(&d->input_queue);
                        slock_unlock(d->queue_lock);
                    }
                }
                if (notify->child == d->output_endpoint)
                {
                    RARCH_LOG("[MIDI] Output endpoint removed, clearing.\n");
                    d->output_endpoint = 0;
                }
            }
            slock_unlock(instances_lock);
            break;
        }
        case kMIDIMsgSetupChanged:
            RARCH_LOG("[MIDI] MIDI setup changed.\n");
            break;
        default:
            break;
    }
}

/* Validate that an endpoint still exists in the system */
static bool coremidi_validate_endpoint(MIDIEndpointRef endpoint, bool is_source)
{
    ItemCount i, count;

    if (!endpoint)
        return false;

    if (is_source)
    {
        count = MIDIGetNumberOfSources();
        for (i = 0; i < count; i++)
        {
            if (MIDIGetSource(i) == endpoint)
                return true;
        }
    }
    else
    {
        count = MIDIGetNumberOfDestinations();
        for (i = 0; i < count; i++)
        {
            if (MIDIGetDestination(i) == endpoint)
                return true;
        }
    }

    return false;
}

/* Write to the queue */
static bool coremidi_queue_write(coremidi_queue_t *q, const midi_event_t *ev)
{
    size_t copy_size;
    int next_write = (q->write_index + 1) % CORE_MIDI_QUEUE_SIZE;
    if (next_write == q->read_index) /* Queue full */
        return false;

    /* Validate event data size */
    if (!ev->data || ev->data_size == 0 || ev->data_size > CORE_MIDI_MAX_EVENT_SIZE)
        return false;

    /* Copy data inline instead of storing pointer */
    copy_size = ev->data_size;
    memcpy(q->events[q->write_index].data, ev->data, copy_size);
    q->events[q->write_index].data_size = copy_size;
    q->events[q->write_index].delta_time = ev->delta_time;

    q->write_index = next_write;
    return true;
}

/* MIDIReadProc callback function */
static void midi_read_callback(const MIDIPacketList *pktlist,
   void *readProcRefCon, void *srcConnRefCon)
{
    uint32_t i;
    midi_event_t event;
    coremidi_t *d = (coremidi_t *)readProcRefCon;
    const MIDIPacket *packet;

    /* CRITICAL: Check shutdown flag immediately to prevent use-after-free */
    if (!d || d->is_shutting_down)
        return;

    /* Early validation */
    if (!d->queue_lock)
        return;

    packet = &pktlist->packet[0];

    for (i = 0; i < pktlist->numPackets; i++)
    {
        const uint8_t *data = packet->data;
        size_t length       = packet->length;
        const MIDITimeStamp timestamp = packet->timeStamp;

        while (length > 0)
        {
            size_t msg_size = midi_driver_get_event_size(data[0]);
            if (msg_size == 0 || msg_size > length)
                break;

            event.data       = (uint8_t*)data;
            event.data_size  = msg_size;
            event.delta_time = (uint32_t)timestamp;

            /* Add to queue with lock protection */
            slock_lock(d->queue_lock);
            coremidi_queue_write(&d->input_queue, &event);
            slock_unlock(d->queue_lock);

            data   += msg_size;
            length -= msg_size;
        }

        packet = MIDIPacketNext(packet);
    }
}

/* Initialize the CoreMIDI client and ports */
static void *coremidi_init(const char *input, const char *output)
{
    OSStatus err;
    coremidi_t *d;

    /* Initialize global instance lock on first call */
    if (!instances_lock)
    {
        instances_lock = slock_new();
        if (!instances_lock)
        {
            RARCH_ERR("[MIDI] Failed to create instances lock.\n");
            return NULL;
        }
    }

    /* Create persistent shared client on first call */
    if (!shared_midi_client)
    {
        err = MIDIClientCreate(CFSTR("RetroArch MIDI Client"),
                       midi_notify_callback, NULL, &shared_midi_client);
        if (err != noErr)
        {
            RARCH_ERR("[MIDI] MIDIClientCreate failed: %d.\n", err);
            return NULL;
        }
        RARCH_LOG("[MIDI] Created persistent CoreMIDI client.\n");
    }

    d = (coremidi_t *)calloc(1, sizeof(coremidi_t));
    if (!d)
    {
        RARCH_ERR("[MIDI] Out of memory.\n");
        return NULL;
    }

    /* Initialize synchronization primitives */
    d->queue_lock = slock_new();
    if (!d->queue_lock)
    {
        RARCH_ERR("[MIDI] Failed to create queue lock.\n");
        free(d);
        return NULL;
    }

    d->is_shutting_down = false;

    /* Store reference to shared client */
    d->client = shared_midi_client;

    /* Register this instance in the global list */
    if (!register_instance(d))
    {
        RARCH_ERR("[MIDI] Failed to register MIDI instance.\n");
        slock_free(d->queue_lock);
        free(d);
        return NULL;
    }

    /* Create input port if specified */
    if (input)
    {
        err = MIDIInputPortCreate(d->client, CFSTR("Input Port"),
              midi_read_callback, d, &d->input_port);
        if (err != noErr)
        {
            RARCH_ERR("[MIDI] MIDIInputPortCreate failed: %d.\n", err);
            unregister_instance(d);
            slock_free(d->queue_lock);
            free(d);
            return NULL;
        }
        RARCH_LOG("[MIDI] CoreMIDI input port created successfully.\n");
    }

    /* Create output port if specified */
    if (output)
    {
        err = MIDIOutputPortCreate(d->client,
              CFSTR("Output Port"), &d->output_port);
        if (err != noErr)
        {
            RARCH_ERR("[MIDI] MIDIOutputPortCreate failed: %d.\n", err);
            if (d->input_port)
                MIDIPortDispose(d->input_port);
            unregister_instance(d);
            slock_free(d->queue_lock);
            free(d);
            return NULL;
        }
        RARCH_LOG("[MIDI] CoreMIDI output port created successfully.\n");
    }

    return d;
}

/* Get available input devices */
static bool coremidi_get_avail_inputs(struct string_list *inputs)
{
    ItemCount count = MIDIGetNumberOfSources();
    union string_list_elem_attr attr = {0};

    for (ItemCount i = 0; i < count; i++)
    {
        MIDIEndpointRef endpoint = MIDIGetSource(i);
        CFStringRef name;
        char buf[256];

        OSStatus err = MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &name);
        if (err == noErr)
        {
            if (CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8))
            {
                if (!string_list_append(inputs, buf, attr))
                {
                    RARCH_ERR("[MIDI] Failed to append input device to list: %s.\n", buf);
                    CFRelease(name);
                    return false;
                }
                RARCH_LOG("[MIDI] Input device added to list: %s.\n", buf);
            }
            CFRelease(name);
        }
        else
        {
            RARCH_WARN("[MIDI] Failed to get display name for input device %d: %d.\n", i, err);
        }
    }

    return true;
}

/* Get available output devices */
static bool coremidi_get_avail_outputs(struct string_list *outputs)
{
    ItemCount count = MIDIGetNumberOfDestinations();
    union string_list_elem_attr attr = {0};

    for (ItemCount i = 0; i < count; i++)
    {
        char buf[256];
        MIDIEndpointRef endpoint = MIDIGetDestination(i);
        CFStringRef name;
        OSStatus err = MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &name);
        if (err == noErr)
        {
            if (CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8))
            {
                if (!string_list_append(outputs, buf, attr))
                {
                    RARCH_ERR("[MIDI] Failed to append output device to list: %s.\n", buf);
                    CFRelease(name);
                    return false;
                }
                RARCH_LOG("[MIDI] Output device added to list: %s.\n", buf);
            }
            CFRelease(name);
        }
        else
        {
            RARCH_WARN("[MIDI] Failed to get display name for output device %d: %d.\n", i, err);
        }
    }

    return true;
}

/* Set the input device */
static bool coremidi_set_input(void *p, const char *input)
{
    ItemCount count;
    coremidi_t *d = (coremidi_t *)p;
    if (!d || !d->input_port)
    {
        RARCH_WARN("[MIDI] Input port not initialized.\n");
        return false;
    }

    /* Disconnect current input endpoint */
    if (d->input_endpoint)
    {
        RARCH_LOG("[MIDI] Disconnecting current input endpoint.\n");
        MIDIPortDisconnectSource(d->input_port, d->input_endpoint);
        d->input_endpoint = 0;

        /* Clear the input queue when disconnecting */
        if (d->queue_lock)
        {
            slock_lock(d->queue_lock);
            coremidi_queue_clear(&d->input_queue);
            slock_unlock(d->queue_lock);
        }
    }

    /* If input is NULL or "Off", just return success */
    if (!input || string_is_equal(input, "Off"))
    {
        RARCH_LOG("[MIDI] Input set to Off.\n");
        return true;
    }

    /* Find the input endpoint by name */
    count = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < count; i++)
    {
        char buf[256];
        MIDIEndpointRef endpoint = MIDIGetSource(i);
        CFStringRef name;
        OSStatus err = MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &name);
        if (err == noErr)
        {
            if (CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8)
                && string_is_equal(input, buf))
            {
                err = MIDIPortConnectSource(d->input_port, endpoint, NULL);
                CFRelease(name);
                if (err == noErr)
                {
                    d->input_endpoint = endpoint;
                    RARCH_LOG("[MIDI] Input endpoint set to %s.\n", buf);
                    return true;
                }
                RARCH_WARN("[MIDI] Failed to connect input endpoint: %d.\n", err);
                return false;
            }
            CFRelease(name);
        }
    }

    RARCH_WARN("[MIDI] Input device not found: %s.\n", input);
    return false;
}

/* Set the output device */
static bool coremidi_set_output(void *p, const char *output)
{
    ItemCount i, count;
    coremidi_t *d = (coremidi_t *)p;
    if (!d || !d->output_port)
    {
        RARCH_WARN("[MIDI] Output port not initialized.\n");
        return false;
    }

    /* If output is NULL or "Off", just return success */
    if (!output || string_is_equal(output, "Off"))
    {
        RARCH_LOG("[MIDI] Output set to Off.\n");
        d->output_endpoint = 0;
        return true;
    }

    /* Find the output endpoint by name */
    count = MIDIGetNumberOfDestinations();
    for (i = 0; i < count; i++)
    {
        char buf[256];
        MIDIEndpointRef endpoint = MIDIGetDestination(i);
        CFStringRef name;
        OSStatus err = MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &name);
        if (err == noErr)
        {
            if (CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8)
                && string_is_equal(output, buf))
            {
                d->output_endpoint = endpoint;
                RARCH_LOG("[MIDI] Output endpoint set to %s.\n", buf);
                CFRelease(name);
                return true;
            }
            CFRelease(name);
        }
    }

    RARCH_WARN("[MIDI] Output device not found: %s.\n", output);
    return false;
}

/* Read from the queue */
static bool coremidi_queue_read(coremidi_queue_t *q, midi_event_t *ev)
{
    if (q->read_index == q->write_index) /* Queue empty */
        return false;

    /* Return pointer to inline data buffer */
    ev->data = q->events[q->read_index].data;
    ev->data_size = q->events[q->read_index].data_size;
    ev->delta_time = q->events[q->read_index].delta_time;

    q->read_index = (q->read_index + 1) % CORE_MIDI_QUEUE_SIZE;
    return true;
}

/* Read a MIDI event */
static bool coremidi_read(void *p, midi_event_t *event)
{
    bool result;
    coremidi_t *d = (coremidi_t *)p;

    if (!d || !event)
    {
        RARCH_WARN("[MIDI] Invalid parameters in coremidi_read.\n");
        return false;
    }

    if (!d->input_port || !d->input_endpoint)
    {
        RARCH_WARN("[MIDI] Input not configured.\n");
        return false;
    }

    /* Validate endpoint still exists */
    if (!coremidi_validate_endpoint(d->input_endpoint, true))
    {
        RARCH_WARN("[MIDI] Input endpoint no longer valid.\n");
        d->input_endpoint = 0;
        return false;
    }

    if (!d->queue_lock)
        return false;

    slock_lock(d->queue_lock);
    result = coremidi_queue_read(&d->input_queue, event);
    slock_unlock(d->queue_lock);

#if DEBUG
    RARCH_LOG("[MIDI] Input queue read result: %d.\n", result);
#endif
    return result;
}

/* Write a MIDI event */
static bool coremidi_write(void *p, const midi_event_t *event)
{
    coremidi_t *d = (coremidi_t *)p;

    if (!d || !event)
    {
        RARCH_WARN("[MIDI] Invalid parameters in coremidi_write.\n");
        return false;
    }

    if (!d->output_port || !d->output_endpoint)
    {
        RARCH_WARN("[MIDI] Output not configured.\n");
        return false;
    }

    /* Validate endpoint still exists */
    if (!coremidi_validate_endpoint(d->output_endpoint, false))
    {
        RARCH_WARN("[MIDI] Output endpoint no longer valid.\n");
        d->output_endpoint = 0;
        return false;
    }

    /* Validate event data */
    if (!event->data || event->data_size == 0 || event->data_size > 65535)
    {
        RARCH_WARN("[MIDI] Invalid event data in coremidi_write.\n");
        return false;
    }

    /* Create a MIDIPacketList with sufficient buffer size */
    char packetListBuffer[sizeof(MIDIPacketList) + sizeof(MIDIPacket) + event->data_size];
    MIDIPacketList *packetList = (MIDIPacketList *)packetListBuffer;
    MIDIPacket *packet = MIDIPacketListInit(packetList);

    /* Add the event to the packet list */
    if (!(packet = MIDIPacketListAdd(packetList, sizeof(packetListBuffer),
             packet, 0, event->data_size, event->data)))
    {
        RARCH_WARN("[MIDI] Failed to create MIDIPacketList.\n");
        return false;
    }

    /* Send the packet */
    OSStatus err = MIDISend(d->output_port, d->output_endpoint, packetList);
    if (err != noErr)
    {
        RARCH_WARN("[MIDI] MIDISend failed: %d.\n", err);
        return false;
    }

    return true;
}

/* Flush the output buffer */
static bool coremidi_flush(void *p)
{
    coremidi_t *d = (coremidi_t *)p;

    if (!d)
    {
        RARCH_WARN("[MIDI] Invalid parameters in coremidi_flush.\n");
        return false;
    }

    if (!d->output_port || !d->output_endpoint)
    {
        RARCH_WARN("[MIDI] Output not configured.\n");
        return false;
    }

    return true;
}

/* Free resources and cleanup */
static void coremidi_free(void *p)
{
    coremidi_t *d = (coremidi_t *)p;

    if (!d)
    {
        RARCH_WARN("[MIDI] Invalid parameters in coremidi_free.\n");
        return;
    }

    /* CRITICAL: Set shutdown flag and unregister from global list FIRST.
     * This prevents callbacks from accessing this instance. */
    d->is_shutting_down = true;
    unregister_instance(d);

    /* Now safe to tear down MIDI resources */
    if (d->input_port)
    {
        RARCH_LOG("[MIDI] Disconnecting and disposing input port...\n");
        if (d->input_endpoint)
            MIDIPortDisconnectSource(d->input_port, d->input_endpoint);
        MIDIPortDispose(d->input_port);
    }

    if (d->output_port)
    {
        RARCH_LOG("[MIDI] Disposing output port...\n");
        MIDIPortDispose(d->output_port);
    }

    /* Note: Don't dispose shared_midi_client, it's persistent */

    /* Free synchronization primitives */
    if (d->queue_lock)
        slock_free(d->queue_lock);

    /* Finally, free the driver instance */
    free(d);
    RARCH_LOG("[MIDI] CoreMIDI driver freed.\n");
}

/* CoreMIDI driver API */
midi_driver_t midi_coremidi = {
    .ident = "coremidi",
    .init = coremidi_init,
    .free = coremidi_free,
    .set_input = coremidi_set_input,
    .set_output = coremidi_set_output,
    .read = coremidi_read,
    .write = coremidi_write,
    .flush = coremidi_flush,
    .get_avail_inputs = coremidi_get_avail_inputs,
    .get_avail_outputs = coremidi_get_avail_outputs,
};
