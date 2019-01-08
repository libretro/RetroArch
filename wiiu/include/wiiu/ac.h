#pragma once

/**
 * These functions manage the Wii U's AutoConnect library--basically
 * connecting to the LAN/Internet via the network profile set up in
 * System Preferences.
 */

typedef int ACResult;
enum {
   AC_FAILED = -1,
   AC_OK = 0,
   AC_BUSY = 1
};

typedef unsigned long ACIpAddress;

ACResult ACInitialize(void);
void ACFinalize(void);
ACResult ACConnect(void);
ACResult ACClose(void);
ACResult ACGetAssignedAddress(ACIpAddress *addr);
ACResult ACGetAssignedSubnet(ACIpAddress *addr);
