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
 *  bt_control_iphone.c
 *
 *  control Bluetooth module using BlueTool
 *
 *  Created by Matthias Ringwald on 5/19/09.
 *  PowerManagement implementation by Jens David, DG1KJD on 20110801.
 *
 *  Bluetooth Toggle by BigBoss
 */

#include "config.h"

#include "bt_control_iphone.h"
#include "hci_transport.h"
#include "hci.h"
#include "debug.h"

#include <errno.h>
#include <fcntl.h>        // open
#include <stdlib.h>       // system, random, srandom
#include <stdio.h>        // sscanf, printf
#include <string.h>       // strcpy, strcat, strncmp
#include <sys/utsname.h>  // uname
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

#include "../SpringBoardAccess/SpringBoardAccess.h"

// minimal IOKit
#include <Availability.h>
#include <Foundation/Foundation.h>
#include <CoreFoundation/CoreFoundation.h>

// compile issue fix
#undef NSEC_PER_USEC
#undef USEC_PER_SEC
#undef NSEC_PER_SEC
// end of fix

#include <mach/mach.h>
#define IOKIT
#include <device/device_types.h>

// costants
#define sys_iokit                          err_system(0x38)
#define sub_iokit_common                   err_sub(0)

#define iokit_common_msg(message)          (UInt32)(sys_iokit|sub_iokit_common|message)

#define kIOMessageCanDevicePowerOff        iokit_common_msg(0x200)
#define kIOMessageDeviceWillPowerOff       iokit_common_msg(0x210)
#define kIOMessageDeviceWillNotPowerOff    iokit_common_msg(0x220)
#define kIOMessageDeviceHasPoweredOn       iokit_common_msg(0x230)
#define kIOMessageCanSystemPowerOff        iokit_common_msg(0x240)
#define kIOMessageSystemWillPowerOff       iokit_common_msg(0x250)
#define kIOMessageSystemWillNotPowerOff    iokit_common_msg(0x260)
#define kIOMessageCanSystemSleep           iokit_common_msg(0x270)
#define kIOMessageSystemWillSleep          iokit_common_msg(0x280)
#define kIOMessageSystemWillNotSleep       iokit_common_msg(0x290)
#define kIOMessageSystemHasPoweredOn       iokit_common_msg(0x300)
#define kIOMessageSystemWillRestart        iokit_common_msg(0x310)
#define kIOMessageSystemWillPowerOn        iokit_common_msg(0x320)

// types
typedef io_object_t io_connect_t;
typedef io_object_t	io_service_t;
typedef	kern_return_t	IOReturn;
typedef struct IONotificationPort * IONotificationPortRef;

// prototypes
kern_return_t IOMasterPort( mach_port_t	bootstrapPort, mach_port_t * masterPort );
CFMutableDictionaryRef IOServiceNameMatching(const char * name );
CFTypeRef IORegistryEntrySearchCFProperty(mach_port_t entry, const io_name_t plane,
                                          CFStringRef key, CFAllocatorRef allocator, UInt32 options );
mach_port_t IOServiceGetMatchingService(mach_port_t masterPort, CFDictionaryRef matching );
kern_return_t IOObjectRelease(mach_port_t object);

typedef void (*IOServiceInterestCallback)(void * refcon, io_service_t service, uint32_t messageType, void * messageArgument);
io_connect_t IORegisterForSystemPower (void * refcon, IONotificationPortRef * thePortRef,
                                       IOServiceInterestCallback callback, io_object_t * notifier );
IOReturn IODeregisterForSystemPower (io_object_t *notifier); 
CFRunLoopSourceRef IONotificationPortGetRunLoopSource(IONotificationPortRef	notify );
IOReturn IOAllowPowerChange ( io_connect_t kernelPort, long notificationID );
IOReturn IOCancelPowerChange ( io_connect_t kernelPort, long notificationID );

// local globals
static io_connect_t  root_port = 0; // a reference to the Root Power Domain IOService
static int power_notification_pipe_fds[2];
static data_source_t power_notification_ds;

static void (*power_notification_callback)(POWER_NOTIFICATION_t event) = NULL;

#define BUFF_LEN 80
static char buffer[BUFF_LEN+1];

// local mac address and default transport speed from IORegistry
static bd_addr_t local_mac_address;
static uint32_t  transport_speed;

static int power_management_active = 0;
static char *os3xBlueTool = "BlueTool";
static char *os4xBlueTool = "/usr/local/bin/BlueToolH4";

/**
 * check OS version for >= 4.0
 */
static int iphone_os_at_least_40(){
    return kCFCoreFoundationVersionNumber >= 550.32;
}

/** 
 * get machine name
 */
static struct utsname unmae_info;
static char *get_machine_name(void){
	uname(&unmae_info);
	return unmae_info.machine;
}

/**
 * on iPhone/iPod touch
 */
static int iphone_valid(void *config){
	char * machine = get_machine_name();
	if (!strncmp("iPod1", machine, strlen("iPod1"))) return 0;     // 1st gen touch no BT
    return 1;
}

static const char * iphone_name(void *config){
    return get_machine_name();
}

// Get BD_ADDR from IORegistry
static void ioregistry_get_info() {
    mach_port_t mp;
    IOMasterPort(MACH_PORT_NULL,&mp);
    CFMutableDictionaryRef bt_matching = IOServiceNameMatching("bluetooth");
    mach_port_t bt_service = IOServiceGetMatchingService(mp, bt_matching);

    // local-mac-address
    CFTypeRef local_mac_address_ref = IORegistryEntrySearchCFProperty(bt_service,"IODeviceTree",CFSTR("local-mac-address"), kCFAllocatorDefault, 1);
    CFDataGetBytes(local_mac_address_ref,CFRangeMake(0,CFDataGetLength(local_mac_address_ref)),local_mac_address); // buffer needs to be unsigned char

    // transport-speed
    CFTypeRef transport_speed_ref = IORegistryEntrySearchCFProperty(bt_service,"IODeviceTree",CFSTR("transport-speed"), kCFAllocatorDefault, 1);
    int transport_speed_len = CFDataGetLength(transport_speed_ref);
    CFDataGetBytes(transport_speed_ref,CFRangeMake(0,transport_speed_len), (uint8_t*) &transport_speed); // buffer needs to be unsigned char

    IOObjectRelease(bt_service);
    
    // dump info
    log_info("local-mac-address: %s\n", bd_addr_to_str(local_mac_address));
    log_info("transport-speed:   %u\n", transport_speed);
}

static int iphone_has_csr(){
    // construct script path from device name
    char *machine = get_machine_name();
	if (strncmp(machine, "iPhone1,", strlen("iPhone1,")) == 0) {
        return 1;
	}
    return 0;
}

static void iphone_write_string(int fd, char *string){
    int len = strlen(string);
    write(fd, string, len);
}

static void iphone_csr_set_pskey(int fd, int key, int value){
    int len = sprintf(buffer, "csr -p 0x%04x=0x%04x\n", key, value);
    write(fd, buffer, len);
}

static void iphone_csr_set_bd_addr(int fd){
    int len = sprintf(buffer,"\ncsr -p 0x0001=0x00%.2x,0x%.2x%.2x,0x00%.2x,0x%.2x%.2x\n",
            local_mac_address[3], local_mac_address[4], local_mac_address[5], local_mac_address[2], local_mac_address[0], local_mac_address[1]);
    write(fd, buffer, len);
}

static void iphone_csr_set_baud(int fd, int baud){
    // calculate baud rate (assume rate is multiply of 100)
    uint32_t baud_key = (4096 * (baud/100) + 4999) / 10000;
    iphone_csr_set_pskey(fd, 0x01be, baud_key);
}

static void iphone_bcm_set_bd_addr(int fd){
    int len = sprintf(buffer, "bcm -a %.2x:%.2x:%.2x:%.2x:%.2x:%.2x\n", local_mac_address[0], local_mac_address[1],
            local_mac_address[2], local_mac_address[3], local_mac_address[4], local_mac_address[5]);
    write(fd, buffer, len);
}

static void iphone_bcm_set_baud(int fd, int baud){
    int len = sprintf(buffer, "bcm -b %u\n",  baud);
    write(fd, buffer, len);
}

// OS 3.x
static int iphone_write_initscript (int output, int baudrate){
        
    // construct script path from device name
    strcpy(buffer, "/etc/bluetool/");
    char *machine = get_machine_name();
    strcat(buffer, machine);
    if (iphone_has_csr()){
		strcat(buffer, ".init.script");
    } else {
		strcat(buffer, ".boot.script");
    }
        
    // open script
    int input = open(buffer, O_RDONLY);
    
    int pos = 0;
    int mirror = 0;
    int store = 1;
    while (1){
        int chars = read(input, &buffer[pos], 1);
        
        // end-of-line
        if (chars == 0 || buffer[pos]=='\n' || buffer[pos]== '\r'){
            if (store) {
                // stored characters
                write(output, buffer, pos+chars);
            }
            if (mirror) {
                write(output, "\n", 1);
            }
            pos = 0;
            mirror = 0;
            store = 1;
            if (chars) {
                continue;
            } else {
                break;
            }
        }
        
        // mirror
        if (mirror){
            write(output, &buffer[pos], 1);
        }
        
        // store
        if (store) {
            pos++;
        }
        			
		// iPhone - set BD_ADDR after "csr -i"
		if (store == 1 && pos == 6 && strncmp(buffer, "csr -i", 6) == 0) {
			store = 0;
			write(output, buffer, pos); // "csr -i"
            iphone_csr_set_bd_addr(output);
		}
		
		// iPod2,1
		// check for "bcm -X" cmds
		if (store == 1 && pos == 6){
 			if (strncmp(buffer, "bcm -", 5) == 0) {
				store  = 0;
				switch (buffer[5]){
					case 'a': // BT Address
                        iphone_bcm_set_bd_addr(output);
						mirror = 0;
						break;
					case 'b': // baud rate command OS 2.x
                    case 'B': // baud rate command OS 3.x
                        iphone_bcm_set_baud(output, baudrate);
						mirror = 0;
						break;
					case 's': // sleep mode - replace with "wake" command?
						iphone_write_string(output, "wake on\n");
						mirror = 0;
						break;
					default: // other "bcm -X" command
						write(output, buffer, pos);
						mirror = 1;
				}
			}
		}
        
        // iPhone1,1 & iPhone 2,1: OS 3.x
        // check for "csr -B" and "csr -T"
        if (store == 1 && pos == 6){
			if (strncmp(buffer, "csr -", 5) == 0) {
                switch(buffer[5]){
                    case 'T':   // Transport Mode
                        store  = 0;
                        break;
                    case 'B':   // Baud rate
                        iphone_csr_set_baud(output, baudrate);
                        store  = 0;
                        break;
                    default:    // wait for full command
                        break;
                }
            }
        }
                
		// iPhone1,1 & iPhone 2,1: OS 2.x
        // check for "csr -p 0x1234=0x5678" (20)
        if (store == 1 && pos == 20) {
            int pskey, value;
            store = 0;
            if (sscanf(buffer, "csr -p 0x%x=0x%x", &pskey, &value) == 2){
                switch (pskey) {
                    case 0x01f9:    // UART MODE
                    case 0x01c1:    // Configure H5 mode
                        mirror = 0;
                        break;
                    case 0x01be:    // PSKEY_UART_BAUD_RATE
                        iphone_csr_set_baud(output, baudrate);
                        mirror = 0;
                        break;
                    default:
                        // anything else: dump buffer and start forwarding
                        write(output, buffer, pos);
                        mirror = 1;
                        break;
                }
            } else {
                write(output, buffer, pos);
                mirror = 1;
            }
        }
    }
    // close input
    close(input);

#ifdef USE_POWERMANAGEMENT
    if (iphone_has_csr()) {
        /* CSR BT module: deactivated since it didn't work on iPhone 3G, 3.1.3
           the first few packets didn't get received when iPhone is sleeping.
           That's quite possible since Apple uses H5, but BTstack uses H4
           H5's packet retransmission should take care of that */
        // iphone_write_string(output, "msleep 50\n");
        // iphone_write_string(output, "csr -p 0x01ca=0x0031\n");
        // iphone_write_string(output, "msleep 50\n");
        // iphone_write_string(output, "csr -p 0x01c7=0x0001,0x01f4,0x0005,0x0020\n");
    } else {
        /* BCM BT module, deactivated since untested for now */
        // iphone_write_string(output, "bcm -s 0x01,0x00,0x00,0x01,0x01,0x00,0x01,0x00,0x00,0x00,0x00,0x01\n");
        // iphone_write_string(output, "msleep 50\n");
    }
#endif

    return 0;
}

// OS 4.x and higher
static void iphone_write_configscript(int fd, int baudrate){
    iphone_write_string(fd, "device -D -S\n");
    if (iphone_has_csr()) {
        iphone_csr_set_bd_addr(fd);
        if (baudrate) {
            iphone_csr_set_baud(fd, baudrate);
        }
        iphone_write_string(fd, "csr -r\n");
#ifdef USE_POWERMANAGEMENT
        /* CSR BT module: deactivated since untested, but it most likely won't work
           see comments in 3.x init sequence above */
        // iphone_write_string(fd, "msleep 50\n");
        // iphone_write_string(fd, "csr -p 0x01ca=0x0031\n");
        // iphone_write_string(fd, "msleep 50\n");
        // iphone_write_string(fd, "csr -p 0x01c7=0x0001,0x01f4,0x0005,0x0020\n");
#endif
    } else {
        iphone_bcm_set_bd_addr(fd);
        if (baudrate) {
            iphone_bcm_set_baud(fd, baudrate);
            iphone_write_string(fd, "msleep 200\n");
        }
#ifdef USE_POWERMANAGEMENT
        // power management only active on 4.x with BCM (iPhone 3GS and higher, all iPads, iPod touch 3G and higher)
        iphone_write_string(fd, "bcm -s 0x01,0x00,0x00,0x01,0x01,0x00,0x01,0x00,0x00,0x00,0x00,0x01\n");
        iphone_write_string(fd, "msleep 50\n");
#endif
    }
    iphone_write_string(fd, "quit\n");
}

static int iphone_on (void *transport_config){
    // hci_uart_config->baudrate_init == 0, if using native speed    
    
    log_info("iphone_on: entered\n");

    int err = 0;

    hci_uart_config_t * hci_uart_config = (hci_uart_config_t*) transport_config;

    // get local-mac-addr and transport-speed from IORegistry 
	ioregistry_get_info();

#ifdef USE_RANDOM_BD_ADDR
    // While developing an app that emulates a Bluetooth HID keyboard, we've learnt
    // that in (some versions/driver combinations of) Windows XP, information about
    // a device with incorrect SDP descriptions are stored forever.
    // 
    // To continue development, this option was added that picks a random BD_ADDR
    // on start to trick windows in giving us a fresh start on each try.
    //
    // Use with caution!
    
    srandom(time(NULL));
    bt_store_32(local_mac_address, 0, random());
    bt_store_16(local_mac_address, 4, random());
#endif
        
    if (iphone_system_bt_enabled()){
        perror("iphone_on: System Bluetooth enabled!");
        return 1;
    }
    
    // unload BTServer
    log_info("iphone_on: unload BTServer\n");
    err = system ("launchctl unload /System/Library/LaunchDaemons/com.apple.BTServer.plist");
        
    // check for os version >= 4.0
    int os4x = iphone_os_at_least_40();
    
    // OS 4.0
    char * bluetool = os3xBlueTool;
    if (os4x) {
        bluetool = os4xBlueTool;
    }
    
    // quick test if Bluetooth UART can be opened - and try to start cleanly
    int fd = open(hci_uart_config->device_name, O_RDWR | O_NOCTTY | O_NDELAY);
    if (fd > 0)  {
        // everything's fine
        close(fd);
    } else {
        // no way!
        log_error( "bt_control.c:iphone_on(): Failed to open '%s', trying killall %s\n", hci_uart_config->device_name, bluetool);
        system("killall -9 BlueToolH4");
        system("killall -9 BlueTool");
        sleep(3); 
        
        // try again
        fd = open(hci_uart_config->device_name, O_RDWR | O_NOCTTY | O_NDELAY);
        if (fd > 0){
            close(fd);
        } else {
            log_error( "bt_control.c:iphone_on(): Failed to open '%s' again, trying killall BTServer and killall %s\n", hci_uart_config->device_name, bluetool);
            system("killall -9 BTServer");
            system("killall -9 BlueToolH4");
            system("killall -9 BlueTool");
            sleep(3); 
        }
    }
    
    // basic config on 4.0+
    if (os4x) {
        sprintf(buffer, "%s -F boot", bluetool);
        system(buffer);
        sprintf(buffer, "%s -F init", bluetool);
        system(buffer);
    }
    
    // advanced config - set bd addr, use custom baud rate, enable deep sleep
    FILE * outputFile = popen(bluetool, "r+");
    setvbuf(outputFile, NULL, _IONBF, 0);
    int output = fileno(outputFile);
    
    if (os4x) {
        // 4.x - send custom config
        iphone_write_configscript(output, hci_uart_config->baudrate_init);
    } else {
        // 3.x - modify original script on the fly
        iphone_write_initscript(output, hci_uart_config->baudrate_init);
    }
    
    // log output
    fflush(outputFile);
    int singlechar = 0;
    char linebuf[80];
    int pos = 0;
    while (singlechar != EOF) {
        singlechar   = fgetc(outputFile);
        linebuf[pos] = singlechar;
        if (singlechar != EOF && singlechar != '\n' && singlechar != '\r' && pos < 78) {
            pos++;
            continue;
        };
        linebuf[pos] = 0;
        if (pos > 0) {
            log_info("%s", linebuf);
        }
        pos = 0;
    };
    err = pclose(outputFile);

#ifdef USE_POWERMANAGEMENT
    power_management_active = bt_control_iphone_power_management_supported();

    // if baud == 0, we're using system default: set in transport config
    if (hci_uart_config->baudrate_init == 0) {
        hci_uart_config->baudrate_init = transport_speed;
    }
#endif
    
    // if we sleep for about 3 seconds, we miss a strage packet... but we don't care
    // sleep(3); 

    return err;
}

static int iphone_off (void *config){
	
/*
    char *machine = get_machine_name();
    if (strncmp(machine, "iPad", strlen("iPad")) == 0) {
		// put iPad Bluetooth into deep sleep
		system ("echo \"wake off\n quit\" | BlueTool");
	} else {
		// power off for iPhone and iPod
		system ("echo \"power off\n quit\" | BlueTool");
	}
*/	
    // power off (all models)
    log_info("iphone_off: turn off using BlueTool\n");
    system ("echo \"power off\nquit\" | BlueTool");
    
    // kill Apple BTServer as it gets confused and fails to start anyway
    // system("killall BTServer");
    
    // reload BTServer
    log_info("iphone_off: reload BTServer\n");
    system ("launchctl load /System/Library/LaunchDaemons/com.apple.BTServer.plist");
    log_info("iphone_off: done\n");

    return 0;
}

static int iphone_sleep(void *config){

    // will sleep by itself
    if (power_management_active) return 0;
    
    // put Bluetooth into deep sleep
    system ("echo \"wake off\nquit\" | BlueTool");
    return 0;
}

static int iphone_wake(void *config){

    // will wake by itself
    if (power_management_active) return 0;
    
    // wake up Bluetooth module
    system ("echo \"wake on\nquit\" | BlueTool");
    return 0;
}

static void MySleepCallBack( void * refCon, io_service_t service, natural_t messageType, void * messageArgument ) {
    
    char data;
    log_info( "messageType %08lx, arg %08lx\n", (long unsigned int)messageType, (long unsigned int)messageArgument);
    switch ( messageType ) {
        case kIOMessageCanSystemSleep:
            /* Idle sleep is about to kick in. This message will not be sent for forced sleep.
			 Applications have a chance to prevent sleep by calling IOCancelPowerChange.
			 Most applications should not prevent idle sleep.
			 
			 Power Management waits up to 30 seconds for you to either allow or deny idle sleep.
			 If you don't acknowledge this power change by calling either IOAllowPowerChange
			 or IOCancelPowerChange, the system will wait 30 seconds then go to sleep.
			 */
			
            // Uncomment to cancel idle sleep
			// IOCancelPowerChange( root_port, (long)messageArgument );
            // we will allow idle sleep
            IOAllowPowerChange( root_port, (long)messageArgument );
			break;
			
        case kIOMessageSystemWillSleep:
            /* The system WILL go to sleep. If you do not call IOAllowPowerChange or
			 IOCancelPowerChange to acknowledge this message, sleep will be
			 delayed by 30 seconds.
			 
			 NOTE: If you call IOCancelPowerChange to deny sleep it returns kIOReturnSuccess,
			 however the system WILL still go to sleep. 
			 */
			
            // notify main thread
            data = POWER_WILL_SLEEP;
            write(power_notification_pipe_fds[1], &data, 1);

            // don't allow power change, even when power management is active
            // BTstack needs to disable discovery mode during sleep to save power

            // if (!power_management_active) break;
            // IOAllowPowerChange( root_port, (long)messageArgument );
            
            break;
			
        case kIOMessageSystemWillPowerOn:
            
            data = POWER_WILL_WAKE_UP;
            write(power_notification_pipe_fds[1], &data, 1);
            break;
			
        case kIOMessageSystemHasPoweredOn:
            //System has finished waking up...
			break;
			
        default:
            break;
			
    }
}

static int  power_notification_process(struct data_source *ds) {

    if (!power_notification_callback) return -1;

    // get token
    char token;
    int bytes_read = read(power_notification_pipe_fds[0], &token, 1);
    if (bytes_read != 1) return -1;
        
    log_info("power_notification_process: %u\n", token);

    power_notification_callback( (POWER_NOTIFICATION_t) token );    
    
    return 0;
}

/**
 * assumption: called only once, cb != null
 */
void iphone_register_for_power_notifications(void (*cb)(POWER_NOTIFICATION_t event)){

    // handle IOKIT power notifications - http://developer.apple.com/library/mac/#qa/qa2004/qa1340.html
    IONotificationPortRef  notifyPortRef =  NULL; // notification port allocated by IORegisterForSystemPower
    io_object_t            notifierObject = 0;    // notifier object, used to deregister later
    root_port = IORegisterForSystemPower(NULL, &notifyPortRef, MySleepCallBack, &notifierObject);
    if (!root_port) {
        log_info("IORegisterForSystemPower failed\n");
        return;
    }
    
    // add the notification port to this thread's runloop
    CFRunLoopAddSource(CFRunLoopGetCurrent(), IONotificationPortGetRunLoopSource(notifyPortRef), kCFRunLoopCommonModes);
    
    // 
    power_notification_callback = cb;
    
 	// create pipe to deliver power notification to main run loop
	pipe(power_notification_pipe_fds);

    // set up data source handler
    power_notification_ds.fd =      power_notification_pipe_fds[0];
    power_notification_ds.process = power_notification_process;
    run_loop_add_data_source(&power_notification_ds);  
}

int bt_control_iphone_power_management_supported(void){
    // only supported on Broadcom chipsets with iOS 4.0+
    if ( iphone_has_csr()) return 0;
    if (!iphone_os_at_least_40()) return 0;
    return 1;
}

// direct access
int bt_control_iphone_power_management_enabled(void){
    return power_management_active;
}

// single instance
bt_control_t bt_control_iphone = {
    .on     = iphone_on,
    .off    = iphone_off,
    .sleep  = iphone_sleep,
    .wake   = iphone_wake,
    .valid  = iphone_valid,
    .name   = iphone_name,
    .register_for_power_notifications = iphone_register_for_power_notifications
};

int iphone_system_bt_enabled(void){
    return SBA_getBluetoothEnabled();
}

void iphone_system_bt_set_enabled(int enabled)
{
    SBA_setBluetoothEnabled(enabled);
    sleep(2); // give change a chance
}

int iphone_system_has_csr(void){
    return iphone_has_csr();
}