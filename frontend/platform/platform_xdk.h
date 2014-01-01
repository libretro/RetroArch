/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 * Copyright (C) 2011-2014 - Daniel De Matteis
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

// Thanks and credit to everlame, Team Evox, and Woodoo.
//
// This file contains declarations for accessing the internal NT API.
// Some calls have changed from NT, so be careful!
//
// For the most part, all NT calls use ANSI instead of Unicode strings now.
//
// It is not known which flags work.  You will have to experiment.

#ifndef __XBOX_INTERNAL_H__
#define __XBOX_INTERNAL_H__

#ifdef _XBOX1

#include <pshpack4.h>

#ifdef __cplusplus
extern "C" {
#endif

// Don't do __declspec(dllimport) for things like emulators
#if defined(NTSYSAPI) && defined(DONT_IMPORT_INTERNAL)
#undef NTSYSAPI
#endif
#ifdef DONT_IMPORT_INTERNAL
#define NTSYSAPI
#endif

// The normal headers don't have this...?
#define FASTCALL __fastcall

// The usual NTSTATUS
typedef LONG NTSTATUS;

// The usual NT_SUCCESS
#define NT_SUCCESS(Status) ((NTSTATUS)(Status) >= 0)

// Just for documentation
#define EXPORTNUM(x)


// Needed for object structures and related things
typedef CONST SHORT CSHORT;


// String types
typedef CHAR *PSZ;
typedef CONST CHAR *PCSZ;

// ANSI_STRING
// Differences from NT: None.
typedef struct _STRING {
    USHORT Length;
    USHORT MaximumLength;
    PCHAR Buffer;
} STRING;
typedef STRING *PSTRING;

typedef STRING ANSI_STRING;
typedef PSTRING PANSI_STRING;


// IO Status Block type (UNVERIFIED)
// Differences from NT: None.
typedef struct _IO_STATUS_BLOCK {
    union {
        NTSTATUS Status;
        PVOID Pointer;
    };

    ULONG_PTR Information;
} IO_STATUS_BLOCK, *PIO_STATUS_BLOCK;

// APC routine
typedef
VOID
(NTAPI *PIO_APC_ROUTINE) (
    IN PVOID ApcContext,
    IN PIO_STATUS_BLOCK IoStatusBlock,
    IN ULONG Reserved
    );


// Header for dispatcher objects
// Differences from NT: None.
typedef struct _DISPATCHER_HEADER {
    UCHAR Type;
    UCHAR Absolute;
    UCHAR Size;
    UCHAR Inserted;
    LONG SignalState;
    LIST_ENTRY WaitListHead;
} DISPATCHER_HEADER;


// Object types
#define NotificationTimerObject         8
#define SynchronizationTimerObject      9
#define DpcObject                       19


// Object Attributes type
// Differences from NT: There are no Length, SecurityDescriptor, or
//     SecurityQualityOfService fields.  Also, ObjectName is ANSI, not
//     Unicode.
typedef struct _OBJECT_ATTRIBUTES {
    HANDLE RootDirectory;
    PANSI_STRING ObjectName;
    ULONG Attributes;
} OBJECT_ATTRIBUTES;
typedef OBJECT_ATTRIBUTES *POBJECT_ATTRIBUTES;

// Flags for OBJECT_ATTRIBUTES::Attributes
#define OBJ_INHERIT             0x00000002L
#define OBJ_PERMANENT           0x00000010L
#define OBJ_EXCLUSIVE           0x00000020L
#define OBJ_CASE_INSENSITIVE    0x00000040L
#define OBJ_OPENIF              0x00000080L
#define OBJ_OPENLINK            0x00000100L
#define OBJ_KERNEL_HANDLE       0x00000200L
#define OBJ_VALID_ATTRIBUTES    0x000003F2L

// CreateDisposition values for NtCreateFile()
#define FILE_SUPERSEDE                  0x00000000
#define FILE_OPEN                       0x00000001
#define FILE_CREATE                     0x00000002
#define FILE_OPEN_IF                    0x00000003
#define FILE_OVERWRITE                  0x00000004
#define FILE_OVERWRITE_IF               0x00000005
#define FILE_MAXIMUM_DISPOSITION        0x00000005

// CreateOption values for NtCreateFile()
// FILE_NON_DIRECTORY_FILE | FILE_SYNCHRONOUS_IO_NONALERT is what CreateFile
// uses for most things when translating to NtCreateFile.
#define FILE_DIRECTORY_FILE                     0x00000001
#define FILE_WRITE_THROUGH                      0x00000002
#define FILE_SEQUENTIAL_ONLY                    0x00000004
#define FILE_NO_INTERMEDIATE_BUFFERING          0x00000008
#define FILE_SYNCHRONOUS_IO_ALERT               0x00000010
#define FILE_SYNCHRONOUS_IO_NONALERT            0x00000020
#define FILE_NON_DIRECTORY_FILE                 0x00000040
#define FILE_CREATE_TREE_CONNECTION             0x00000080
#define FILE_COMPLETE_IF_OPLOCKED               0x00000100
#define FILE_NO_EA_KNOWLEDGE                    0x00000200
#define FILE_OPEN_FOR_RECOVERY                  0x00000400
#define FILE_RANDOM_ACCESS                      0x00000800
#define FILE_DELETE_ON_CLOSE                    0x00001000
#define FILE_OPEN_BY_FILE_ID                    0x00002000
#define FILE_OPEN_FOR_BACKUP_INTENT             0x00004000
#define FILE_NO_COMPRESSION                     0x00008000
#define FILE_RESERVE_OPFILTER                   0x00100000
#define FILE_OPEN_REPARSE_POINT                 0x00200000
#define FILE_OPEN_NO_RECALL                     0x00400000
#define FILE_OPEN_FOR_FREE_SPACE_QUERY          0x00800000
#define FILE_COPY_STRUCTURED_STORAGE            0x00000041
#define FILE_STRUCTURED_STORAGE                 0x00000441
#define FILE_VALID_OPTION_FLAGS                 0x00ffffff
#define FILE_VALID_PIPE_OPTION_FLAGS            0x00000032
#define FILE_VALID_MAILSLOT_OPTION_FLAGS        0x00000032
#define FILE_VALID_SET_FLAGS                    0x00000036


// NtQueryVolumeInformation / NtSetVolumeInformation stuff
// Type of information to retrieve; FileFsSizeInformation and
// FileFsDeviceInformation are the only ones confirmed to work.
typedef enum _FSINFOCLASS {
	FileFsVolumeInformation = 1,
	FileFsLabelInformation,
	FileFsSizeInformation,
	FileFsDeviceInformation,
	FileFsAttributeInformation,
	FileFsControlInformation,
	FileFsFullSizeInformation,
	FileFsObjectInformation
} FS_INFORMATION_CLASS, *PFS_INFORMATION_CLASS;

// Structure of FileFsSizeInformation
typedef struct _FILE_FS_SIZE_INFORMATION {
	LARGE_INTEGER TotalAllocationUnits;
	LARGE_INTEGER AvailableAllocationUnits;
	ULONG SectorsPerAllocationUnit;
	ULONG BytesPerSector;
} FILE_FS_SIZE_INFORMATION, *PFILE_FS_SIZE_INFORMATION;

#define DEVICE_TYPE ULONG

// Structure of FileFsDeviceInformation
typedef struct _FILE_FS_DEVICE_INFORMATION {                    
    DEVICE_TYPE DeviceType;                                     
    ULONG Characteristics;                                      
} FILE_FS_DEVICE_INFORMATION, *PFILE_FS_DEVICE_INFORMATION;     

// DEVICE_TYPEs (I took a guess as to which the XBOX might have.)
#define FILE_DEVICE_CD_ROM              0x00000002
#define FILE_DEVICE_CD_ROM_FILE_SYSTEM  0x00000003
#define FILE_DEVICE_CONTROLLER          0x00000004
#define FILE_DEVICE_DISK                0x00000007
#define FILE_DEVICE_DISK_FILE_SYSTEM    0x00000008
#define FILE_DEVICE_FILE_SYSTEM         0x00000009
#define FILE_DEVICE_NULL                0x00000015
#define FILE_DEVICE_SCREEN              0x0000001c
#define FILE_DEVICE_SOUND               0x0000001d
#define FILE_DEVICE_UNKNOWN             0x00000022
#define FILE_DEVICE_VIDEO               0x00000023
#define FILE_DEVICE_VIRTUAL_DISK        0x00000024
#define FILE_DEVICE_FULLSCREEN_VIDEO    0x00000034

// Characteristics
#define FILE_REMOVABLE_MEDIA            0x00000001
#define FILE_READ_ONLY_DEVICE           0x00000002
#define FILE_FLOPPY_DISKETTE            0x00000004
#define FILE_WRITE_ONCE_MEDIA           0x00000008
#define FILE_REMOTE_DEVICE              0x00000010
#define FILE_DEVICE_IS_MOUNTED          0x00000020
#define FILE_VIRTUAL_VOLUME             0x00000040
#define FILE_AUTOGENERATED_DEVICE_NAME  0x00000080
#define FILE_DEVICE_SECURE_OPEN         0x00000100



// Physical address
// Differences from NT: 32 bit address instead of 64.
typedef ULONG PHYSICAL_ADDRESS, *PPHYSICAL_ADDRESS;

// NtCreateFile/NtOpenFile stuff
#define FILE_SUPERSEDED                 0x00000000
#define FILE_OPENED                     0x00000001
#define FILE_CREATED                    0x00000002
#define FILE_OVERWRITTEN                0x00000003
#define FILE_EXISTS                     0x00000004
#define FILE_DOES_NOT_EXIST             0x00000005

// NtReadFile/NtWriteFile stuff
#define FILE_WRITE_TO_END_OF_FILE       0xffffffff
#define FILE_USE_FILE_POINTER_POSITION  0xfffffffe

// Device types
#define FILE_DEVICE_CD_ROM              0x00000002
#define FILE_DEVICE_CD_ROM_FILE_SYSTEM  0x00000003
#define FILE_DEVICE_CONTROLLER          0x00000004
#define FILE_DEVICE_SCSI                FILE_DEVICE_CONTROLLER
#define IOCTL_SCSI_BASE                 FILE_DEVICE_CONTROLLER
#define FILE_DEVICE_DISK                0x00000007
#define FILE_DEVICE_DISK_FILE_SYSTEM    0x00000008
#define FILE_DEVICE_DVD                 0x00000033

// Access types
#define FILE_ANY_ACCESS                 0
#define FILE_READ_ACCESS          ( 0x0001 )    // file & pipe
#define FILE_WRITE_ACCESS         ( 0x0002 )    // file & pipe

// Method types
#define METHOD_BUFFERED                 0
#define METHOD_IN_DIRECT                1
#define METHOD_OUT_DIRECT               2
#define METHOD_NEITHER                  3

// The all-important CTL_CODE
#define CTL_CODE( DeviceType, Function, Method, Access ) (                 \
    ((DeviceType) << 16) | ((Access) << 14) | ((Function) << 2) | (Method) \
)

// IDE/SCSI codes
// IOCTL_SCSI_PASS_THROUGH_DIRECT is the only one known to be used.
// Differences from NT: None.
#define IOCTL_SCSI_PASS_THROUGH         CTL_CODE(IOCTL_SCSI_BASE, 0x0401, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_SCSI_MINIPORT             CTL_CODE(IOCTL_SCSI_BASE, 0x0402, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_SCSI_GET_INQUIRY_DATA     CTL_CODE(IOCTL_SCSI_BASE, 0x0403, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SCSI_GET_CAPABILITIES     CTL_CODE(IOCTL_SCSI_BASE, 0x0404, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SCSI_PASS_THROUGH_DIRECT  CTL_CODE(IOCTL_SCSI_BASE, 0x0405, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)
#define IOCTL_SCSI_GET_ADDRESS          CTL_CODE(IOCTL_SCSI_BASE, 0x0406, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SCSI_RESCAN_BUS           CTL_CODE(IOCTL_SCSI_BASE, 0x0407, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SCSI_GET_DUMP_POINTERS    CTL_CODE(IOCTL_SCSI_BASE, 0x0408, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_SCSI_FREE_DUMP_POINTERS   CTL_CODE(IOCTL_SCSI_BASE, 0x0409, METHOD_BUFFERED, FILE_ANY_ACCESS)
#define IOCTL_IDE_PASS_THROUGH          CTL_CODE(IOCTL_SCSI_BASE, 0x040a, METHOD_BUFFERED, FILE_READ_ACCESS | FILE_WRITE_ACCESS)

// Special XBOX code?
#define IOCTL_CDROM_AUTHENTICATE_DISK   CTL_CODE(FILE_DEVICE_CD_ROM, 0x0020, METHOD_BUFFERED, FILE_READ_ACCESS)

// Structure for IOCTL_SCSI_PASS_THROUGH_DIRECT
// Differences from NT: None, believe it or not.
typedef struct _SCSI_PASS_THROUGH_DIRECT {
    /*000*/ USHORT Length;
    /*002*/ UCHAR ScsiStatus;
    /*003*/ UCHAR PathId;
    /*004*/ UCHAR TargetId;
    /*005*/ UCHAR Lun;
    /*006*/ UCHAR CdbLength;
    /*007*/ UCHAR SenseInfoLength;
    /*008*/ UCHAR DataIn;
    /*00C*/ ULONG DataTransferLength;
    /*010*/ ULONG TimeOutValue;
    /*014*/ PVOID DataBuffer;
    /*018*/ ULONG SenseInfoOffset;
    /*01C*/ UCHAR Cdb[16];
}SCSI_PASS_THROUGH_DIRECT, *PSCSI_PASS_THROUGH_DIRECT;

// DataIn fields for IOCTL_SCSI_PASS_THROUGH_DIRECT
#define SCSI_IOCTL_DATA_OUT          0
#define SCSI_IOCTL_DATA_IN           1
#define SCSI_IOCTL_DATA_UNSPECIFIED  2

// Kernel object type (unsure about the structure...)
typedef struct _OBJECT_TYPE
{
	// Same prototype as ExAllocatePoolWithTag, because that's the usual one
	PVOID
	(NTAPI *AllocationFunction)(
		SIZE_T NumberOfBytes,
		ULONG Tag
		);

	// Same prototype as ExFreePool, because that's the usual one
	VOID
	(NTAPI *FreeFunction)(
		IN PVOID P
		);

	// The prototypes of these are unknown
	void *CloseFunction;
	void *DeleteFunction;
	void *ParseFunction;

	// Unknown DWORD...  Size of this object type maybe?
	void *DefaultObjectMaybe;

	// 4 letter tag for this object type
	CHAR Tag[4];
} OBJECT_TYPE;
typedef OBJECT_TYPE *POBJECT_TYPE;

// Object types
extern POBJECT_TYPE IoFileObjectType;
extern POBJECT_TYPE ExEventObjectType;
extern POBJECT_TYPE ExSemaphoreObjectType;
extern POBJECT_TYPE IoCompletionObjectType;
extern POBJECT_TYPE IoDeviceObjectType;


// *_OBJECT and related structures (mostly opaque since I'm lazy)
typedef struct _DRIVER_OBJECT {
    CSHORT Type;
    CSHORT Size;
    struct _DEVICE_OBJECT *DeviceObject;
	// ...
} DRIVER_OBJECT;
typedef DRIVER_OBJECT *PDRIVER_OBJECT;

typedef struct _DEVICE_OBJECT {
    CSHORT Type;
    USHORT Size;
    LONG ReferenceCount;
    PDRIVER_OBJECT DriverObject;
	// ...
} DEVICE_OBJECT;
typedef DEVICE_OBJECT *PDEVICE_OBJECT;

typedef struct _FILE_OBJECT {
    CSHORT Type;
    CSHORT Size;
    PDEVICE_OBJECT DeviceObject;
	// ...
} FILE_OBJECT;
typedef FILE_OBJECT *PFILE_OBJECT;


// Thread information structures

// IRQL
typedef UCHAR KIRQL, *PKIRQL;
#define PASSIVE_LEVEL 0             // Passive release level
#define LOW_LEVEL 0                 // Lowest interrupt level
#define APC_LEVEL 1                 // APC interrupt level
#define DISPATCH_LEVEL 2            // Dispatcher level

// Thread entry point
// NOTE: This is not a standard call!  You can't call this function from C code!
// You push registers like stdcall, but ebp + 4 must point to the first argument before the call!
//
// Differences from NT: 2 parameters instead of 1; strange calling convention
typedef
VOID
(NTAPI *PKSTART_ROUTINE) (
	IN PVOID StartContext1,
	IN PVOID StartContext2
	);

// Structure of a critical section
// Same as the XBOX's RTL_CRITICAL_SECTION, but with the more explicit header
typedef struct _KCRITICAL_SECTION
{
	// 000 Dispatcher header
	DISPATCHER_HEADER Header;
	// 010 Lock count of the critical section
	LONG LockCount;
	// 014 Recursion count of the critical section
	LONG RecursionCount;
	// 018 Thread ID of the thread that currently owns this critical section
	ULONG OwningThread;
} KCRITICAL_SECTION, *PKCRITICAL_SECTION;

// Structure of a thread object
typedef struct _KTHREAD
{
	// 000 Dispatcher header
	DISPATCHER_HEADER Header;
	// 010 Unknown
	BYTE unknown[0x18];
	// 028 Pointer to TLS data
	PVOID TlsData;
	// ??? just padding - real size is unknown
	BYTE unknown2[0x100];
} KTHREAD, *PKTHREAD;

// Structure of the data at FS
typedef struct _FS_STRUCTURE
{
	// 000 Current exception handler information
	PVOID *ExceptionFrame;
	// 004 Pointer to current TLS data top
	PVOID TlsDataTop;
	// 008
	BYTE unknown2[0x1C];
	// 024 Current IRQL of the OS
	KIRQL CurrentIrql;
	// 028 Thread structure of the current thread
	PKTHREAD ThreadObject;
	// ??? just padding - real size is unknown
	BYTE unknown3[0x100];
} FS_STRUCTURE, *PFS_STRUCTURE;

// DPC routine
typedef
VOID
(*PKDEFERRED_ROUTINE) (
	IN struct _KDPC *Dpc,
	IN PVOID DeferredContext,
	IN PVOID SystemArgument1,
	IN PVOID SystemArgument2
	);

// DPC information
// It's not known which of these fields are used on XBOX.
typedef struct _KDPC {
	CSHORT Type;
	UCHAR Number;
	UCHAR Importance;
	LIST_ENTRY DpcListEntry;
	PKDEFERRED_ROUTINE DeferredRoutine;
	PVOID DeferredContext;
	PVOID SystemArgument1;
	PVOID SystemArgument2;
	PULONG_PTR Lock;
} KDPC, *PKDPC;


// Timers
typedef enum _TIMER_TYPE {
    NotificationTimer,
    SynchronizationTimer
    } TIMER_TYPE;

typedef struct _KTIMER {
    DISPATCHER_HEADER Header;
    ULARGE_INTEGER DueTime;
    LIST_ENTRY TimerListEntry;
    struct _KDPC *Dpc;
    LONG Period;
} KTIMER, *PKTIMER;



// XBE stuff
// Not used in any exported kernel calls, but still useful.


// XBE header information
typedef struct _XBE_HEADER {
	// 000 "XBEH"
	CHAR Magic[4];
	// 004 RSA digital signature of the entire header area
	UCHAR HeaderSignature[256];
	// 104 Base address of XBE image (must be 0x00010000?)
	PVOID BaseAddress;
	// 108 Size of all headers combined - other headers must be within this
	ULONG HeaderSize;
	// 10C Size of entire image
	ULONG ImageSize;
	// 110 Size of this header (always 0x178?)
	ULONG XbeHeaderSize;
	// 114 Image timestamp - unknown format
	ULONG Timestamp;
	// 118 Pointer to certificate data (must be within HeaderSize)
	struct _XBE_CERTIFICATE *Certificate;
	// 11C Number of sections
	DWORD NumSections;
	// 120 Pointer to section headers (must be within HeaderSize)
	struct _XBE_SECTION *Sections;
	// 124 Initialization flags
	ULONG InitFlags;
	// 128 Entry point (XOR'd; see xboxhacker.net)
	PVOID EntryPoint;
	// 12C Pointer to TLS directory
	struct _XBE_TLS_DIRECTORY *TlsDirectory;
	// 130 Stack commit size
	ULONG StackCommit;
	// 134 Heap reserve size
	ULONG HeapReserve;
	// 138 Heap commit size
	ULONG HeapCommit;
	// 13C PE base address (?)
	PVOID PeBaseAddress;
	// 140 PE image size (?)
	ULONG PeImageSize;
	// 144 PE checksum (?)
	ULONG PeChecksum;
	// 148 PE timestamp (?)
	ULONG PeTimestamp;
	// 14C PC path and filename to EXE file from which XBE is derived
	PCSZ PcExePath;
	// 150 PC filename (last part of PcExePath) from which XBE is derived
	PCSZ PcExeFilename;
	// 154 PC filename (Unicode version of PcExeFilename)
	PWSTR PcExeFilenameUnicode;
	// 158 Pointer to kernel thunk table (XOR'd; EFB1F152 debug)
	ULONG_PTR *KernelThunkTable;
	// 15C Non-kernel import table (debug only)
	PVOID DebugImportTable;
	// 160 Number of library headers
	ULONG NumLibraries;
	// 164 Pointer to library headers
	struct _XBE_LIBRARY *Libraries;
	// 168 Pointer to kernel library header
	struct _XBE_LIBRARY *KernelLibrary;
	// 16C Pointer to XAPI library
	struct _XBE_LIBRARY *XapiLibrary;
	// 170 Pointer to logo bitmap (NULL = use default of Microsoft)
	PVOID LogoBitmap;
	// 174 Size of logo bitmap
	ULONG LogoBitmapSize;
	// 178
} XBE_HEADER, *PXBE_HEADER;

// Certificate structure
typedef struct _XBE_CERTIFICATE {
	// 000 Size of certificate
	ULONG Size;
	// 004 Certificate timestamp (unknown format)
	ULONG Timestamp;
	// 008 Title ID
	ULONG TitleId;
	// 00C Name of the game (Unicode)
	WCHAR TitleName[40];
	// 05C Alternate title ID's (0-terminated)
	ULONG AlternateTitleIds[16];
	// 09C Allowed media types - 1 bit match between XBE and media = boots
	ULONG MediaTypes;
	// 0A0 Allowed game regions - 1 bit match between this and XBOX = boots
	ULONG GameRegion;
	// 0A4 Allowed game ratings - 1 bit match between this and XBOX = boots
	ULONG GameRating;
	// 0A8 Disk number (?)
	ULONG DiskNumber;
	// 0AC Version (?)
	ULONG Version;
	// 0B0 LAN key for this game
	UCHAR LanKey[16];
	// 0C0 Signature key for this game
	UCHAR SignatureKey[16];
	// 0D0 Signature keys for the alternate title ID's
	UCHAR AlternateSignatureKeys[16][16];
	// 1D0
} XBE_CERTIFICATE, *PXBE_CERTIFICATE;

// Section headers
typedef struct _XBE_SECTION {
	// 000 Flags
	ULONG Flags;
	// 004 Virtual address (where this section loads in RAM)
	PVOID VirtualAddress;
	// 008 Virtual size (size of section in RAM; after FileSize it's 00'd)
	ULONG VirtualSize;
	// 00C File address (where in the file from which this section comes)
	ULONG FileAddress;
	// 010 File size (size of the section in the XBE file)
	ULONG FileSize;
	// 014 Pointer to section name
	PCSZ SectionName;
	// 018 Section reference count - when >= 1, section is loaded
	LONG SectionReferenceCount;
	// 01C Pointer to head shared page reference count
	WORD *HeadReferenceCount;
	// 020 Pointer to tail shared page reference count
	WORD *TailReferenceCount;
	// 024 SHA hash.  Hash DWORD containing FileSize, then hash section.
	DWORD ShaHash[5];
	// 038
} XBE_SECTION, *PXBE_SECTION;

// TLS directory information needed later
// Library version data needed later

// Initialization flags
#define XBE_INIT_MOUNT_UTILITY          0x00000001
#define XBE_INIT_FORMAT_UTILITY         0x00000002
#define XBE_INIT_64M_RAM_ONLY           0x00000004
#define XBE_INIT_DONT_SETUP_HDD         0x00000008

// Region codes
#define XBE_REGION_US_CANADA            0x00000001
#define XBE_REGION_JAPAN                0x00000002
#define XBE_REGION_ELSEWHERE            0x00000004
#define XBE_REGION_DEBUG                0x80000000

// Media types
#define XBE_MEDIA_HDD                   0x00000001
#define XBE_MEDIA_XBOX_DVD              0x00000002
#define XBE_MEDIA_ANY_CD_OR_DVD         0x00000004
#define XBE_MEDIA_CD                    0x00000008
#define XBE_MEDIA_1LAYER_DVDROM         0x00000010
#define XBE_MEDIA_2LAYER_DVDROM         0x00000020
#define XBE_MEDIA_1LAYER_DVDR           0x00000040
#define XBE_MEDIA_2LAYER_DVDR           0x00000080
#define XBE_MEDIA_USB                   0x00000100
#define XBE_MEDIA_ALLOW_UNLOCKED_HDD    0x40000000

// Section flags
#define XBE_SEC_WRITABLE                0x00000001
#define XBE_SEC_PRELOAD                 0x00000002
#define XBE_SEC_EXECUTABLE              0x00000004
#define XBE_SEC_INSERTED_FILE           0x00000008
#define XBE_SEC_RO_HEAD_PAGE            0x00000010
#define XBE_SEC_RO_TAIL_PAGE            0x00000020


// x86 page size
#define PAGE_SIZE 0x1000


// Native NT API calls on the XBOX


// PAGE_ALIGN:
// Returns an address rounded down to the nearest page boundary.
//
// Differences from NT: None.
#define PAGE_ALIGN(Va) ((PVOID)((ULONG_PTR)(Va) & ~(PAGE_SIZE - 1)))


// NtReadFile:
// Reads a file.
//
// Differences from NT: There is no Key parameter.
NTSYSAPI
EXPORTNUM(219)
NTSTATUS
NTAPI
NtReadFile(
	IN HANDLE FileHandle,
	IN HANDLE Event OPTIONAL,
	IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
	IN PVOID ApcContext OPTIONAL,
	OUT PIO_STATUS_BLOCK IoStatusBlock,
	OUT PVOID Buffer,
	IN ULONG Length,
	IN PLARGE_INTEGER ByteOffset
	);

// NtWriteFile:
// Writes a file.
//
// Differences from NT: There is no Key parameter.
NTSYSAPI
EXPORTNUM(236)
NTSTATUS
NTAPI
NtWriteFile(
	IN HANDLE FileHandle,
	IN HANDLE Event OPTIONAL,
	IN PIO_APC_ROUTINE ApcRoutine OPTIONAL,
	IN PVOID ApcContext OPTIONAL,
	OUT PIO_STATUS_BLOCK IoStatusBlock,
	IN PVOID Buffer,
	IN ULONG Length,
	IN PLARGE_INTEGER ByteOffset
	);

// NtQueryVolumeInformation:
// Queries information about a file system.  This is not documented by
// Microsoft even under NT.
//
// Differences from NT: None known.
NTSYSAPI
EXPORTNUM(218)
NTSTATUS
NTAPI
NtQueryVolumeInformationFile(
	IN HANDLE FileHandle,
	OUT PIO_STATUS_BLOCK IoStatusBlock,
	OUT PVOID VolumeInformation,
	IN ULONG VolumeInformationLength,
	IN FS_INFORMATION_CLASS VolumeInformationClass
	);

// NtClose:
// Closes a file or other handle.
//
// Differences from NT: None.
NTSYSAPI
EXPORTNUM(187)
NTSTATUS
NTAPI
NtClose(
	IN HANDLE Handle
	);

// NtAllocateVirtualMemory:
// Allocates virtual memory.
//
// Differences from NT: There is no ProcessHandle parameter.
NTSYSAPI
EXPORTNUM(184)
NTSTATUS
NTAPI
NtAllocateVirtualMemory(
	IN OUT PVOID *BaseAddress,
	IN ULONG ZeroBits,
	IN OUT PULONG AllocationSize,
	IN ULONG AllocationType,
	IN ULONG Protect
	);

// NtFreeVirtualMemory:
// Frees virtual memory.
//
// Differences from NT: There is no ProcessHandle parameter.
NTSYSAPI
EXPORTNUM(199)
NTSTATUS
NTAPI
NtFreeVirtualMemory(
	IN OUT PVOID *BaseAddress,
	IN OUT PULONG FreeSize,
	IN ULONG FreeType
	);


// Kernel-level routines

// MmMapIoSpace:
// Maps a physical address area into the virtual address space.
// DO NOT USE MEMORY MAPPED WITH THIS AS A BUFFER TO OTHER CALLS.  For
// example, don't WriteFile or NtWriteFile these buffers.  Copy them first.
//
// Differences from NT: PhysicalAddress is 32 bit, not 64.  ProtectionType
//     specifies the page protections, but it's a Win32 PAGE_ macro instead
//     of the normal NT enumeration.  PAGE_READWRITE is probably what you
//     want...
NTSYSAPI
EXPORTNUM(177)
PVOID
NTAPI
MmMapIoSpace(
	IN PHYSICAL_ADDRESS PhysicalAddress,
	IN ULONG NumberOfBytes,
	IN ULONG ProtectionType
	);

// MmGetPhysicalAddress:
// Translates a virtual address into a physical address.
//
// Differences from NT: PhysicalAddress is 32 bit, not 64.
NTSYSAPI
EXPORTNUM(173)
PHYSICAL_ADDRESS
NTAPI
MmGetPhysicalAddress(
	IN PVOID BaseAddress
	);

// MmUnmapIoSpace:
// Unmaps a virtual address mapping made by MmMapIoSpace.
//
// Differences from NT: None.
NTSYSAPI
EXPORTNUM(183)
PVOID
NTAPI
MmUnmapIoSpace(
	IN PVOID BaseAddress,
	IN ULONG NumberOfBytes
	);

// MmAllocateContiguousMemory:
// Allocates a range of physically contiguous, cache-aligned memory from the
// non-paged pool (= main pool on XBOX).
//
// Differences from NT: HighestAcceptableAddress was deleted, opting instead
//     to not care about the highest address.
NTSYSAPI
EXPORTNUM(165)
PVOID
NTAPI
MmAllocateContiguousMemory(
	IN ULONG NumberOfBytes
	);

// MmFreeContiguousMemory:
// Frees memory allocated with MmAllocateContiguousMemory.
//
// Differences from NT: None.
NTSYSAPI
EXPORTNUM(171)
VOID
NTAPI
MmFreeContiguousMemory(
	IN PVOID BaseAddress
	);

// IoCreateSymbolicLink:
// Creates a symbolic link in the object namespace.
// NtCreateSymbolicLinkObject is much harder to use than this simple
// function, so just use this one.
//
// Differences from NT: Uses ANSI_STRING instead of UNICODE_STRING.
NTSYSAPI
EXPORTNUM(67)
NTSTATUS
NTAPI
IoCreateSymbolicLink(
	IN PANSI_STRING SymbolicLinkName,
	IN PANSI_STRING DeviceName
	);

// IoDeleteSymbolicLink:
// Creates a symbolic link in the object namespace.  Deleting symbolic links
// through the Nt* functions is a pain, so use this instead.
//
// Differences from NT: Uses ANSI_STRING instead of UNICODE_STRING.
NTSYSAPI
EXPORTNUM(69)
NTSTATUS
NTAPI
IoDeleteSymbolicLink(
	IN PANSI_STRING SymbolicLinkName
	);


// ObReferenceObjectByHandle:
// Turns a handle into a kernel object pointer.  The ObjectType parameter
// specifies what type of object it is.  This function also increments the
// object's reference count.
//
// Differences from NT: There are no DesiredAccess, AccessMode, or
//     HandleInformation parameters.
NTSYSAPI
EXPORTNUM(246)
NTSTATUS
NTAPI
ObReferenceObjectByHandle(
    IN HANDLE Handle,
    IN POBJECT_TYPE ObjectType OPTIONAL,
    OUT PVOID *Object
    );

// ObfReferenceObject/ObReferenceObject:
// Increments the object's reference count.
//
// Differences from NT: None.
#define ObReferenceObject(Object) ObfReferenceObject(Object)
NTSYSAPI
EXPORTNUM(251)
VOID
FASTCALL
ObfReferenceObject(
    IN PVOID Object
    );

// ObfDereferenceObject/ObDereferenceObject:
// Decrements the object's reference count, deleting it if it is now unused.
//
// Differences from NT: None.
#define ObDereferenceObject(a) ObfDereferenceObject(a)
NTSYSAPI
EXPORTNUM(250)
VOID
FASTCALL
ObfDereferenceObject(
    IN PVOID Object
    );

// Kernel routines only in the XBOX

// HalEnableSecureTrayEject:
// Notifies the SMBUS that ejecting the DVD-ROM should not reset the system.
// Note that this function can't really be called directly...
//
// New to the XBOX.
NTSYSAPI
EXPORTNUM(365)
VOID
NTAPI
HalEnableSecureTrayEject(
	VOID
	);

// XeLoadSection:
// Adds one to the reference count of the specified section and loads if the
// count is now above zero.
//
// New to the XBOX.
NTSYSAPI
EXPORTNUM(327)
NTSTATUS
NTAPI
XeLoadSection(
	IN OUT PXBE_SECTION section
	);

// Error codes
#define STATUS_SUCCESS					0x00000000
#define STATUS_UNSUCCESSFUL				0xC0000001
#define STATUS_UNRECOGNIZED_MEDIA		0xC0000014
// The SCSI input buffer was too large (not necessarily an error!)
#define STATUS_DATA_OVERRUN				0xC000003C
#define STATUS_INVALID_IMAGE_FORMAT     0xC000007B
#define STATUS_INSUFFICIENT_RESOURCES   0xC000009A
#define STATUS_TOO_MANY_SECRETS			0xC0000156
#define STATUS_REGION_MISMATCH			0xC0050001

#ifdef __cplusplus
};
#endif

#include <poppack.h>

extern "C"
{
	// Thanks and credit go to Woodoo
	extern VOID  WINAPI HalWriteSMBusValue(BYTE, BYTE, BOOL, BYTE);
	extern VOID  WINAPI HalReadSMCTrayState(DWORD* state, DWORD* count);

	// Thanks and credit go to Team Evox
	extern VOID	 WINAPI HalReturnToFirmware(DWORD);

	extern INT WINAPI XNetLoadConfigParams(LPBYTE);   
	extern INT WINAPI XNetSaveConfigParams(LPBYTE);     

	extern INT WINAPI XWriteTitleInfoNoReboot(LPVOID,LPVOID,DWORD,DWORD,LPVOID);

	extern DWORD* LaunchDataPage;  
}

#endif

#endif // __XBOX_INTERNAL_H__
