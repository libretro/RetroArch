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
#include <string.h>
#include <malloc.h>
#include "iosuhax.h"
#include "iosuhax_disc_interface.h"

#define FSA_REF_SD      0x01
#define FSA_REF_USB     0x02

static int initialized = 0;

static int fsaFdSd = 0;
static int fsaFdUsb = 0;
static int sdioFd = 0;
static int usbFd = 0;

static void IOSUHAX_disc_io_initialize(void)
{
    if(initialized == 0)
    {
        initialized = 1;
        fsaFdSd = -1;
        fsaFdUsb = -1;
        sdioFd = -1;
        usbFd = -1;
    }
}

static bool IOSUHAX_disc_io_fsa_open(int fsaFd)
{
    IOSUHAX_disc_io_initialize();

    if(IOSUHAX_Open(NULL) < 0)
        return false;

    if(fsaFd == FSA_REF_SD)
    {
        if(fsaFdSd < 0)
        {
            fsaFdSd = IOSUHAX_FSA_Open();
        }

        if(fsaFdSd >= 0)
            return true;
    }
    else if(fsaFd == FSA_REF_USB)
    {
        if(fsaFdUsb < 0)
        {
            fsaFdUsb = IOSUHAX_FSA_Open();
        }

        if(fsaFdUsb >= 0)
            return true;
    }

    return false;
}

static void IOSUHAX_disc_io_fsa_close(int fsaFd)
{
    if(fsaFd == FSA_REF_SD)
    {
        if(fsaFdSd >= 0)
        {
            IOSUHAX_FSA_Close(fsaFdSd);
            fsaFdSd = -1;
        }
    }
    else if(fsaFd == FSA_REF_USB)
    {
        if(fsaFdUsb >= 0)
        {
            IOSUHAX_FSA_Close(fsaFdUsb);
            fsaFdUsb = -1;
        }
    }
}

static bool IOSUHAX_sdio_startup(void)
{
    if(!IOSUHAX_disc_io_fsa_open(FSA_REF_SD))
        return false;

    if(sdioFd < 0)
    {
        int res = IOSUHAX_FSA_RawOpen(fsaFdSd, "/dev/sdcard01", &sdioFd);
        if(res < 0)
        {
            IOSUHAX_disc_io_fsa_close(FSA_REF_SD);
            sdioFd = -1;
        }
    }

    return (sdioFd >= 0);
}

static bool IOSUHAX_sdio_isInserted(void)
{
    //! TODO: check for SD card inserted with IOSUHAX_FSA_GetDeviceInfo()
    return initialized && (fsaFdSd >= 0) && (sdioFd >= 0);
}

static bool IOSUHAX_sdio_clearStatus(void)
{
    return true;
}

static bool IOSUHAX_sdio_shutdown(void)
{
    if(!IOSUHAX_sdio_isInserted())
        return false;

    IOSUHAX_FSA_RawClose(fsaFdSd, sdioFd);
    IOSUHAX_disc_io_fsa_close(FSA_REF_SD);
    sdioFd = -1;
    return true;
}

static bool IOSUHAX_sdio_readSectors(uint32_t sector, uint32_t numSectors, void* buffer)
{
    if(!IOSUHAX_sdio_isInserted() || !buffer)
        return false;

    int res = IOSUHAX_FSA_RawRead(fsaFdSd, buffer, 512, numSectors, sector, sdioFd);
    if(res < 0)
    {
        return false;
    }

    return true;
}

static bool IOSUHAX_sdio_writeSectors(uint32_t sector, uint32_t numSectors, const void* buffer)
{
    if(!IOSUHAX_sdio_isInserted() || !buffer)
        return false;

    int res = IOSUHAX_FSA_RawWrite(fsaFdSd, buffer, 512, numSectors, sector, sdioFd);
    if(res < 0)
    {
        return false;
    }

    return true;
}

const DISC_INTERFACE IOSUHAX_sdio_disc_interface =
{
	DEVICE_TYPE_WII_U_SD,
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_WII_U_SD,
    IOSUHAX_sdio_startup,
    IOSUHAX_sdio_isInserted,
    IOSUHAX_sdio_readSectors,
    IOSUHAX_sdio_writeSectors,
    IOSUHAX_sdio_clearStatus,
    IOSUHAX_sdio_shutdown
};

static bool IOSUHAX_usb_startup(void)
{
    if(!IOSUHAX_disc_io_fsa_open(FSA_REF_USB))
        return false;

    if(usbFd < 0)
    {
        int res = IOSUHAX_FSA_RawOpen(fsaFdUsb, "/dev/usb01", &usbFd);
        if(res < 0)
        {
            res = IOSUHAX_FSA_RawOpen(fsaFdUsb, "/dev/usb02", &usbFd);
            if(res < 0)
            {
                IOSUHAX_disc_io_fsa_close(FSA_REF_USB);
                usbFd = -1;
            }
        }
    }
    return (usbFd >= 0);
}

static bool IOSUHAX_usb_isInserted(void)
{
    return initialized && (fsaFdUsb >= 0) && (usbFd >= 0);
}

static bool IOSUHAX_usb_clearStatus(void)
{
    return true;
}

static bool IOSUHAX_usb_shutdown(void)
{
    if(!IOSUHAX_usb_isInserted())
        return false;

    IOSUHAX_FSA_RawClose(fsaFdUsb, usbFd);
    IOSUHAX_disc_io_fsa_close(FSA_REF_USB);
    usbFd = -1;
    return true;
}

static bool IOSUHAX_usb_readSectors(uint32_t sector, uint32_t numSectors, void* buffer)
{
    if(!IOSUHAX_usb_isInserted() || !buffer)
        return false;

    int res = IOSUHAX_FSA_RawRead(fsaFdUsb, buffer, 512, numSectors, sector, usbFd);
    if(res < 0)
    {
        return false;
    }

    return true;
}

static bool IOSUHAX_usb_writeSectors(uint32_t sector, uint32_t numSectors, const void* buffer)
{
    if(!IOSUHAX_usb_isInserted() || !buffer)
        return false;

    int res = IOSUHAX_FSA_RawWrite(fsaFdUsb, buffer, 512, numSectors, sector, usbFd);
    if(res < 0)
    {
        return false;
    }

    return true;
}

const DISC_INTERFACE IOSUHAX_usb_disc_interface =
{
	DEVICE_TYPE_WII_U_USB,
	FEATURE_MEDIUM_CANREAD | FEATURE_MEDIUM_CANWRITE | FEATURE_WII_U_USB,
    IOSUHAX_usb_startup,
    IOSUHAX_usb_isInserted,
    IOSUHAX_usb_readSectors,
    IOSUHAX_usb_writeSectors,
    IOSUHAX_usb_clearStatus,
    IOSUHAX_usb_shutdown
};
