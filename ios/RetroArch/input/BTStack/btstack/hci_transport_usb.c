/*
 * Copyright (C) 2009-2012 by Matthias Ringwald
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at btstack@ringwald.ch
 *
 */

/*
 *  hci_transport_usb.c
 *
 *  HCI Transport API implementation for USB
 *
 *  Created by Matthias Ringwald on 7/5/09.
 */

// delock bt class 2 - csr
// 0a12:0001 (bus 27, device 2)

// Interface Number - Alternate Setting - suggested Endpoint Address - Endpoint Type - Suggested Max Packet Size 
// HCI Commands 0 0 0x00 Control 8/16/32/64 
// HCI Events   0 0 0x81 Interrupt (IN) 16 
// ACL Data     0 0 0x82 Bulk (IN) 32/64 
// ACL Data     0 0 0x02 Bulk (OUT) 32/64 

#include <stdio.h>
#include <strings.h>
#include <string.h>
#include <unistd.h>   /* UNIX standard function definitions */
#include <sys/types.h>

#include <libusb-1.0/libusb.h>

#include "config.h"

#include "debug.h"
#include "hci.h"
#include "hci_transport.h"
#include "hci_dump.h"

// prototypes
static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size); 
static int usb_close(void *transport_config);
    
enum {
    LIB_USB_CLOSED = 0,
    LIB_USB_OPENED,
    LIB_USB_DEVICE_OPENDED,
    LIB_USB_KERNEL_DETACHED,
    LIB_USB_INTERFACE_CLAIMED,
    LIB_USB_TRANSFERS_ALLOCATED
} libusb_state = LIB_USB_CLOSED;

// single instance
static hci_transport_t * hci_transport_usb = NULL;

static  void (*packet_handler)(uint8_t packet_type, uint8_t *packet, uint16_t size) = dummy_handler;

// libusb
#if !USB_VENDOR_ID || !USB_PRODUCT_ID
static struct libusb_device_descriptor desc;
static libusb_device        * dev;
#endif
static libusb_device_handle * handle;

#define ASYNC_BUFFERS 4

static struct libusb_transfer *event_in_transfer[ASYNC_BUFFERS];
static struct libusb_transfer *bulk_in_transfer[ASYNC_BUFFERS];

static uint8_t hci_event_in_buffer[ASYNC_BUFFERS][HCI_ACL_BUFFER_SIZE]; // bigger than largest packet
static uint8_t hci_bulk_in_buffer[ASYNC_BUFFERS][HCI_ACL_BUFFER_SIZE];  // bigger than largest packet

// For (ab)use as a linked list of received packets
static struct libusb_transfer *handle_packet;

static int doing_pollfds;
static timer_source_t usb_timer;
static int usb_timer_active;

// endpoint addresses
static int event_in_addr;
static int acl_in_addr;
static int acl_out_addr;

#if !USB_VENDOR_ID || !USB_PRODUCT_ID
void scan_for_bt_endpoints(void) {
    int r;

    // get endpoints from interface descriptor
    struct libusb_config_descriptor *config_descriptor;
    r = libusb_get_active_config_descriptor(dev, &config_descriptor);
    log_info("configuration: %u interfaces\n", config_descriptor->bNumInterfaces);

    const struct libusb_interface *interface = config_descriptor->interface;
    const struct libusb_interface_descriptor * interface0descriptor = interface->altsetting;
    log_info("interface 0: %u endpoints\n", interface0descriptor->bNumEndpoints);

    const struct libusb_endpoint_descriptor *endpoint = interface0descriptor->endpoint;

    for (r=0;r<interface0descriptor->bNumEndpoints;r++,endpoint++){
        log_info("endpoint %x, attributes %x\n", endpoint->bEndpointAddress, endpoint->bmAttributes);

        if ((endpoint->bmAttributes & 0x3) == LIBUSB_TRANSFER_TYPE_INTERRUPT){
            event_in_addr = endpoint->bEndpointAddress;
            log_info("Using 0x%2.2X for HCI Events\n", event_in_addr);
        }
        if ((endpoint->bmAttributes & 0x3) == LIBUSB_TRANSFER_TYPE_BULK){
            if (endpoint->bEndpointAddress & 0x80) {
                acl_in_addr = endpoint->bEndpointAddress;
                log_info("Using 0x%2.2X for ACL Data In\n", acl_in_addr);
            } else {
                acl_out_addr = endpoint->bEndpointAddress;
                log_info("Using 0x%2.2X for ACL Data Out\n", acl_out_addr);
            }
        }
    }
}

static libusb_device * scan_for_bt_device(libusb_device **devs) {
    int i = 0;
    while ((dev = devs[i++]) != NULL) {
        int r = libusb_get_device_descriptor(dev, &desc);
        if (r < 0) {
            log_error("failed to get device descriptor");
            return 0;
        }
        
        log_info("%04x:%04x (bus %d, device %d) - class %x subclass %x protocol %x \n",
               desc.idVendor, desc.idProduct,
               libusb_get_bus_number(dev), libusb_get_device_address(dev),
               desc.bDeviceClass, desc.bDeviceSubClass, desc.bDeviceProtocol);
        
        // @TODO: detect BT USB Dongle based on character and not by id
        // The class code (bDeviceClass) is 0xE0 – Wireless Controller. 
        // The SubClass code (bDeviceSubClass) is 0x01 – RF Controller. 
        // The Protocol code (bDeviceProtocol) is 0x01 – Bluetooth programming.
        // if (desc.bDeviceClass == 0xe0 && desc.bDeviceSubClass == 0x01 && desc.bDeviceProtocol == 0x01){
        if (desc.bDeviceClass == 0xE0 && desc.bDeviceSubClass == 0x01
                && desc.bDeviceProtocol == 0x01) {
            log_info("BT Dongle found.\n");
            return dev;
        }
    }
    return NULL;
}
#endif

static void LIBUSB_CALL async_callback(struct libusb_transfer *transfer)
{
    int r;
    //log_info("in async_callback %d\n", transfer->endpoint);

    if (transfer->status == LIBUSB_TRANSFER_COMPLETED ||
            (transfer->status == LIBUSB_TRANSFER_TIMED_OUT && transfer->actual_length > 0)) {
        if (handle_packet == NULL) {
            handle_packet = transfer;
        } else {
            // Walk to end of list and add current packet there
            struct libusb_transfer *temp = handle_packet;

            while (temp->user_data) {
                temp = temp->user_data;
            }
            temp->user_data = transfer;
        }
    } else {
        // No usable data, just resubmit packet
        if (libusb_state == LIB_USB_TRANSFERS_ALLOCATED) {
            r = libusb_submit_transfer(transfer);

            if (r) {
                log_error("Error re-submitting transfer %d\n", r);
            }
        }
    }
}

static int usb_process_ds(struct data_source *ds) {
    struct timeval tv;
    int r;

    //log_info("in usb_process_ds\n");

    if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED) return -1;

    // always handling an event as we're called when data is ready
    memset(&tv, 0, sizeof(struct timeval));
    libusb_handle_events_timeout(NULL, &tv);

    // Handle any packet in the order that they were received
    while (handle_packet) {
        void * next = handle_packet->user_data;
        //log_info("handle packet %x, endpoint", handle_packet, handle_packet->endpoint);

        if (handle_packet->endpoint == event_in_addr) {
                hci_dump_packet( HCI_EVENT_PACKET, 1, handle_packet-> buffer,
                    handle_packet->actual_length);
                packet_handler(HCI_EVENT_PACKET, handle_packet-> buffer,
                    handle_packet->actual_length);
        }

        if (handle_packet->endpoint == acl_in_addr) {
                hci_dump_packet( HCI_ACL_DATA_PACKET, 1, handle_packet-> buffer,
                    handle_packet->actual_length);
                packet_handler(HCI_ACL_DATA_PACKET, handle_packet-> buffer,
                    handle_packet->actual_length);
        }

        // Re-submit transfer 
        if (libusb_state == LIB_USB_TRANSFERS_ALLOCATED) {
            handle_packet->user_data = NULL;
            r = libusb_submit_transfer(handle_packet);

            if (r) {
                log_error("Error re-submitting transfer %d\n", r);
            }
        }

        // Move to next in the list of packets to handle
        if (next) {
            handle_packet = next;
        } else {
            handle_packet = NULL;
        }
    }

    return 0;
}

void usb_process_ts(timer_source_t *timer) {
    struct timeval tv, now;
    long msec;

    //log_info("in usb_process_ts\n");

    // Deactivate timer
    run_loop_remove_timer(&usb_timer);
    usb_timer_active = 0;

    if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED) return;

    // actuall handled the packet in the pollfds function
    usb_process_ds((struct data_source *) NULL);

    // Compute the amount of time until next event is due
    gettimeofday(&now, NULL);
    msec = (now.tv_sec - tv.tv_sec) * 1000;
    msec = (now.tv_usec - tv.tv_usec) / 1000;

    // Maximum wait time, async packet can come in earlier than timeout
    if (msec > 10) msec = 10;

    // Activate timer
    run_loop_set_timer(&usb_timer, msec);
    run_loop_add_timer(&usb_timer);
    usb_timer_active = 1;

    return;
}

static int usb_open(void *transport_config){
    int r,c;
#if !USB_VENDOR_ID || !USB_PRODUCT_ID
    libusb_device * aDev;
    libusb_device **devs;
    ssize_t cnt;
#endif

    handle_packet = NULL;

    // default endpoint addresses
    event_in_addr = 0x81; // EP1, IN interrupt
    acl_in_addr =   0x82; // EP2, IN bulk
    acl_out_addr =  0x02; // EP2, OUT bulk

    // USB init
    r = libusb_init(NULL);
    if (r < 0) return -1;

    libusb_state = LIB_USB_OPENED;

    // configure debug level
    libusb_set_debug(0,3);
    
#if USB_VENDOR_ID && USB_PRODUCT_ID
    // Use a specified device
    log_info("Want vend: %04x, prod: %04x\n", USB_VENDOR_ID, USB_PRODUCT_ID);
    handle = libusb_open_device_with_vid_pid(NULL, USB_VENDOR_ID, USB_PRODUCT_ID);

    if (!handle){
        log_error("libusb_open_device_with_vid_pid failed!\n");
        usb_close(handle);
        return -1;
    }
#else
    // Scan system for an appropriate device
    log_info("Scanning for a device");
    cnt = libusb_get_device_list(NULL, &devs);
    if (cnt < 0) {
        usb_close(handle);
        return -1;
    }
    // Find BT modul
    aDev  = scan_for_bt_device(devs);
    if (!aDev){
        libusb_free_device_list(devs, 1);
        usb_close(handle);
        return -1;
    }

    dev = aDev;
    r = libusb_open(dev, &handle);

    libusb_free_device_list(devs, 1);

    if (r < 0) {
        usb_close(handle);
        return r;
    }
#endif

    log_info("libusb open %d, handle %p\n", r, handle);
    libusb_state = LIB_USB_OPENED;

    // Detach OS driver (not possible for OS X)
#ifndef __APPLE__
    r = libusb_kernel_driver_active(handle, 0);
    if (r < 0) {
        log_error("libusb_kernel_driver_active error %d\n", r);
        usb_close(handle);
        return r;
    }

    if (r == 1) {
        r = libusb_detach_kernel_driver(handle, 0);
        if (r < 0) {
            log_error("libusb_detach_kernel_driver error %d\n", r);
            usb_close(handle);
            return r;
        }
    }
    log_info("libusb_detach_kernel_driver\n");
#endif
    libusb_state = LIB_USB_KERNEL_DETACHED;

    // reserve access to device
    log_info("claiming interface 0...\n");
    r = libusb_claim_interface(handle, 0);
    if (r < 0) {
        log_error("Error claiming interface %d\n", r);
        usb_close(handle);
        return r;
    }

    libusb_state = LIB_USB_INTERFACE_CLAIMED;
    log_info("claimed interface 0\n");
    
#if !USB_VENDOR_ID || !USB_PRODUCT_ID
    scan_for_bt_endpoints();
#endif

    // allocate transfer handlers
    for (c = 0 ; c < ASYNC_BUFFERS ; c++) {
        event_in_transfer[c] = libusb_alloc_transfer(0); // 0 isochronous transfers Events
        bulk_in_transfer[c]  = libusb_alloc_transfer(0); // 0 isochronous transfers ACL in
 
        if ( !event_in_transfer[c] || !bulk_in_transfer[c] ) {
            usb_close(handle);
            return LIBUSB_ERROR_NO_MEM;
        }
    }

    libusb_state = LIB_USB_TRANSFERS_ALLOCATED;

    for (c = 0 ; c < ASYNC_BUFFERS ; c++) {
        // configure event_in handlers
        libusb_fill_interrupt_transfer(event_in_transfer[c], handle, event_in_addr, 
                hci_event_in_buffer[c], HCI_ACL_BUFFER_SIZE, async_callback, NULL, 2000) ;
 
        r = libusb_submit_transfer(event_in_transfer[c]);
        if (r) {
            log_error("Error submitting interrupt transfer %d\n", r);
            usb_close(handle);
            return r;
        }
 
        // configure bulk_in handlers
        libusb_fill_bulk_transfer(bulk_in_transfer[c], handle, acl_in_addr, 
                hci_bulk_in_buffer[c], HCI_ACL_BUFFER_SIZE, async_callback, NULL, 2000) ;
 
        r = libusb_submit_transfer(bulk_in_transfer[c]);
        if (r) {
            log_error("Error submitting bulk in transfer %d\n", r);
            usb_close(handle);
            return r;
        }
    }

    // Check for pollfds functionality
    doing_pollfds = libusb_pollfds_handle_timeouts(NULL);
    
    if (doing_pollfds) {
        log_info("Async using pollfds:\n");

        const struct libusb_pollfd ** pollfd = libusb_get_pollfds(NULL);
        for (r = 0 ; pollfd[r] ; r++) {
            data_source_t *ds = malloc(sizeof(data_source_t));
            ds->fd = pollfd[r]->fd;
            ds->process = usb_process_ds;
            run_loop_add_data_source(ds);
            log_info("%u: %p fd: %u, events %x\n", r, pollfd[r], pollfd[r]->fd, pollfd[r]->events);
        }
    } else {
        log_info("Async using timers:\n");

        usb_timer.process = usb_process_ts;
        run_loop_set_timer(&usb_timer, 100);
        run_loop_add_timer(&usb_timer);
        usb_timer_active = 1;
    }

    return 0;
}
static int usb_close(void *transport_config){
    int c;
    // @TODO: remove all run loops!

    switch (libusb_state){
        case LIB_USB_CLOSED:
            break;

        case LIB_USB_TRANSFERS_ALLOCATED:
            libusb_state = LIB_USB_INTERFACE_CLAIMED;

            if(usb_timer_active) {
                run_loop_remove_timer(&usb_timer);
                usb_timer_active = 0;
            }

            // Cancel any asynchronous transfers
            for (c = 0 ; c < ASYNC_BUFFERS ; c++) {
                libusb_cancel_transfer(event_in_transfer[c]);
                libusb_cancel_transfer(bulk_in_transfer[c]);
            }

            /* TODO - find a better way to ensure that all transfers have completed */
            struct timeval tv;
            memset(&tv, 0, sizeof(struct timeval));
            libusb_handle_events_timeout(NULL, &tv);

        case LIB_USB_INTERFACE_CLAIMED:
            for (c = 0 ; c < ASYNC_BUFFERS ; c++) {
                if (event_in_transfer[c]) libusb_free_transfer(event_in_transfer[c]);
                if (bulk_in_transfer[c])  libusb_free_transfer(bulk_in_transfer[c]);
            }

            libusb_release_interface(handle, 0);

        case LIB_USB_KERNEL_DETACHED:
#ifndef __APPLE__
            libusb_attach_kernel_driver (handle, 0);
#endif
        case LIB_USB_DEVICE_OPENDED:
            libusb_close(handle);

        case LIB_USB_OPENED:
            libusb_exit(NULL);
    }
    return 0;
}

static int usb_send_cmd_packet(uint8_t *packet, int size){
    int r;

    if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED) return -1;
    
    hci_dump_packet( HCI_COMMAND_DATA_PACKET, 0, packet, size);
    
    // Use synchronous call to sent out command
    r = libusb_control_transfer(handle, 
        LIBUSB_REQUEST_TYPE_CLASS | LIBUSB_RECIPIENT_INTERFACE,
        0, 0, 0, packet, size, 2000);

    if (r < 0 || r !=size ) {
        log_error("Error submitting control transfer %d\n", r);
        return r;
    }
    
    return 0;
}

static int usb_send_acl_packet(uint8_t *packet, int size){
    int r, t;
    
    if (libusb_state != LIB_USB_TRANSFERS_ALLOCATED) return -1;
    
    hci_dump_packet( HCI_ACL_DATA_PACKET, 0, packet, size);
    
    // Use synchronous call to sent out data
    r = libusb_bulk_transfer(handle, acl_out_addr, packet, size, &t, 1000);
    if(r < 0){
        log_error("Error submitting data transfer");
    }

    return 0;
}

static int usb_send_packet(uint8_t packet_type, uint8_t * packet, int size){
    switch (packet_type){
        case HCI_COMMAND_DATA_PACKET:
            return usb_send_cmd_packet(packet, size);
        case HCI_ACL_DATA_PACKET:
            return usb_send_acl_packet(packet, size);
        default:
            return -1;
    }
}

static void usb_register_packet_handler(void (*handler)(uint8_t packet_type, uint8_t *packet, uint16_t size)){
    log_info("registering packet handler\n");
    packet_handler = handler;
}

static const char * usb_get_transport_name(void){
    return "USB";
}

static void dummy_handler(uint8_t packet_type, uint8_t *packet, uint16_t size){
}

// get usb singleton
hci_transport_t * hci_transport_usb_instance() {
    if (!hci_transport_usb) {
        hci_transport_usb = malloc( sizeof(hci_transport_t));
        hci_transport_usb->open                          = usb_open;
        hci_transport_usb->close                         = usb_close;
        hci_transport_usb->send_packet                   = usb_send_packet;
        hci_transport_usb->register_packet_handler       = usb_register_packet_handler;
        hci_transport_usb->get_transport_name            = usb_get_transport_name;
        hci_transport_usb->set_baudrate                  = NULL;
        hci_transport_usb->can_send_packet_now           = NULL;
    }
    return hci_transport_usb;
}
