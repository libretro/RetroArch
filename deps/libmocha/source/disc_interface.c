/***************************************************************************
 * Copyright (C) 2016
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/
#include "mocha/disc_interface.h"
#include "mocha/fsa.h"
#include "mocha/mocha.h"
#include <coreinit/ios.h>
#include <stdbool.h>

#define FSA_REF_SD  0x01
#define FSA_REF_USB 0x02

static int initialized = 0;

static int fsaFdSd  = 0;
static int fsaFdUsb = 0;
static int sdioFd   = 0;
static int usbFd    = 0;

static void Mocha_disc_io_initialize(void) {
    if (initialized == 0) {
        initialized = 1;
        fsaFdSd     = -1;
        fsaFdUsb    = -1;
        sdioFd      = -1;
        usbFd       = -1;
    }
}

static bool Mocha_disc_io_fsa_open(int fsaFd) {
    Mocha_disc_io_initialize();

    if (fsaFd == FSA_REF_SD) {
        if (fsaFdSd < 0) {
            fsaFdSd = IOS_Open("/dev/fsa", IOS_OPEN_READWRITE);
            if (fsaFdSd >= 0 && Mocha_UnlockFSClientEx(fsaFdSd) != MOCHA_RESULT_SUCCESS) {
                IOS_Close(fsaFdSd);
                fsaFdSd = -1;
            }
        }

        if (fsaFdSd >= 0) {
            return true;
        }
    } else if (fsaFd == FSA_REF_USB) {
        if (fsaFdUsb < 0) {
            fsaFdUsb = IOS_Open("/dev/fsa", IOS_OPEN_READWRITE);
            if (fsaFdUsb >= 0 && Mocha_UnlockFSClientEx(fsaFdUsb) != MOCHA_RESULT_SUCCESS) {
                IOS_Close(fsaFdUsb);
                fsaFdUsb = -1;
            }
        }
        if (fsaFdUsb >= 0) {
            return true;
        }
    }

    return false;
}

static void Mocha_disc_io_fsa_close(int fsaFd) {
    if (fsaFd == FSA_REF_SD) {
        if (fsaFdSd >= 0) {
            IOS_Close(fsaFdSd);
            fsaFdSd = -1;
        }
    } else if (fsaFd == FSA_REF_USB) {
        if (fsaFdUsb >= 0) {
            IOS_Close(fsaFdUsb);
            fsaFdUsb = -1;
        }
    }
}

static bool Mocha_sdio_startup(void) {
    if (!Mocha_disc_io_fsa_open(FSA_REF_SD))
        return false;

    if (sdioFd < 0) {
        int res = FSAEx_RawOpenEx(fsaFdSd, "/dev/sdcard01", &sdioFd);
        if (res < 0) {
            Mocha_disc_io_fsa_close(FSA_REF_SD);
            sdioFd = -1;
        }
    }

    return (sdioFd >= 0);
}

static bool Mocha_sdio_isInserted(void) {
    //! TODO: check for SD card inserted with Mocha_FSA_GetDeviceInfo()
    return initialized && (fsaFdSd >= 0) && (sdioFd >= 0);
}

static bool Mocha_sdio_clearStatus(void) {
    return true;
}

static bool Mocha_sdio_shutdown(void) {
    if (!Mocha_sdio_isInserted())
        return false;

    FSAEx_RawCloseEx(fsaFdSd, sdioFd);
    Mocha_disc_io_fsa_close(FSA_REF_SD);
    sdioFd = -1;
    return true;
}

static bool Mocha_sdio_readSectors(uint32_t sector, uint32_t numSectors, void *buffer) {
    if (!Mocha_sdio_isInserted()) {
        return false;
    }

    int res = FSAEx_RawReadEx(fsaFdSd, buffer, 512, numSectors, sector, sdioFd);
    if (res < 0) {
        return false;
    }

    return true;
}

static bool Mocha_sdio_writeSectors(uint32_t sector, uint32_t numSectors, const void *buffer) {
    if (!Mocha_sdio_isInserted()) {
        return false;
    }

    int res = FSAEx_RawWriteEx(fsaFdSd, buffer, 512, numSectors, sector, sdioFd);
    if (res < 0) {
        return false;
    }

    return true;
}

const DISC_INTERFACE Mocha_sdio_disc_interface = {
        DEVICE_TYPE_WII_U_SD,
        FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_WII_U_SD,
        Mocha_sdio_startup,
        Mocha_sdio_isInserted,
        Mocha_sdio_readSectors,
        Mocha_sdio_writeSectors,
        Mocha_sdio_clearStatus,
        Mocha_sdio_shutdown};

static bool Mocha_usb_startup(void) {
    if (!Mocha_disc_io_fsa_open(FSA_REF_USB)) {
        return false;
    }

    if (usbFd < 0) {
        int res = FSAEx_RawOpenEx(fsaFdUsb, "/dev/usb01", &usbFd);
        if (res < 0) {
            res = FSAEx_RawOpenEx(fsaFdUsb, "/dev/usb02", &usbFd);
            if (res < 0) {
                Mocha_disc_io_fsa_close(FSA_REF_USB);
                usbFd = -1;
            }
        }
    }
    return (usbFd >= 0);
}

static bool Mocha_usb_isInserted(void) {
    return initialized && (fsaFdUsb >= 0) && (usbFd >= 0);
}

static bool Mocha_usb_clearStatus(void) {
    return true;
}

static bool Mocha_usb_shutdown(void) {
    if (!Mocha_usb_isInserted()) {
        return false;
    }

    FSAEx_RawCloseEx(fsaFdUsb, usbFd);
    Mocha_disc_io_fsa_close(FSA_REF_USB);
    usbFd = -1;
    return true;
}

static bool Mocha_usb_readSectors(uint32_t sector, uint32_t numSectors, void *buffer) {
    if (!Mocha_usb_isInserted()) {
        return false;
    }

    int res = FSAEx_RawReadEx(fsaFdUsb, buffer, 512, numSectors, sector, usbFd);
    if (res < 0) {
        return false;
    }

    return true;
}

static bool Mocha_usb_writeSectors(uint32_t sector, uint32_t numSectors, const void *buffer) {
    if (!Mocha_usb_isInserted()) {
        return false;
    }

    int res = FSAEx_RawWriteEx(fsaFdUsb, buffer, 512, numSectors, sector, usbFd);
    if (res < 0) {
        return false;
    }

    return true;
}

const DISC_INTERFACE Mocha_usb_disc_interface = {
        DEVICE_TYPE_WII_U_USB,
        FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_WII_U_USB,
        Mocha_usb_startup,
        Mocha_usb_isInserted,
        Mocha_usb_readSectors,
        Mocha_usb_writeSectors,
        Mocha_usb_clearStatus,
        Mocha_usb_shutdown};
