// IoSupport.cpp: implementation of the CIoSupport class.
//
//////////////////////////////////////////////////////////////////////
#include "iosupport.h"
#include "undocumented.h"

#include <stdio.h>

#define CTLCODE(DeviceType, Function, Method, Access) ( ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method)  )
#define FSCTL_DISMOUNT_VOLUME  CTLCODE( FILE_DEVICE_FILE_SYSTEM, 0x08, METHOD_BUFFERED, FILE_ANY_ACCESS )

//////////////////////////////////////////////////////////////////////
// Construction/Destruction
//////////////////////////////////////////////////////////////////////

CIoSupport g_IOSupport;

CIoSupport::CIoSupport()
{
	m_dwLastTrayState = 0;
}

CIoSupport::~CIoSupport()
{

}

// szDrive e.g. "D:"
// szDevice e.g. "Cdrom0" or "Harddisk0\Partition6"

HRESULT CIoSupport::Mount(char *szDrive, char *szDevice)
{
	char szSourceDevice[48];
	char szDestinationDrive[16];

	snprintf(szSourceDevice, sizeof(szSourceDevice), "\\Device\\%s", szDevice);
	snprintf(szDestinationDrive, sizeof(szDestinationDrive), "\\??\\%s", szDrive);

	STRING DeviceName =
	{
		strlen(szSourceDevice),
		strlen(szSourceDevice) + 1,
		szSourceDevice
	};

	STRING LinkName =
	{
		strlen(szDestinationDrive),
		strlen(szDestinationDrive) + 1,
		szDestinationDrive
	};

	IoCreateSymbolicLink(&LinkName, &DeviceName);

	return S_OK;
}



// szDrive e.g. "D:"

HRESULT CIoSupport::Unmount(char *szDrive)
{
	char szDestinationDrive[16];
	snprintf(szDestinationDrive, sizeof(szDestinationDrive), "\\??\\%s", szDrive);

	STRING LinkName =
	{
		strlen(szDestinationDrive),
		strlen(szDestinationDrive) + 1,
		szDestinationDrive
	};

	IoDeleteSymbolicLink(&LinkName);

	return S_OK;
}

HRESULT CIoSupport::Remount(char *szDrive, char *szDevice)
{
	char szSourceDevice[48];
	snprintf(szSourceDevice, sizeof(szSourceDevice), "\\Device\\%s", szDevice);

	Unmount(szDrive);

	ANSI_STRING filename;
	OBJECT_ATTRIBUTES attributes;
	IO_STATUS_BLOCK status;
	HANDLE hDevice;
	NTSTATUS error;
	DWORD dummy;

	RtlInitAnsiString(&filename, szSourceDevice);
	InitializeObjectAttributes(&attributes, &filename, OBJ_CASE_INSENSITIVE, NULL);

	if (!NT_SUCCESS(error = NtCreateFile(&hDevice, GENERIC_READ |
	                                     SYNCHRONIZE | FILE_READ_ATTRIBUTES, &attributes, &status, NULL, 0,
	                                     FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, FILE_OPEN,
	                                     FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT)))
	{
		return E_FAIL;
	}

	if (!DeviceIoControl(hDevice, FSCTL_DISMOUNT_VOLUME, NULL, 0, NULL, 0, &dummy, NULL))
	{
		CloseHandle(hDevice);
		return E_FAIL;
	}

	CloseHandle(hDevice);
	Mount(szDrive, szDevice);

	return S_OK;
}

HRESULT CIoSupport::Remap(char *szMapping)
{
	char szMap[32];
	strlcpy(szMap, szMapping, sizeof(szMap));

	char *pComma = strstr(szMap, ",");
	if (pComma)
	{
		*pComma = 0;

		// map device to drive letter
		Unmount(szMap);
		Mount(szMap, &pComma[1]);
		return S_OK;
	}

	return E_FAIL;
}


HRESULT CIoSupport::EjectTray()
{
	HalWriteSMBusValue(0x20, 0x0C, FALSE, 0);  // eject tray
	return S_OK;
}

HRESULT CIoSupport::CloseTray()
{
	HalWriteSMBusValue(0x20, 0x0C, FALSE, 1);  // close tray
	return S_OK;
}

DWORD CIoSupport::GetTrayState()
{
	HalReadSMCTrayState(&m_dwTrayState, &m_dwTrayCount);

	if(m_dwTrayState == TRAY_CLOSED_MEDIA_PRESENT)
	{
		if (m_dwLastTrayState != TRAY_CLOSED_MEDIA_PRESENT)
		{
			m_dwLastTrayState = m_dwTrayState;
			return DRIVE_CLOSED_MEDIA_PRESENT;
		}
		else
		{
			return DRIVE_READY;
		}
	}
	else if(m_dwTrayState == TRAY_CLOSED_NO_MEDIA)
	{
		m_dwLastTrayState = m_dwTrayState;
		return DRIVE_CLOSED_NO_MEDIA;
	}
	else if(m_dwTrayState == TRAY_OPEN)
	{
		m_dwLastTrayState = m_dwTrayState;
		return DRIVE_OPEN;
	}
	else
	{
		m_dwLastTrayState = m_dwTrayState;
	}

	return DRIVE_NOT_READY;
}

HRESULT CIoSupport::Shutdown()
{
	HalInitiateShutdown();
	return S_OK;
}