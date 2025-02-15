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

#include "../midi_driver.h"
#include "../../verbosity.h"

#define CORE_MIDI_QUEUE_SIZE 1024

typedef struct {
    midi_event_t events[CORE_MIDI_QUEUE_SIZE]; ///< Event buffer
    int read_index;         ///< Current read position
    int write_index;        ///< Current write position
} coremidi_queue_t;

typedef struct {
    MIDIClientRef client;       ///< CoreMIDI client
    MIDIPortRef input_port;     ///< Input port for receiving MIDI data
    MIDIPortRef output_port;    ///< Output port for sending MIDI data
    MIDIEndpointRef input_endpoint; ///< Selected input endpoint
    MIDIEndpointRef output_endpoint; ///< Selected output endpoint
    coremidi_queue_t input_queue; ///< Queue for incoming MIDI events
} coremidi_t;

/// Write to the queue
static bool coremidi_queue_write(coremidi_queue_t *q, const midi_event_t *ev) {
    int next_write = (q->write_index + 1) % CORE_MIDI_QUEUE_SIZE;
    if (next_write == q->read_index) // Queue full
        return false;

    memcpy(&q->events[q->write_index], ev, sizeof(*ev));
    q->write_index = next_write;
    return true;
}

/// MIDIReadProc callback function
static void midi_read_callback(const MIDIPacketList *pktlist, void *readProcRefCon, void *srcConnRefCon) {
    coremidi_t *d = (coremidi_t *)readProcRefCon;
    const MIDIPacket *packet = &pktlist->packet[0];
    midi_event_t event;
    uint32_t i;

    for (i = 0; i < pktlist->numPackets; i++) {
        const uint8_t *data = packet->data;
        size_t length = packet->length;
        const MIDITimeStamp timestamp = packet->timeStamp;

        while (length > 0) {
            size_t msg_size = midi_driver_get_event_size(data[0]);
            if (msg_size == 0 || msg_size > length)
                break;

            event.data = (uint8_t *)data;
            event.data_size = msg_size;
            event.delta_time = (uint32_t)timestamp;

            // Add to queue
            coremidi_queue_write(&d->input_queue, &event);

            data += msg_size;
            length -= msg_size;
        }

        packet = MIDIPacketNext(packet);
    }
}

/// Initialize the CoreMIDI client and ports
static void *coremidi_init(const char *input, const char *output) {
    coremidi_t *d = (coremidi_t *)calloc(1, sizeof(coremidi_t));
    if (!d) {
        RARCH_ERR("[MIDI]: Out of memory.\n");
        return NULL;
    }

    OSStatus err = MIDIClientCreate(CFSTR("RetroArch MIDI Client"), NULL, NULL, &d->client);
    if (err != noErr) {
        RARCH_ERR("[MIDI]: MIDIClientCreate failed: %d\n", err);
        free(d);
        return NULL;
    } else {
        RARCH_LOG("[MIDI]: CoreMIDI client created successfully\n");
    }

    // Create input port if specified
    if (input) {
        err = MIDIInputPortCreate(d->client, CFSTR("Input Port"), midi_read_callback, d, &d->input_port);
        if (err != noErr) {
            RARCH_ERR("[MIDI]: MIDIInputPortCreate failed: %d\n", err);
            MIDIClientDispose(d->client);
            free(d);
            return NULL;
        }
    } else {
        RARCH_LOG("[MIDI]: CoreMIDI input port created successfully\n");
    }

    // Create output port if specified
    if (output) {
        err = MIDIOutputPortCreate(d->client, CFSTR("Output Port"), &d->output_port);
        if (err != noErr) {
            RARCH_ERR("[MIDI]: MIDIOutputPortCreate failed: %d\n", err);
            if (d->input_port)
                MIDIPortDispose(d->input_port);
            MIDIClientDispose(d->client);
            free(d);
            return NULL;
        }
    } else {
        RARCH_LOG("[MIDI]: CoreMIDI output port created successfully\n");
    }

    return d;
}

/// Get available input devices
static bool coremidi_get_avail_inputs(struct string_list *inputs) {
    ItemCount count = MIDIGetNumberOfSources();
    union string_list_elem_attr attr = {0};

    for (ItemCount i = 0; i < count; i++) {
        MIDIEndpointRef endpoint = MIDIGetSource(i);
        CFStringRef name;
        char buf[256];

        OSStatus err = MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &name);
        if (err == noErr) {
            if (CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8)) {
                if (!string_list_append(inputs, buf, attr)) {
                    RARCH_ERR("[MIDI]: Failed to append input device to list: %s\n", buf);
                    CFRelease(name);
                    return false;
                } else {
                    RARCH_LOG("[MIDI]: Input device added to list: %s\n", buf);
                }
            }
            CFRelease(name);
        } else {
            RARCH_WARN("[MIDI]: Failed to get display name for input device %d: %d\n", i, err);
        }
    }

    return true;
}

/// Get available output devices
static bool coremidi_get_avail_outputs(struct string_list *outputs) {
    ItemCount count = MIDIGetNumberOfDestinations();
    union string_list_elem_attr attr = {0};

    for (ItemCount i = 0; i < count; i++) {
        MIDIEndpointRef endpoint = MIDIGetDestination(i);
        CFStringRef name;
        char buf[256];

        OSStatus err = MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &name);
        if (err == noErr) {
            if (CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8)) {
                if (!string_list_append(outputs, buf, attr)) {
                    RARCH_ERR("[MIDI]: Failed to append output device to list: %s\n", buf);
                    CFRelease(name);
                    return false;
                } else {
                    RARCH_LOG("[MIDI]: Output device added to list: %s\n", buf);
                }
            }
            CFRelease(name);
        } else {
            RARCH_WARN("[MIDI]: Failed to get display name for output device %d: %d\n", i, err);
        }
    }

    return true;
}

/// Set the input device
static bool coremidi_set_input(void *p, const char *input) {
    coremidi_t *d = (coremidi_t *)p;
    if (!d || !d->input_port) {
        RARCH_WARN("[MIDI]: Input port not initialized\n");
        return false;
    }

    // Disconnect current input endpoint
    if (d->input_endpoint) {
        RARCH_LOG("[MIDI]: Disconnecting current input endpoint\n");
        MIDIPortDisconnectSource(d->input_port, d->input_endpoint);
        d->input_endpoint = 0;
    }

    // If input is NULL or "Off", just return success
    if (!input || string_is_equal(input, "Off")) {
        RARCH_LOG("[MIDI]: Input set to Off\n");
        return true;
    }

    // Find the input endpoint by name
    ItemCount count = MIDIGetNumberOfSources();
    for (ItemCount i = 0; i < count; i++) {
        MIDIEndpointRef endpoint = MIDIGetSource(i);
        CFStringRef name;
        char buf[256];

        OSStatus err = MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &name);
        if (err == noErr) {
            if (CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8) && string_is_equal(input, buf)) {
                err = MIDIPortConnectSource(d->input_port, endpoint, NULL);
                CFRelease(name);
                if (err == noErr) {
                    d->input_endpoint = endpoint;
                    RARCH_LOG("[MIDI]: Input endpoint set to %s\n", buf);
                    return true;
                } else {
                    RARCH_WARN("[MIDI]: Failed to connect input endpoint: %d\n", err);
                    return false;
                }
            }
            CFRelease(name);
        }
    }

    RARCH_WARN("[MIDI]: Input device not found: %s\n", input);
    return false;
}

/// Set the output device
static bool coremidi_set_output(void *p, const char *output) {
    coremidi_t *d = (coremidi_t *)p;
    if (!d || !d->output_port) {
        RARCH_WARN("[MIDI]: Output port not initialized\n");
        return false;
    }

    // If output is NULL or "Off", just return success
    if (!output || string_is_equal(output, "Off")) {
        RARCH_LOG("[MIDI]: Output set to Off\n");
        d->output_endpoint = 0;
        return true;
    }

    // Find the output endpoint by name
    ItemCount count = MIDIGetNumberOfDestinations();
    for (ItemCount i = 0; i < count; i++) {
        MIDIEndpointRef endpoint = MIDIGetDestination(i);
        CFStringRef name;
        char buf[256];

        OSStatus err = MIDIObjectGetStringProperty(endpoint, kMIDIPropertyDisplayName, &name);
        if (err == noErr) {
            if (CFStringGetCString(name, buf, sizeof(buf), kCFStringEncodingUTF8) && string_is_equal(output, buf)) {
                d->output_endpoint = endpoint;
                RARCH_LOG("[MIDI]: Output endpoint set to %s\n", buf);
                CFRelease(name);
                return true;
            }
            CFRelease(name);
        }
    }

    RARCH_WARN("[MIDI]: Output device not found: %s\n", output);
    return false;
}

/// Read from the queue
static bool coremidi_queue_read(coremidi_queue_t *q, midi_event_t *ev) {
    if (q->read_index == q->write_index) // Queue empty
        return false;

    memcpy(ev, &q->events[q->read_index], sizeof(*ev));
    q->read_index = (q->read_index + 1) % CORE_MIDI_QUEUE_SIZE;
    return true;
}

/// Read a MIDI event
static bool coremidi_read(void *p, midi_event_t *event) {
    coremidi_t *d = (coremidi_t *)p;

    if (!d || !event) {
        RARCH_WARN("[MIDI]: Invalid parameters in coremidi_read\n");
        return false;
    }

    if (!d->input_port || !d->input_endpoint) {
        RARCH_WARN("[MIDI]: Input not configured\n");
        return false;
    }

    int result = coremidi_queue_read(&d->input_queue, event);
//    #if DEBUG
    RARCH_LOG("[MIDI]: Input queue read result: %d\n", result);
//    #endif
    return result;
}

/// Write a MIDI event
static bool coremidi_write(void *p, const midi_event_t *event) {
    coremidi_t *d = (coremidi_t *)p;

    if (!d || !event) {
        RARCH_WARN("[MIDI]: Invalid parameters in coremidi_write\n");
        return false;
    }

    if (!d->output_port || !d->output_endpoint) {
        RARCH_WARN("[MIDI]: Output not configured\n");
        return false;
    }

    // Validate event data
    if (!event->data || event->data_size == 0 || event->data_size > 65535) {
        RARCH_WARN("[MIDI]: Invalid event data in coremidi_write\n");
        return false;
    }

    // Create a MIDIPacketList with sufficient buffer size
    char packetListBuffer[sizeof(MIDIPacketList) + sizeof(MIDIPacket) + event->data_size];
    MIDIPacketList *packetList = (MIDIPacketList *)packetListBuffer;
    MIDIPacket *packet = MIDIPacketListInit(packetList);

    // Add the event to the packet list
    packet = MIDIPacketListAdd(packetList, sizeof(packetListBuffer), packet, 0, event->data_size, event->data);
    if (!packet) {
        RARCH_WARN("[MIDI]: Failed to create MIDIPacketList\n");
        return false;
    }

    // Send the packet
    OSStatus err = MIDISend(d->output_port, d->output_endpoint, packetList);
    if (err != noErr) {
        RARCH_WARN("[MIDI]: MIDISend failed: %d\n", err);
        return false;
    }

    return true;
}

/// Flush the output buffer
static bool coremidi_flush(void *p) {
    coremidi_t *d = (coremidi_t *)p;

    if (!d) {
        RARCH_WARN("[MIDI]: Invalid parameters in coremidi_flush\n");
        return false;
    }

    if (!d->output_port || !d->output_endpoint) {
        RARCH_WARN("[MIDI]: Output not configured\n");
        return false;
    }

    // CoreMIDI doesn't require explicit flushing, so we just return success
    // RARCH_LOG("[MIDI]: Output buffer flushed\n");
    return true;
}

/// Free resources and cleanup
static void coremidi_free(void *p) {
    coremidi_t *d = (coremidi_t *)p;

    if (!d) {
        RARCH_WARN("[MIDI]: Invalid parameters in coremidi_free\n");
        return;
    }

    // Clean up MIDI resources
    if (d->input_port) {
        RARCH_LOG("[MIDI]: Disconnecting input port\n");
        MIDIPortDisconnectSource(d->input_port, d->input_endpoint);
        MIDIPortDispose(d->input_port);
    }

    if (d->output_port) {
        RARCH_LOG("[MIDI]: Disconnecting output port\n");
        MIDIPortDisconnectSource(d->output_port, d->output_endpoint);
        MIDIPortDispose(d->output_port);
    }

    if (d->client) {
        RARCH_LOG("[MIDI]: Disposing of CoreMIDI client\n");
        MIDIClientDispose(d->client);
    }

    // Free the driver instance
    free(d);
}

/// CoreMIDI driver API
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
