/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

/* The WDM-KS audio driver.
 *
 * Kernel streaming: the audio path below DirectSound and beside
 * WASAPI, where the frontend writes to a filter's pin and the packets
 * reach the driver without a mixer between. On Windows 2000 and up;
 * where the system has no kernel streaming this enumerates nothing
 * and declines, so the frontend falls back.
 *
 * ---- the interface, declared here ---------------------------------
 *
 * The slice of the kernel-streaming interface this driver uses,
 * spelled here rather than taken from ks.h and ksmedia.h.
 *
 * Those headers ship with the driver kit, not with the Platform SDK -
 * the 2005 toolchain this project still builds with does not have
 * them, and neither do several of the cross toolchains. Nothing in the
 * tree includes them today, and starting to would mean a driver that
 * cannot be built where it most needs to be. So this is the same
 * arrangement audio/drivers/asio.c uses for the ASIO interface and
 * audio/drivers/openal.c for the OpenAL Soft extensions: declare what
 * is used, and nothing else.
 *
 * This is an ABI. Every field order, width and constant below is the
 * system's and not ours, taken from the published headers, and the
 * build-time assertions at the bottom fail the build rather than the
 * stream if any of it drifts. Names are prefixed so that a
 * translation unit which does include ks.h - a future microphone
 * path, say - does not collide, and so that the griffin build, which
 * puts every driver in one translation unit, has no name of ours to
 * clash with anything else in it.
 *
 * The types are the ones the DDK uses, so that a structure laid out
 * here is laid out the way the kernel writes it: ULONG is 32-bit on
 * both 32- and 64-bit Windows, and a GUID is 16 bytes on both. Nothing
 * here holds a pointer, which is what would have differed. */

#include <stdlib.h>
#include <string.h>

#include <windows.h>
/* WAVEFORMATEX and WAVEFORMATEXTENSIBLE. mmreg.h is where the plain
 * one lives; mmsystem.h drags in the rest of multimedia and is not
 * wanted here. */
#include <mmreg.h>

#include <boolean.h>
#include <retro_miscellaneous.h>
#include <lists/string_list.h>
#include <string/stdstring.h>

#include "../audio_upmix.h"
#include "../audio_driver.h"
#include "../../verbosity.h"

/* ---- identifiers ------------------------------------------------- */

/* The device interface classes an audio filter registers under.
 * KSCATEGORY_AUDIO is every audio filter; RENDER and CAPTURE are the
 * aliases that say which way a given one goes, which is how a render
 * device is told from a capture one without opening it. */
#define RA_KS_GUID(n, a, b, c, d0, d1, d2, d3, d4, d5, d6, d7) \
   static const GUID n = { a, b, c, { d0, d1, d2, d3, d4, d5, d6, d7 } }

RA_KS_GUID(ra_ks_category_audio,
      0x6994AD04, 0x93EF, 0x11D0, 0xA3, 0xCC, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96);
RA_KS_GUID(ra_ks_category_render,
      0x65E8773E, 0x8F56, 0x11D0, 0xA3, 0xB9, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96);
/* Capture is not used by this driver - it is here because the ABI
 * check compares it, and because a microphone path would want it. */
/* A filter carrying this alias streams WaveRT: its pins are driven
 * through a mapped buffer and a position register rather than by
 * submitting packets, and they refuse the standard streaming
 * interface this driver opens them with. Detected so that such a
 * device can say what it is instead of refusing every format for no
 * stated reason - which is what an NVIDIA HDMI output does. */
RA_KS_GUID(ra_ks_category_realtime,
      0xEB115FFC, 0x10C8, 0x4964, 0x83, 0x1D, 0x6D, 0xCB, 0x02, 0xE6, 0xF2, 0x3F);

RA_KS_GUID(ra_ks_category_capture,
      0x65E8773D, 0x8F56, 0x11D0, 0xA3, 0xB9, 0x00, 0xA0, 0xC9, 0x22, 0x31, 0x96);

/* The property set every pin answers: how many pins there are, which
 * way each one flows, and what formats each will take. */
RA_KS_GUID(ra_ks_propsetid_pin,
      0x8C134960, 0x51AD, 0x11CF, 0x87, 0x8A, 0x94, 0xF8, 0x01, 0xC1, 0x00, 0x00);

/* Format identifiers. The subtypes are the WAVE format tags in the
 * 0000-0010-8000-00AA00389B71 family, the same family WASAPI's
 * WAVEFORMATEXTENSIBLE subtypes come from - 0x0001 integer PCM,
 * 0x0003 float - so audio/common/mmdevice_common_inline.h carries the
 * same two values for the same reason. */
RA_KS_GUID(ra_ks_dataformat_type_audio,
      0x73647561, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71);
RA_KS_GUID(ra_ks_dataformat_subtype_pcm,
      0x00000001, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71);
RA_KS_GUID(ra_ks_dataformat_subtype_float,
      0x00000003, 0x0000, 0x0010, 0x80, 0x00, 0x00, 0xAA, 0x00, 0x38, 0x9B, 0x71);
RA_KS_GUID(ra_ks_dataformat_specifier_wfx,
      0x05589F81, 0xC356, 0x11CE, 0xBF, 0x01, 0x00, 0xAA, 0x00, 0x55, 0x59, 0x5A);

/* ---- properties -------------------------------------------------- */

/* KSPROPERTY_PIN, in the header's order - the values are the
 * positions, so the whole enumeration is written out even though only
 * four members are asked for, because leaving a gap would renumber
 * everything after it. */
enum
{
   RA_KSPROPERTY_PIN_CINSTANCES = 0,
   RA_KSPROPERTY_PIN_CTYPES,
   RA_KSPROPERTY_PIN_DATAFLOW,
   RA_KSPROPERTY_PIN_DATARANGES,
   RA_KSPROPERTY_PIN_DATAINTERSECTION,
   RA_KSPROPERTY_PIN_INTERFACES,
   RA_KSPROPERTY_PIN_MEDIUMS,
   RA_KSPROPERTY_PIN_COMMUNICATION,
   RA_KSPROPERTY_PIN_GLOBALCINSTANCES,
   RA_KSPROPERTY_PIN_NECESSARYINSTANCES,
   RA_KSPROPERTY_PIN_PHYSICALCONNECTION,
   RA_KSPROPERTY_PIN_CATEGORY,
   RA_KSPROPERTY_PIN_NAME,
   RA_KSPROPERTY_PIN_CONSTRAINEDDATARANGES,
   RA_KSPROPERTY_PIN_PROPOSEDATAFORMAT
};

/* KSPIN_DATAFLOW. IN is into the filter, which for a render filter is
 * the direction audio is written - the pin this frontend wants. */
enum
{
   RA_KSPIN_DATAFLOW_IN = 1,
   RA_KSPIN_DATAFLOW_OUT
};

/* KSPIN_COMMUNICATION. A pin worth opening is one this side may be
 * the source for, which is SINK or BOTH. */
enum
{
   RA_KSPIN_COMMUNICATION_NONE = 0,
   RA_KSPIN_COMMUNICATION_SINK,
   RA_KSPIN_COMMUNICATION_SOURCE,
   RA_KSPIN_COMMUNICATION_BOTH,
   RA_KSPIN_COMMUNICATION_BRIDGE
};

/* KSSTATE, for the transitions a running pin is driven through. */
enum
{
   RA_KSSTATE_STOP = 0,
   RA_KSSTATE_ACQUIRE,
   RA_KSSTATE_PAUSE,
   RA_KSSTATE_RUN
};

/* KSPROPERTY_TYPE_*, the request flags. */
#define RA_KSPROPERTY_TYPE_GET      0x00000001
#define RA_KSPROPERTY_TYPE_SET      0x00000002
#define RA_KSPROPERTY_TYPE_TOPOLOGY 0x10000000

/* IOCTL_KS_PROPERTY, built the way the DDK builds it:
 * CTL_CODE(FILE_DEVICE_KS, 0x000, METHOD_NEITHER, FILE_ANY_ACCESS).
 * Spelled as the arithmetic rather than the number so it can be read
 * against the definition. */
#define RA_FILE_DEVICE_KS 0x0000002f
#define RA_KS_CTL_CODE(fn) \
   (((RA_FILE_DEVICE_KS) << 16) | ((FILE_ANY_ACCESS) << 14) | ((fn) << 2) | (METHOD_NEITHER))
#define RA_IOCTL_KS_PROPERTY RA_KS_CTL_CODE(0x000)

/* ---- structures -------------------------------------------------- */

/* KSIDENTIFIER, which KSPROPERTY is a typedef of: the set a request
 * belongs to, which member of it, and what kind of request. */
typedef struct
{
   GUID  Set;
   ULONG Id;
   ULONG Flags;
} ra_ksproperty_t;

/* KSP_PIN: a pin property carries the pin it is about. */
typedef struct
{
   ra_ksproperty_t Property;
   ULONG           PinId;
   ULONG           Reserved;
} ra_ksp_pin_t;

/* KSMULTIPLE_ITEM, the header on a list-valued property - the pin
 * data ranges come back behind one of these. */
typedef struct
{
   ULONG Size;
   ULONG Count;
} ra_ksmultiple_item_t;

/* KSDATAFORMAT, which KSDATARANGE is also a typedef of.
 *
 * The real declaration is a union of the fields with a LONGLONG, and
 * that LONGLONG is not decoration: it aligns the type to eight, which
 * pads anything embedding it. Declared as a plain struct this comes
 * out 84 bytes where the system says 88, and every field of a data
 * range past the first would have been read from four bytes off. The
 * union is kept for that reason and for no other - nothing reads
 * Alignment. */
typedef union
{
   struct
   {
      ULONG FormatSize;
      ULONG Flags;
      ULONG SampleSize;
      ULONG Reserved;
      GUID  MajorFormat;
      GUID  SubFormat;
      GUID  Specifier;
   } f;
   LONGLONG Alignment;
} ra_ksdataformat_t;

/* KSDATARANGE_AUDIO: what a pin will take, as a range rather than one
 * format. A pin may report several of these. */
typedef struct
{
   ra_ksdataformat_t DataRange;
   ULONG             MaximumChannels;
   ULONG             MinimumBitsPerSample;
   ULONG             MaximumBitsPerSample;
   ULONG             MinimumSampleFrequency;
   ULONG             MaximumSampleFrequency;
} ra_ksdatarange_audio_t;

/* KSIDENTIFIER again, under the names the connect request uses. */
typedef ra_ksproperty_t ra_kspin_interface_t;
typedef ra_ksproperty_t ra_kspin_medium_t;

typedef struct
{
   ULONG PriorityClass;
   ULONG PrioritySubClass;
} ra_kspriority_t;

/* KSPIN_CONNECT: the request that instantiates a pin. PinToHandle is
 * a HANDLE, so this structure is one of the few whose size differs
 * between 32- and 64-bit - the assertions compare it against the
 * system's rather than against a number. */
typedef struct
{
   ra_kspin_interface_t Interface;
   ra_kspin_medium_t    Medium;
   ULONG                PinId;
   HANDLE               PinToHandle;
   ra_kspriority_t      Priority;
} ra_kspin_connect_t;

/* KSDATAFORMAT_WAVEFORMATEX, which the system declares inside
 * pshpack1.h - it is packed to one, not padded. A WAVEFORMATEX is 18
 * bytes, and unpacked this would come out 88 where the system says
 * 82, putting the format six bytes adrift of where the driver reads
 * it. The pragma is what the system does and is not optional. */
#pragma pack(push, 1)
typedef struct
{
   ra_ksdataformat_t DataFormat;
   WAVEFORMATEX      WaveFormatEx;
} ra_ksdataformat_wfx_t;
#pragma pack(pop)

/* The standard streaming interface and medium a render pin is opened
 * on, and the connection property set its state is driven through. */
RA_KS_GUID(ra_ks_interfacesetid_standard,
      0x1A8766A0, 0x62CE, 0x11CF, 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00);
RA_KS_GUID(ra_ks_mediumsetid_standard,
      0x4747B320, 0x62CE, 0x11CF, 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00);
RA_KS_GUID(ra_ks_propsetid_connection,
      0x1D58C920, 0xAC9B, 0x11CF, 0xA5, 0xD6, 0x28, 0xDB, 0x04, 0xC1, 0x00, 0x00);

#define RA_KSINTERFACE_STANDARD_STREAMING 0
#define RA_KSMEDIUM_TYPE_ANYINSTANCE      0
#define RA_KSPRIORITY_NORMAL              0x40000000
#define RA_KSPROPERTY_CONNECTION_STATE    0

/* KSTIME and KSSTREAM_HEADER: the packet handed to the pin.
 *
 * The header has a trailing Reserved on 64-bit only - the system
 * declares it inside #ifdef _WIN64 - so its size differs by word, and
 * the assertions compare it against the system's sizeof rather than a
 * number. Data is a pointer, which differs anyway. */
typedef struct
{
   LONGLONG Time;
   ULONG    Numerator;
   ULONG    Denominator;
} ra_kstime_t;

typedef struct
{
   ULONG       Size;
   ULONG       TypeSpecificFlags;
   ra_kstime_t PresentationTime;
   LONGLONG    Duration;
   ULONG       FrameExtent;
   ULONG       DataUsed;
   void       *Data;
   ULONG       OptionsFlags;
#ifdef _WIN64
   ULONG       Reserved;
#endif
} ra_ksstream_header_t;

/* IOCTL_KS_WRITE_STREAM, built as the DDK builds it:
 * CTL_CODE(FILE_DEVICE_KS, 0x004, METHOD_NEITHER, FILE_WRITE_ACCESS). */
#define RA_IOCTL_KS_WRITE_STREAM \
   (((RA_FILE_DEVICE_KS) << 16) | ((FILE_WRITE_ACCESS) << 14) \
    | ((0x004) << 2) | (METHOD_NEITHER))

/* The audio property set, for the pin's own position. PlayOffset is
 * what the device has played, in bytes, and it is the hardware's
 * count rather than a tally of what this driver submitted - which is
 * what makes it worth pairing with a clock. */
RA_KS_GUID(ra_ks_propsetid_audio,
      0x45FFAAA0, 0x6E1B, 0x11D0, 0xBC, 0xF2, 0x44, 0x45, 0x53, 0x54, 0x00, 0x00);

#define RA_KSPROPERTY_AUDIO_LATENCY  1
#define RA_KSPROPERTY_AUDIO_POSITION 5

typedef struct
{
   DWORDLONG PlayOffset;
   DWORDLONG WriteOffset;
} ra_ksaudio_position_t;

/* ---- the layout is the system's, and the build says so ------------ */

/* A wrong layout here does not misbehave visibly - it hands the kernel
 * a structure whose fields are in the wrong places, and reads its
 * answers out of the wrong offsets. These fail the build instead. The
 * sizes are the same on 32- and 64-bit Windows because nothing above
 * holds a pointer. */
typedef char ra_ks_assert_property[(sizeof(ra_ksproperty_t) == 24) ? 1 : -1];
typedef char ra_ks_assert_ksp_pin[(sizeof(ra_ksp_pin_t) == 32) ? 1 : -1];
typedef char ra_ks_assert_multiple[(sizeof(ra_ksmultiple_item_t) == 8) ? 1 : -1];
typedef char ra_ks_assert_dataformat[(sizeof(ra_ksdataformat_t) == 64) ? 1 : -1];
typedef char ra_ks_assert_range_audio[(sizeof(ra_ksdatarange_audio_t) == 88) ? 1 : -1];
typedef char ra_ks_assert_guid[(sizeof(GUID) == 16) ? 1 : -1];
typedef char ra_ks_assert_priority[(sizeof(ra_kspriority_t) == 8) ? 1 : -1];
typedef char ra_ks_assert_kstime[(sizeof(ra_kstime_t) == 16) ? 1 : -1];
typedef char ra_ks_assert_position[(sizeof(ra_ksaudio_position_t) == 16) ? 1 : -1];
/* Packed, so 64 + 18 with nothing between or after. */
typedef char ra_ks_assert_wfx[(sizeof(ra_ksdataformat_wfx_t) == 82) ? 1 : -1];


/* ---- the setup interface, resolved rather than imported ----------- */

/* SetupAPI is how a filter is found: audio filters register a device
 * interface, and the render ones register an alias saying so, which is
 * how a playback device is told from a capture one without opening it.
 *
 * The entry points are resolved through LoadLibrary rather than
 * imported, and the structures are declared here rather than taken
 * from setupapi.h, for the same two reasons as the kernel-streaming
 * block above: this file has to build with toolchains whose SDK is
 * old, and the binary has to LOAD on a Windows that has none of this -
 * an import of a missing symbol stops the process before main, where a
 * resolve that fails is a driver that reports no devices and lets the
 * frontend fall back to DirectSound. */

#define RA_DIGCF_PRESENT         0x00000002
#define RA_DIGCF_DEVICEINTERFACE 0x00000010
#define RA_SPDRP_FRIENDLYNAME    0x0000000C
#define RA_SPDRP_DEVICEDESC      0x00000000

typedef struct
{
   DWORD     cbSize;
   GUID      InterfaceClassGuid;
   DWORD     Flags;
   ULONG_PTR Reserved;
} ra_sp_device_interface_data;

typedef struct
{
   DWORD     cbSize;
   GUID      ClassGuid;
   DWORD     DevInst;
   ULONG_PTR Reserved;
} ra_sp_devinfo_data;

typedef struct
{
   DWORD cbSize;
   WCHAR DevicePath[1];
} ra_sp_device_interface_detail_data_w;

typedef HANDLE (WINAPI *ra_setupdi_get_class_devs_w_t)(
      const GUID*, PCWSTR, HWND, DWORD);
typedef BOOL (WINAPI *ra_setupdi_enum_interfaces_t)(
      HANDLE, ra_sp_devinfo_data*, const GUID*, DWORD,
      ra_sp_device_interface_data*);
typedef BOOL (WINAPI *ra_setupdi_get_interface_detail_w_t)(
      HANDLE, ra_sp_device_interface_data*,
      ra_sp_device_interface_detail_data_w*, DWORD, DWORD*,
      ra_sp_devinfo_data*);
typedef BOOL (WINAPI *ra_setupdi_get_interface_alias_t)(
      HANDLE, ra_sp_device_interface_data*, const GUID*,
      ra_sp_device_interface_data*);
typedef BOOL (WINAPI *ra_setupdi_get_registry_property_w_t)(
      HANDLE, ra_sp_devinfo_data*, DWORD, DWORD*, PBYTE, DWORD, DWORD*);
typedef BOOL (WINAPI *ra_setupdi_destroy_list_t)(HANDLE);

typedef struct
{
   HMODULE                              lib;
   ra_setupdi_get_class_devs_w_t        get_class_devs;
   ra_setupdi_enum_interfaces_t         enum_interfaces;
   ra_setupdi_get_interface_detail_w_t  get_detail;
   ra_setupdi_get_interface_alias_t     get_alias;
   ra_setupdi_get_registry_property_w_t get_property;
   ra_setupdi_destroy_list_t            destroy_list;
} ra_setupapi_t;

static ra_setupapi_t g_setupapi;

/* GetProcAddress hands back a FARPROC, and turning that into the
 * function it actually is is a cast C has no clean spelling for:
 * straight through warns about the function type, through void* is
 * not allowed in C90, and a compound literal is not C90 either. A
 * plain union variable is, so the resolver below uses one. */
typedef union
{
   FARPROC proc;
   ra_setupdi_get_class_devs_w_t        get_class_devs;
   ra_setupdi_enum_interfaces_t         enum_interfaces;
   ra_setupdi_get_interface_detail_w_t  get_detail;
   ra_setupdi_get_interface_alias_t     get_alias;
   ra_setupdi_get_registry_property_w_t get_property;
   ra_setupdi_destroy_list_t            destroy_list;
} ra_wdmks_proc_t;

static bool wdmks_setupapi_init(void)
{
   ra_wdmks_proc_t p;

   if (g_setupapi.lib)
      return true;
   if (!(g_setupapi.lib = LoadLibraryA("setupapi.dll")))
      return false;

   p.proc = GetProcAddress(g_setupapi.lib, "SetupDiGetClassDevsW");
   g_setupapi.get_class_devs = p.get_class_devs;
   p.proc = GetProcAddress(g_setupapi.lib, "SetupDiEnumDeviceInterfaces");
   g_setupapi.enum_interfaces = p.enum_interfaces;
   p.proc = GetProcAddress(g_setupapi.lib, "SetupDiGetDeviceInterfaceDetailW");
   g_setupapi.get_detail = p.get_detail;
   p.proc = GetProcAddress(g_setupapi.lib, "SetupDiGetDeviceInterfaceAlias");
   g_setupapi.get_alias = p.get_alias;
   p.proc = GetProcAddress(g_setupapi.lib, "SetupDiGetDeviceRegistryPropertyW");
   g_setupapi.get_property = p.get_property;
   p.proc = GetProcAddress(g_setupapi.lib, "SetupDiDestroyDeviceInfoList");
   g_setupapi.destroy_list = p.destroy_list;

   /* All or nothing: a partial set is a Windows this driver does not
    * know, and half an enumeration is worse than none. */
   if (     !g_setupapi.get_class_devs  || !g_setupapi.enum_interfaces
         || !g_setupapi.get_detail      || !g_setupapi.get_alias
         || !g_setupapi.get_property    || !g_setupapi.destroy_list)
   {
      FreeLibrary(g_setupapi.lib);
      memset(&g_setupapi, 0, sizeof(g_setupapi));
      return false;
   }
   return true;
}

/* The size SetupDiGetDeviceInterfaceDetailW wants in cbSize is not
 * sizeof this structure. The system's declaration is packed to 1 on
 * 32-bit, where it is six bytes - a DWORD and one WCHAR - while any
 * sane compiler pads ours to eight. On 64-bit both agree at eight.
 * Passing the wrong one fails the call with ERROR_INVALID_USER_BUFFER
 * and nothing else, which is a confusing way to find out. */
static DWORD wdmks_detail_cb_size(void)
{
   return (sizeof(void*) == 8) ? 8 : 6;
}

/* ---- properties -------------------------------------------------- */

/* One KS property request. The kernel-streaming property IOCTL is
 * METHOD_NEITHER, so the property goes in as the input buffer and the
 * answer comes back in the output buffer; there is no packing beyond
 * that. Returns the bytes written, or 0. */
static ULONG wdmks_property(HANDLE filter, const GUID *set, ULONG id,
      ULONG flags, void *prop, ULONG prop_size, void *out, ULONG out_size)
{
   DWORD written = 0;
   ra_ksproperty_t *p = (ra_ksproperty_t*)prop;

   if (!filter || filter == INVALID_HANDLE_VALUE)
      return 0;

   p->Set   = *set;
   p->Id    = id;
   p->Flags = flags;

   if (!DeviceIoControl(filter, RA_IOCTL_KS_PROPERTY,
            prop, prop_size, out, out_size, &written, NULL))
      return 0;
   return (ULONG)written;
}

/* A pin property, which carries the pin it asks about. */
static ULONG wdmks_pin_property(HANDLE filter, ULONG pin_id, ULONG id,
      void *out, ULONG out_size)
{
   ra_ksp_pin_t p;
   memset(&p, 0, sizeof(p));
   p.PinId = pin_id;
   return wdmks_property(filter, &ra_ks_propsetid_pin, id,
         RA_KSPROPERTY_TYPE_GET, &p, sizeof(p), out, out_size);
}

/* A pin property whose answer is a list, which has to be asked for
 * twice: once for the size, once for the data. The caller frees. */
static ra_ksmultiple_item_t *wdmks_pin_property_multi(HANDLE filter,
      ULONG pin_id, ULONG id)
{
   ra_ksp_pin_t          p;
   ra_ksmultiple_item_t *item = NULL;
   ULONG                 size = 0;
   DWORD                 written = 0;

   memset(&p, 0, sizeof(p));
   p.PinId      = pin_id;
   p.Property.Set   = ra_ks_propsetid_pin;
   p.Property.Id    = id;
   p.Property.Flags = RA_KSPROPERTY_TYPE_GET;

   /* A NULL output buffer asks how large the answer is. The call fails
    * either way - what is wanted is the length it reports. */
   DeviceIoControl(filter, RA_IOCTL_KS_PROPERTY,
         &p, sizeof(p), NULL, 0, &written, NULL);
   size = (ULONG)written;
   if (size < sizeof(ra_ksmultiple_item_t))
      return NULL;

   if (!(item = (ra_ksmultiple_item_t*)calloc(1, size)))
      return NULL;

   if (!DeviceIoControl(filter, RA_IOCTL_KS_PROPERTY,
            &p, sizeof(p), item, size, &written, NULL))
   {
      free(item);
      return NULL;
   }
   return item;
}

/* ---- what a pin will take ---------------------------------------- */

/* A render pin, and the formats it said it would accept. The ranges a
 * pin reports are ranges rather than a list of formats: a minimum and
 * maximum rate, a minimum and maximum width, and a channel ceiling.
 * A pin may report several, and what this keeps is the widest of each
 * across all of them - a pin offering 44100-48000 stereo and
 * 44100-192000 8-channel is a pin that can do 192000 and 8, so the
 * ranges are collapsed here and the exact combination is settled when
 * the format is proposed. */
typedef struct
{
   ULONG pin_id;
   ULONG max_channels;
   ULONG min_bits;
   ULONG max_bits;
   ULONG min_rate;
   ULONG max_rate;
   bool  takes_pcm;
   bool  takes_float;
} wdmks_pin_t;

/* One audio filter: the device path to open it by, the name to show,
 * and the render pins found on it. */
#define WDMKS_MAX_PINS 32

typedef struct
{
   WCHAR       *path;
   char        *name;
   wdmks_pin_t  pins[WDMKS_MAX_PINS];
   unsigned     pin_count;
   bool         wavert;
} wdmks_device_t;

/* Reads one pin's data ranges and folds them into the pin record.
 * Returns false where the pin reports nothing usable, which is the
 * normal answer for the many pins on a filter that are not audio
 * sinks at all. */
static bool wdmks_pin_read_ranges(HANDLE filter, ULONG pin_id,
      wdmks_pin_t *pin)
{
   ra_ksmultiple_item_t *item;
   unsigned char        *walk;
   unsigned char        *end;
   ULONG                 i;
   bool                  any = false;

   if (!(item = wdmks_pin_property_multi(filter, pin_id,
               RA_KSPROPERTY_PIN_DATARANGES)))
      return false;

   walk = (unsigned char*)(item + 1);
   end  = (unsigned char*)item + item->Size;

   for (i = 0; i < item->Count; i++)
   {
      ra_ksdatarange_audio_t *r = (ra_ksdatarange_audio_t*)walk;
      ULONG                   step;

      /* Each range says its own length, and the next follows it
       * aligned to eight. A length that does not fit inside what the
       * kernel returned is a malformed answer and ends the walk
       * rather than reading past the buffer. */
      if (     walk + sizeof(ra_ksdataformat_t) > end
            || r->DataRange.f.FormatSize < sizeof(ra_ksdataformat_t))
         break;
      step = (r->DataRange.f.FormatSize + 7) & ~7u;
      if (walk + step > end)
         break;

      /* Audio ranges only, and only the two subtypes this frontend
       * writes. A pin also reports ranges for things like MIDI and
       * for the WILDCARD subtype, which promises nothing. */
      if (!memcmp(&r->DataRange.f.MajorFormat, &ra_ks_dataformat_type_audio, sizeof(GUID))
            && walk + sizeof(ra_ksdatarange_audio_t) <= end)
      {
         bool pcm   = !memcmp(&r->DataRange.f.SubFormat,
               &ra_ks_dataformat_subtype_pcm, sizeof(GUID));
         bool flt   = !memcmp(&r->DataRange.f.SubFormat,
               &ra_ks_dataformat_subtype_float, sizeof(GUID));

         if (pcm || flt)
         {
            if (pcm)
               pin->takes_pcm   = true;
            if (flt)
               pin->takes_float = true;

            if (!any)
            {
               pin->max_channels = r->MaximumChannels;
               pin->min_bits     = r->MinimumBitsPerSample;
               pin->max_bits     = r->MaximumBitsPerSample;
               pin->min_rate     = r->MinimumSampleFrequency;
               pin->max_rate     = r->MaximumSampleFrequency;
            }
            else
            {
               if (r->MaximumChannels        > pin->max_channels)
                  pin->max_channels = r->MaximumChannels;
               if (r->MinimumBitsPerSample   < pin->min_bits)
                  pin->min_bits     = r->MinimumBitsPerSample;
               if (r->MaximumBitsPerSample   > pin->max_bits)
                  pin->max_bits     = r->MaximumBitsPerSample;
               if (r->MinimumSampleFrequency < pin->min_rate)
                  pin->min_rate     = r->MinimumSampleFrequency;
               if (r->MaximumSampleFrequency > pin->max_rate)
                  pin->max_rate     = r->MaximumSampleFrequency;
            }
            any = true;
         }
      }
      walk += step;
   }

   free(item);
   return any && pin->max_channels > 0 && pin->max_rate > 0;
}

/* Finds the render pins on an open filter: the ones audio flows INTO
 * (a render filter's sink), that this side may be the source for, and
 * that report a usable audio range. */
static void wdmks_filter_read_pins(HANDLE filter, wdmks_device_t *dev)
{
   ra_ksproperty_t prop;
   ULONG           pin_count = 0;
   ULONG           i;

   memset(&prop, 0, sizeof(prop));
   if (wdmks_property(filter, &ra_ks_propsetid_pin,
            RA_KSPROPERTY_PIN_CTYPES, RA_KSPROPERTY_TYPE_GET,
            &prop, sizeof(prop), &pin_count, sizeof(pin_count))
         != sizeof(pin_count))
      return;

   for (i = 0; i < pin_count && dev->pin_count < WDMKS_MAX_PINS; i++)
   {
      ULONG       flow = 0;
      ULONG       comm = 0;
      wdmks_pin_t pin;

      if (wdmks_pin_property(filter, i, RA_KSPROPERTY_PIN_DATAFLOW,
               &flow, sizeof(flow)) != sizeof(flow))
         continue;
      if (flow != RA_KSPIN_DATAFLOW_IN)
         continue;

      if (wdmks_pin_property(filter, i, RA_KSPROPERTY_PIN_COMMUNICATION,
               &comm, sizeof(comm)) != sizeof(comm))
         continue;
      if (     comm != RA_KSPIN_COMMUNICATION_SINK
            && comm != RA_KSPIN_COMMUNICATION_BOTH)
         continue;

      memset(&pin, 0, sizeof(pin));
      pin.pin_id = i;
      if (!wdmks_pin_read_ranges(filter, i, &pin))
         continue;

      dev->pins[dev->pin_count++] = pin;
   }
}

/* ---- enumeration -------------------------------------------------- */

/* Every audio filter that is present and registers the render alias,
 * opened, asked for its pins, and kept if it has any this frontend can
 * write to. The caller frees with wdmks_devices_free().
 *
 * A filter that will not open, or opens and offers no render pin, is
 * skipped without complaint: a machine has several of these - the
 * capture side of a card, a filter that is busy, a virtual device
 * whose driver declines - and they are not errors. */
static wdmks_device_t *wdmks_devices_scan(unsigned *count_out)
{
   HANDLE          list;
   wdmks_device_t *devices = NULL;
   unsigned        count   = 0;
   unsigned        cap     = 0;
   DWORD           index   = 0;

   *count_out = 0;
   if (!wdmks_setupapi_init())
      return NULL;

   list = g_setupapi.get_class_devs(&ra_ks_category_audio, NULL, NULL,
         RA_DIGCF_PRESENT | RA_DIGCF_DEVICEINTERFACE);
   if (!list || list == INVALID_HANDLE_VALUE)
      return NULL;

   for (;;)
   {
      ra_sp_device_interface_data           iface;
      ra_sp_device_interface_data           alias;
      ra_sp_devinfo_data                    info;
      ra_sp_device_interface_detail_data_w *detail;
      DWORD                                 needed = 0;
      HANDLE                                filter;
      wdmks_device_t                        dev;
      WCHAR                                 friendly[256];

      memset(&iface, 0, sizeof(iface));
      iface.cbSize = sizeof(iface);
      if (!g_setupapi.enum_interfaces(list, NULL, &ra_ks_category_audio,
               index++, &iface))
         break;

      /* The render alias is what says this filter plays rather than
       * records. A filter without it is a capture device or a
       * topology node, and is not asked anything further. */
      memset(&alias, 0, sizeof(alias));
      alias.cbSize = sizeof(alias);
      if (!g_setupapi.get_alias(list, &iface, &ra_ks_category_render, &alias))
         continue;
      if (!(alias.Flags & 0x00000001) || (alias.Flags & 0x00000002))
         continue;   /* not active, or removed */

      /* Twice: once for the length, once for the path. */
      memset(&info, 0, sizeof(info));
      info.cbSize = sizeof(info);
      g_setupapi.get_detail(list, &iface, NULL, 0, &needed, NULL);
      if (needed < sizeof(DWORD) + sizeof(WCHAR))
         continue;
      if (!(detail = (ra_sp_device_interface_detail_data_w*)calloc(1, needed)))
         continue;
      detail->cbSize = wdmks_detail_cb_size();
      if (!g_setupapi.get_detail(list, &iface, detail, needed, NULL, &info))
      {
         free(detail);
         continue;
      }

      memset(&dev, 0, sizeof(dev));

      /* WaveRT or WaveCyclic - the realtime alias is what says which,
       * and only the second is streamed here. */
      {
         ra_sp_device_interface_data rt;
         memset(&rt, 0, sizeof(rt));
         rt.cbSize  = sizeof(rt);
         dev.wavert = g_setupapi.get_alias(list, &iface,
               &ra_ks_category_realtime, &rt) ? true : false;
      }

      filter = CreateFileW(detail->DevicePath,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
      if (filter == INVALID_HANDLE_VALUE)
      {
         /* Said rather than skipped in silence. A filter that will
          * not open is usually one something else holds, and a device
          * missing from this list for that reason looks exactly like
          * a device that is not there - which is how an enumeration
          * that found three filters one moment and one the next went
          * unexplained. */
         RARCH_DBG("[WDM-KS] A filter would not open: 0x%08lx.\n",
               (unsigned long)GetLastError());
         free(detail);
         continue;
      }
      wdmks_filter_read_pins(filter, &dev);
      CloseHandle(filter);

      if (!dev.pin_count)
      {
         RARCH_DBG("[WDM-KS] A filter opened but offers no render pin"
               " this driver can write to.\n");
         free(detail);
         continue;
      }

      /* The name the user will see. The friendly name is what the
       * device manager shows; the description is the fallback, and a
       * filter with neither gets its path, which is ugly but is at
       * least selectable. */
      friendly[0] = 0;
      if (!g_setupapi.get_property(list, &info, RA_SPDRP_FRIENDLYNAME,
               NULL, (PBYTE)friendly, sizeof(friendly) - sizeof(WCHAR), NULL))
         g_setupapi.get_property(list, &info, RA_SPDRP_DEVICEDESC,
               NULL, (PBYTE)friendly, sizeof(friendly) - sizeof(WCHAR), NULL);

      if (friendly[0])
      {
         int len = WideCharToMultiByte(CP_UTF8, 0, friendly, -1,
               NULL, 0, NULL, NULL);
         if (len > 0 && (dev.name = (char*)calloc(1, (size_t)len)))
            WideCharToMultiByte(CP_UTF8, 0, friendly, -1, dev.name, len,
                  NULL, NULL);
      }

      {
         size_t chars = wcslen(detail->DevicePath) + 1;
         if ((dev.path = (WCHAR*)calloc(chars, sizeof(WCHAR))))
            memcpy(dev.path, detail->DevicePath, chars * sizeof(WCHAR));
      }
      free(detail);

      if (!dev.path)
      {
         free(dev.name);
         continue;
      }

      if (count == cap)
      {
         unsigned        next = cap ? cap * 2 : 8;
         wdmks_device_t *grow = (wdmks_device_t*)realloc(devices,
               next * sizeof(*devices));
         if (!grow)
         {
            free(dev.path);
            free(dev.name);
            break;
         }
         devices = grow;
         cap     = next;
      }
      devices[count++] = dev;
   }

   g_setupapi.destroy_list(list);
   RARCH_DBG("[WDM-KS] %u render filter(s) usable of %u interface(s) seen.\n",
         count, (unsigned)index);
   *count_out = count;
   return devices;
}

static void wdmks_devices_free(wdmks_device_t *devices, unsigned count)
{
   unsigned i;
   if (!devices)
      return;
   for (i = 0; i < count; i++)
   {
      free(devices[i].path);
      free(devices[i].name);
   }
   free(devices);
}

/* ---- the device list the menu shows ------------------------------- */

static void *wdmks_device_list_new(void *data)
{
   union string_list_elem_attr attr;
   struct string_list *list;
   wdmks_device_t     *devices;
   unsigned            count = 0;
   unsigned            i;

   (void)data;
   if (!(list = string_list_new()))
      return NULL;

   attr.i  = 0;
   devices = wdmks_devices_scan(&count);

   for (i = 0; i < count; i++)
   {
      const char *name = devices[i].name ? devices[i].name : "WDM-KS device";
      string_list_append(list, name, attr);
   }

   wdmks_devices_free(devices, count);
   return list;
}

static void wdmks_device_list_free(void *data, void *array_list_data)
{
   struct string_list *s = (struct string_list*)array_list_data;
   (void)data;
   if (s)
      string_list_free(s);
}

/* ---- ksuser, for the one call that is not an ioctl ---------------- */

/* Instantiating a pin is KsCreatePin, which lives in ksuser.dll.
 * Resolved rather than imported, for the reason the SetupAPI block
 * gives: a missing import stops the process, a missing resolve is a
 * driver that declines. */
typedef DWORD (WINAPI *ra_kscreatepin_t)(HANDLE filter,
      ra_kspin_connect_t *connect, DWORD access, HANDLE *pin);

typedef union
{
   FARPROC          proc;
   ra_kscreatepin_t create_pin;
} ra_wdmks_ksuser_proc_t;

static HMODULE          g_ksuser;
static ra_kscreatepin_t g_ks_create_pin;

static bool wdmks_ksuser_init(void)
{
   ra_wdmks_ksuser_proc_t p;

   if (g_ks_create_pin)
      return true;
   if (!g_ksuser && !(g_ksuser = LoadLibraryA("ksuser.dll")))
      return false;

   p.proc          = GetProcAddress(g_ksuser, "KsCreatePin");
   g_ks_create_pin = p.create_pin;
   if (!g_ks_create_pin)
   {
      FreeLibrary(g_ksuser);
      g_ksuser = NULL;
      return false;
   }
   return true;
}

/* ---- choosing a format ------------------------------------------- */

/* What this frontend will ask a pin for. The pin reported ranges
 * rather than a list, so a rate inside the range is not a promise -
 * a pin saying 8000 to 192000 rarely means every rate between. The
 * format is therefore proposed and the pin's answer believed, which
 * is what wdmks_pin_open() does; this only narrows the candidates. */
typedef struct
{
   unsigned rate;
   unsigned channels;
   unsigned bits;            /* bits that carry signal */
   unsigned container_bits;  /* bits they are carried in: 24 rides in 32 */
   bool     is_float;
} wdmks_format_t;

/* The rates to try, in the order they are tried: what was asked for
 * first, then the common ones nearest it. A device that will not take
 * the requested rate is normal - the frontend resamples to whatever
 * comes back. */
static const unsigned wdmks_rate_order[] =
{ 48000, 44100, 96000, 88200, 192000, 32000, 22050, 16000, 11025, 8000 };

static bool wdmks_format_in_pin(const wdmks_pin_t *pin,
      const wdmks_format_t *fmt)
{
   if (fmt->rate < pin->min_rate || fmt->rate > pin->max_rate)
      return false;
   if (fmt->channels > pin->max_channels || fmt->channels == 0)
      return false;
   if (fmt->bits < pin->min_bits || fmt->bits > pin->max_bits)
      return false;
   if (fmt->is_float && !pin->takes_float)
      return false;
   if (!fmt->is_float && !pin->takes_pcm)
      return false;
   return true;
}

/* Fills a WAVEFORMATEX for a format. Mono and stereo go out as the
 * plain structure; anything wider, and anything above 16 bits, goes
 * out extensible - which is not a preference but what the interface
 * requires, since the plain form has nowhere to say which speaker a
 * channel is for and nowhere to say that 24 bits sit inside 32. */
static void wdmks_format_to_wfx(const wdmks_format_t *fmt, WAVEFORMATEX *wfx)
{
   memset(wfx, 0, sizeof(*wfx));
   wfx->wFormatTag      = (WORD)(fmt->is_float ? 3 : 1);
   wfx->nChannels       = (WORD)fmt->channels;
   wfx->nSamplesPerSec  = (DWORD)fmt->rate;
   wfx->wBitsPerSample  = (WORD)fmt->container_bits;
   wfx->nBlockAlign     = (WORD)(fmt->channels * (fmt->container_bits / 8));
   wfx->nAvgBytesPerSec = (DWORD)(fmt->rate * wfx->nBlockAlign);
   wfx->cbSize          = 0;
}

/* The frontend's own layout mask for a channel count, which is what
 * the layout hook reports so the mixer knows where each channel goes.
 * Kept beside the interface's mask below because the two have to
 * agree about what "six channels" means. */
static uint32_t wdmks_layout_for(unsigned channels)
{
   switch (channels)
   {
      case 1:  return AUDIO_SPEAKER_FRONT_CENTER;
      case 2:  return AUDIO_LAYOUT_STEREO;
      case 4:  return AUDIO_LAYOUT_QUAD;
      case 6:  return AUDIO_LAYOUT_5POINT1;
      case 8:  return AUDIO_LAYOUT_7POINT1;
      default: break;
   }
   return 0;
}

/* The channel mask for a count, in the order WAVEFORMATEXTENSIBLE
 * defines and the frontend's own layouts use. A device is entitled to
 * refuse a mask it does not have speakers for, which is one more
 * reason the format is proposed rather than assumed. */
static DWORD wdmks_channel_mask(unsigned channels)
{
   switch (channels)
   {
      case 1:  return 0x00004;                 /* front centre */
      case 2:  return 0x00003;                 /* front L,R */
      case 4:  return 0x00033;                 /* quad */
      case 6:  return 0x0003F;                 /* 5.1 */
      case 8:  return 0x0063F;                 /* 7.1 */
      default: break;
   }
   return 0;
}

/* Fills the extensible form. Samples is a union in the system's
 * declaration; what goes in it here is the valid bit count, which is
 * how 24-in-32 is expressed - wBitsPerSample says the container, this
 * says how much of it carries signal. */
static void wdmks_format_to_wfx_ext(const wdmks_format_t *fmt,
      WAVEFORMATEXTENSIBLE *ext)
{
   memset(ext, 0, sizeof(*ext));
   wdmks_format_to_wfx(fmt, &ext->Format);
   ext->Format.wFormatTag     = (WORD)0xFFFE;   /* EXTENSIBLE */
   ext->Format.cbSize         = (WORD)(sizeof(*ext) - sizeof(WAVEFORMATEX));
   ext->Samples.wValidBitsPerSample = (WORD)fmt->bits;
   ext->dwChannelMask         = wdmks_channel_mask(fmt->channels);
   memcpy(&ext->SubFormat, fmt->is_float
         ? &ra_ks_dataformat_subtype_float
         : &ra_ks_dataformat_subtype_pcm, sizeof(GUID));
}

/* ---- opening a pin ----------------------------------------------- */

typedef struct
{
   HANDLE handle;
   ULONG  pin_id;
   wdmks_format_t fmt;
} wdmks_stream_t;

/* Asks the filter for one pin in one format. Returns the pin handle,
 * or INVALID_HANDLE_VALUE - which is the ordinary answer for a format
 * the device does not want, and is how the ranges are turned into
 * fact. */
static HANDLE wdmks_pin_try(HANDLE filter, ULONG pin_id,
      const wdmks_format_t *fmt)
{
   /* Big enough for either form. The extensible one is a
    * WAVEFORMATEX with twenty-two bytes behind it, and the data
    * format's FormatSize is what says which was sent. */
   unsigned char         buf[sizeof(ra_kspin_connect_t)
                           + sizeof(ra_ksdataformat_t)
                           + sizeof(WAVEFORMATEXTENSIBLE) + 8];
   ra_kspin_connect_t    *connect = (ra_kspin_connect_t*)buf;
   ra_ksdataformat_wfx_t *format  = (ra_ksdataformat_wfx_t*)(connect + 1);
   HANDLE                 pin     = INVALID_HANDLE_VALUE;
   bool                   ext     = (fmt->channels > 2)
                                 || (fmt->container_bits > 16)
                                 || fmt->is_float;
   ULONG                  fmt_size;

   memset(buf, 0, sizeof(buf));

   connect->Interface.Set   = ra_ks_interfacesetid_standard;
   connect->Interface.Id    = RA_KSINTERFACE_STANDARD_STREAMING;
   connect->Interface.Flags = 0;
   connect->Medium.Set      = ra_ks_mediumsetid_standard;
   connect->Medium.Id       = RA_KSMEDIUM_TYPE_ANYINSTANCE;
   connect->Medium.Flags    = 0;
   connect->PinId           = pin_id;
   connect->PinToHandle     = NULL;
   connect->Priority.PriorityClass    = RA_KSPRIORITY_NORMAL;
   connect->Priority.PrioritySubClass = 1;

   format->DataFormat.f.Flags       = 0;
   format->DataFormat.f.Reserved    = 0;
   format->DataFormat.f.MajorFormat = ra_ks_dataformat_type_audio;
   format->DataFormat.f.SubFormat   = fmt->is_float
      ? ra_ks_dataformat_subtype_float : ra_ks_dataformat_subtype_pcm;
   format->DataFormat.f.Specifier   = ra_ks_dataformat_specifier_wfx;
   if (ext)
   {
      WAVEFORMATEXTENSIBLE wfe;
      wdmks_format_to_wfx_ext(fmt, &wfe);
      memcpy(&format->WaveFormatEx, &wfe, sizeof(wfe));
      fmt_size = (ULONG)(sizeof(ra_ksdataformat_t) + sizeof(wfe));
   }
   else
   {
      wdmks_format_to_wfx(fmt, &format->WaveFormatEx);
      fmt_size = (ULONG)sizeof(ra_ksdataformat_wfx_t);
   }
   format->DataFormat.f.FormatSize  = fmt_size;
   /* SampleSize is one frame, which is what the pin streams in. */
   format->DataFormat.f.SampleSize  = format->WaveFormatEx.nBlockAlign;

   if (!wdmks_ksuser_init())
      return INVALID_HANDLE_VALUE;

   {
      DWORD res = g_ks_create_pin(filter, connect, GENERIC_WRITE, &pin);
      if (res != 0)
      {
         /* Only the first refusal per pin is said, and at a level
          * that is not in the way: a pin is asked for a dozen formats
          * and refusing most of them is the normal course. What
          * matters is that a pin refusing everything can be told
          * apart afterwards - a device already held by something else
          * fails with a busy status for every format, where a device
          * that simply does not do 96 kHz float fails only for that
          * one, and the two used to produce the same single line. */
         RARCH_DBG("[WDM-KS] Pin %u refused %u Hz, %u ch, %s: 0x%08lx.\n",
               (unsigned)pin_id, fmt->rate, fmt->channels,
               fmt->is_float ? "float" : "integer", (unsigned long)res);
         return INVALID_HANDLE_VALUE;
      }
   }
   return pin;
}

/* Opens the best format a pin will actually take, rather than the
 * best its ranges claim. Preference order: the rate asked for before
 * any other, float before integer where the pin says it takes both
 * (the frontend's own pipeline is float, so that is one conversion
 * fewer), and the widest integer before the narrowest. */
static bool wdmks_pin_open(HANDLE filter, const wdmks_pin_t *pin,
      unsigned wanted_rate, unsigned channels, wdmks_stream_t *out)
{
   unsigned r;

   for (r = 0; r <= sizeof(wdmks_rate_order) / sizeof(*wdmks_rate_order); r++)
   {
      unsigned rate = (r == 0) ? wanted_rate : wdmks_rate_order[r - 1];
      unsigned b;
      /* Only what the frontend can hand over: 32-bit float, or
       * 16-bit integer. It sends one or the other according to what
       * use_float() says, and nothing here converts - so a pin opened
       * at 24 or 32-bit integer is a pin being fed samples half the
       * width it is reading, which plays at the wrong pitch rather
       * than failing. Those two widths belong here only once
       * something converts into them. */
      static const unsigned bits_order[] = { 32, 16 };

      if (!rate)
         continue;
      /* The requested rate is tried first and then skipped when it
       * comes round again in the list. */
      if (r > 0 && rate == wanted_rate)
         continue;

      for (b = 0; b < sizeof(bits_order) / sizeof(*bits_order); b++)
      {
         wdmks_format_t fmt;
         HANDLE         h;

         fmt.rate     = rate;
         fmt.channels = channels;
         fmt.bits     = bits_order[b];
         fmt.is_float = (b == 0);
         fmt.container_bits = fmt.bits;

         if (!wdmks_format_in_pin(pin, &fmt))
            continue;

         h = wdmks_pin_try(filter, pin->pin_id, &fmt);
         if (h == INVALID_HANDLE_VALUE)
            continue;

         out->handle = h;
         out->pin_id = pin->pin_id;
         out->fmt    = fmt;
         RARCH_LOG("[WDM-KS] Pin %u opened at %u Hz, %u channel(s), %s,"
               " %u-byte frame.\n",
               (unsigned)pin->pin_id, fmt.rate, fmt.channels,
               fmt.is_float ? "32-bit float" : "16-bit integer",
               fmt.channels * (fmt.container_bits / 8));
         return true;
      }
   }
   return false;
}

/* The pin's state. A pin is driven STOP to ACQUIRE to PAUSE to RUN on
 * the way up and back down the same way: the transitions are ordered
 * and a driver is entitled to refuse a jump. */
static bool wdmks_pin_set_state(wdmks_stream_t *s, ULONG state)
{
   ra_ksproperty_t prop;
   ULONG           value = state;
   DWORD           written = 0;

   if (!s || s->handle == INVALID_HANDLE_VALUE)
      return false;

   memset(&prop, 0, sizeof(prop));
   prop.Set   = ra_ks_propsetid_connection;
   prop.Id    = RA_KSPROPERTY_CONNECTION_STATE;
   prop.Flags = RA_KSPROPERTY_TYPE_SET;

   return DeviceIoControl(s->handle, RA_IOCTL_KS_PROPERTY,
         &prop, sizeof(prop), &value, sizeof(value), &written, NULL)
      ? true : false;
}

static void wdmks_pin_close(wdmks_stream_t *s)
{
   if (!s || s->handle == INVALID_HANDLE_VALUE)
      return;
   wdmks_pin_set_state(s, RA_KSSTATE_PAUSE);
   wdmks_pin_set_state(s, RA_KSSTATE_ACQUIRE);
   wdmks_pin_set_state(s, RA_KSSTATE_STOP);
   CloseHandle(s->handle);
   s->handle = INVALID_HANDLE_VALUE;
}

/* ---- streaming ---------------------------------------------------- */

/* WaveCyclic: the packet path every kernel-streaming device supports.
 * A fixed set of buffers is kept, each with its own event and
 * OVERLAPPED; a write copies into a free one and hands it to the pin
 * with IOCTL_KS_WRITE_STREAM, which completes when the device has
 * taken it. Depth is what buys tolerance to a late frontend, and the
 * device is never left with fewer than one outstanding.
 *
 * WaveRT - mapping the device's own buffer and writing into it
 * directly - is the other family and is a later stage. It is where
 * the lowest latency lives, but it is not universal, and a driver
 * that only does WaveRT works on fewer machines than one that only
 * does this. */

#define WDMKS_PACKETS 4

typedef struct
{
   ra_ksstream_header_t header;
   OVERLAPPED           overlapped;
   unsigned char       *data;
   bool                 pending;
} wdmks_packet_t;

typedef struct
{
   wdmks_stream_t  stream;
   HANDLE          filter;
   wdmks_packet_t  packets[WDMKS_PACKETS];
   size_t          packet_bytes;
   unsigned        next;         /* the packet a write fills */
   unsigned        frame_bytes;
   unsigned        rate;
   bool            running;
   bool            dead;      /* an I/O failed; stop writing, still reclaim */
   bool            nonblock;
   bool            is_float;
   uint32_t        layout;

   /* The device clock - see wdmks_clock_sample(). Touched only from
    * the thread that calls frames_consumed(). */
   uint64_t        clk_freq;
   uint64_t        clk_anchor_frames;
   uint64_t        clk_anchor_ticks;
   bool            clk_have_anchor;
   double          clk_sx, clk_sy, clk_sxx, clk_sxy, clk_n;
   int             clk_ppm;
   bool            clk_valid;
} wdmks_t;

/* Has this packet come back? A packet the device still holds is not
 * free to refill. GetOverlappedResult without waiting is the question
 * being asked; the event is what a blocking write waits on. */
static bool wdmks_packet_done(wdmks_t *w, wdmks_packet_t *p)
{
   DWORD moved = 0;

   if (!p->pending)
      return true;
   if (GetOverlappedResult(w->stream.handle, &p->overlapped, &moved, FALSE))
   {
      p->pending = false;
      return true;
   }
   if (GetLastError() != ERROR_IO_INCOMPLETE)
   {
      /* An error is not a completion. The packet stays pending,
       * because the kernel may still own its buffer and its
       * OVERLAPPED - and if it does, clearing the flag here is what
       * lets the write path refill the buffer and reuse the
       * OVERLAPPED underneath it, and lets teardown free both while
       * the device is still reading out of them. The corruption that
       * follows lands wherever the heap put something else, which is
       * why it showed up in an unrelated thread.
       *
       * The stream is marked dead instead: writes stop, and teardown
       * cancels and waits for every packet that was ever submitted,
       * this one included. */
      w->dead = true;
   }
   return false;
}

static bool wdmks_packet_submit(wdmks_t *w, wdmks_packet_t *p, size_t bytes)
{
   DWORD written = 0;

   memset(&p->header, 0, sizeof(p->header));
   p->header.Size        = sizeof(p->header);
   p->header.Data        = p->data;
   p->header.FrameExtent = (ULONG)w->packet_bytes;
   p->header.DataUsed    = (ULONG)bytes;

   ResetEvent(p->overlapped.hEvent);

   if (DeviceIoControl(w->stream.handle, RA_IOCTL_KS_WRITE_STREAM,
            NULL, 0, &p->header, sizeof(p->header), &written,
            &p->overlapped))
   {
      /* Completed inline, which a device is allowed to do. */
      p->pending = false;
      return true;
   }
   if (GetLastError() == ERROR_IO_PENDING)
   {
      p->pending = true;
      return true;
   }
   return false;
}

static size_t wdmks_free_bytes(wdmks_t *w)
{
   size_t   total = 0;
   unsigned i;

   for (i = 0; i < WDMKS_PACKETS; i++)
      if (wdmks_packet_done(w, &w->packets[i]))
         total += w->packet_bytes;
   return total;
}

static ssize_t wdmks_write(void *data, const void *buf, size_t size)
{
   wdmks_t       *w    = (wdmks_t*)data;
   const unsigned char *src = (const unsigned char*)buf;
   size_t         done = 0;

   if (!w || w->stream.handle == INVALID_HANDLE_VALUE)
      return -1;
   if (w->dead)
      return -1;

   while (done < size)
   {
      wdmks_packet_t *p     = &w->packets[w->next];
      size_t          chunk;

      if (!wdmks_packet_done(w, p))
      {
         if (w->nonblock)
            break;
         /* The packet the device still holds is the one to wait on:
          * waiting on any other would return at a moment this write
          * cannot use. Bounded, so a device that stops taking
          * packets returns what it took rather than holding the
          * audio thread for good. */
         if (WaitForSingleObject(p->overlapped.hEvent, 1000) != WAIT_OBJECT_0)
            break;
         p->pending = false;
      }

      chunk = size - done;
      if (chunk > w->packet_bytes)
         chunk = w->packet_bytes;
      /* Whole frames only: a packet ending mid-frame rotates every
       * channel after it, for good. */
      chunk -= chunk % w->frame_bytes;
      if (!chunk)
         break;

      memcpy(p->data, src + done, chunk);
      if (!wdmks_packet_submit(w, p, chunk))
         return done ? (ssize_t)done : -1;

      done   += chunk;
      w->next = (w->next + 1) % WDMKS_PACKETS;
   }

   return (ssize_t)done;
}

/* ---- the device clock -------------------------------------------- */

/* KSPROPERTY_AUDIO_POSITION gives PlayOffset: what the device has
 * played, in bytes, counted by the hardware. That is the half worth
 * having - it is the device's own progress and not a tally of what
 * this driver submitted, so pairing it with a clock says what the
 * hardware is really doing rather than restating our own accounting.
 *
 * The other half is QueryPerformanceCounter, read next to it. That is
 * this process's clock rather than a timestamp the device handed
 * over, which is weaker than WASAPI, where the position and its QPC
 * instant come back from one call, and weaker than ALSA's kernel
 * htstamp. What sits between the two reads here is a single ioctl -
 * tens of microseconds - against a fit that tolerated two
 * milliseconds of jitter in the harness, so it is noise and not bias.
 *
 * The fit is least squares over every sample rather than two points:
 * noise on a single anchor divides by the window and reads as drift,
 * fifty parts per million for half a millisecond at a ten-second
 * window. Sums are kept in seconds and frames relative to the anchor,
 * because a fit on raw counter values loses its answer to
 * cancellation. Nothing acts on this; it is logged and offered to the
 * overlay so it can be compared against the sink estimate that does
 * drive rate control. */
static bool wdmks_position(wdmks_t *w, uint64_t *frames)
{
   ra_ksproperty_t       prop;
   ra_ksaudio_position_t pos;
   DWORD                 written = 0;

   memset(&prop, 0, sizeof(prop));
   memset(&pos,  0, sizeof(pos));
   prop.Set   = ra_ks_propsetid_audio;
   prop.Id    = RA_KSPROPERTY_AUDIO_POSITION;
   prop.Flags = RA_KSPROPERTY_TYPE_GET;

   if (!DeviceIoControl(w->stream.handle, RA_IOCTL_KS_PROPERTY,
            &prop, sizeof(prop), &pos, sizeof(pos), &written, NULL))
      return false;
   if (written < sizeof(pos) || !w->frame_bytes)
      return false;

   *frames = (uint64_t)(pos.PlayOffset / w->frame_bytes);
   return true;
}

static void wdmks_clock_sample(wdmks_t *w)
{
   LARGE_INTEGER now;
   uint64_t      frames = 0;
   double        x, y, d;

   if (!w->clk_freq || !w->rate)
      return;
   if (!wdmks_position(w, &frames))
      return;
   if (!QueryPerformanceCounter(&now))
      return;

   if (!w->clk_have_anchor)
   {
      w->clk_anchor_frames = frames;
      w->clk_anchor_ticks  = (uint64_t)now.QuadPart;
      w->clk_have_anchor   = true;
      w->clk_sx = w->clk_sy = w->clk_sxx = w->clk_sxy = w->clk_n = 0.0;
      return;
   }
   if (     (uint64_t)now.QuadPart <= w->clk_anchor_ticks
         || frames < w->clk_anchor_frames)
   {
      /* The pin was restarted, or the counter did not move. */
      w->clk_anchor_frames = frames;
      w->clk_anchor_ticks  = (uint64_t)now.QuadPart;
      w->clk_sx = w->clk_sy = w->clk_sxx = w->clk_sxy = w->clk_n = 0.0;
      return;
   }

   x = (double)((uint64_t)now.QuadPart - w->clk_anchor_ticks)
      / (double)w->clk_freq;
   y = (double)(frames - w->clk_anchor_frames);

   w->clk_sx  += x;
   w->clk_sy  += y;
   w->clk_sxx += x * x;
   w->clk_sxy += x * y;
   w->clk_n   += 1.0;

   d = w->clk_n * w->clk_sxx - w->clk_sx * w->clk_sx;
   if (x >= 1.0 && d > 0.0)
   {
      double slope = (w->clk_n * w->clk_sxy - w->clk_sx * w->clk_sy) / d;
      double ppm   = (slope / (double)w->rate - 1.0) * 1000000.0;
      if (ppm > -100000.0 && ppm < 100000.0)
      {
         w->clk_ppm   = (int)ppm;
         w->clk_valid = true;
      }
   }
}

/* What the device has played. The hardware's own count, sampled with
 * the clock so the two are taken together. */
static size_t wdmks_frames_consumed(void *data)
{
   wdmks_t *w = (wdmks_t*)data;
   uint64_t frames = 0;

   if (!w || w->stream.handle == INVALID_HANDLE_VALUE)
      return 0;
   wdmks_clock_sample(w);
   if (!wdmks_position(w, &frames))
      return 0;
   return (size_t)frames;
}

static bool wdmks_device_clock_ppm(void *data, double *ppm)
{
   wdmks_t *w = (wdmks_t*)data;
   if (!w || !w->clk_valid)
      return false;
   *ppm = (double)w->clk_ppm;
   return true;
}

static uint32_t wdmks_layout(void *data)
{
   wdmks_t *w = (wdmks_t*)data;
   if (!w)
      return 0;
   return w->layout;
}

static size_t wdmks_write_avail(void *data)
{
   wdmks_t *w = (wdmks_t*)data;
   return w ? wdmks_free_bytes(w) : 0;
}

/* Blocks until at least len bytes will fit, and returns what fits.
 *
 * The frontend calls this before a write so it can skip a pass rather
 * than have the write block inside the audio path. What is waited on
 * is the packet the next write will fill, because that is the one
 * whose return actually frees room - waiting on whichever completes
 * first returns at a moment the write cannot use.
 *
 * Every wait is bounded and the loop is too: a device that has
 * stopped taking packets returns nothing, which the frontend reads as
 * a pass to skip, rather than holding the audio thread for good. The
 * request is capped at half the queue so a caller asking for more
 * than can ever fit is not waited on for ever. */
#define WDMKS_WAIT_LAPS 4

static size_t wdmks_wait_writable(void *data, size_t len)
{
   wdmks_t *w    = (wdmks_t*)data;
   unsigned laps = WDMKS_WAIT_LAPS;
   size_t   cap;

   if (!w || w->stream.handle == INVALID_HANDLE_VALUE || w->dead)
      return 0;

   cap = (w->packet_bytes * WDMKS_PACKETS) / 2;
   if (len > cap)
      len = cap;

   for (;;)
   {
      size_t          avail = wdmks_free_bytes(w);
      wdmks_packet_t *p;
      DWORD           timeout;

      if (avail >= len)
         return avail;
      if (!laps--)
         return 0;

      p = &w->packets[w->next];
      if (wdmks_packet_done(w, p))
         continue;

      /* Two packets' worth of time: enough for the one being waited
       * on to come back on any device that is still running, short
       * enough that four laps of it is not a hang. */
      timeout = (DWORD)(2 * w->packet_bytes * 1000
            / (w->frame_bytes * w->rate));
      if (timeout < 2)
         timeout = 2;
      if (WaitForSingleObject(p->overlapped.hEvent, timeout) != WAIT_OBJECT_0)
         continue;
      p->pending = false;
   }
}

static size_t wdmks_buffer_size(void *data)
{
   wdmks_t *w = (wdmks_t*)data;
   return w ? w->packet_bytes * WDMKS_PACKETS : 0;
}

static bool wdmks_start(void *data, bool is_shutdown)
{
   wdmks_t *w = (wdmks_t*)data;
   (void)is_shutdown;
   if (!w)
      return false;
   if (w->running)
      return true;
   if (     !wdmks_pin_set_state(&w->stream, RA_KSSTATE_ACQUIRE)
         || !wdmks_pin_set_state(&w->stream, RA_KSSTATE_PAUSE)
         || !wdmks_pin_set_state(&w->stream, RA_KSSTATE_RUN))
      return false;
   w->running = true;
   return true;
}

static bool wdmks_stop(void *data)
{
   wdmks_t *w = (wdmks_t*)data;
   if (!w)
      return false;
   if (!w->running)
      return true;
   if (!wdmks_pin_set_state(&w->stream, RA_KSSTATE_PAUSE))
      return false;
   w->running = false;
   return true;
}

static bool wdmks_alive(void *data)
{
   wdmks_t *w = (wdmks_t*)data;
   return w && w->running;
}

static void wdmks_set_nonblock_state(void *data, bool state)
{
   wdmks_t *w = (wdmks_t*)data;
   if (w)
      w->nonblock = state;
}

static bool wdmks_use_float(void *data)
{
   wdmks_t *w = (wdmks_t*)data;
   return w && w->is_float;
}

static void wdmks_free(void *data)
{
   wdmks_t *w = (wdmks_t*)data;
   unsigned i;

   if (!w)
      return;

   /* What the device clock was doing against the rate the pin runs
    * at. Logged, never acted on, as in the other drivers. */
   if (w->clk_valid)
      RARCH_LOG("[WDM-KS] Device clock, fitted from the pin's position:"
            " %+d ppm against %u Hz.\n", w->clk_ppm, w->rate);

   if (w->stream.handle != INVALID_HANDLE_VALUE)
   {
      /* Every outstanding packet has to come back before the buffers
       * it points at are freed: the device is writing out of them. */
      /* Every packet the kernel was given has to come back before the
       * buffer it points at and the OVERLAPPED it uses are freed -
       * the device is writing out of the one and into the other.
       * CancelIo asks; the wait is what makes it true, and it is done
       * for a failed packet exactly as for a live one, because a
       * failure is not evidence the kernel let go. */
      CancelIo(w->stream.handle);
      for (i = 0; i < WDMKS_PACKETS; i++)
         if (w->packets[i].pending)
         {
            DWORD moved = 0;
            GetOverlappedResult(w->stream.handle,
                  &w->packets[i].overlapped, &moved, TRUE);
            w->packets[i].pending = false;
         }
      wdmks_pin_close(&w->stream);
   }

   for (i = 0; i < WDMKS_PACKETS; i++)
   {
      if (w->packets[i].overlapped.hEvent)
         CloseHandle(w->packets[i].overlapped.hEvent);
      free(w->packets[i].data);
   }

   if (w->filter && w->filter != INVALID_HANDLE_VALUE)
      CloseHandle(w->filter);
   free(w);
}

static void *wdmks_init(const char *device, unsigned rate,
      unsigned latency, unsigned *new_rate)
{
   wdmks_t        *w;
   wdmks_device_t *devices;
   unsigned        count = 0;
   unsigned        i;
   unsigned        chosen = 0;
   bool            opened = false;

   if (!(devices = wdmks_devices_scan(&count)) || !count)
   {
      RARCH_LOG("[WDM-KS] No kernel-streaming render device.\n");
      wdmks_devices_free(devices, count);
      return NULL;
   }

   /* By name where one was asked for; the first that opens otherwise.
    * A named device that will not open is an error rather than a
    * reason to pick another - the user asked for that one. */
   if (device && *device)
   {
      bool found = false;
      for (i = 0; i < count; i++)
         if (devices[i].name && string_is_equal(devices[i].name, device))
         {
            chosen = i;
            found  = true;
            break;
         }
      if (!found)
      {
         RARCH_ERR("[WDM-KS] No device named \"%s\".\n", device);
         wdmks_devices_free(devices, count);
         return NULL;
      }
   }

   if (!(w = (wdmks_t*)calloc(1, sizeof(*w))))
   {
      wdmks_devices_free(devices, count);
      return NULL;
   }
   w->stream.handle = INVALID_HANDLE_VALUE;
   w->filter        = INVALID_HANDLE_VALUE;

   for (i = chosen; i < count && !opened; i++)
   {
      unsigned p;

      /* Which device this pass is on, before anything can go wrong
       * with it. Two runs on the same machine visited six devices and
       * two, against an enumeration that reported six usable both
       * times, and none of the failure paths said a word - so the
       * question is no longer why a device failed but which ones the
       * loop reached at all. */
      RARCH_DBG("[WDM-KS] Trying device %u of %u: \"%s\"%s.\n",
            i + 1, count,
            devices[i].name ? devices[i].name : "unnamed device",
            devices[i].wavert ? " (WaveRT)" : "");

      w->filter = CreateFileW(devices[i].path,
            GENERIC_READ | GENERIC_WRITE,
            FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
            FILE_ATTRIBUTE_NORMAL | FILE_FLAG_OVERLAPPED, NULL);
      if (w->filter == INVALID_HANDLE_VALUE)
      {
         /* Enumeration opened this filter a moment ago - it is in the
          * list because it did - so a failure here is something
          * taking it in between, and the likeliest something is this
          * driver's own previous instance not yet torn down across a
          * reinit. Said, because silence here is what made a run that
          * skipped one WaveRT device and then failed look like a run
          * that had no other device to try. */
         RARCH_WARN("[WDM-KS] \"%s\" would not open: 0x%08lx.\n",
               devices[i].name ? devices[i].name : "unnamed device",
               (unsigned long)GetLastError());
         if (device && *device)
            break;
         continue;
      }

      /* The layout the frontend asked for, then narrower ones: a pin
       * that will not take 7.1 usually takes 5.1, and one that takes
       * neither takes stereo. Falling back here rather than refusing
       * is what keeps a multichannel setting from silencing a stereo
       * card. */
      {
         static const unsigned widths[] = { 8, 6, 4, 2 };
         uint32_t wanted = audio_driver_requested_layout();
         unsigned want_ch =
              (wanted == AUDIO_LAYOUT_7POINT1) ? 8
            : (wanted == AUDIO_LAYOUT_5POINT1) ? 6
            : (wanted == AUDIO_LAYOUT_QUAD)    ? 4 : 2;
         unsigned c;

         /* A WaveRT filter is passed over rather than asked twelve
          * times: its pins do not offer the streaming interface this
          * driver opens them with - which is what ERROR_NOT_FOUND
          * from KsCreatePin means - and no rate or width changes
          * that. continue, not break: the next filter may well be one
          * that works, and on this machine it is. */
         if (devices[i].wavert)
         {
            const char *what = devices[i].name
               ? devices[i].name : "unnamed device";
            CloseHandle(w->filter);
            w->filter = INVALID_HANDLE_VALUE;

            /* A device the user named is not something to walk past.
             * Wandering on to the next one and failing there reports
             * the wrong device and the wrong reason - which is what
             * the last report showed: the setting named a WaveRT
             * output, this skipped it, the neighbour refused, and the
             * error blamed a format. */
            if (device && *device)
            {
               RARCH_ERR("[WDM-KS] \"%s\" streams WaveRT, which this"
                     " driver does not do yet. Choose another device,"
                     " or another audio driver for this one.\n", what);
               break;
            }
            RARCH_WARN("[WDM-KS] \"%s\" streams WaveRT, which this driver"
                  " does not do yet - skipping it.\n", what);
            continue;
         }

      for (c = 0; c < sizeof(widths) / sizeof(*widths) && !opened; c++)
         {
            if (widths[c] > want_ch)
               continue;
            for (p = 0; p < devices[i].pin_count && !opened; p++)
               opened = wdmks_pin_open(w->filter, &devices[i].pins[p],
                     rate, widths[c], &w->stream);
         }
      }

      if (!opened)
      {
         CloseHandle(w->filter);
         w->filter = INVALID_HANDLE_VALUE;
         if (device && *device)
            break;
      }
      else
         RARCH_LOG("[WDM-KS] Using \"%s\".\n",
               devices[i].name ? devices[i].name : "unnamed device");
   }

   wdmks_devices_free(devices, count);

   if (!opened)
   {
      RARCH_ERR("[WDM-KS] No render pin would take a format after"
            " trying every device. Set log"
            " verbosity to debug to see what each pin refused and"
            " why - a device held by another program refuses every"
            " format alike.\n");
      free(w);
      return NULL;
   }

   {
      LARGE_INTEGER f;
      w->clk_freq = QueryPerformanceFrequency(&f)
         ? (uint64_t)f.QuadPart : 0;
   }
   w->rate        = w->stream.fmt.rate;
   w->is_float    = w->stream.fmt.is_float;
   w->frame_bytes = w->stream.fmt.channels
      * (w->stream.fmt.container_bits / 8);
   w->layout      = wdmks_layout_for(w->stream.fmt.channels);
   if (new_rate)
      *new_rate   = w->rate;

   /* The latency setting is the whole queue, divided between the
    * packets, and a packet is never below one millisecond - a device
    * asked for packets smaller than it can turn round spends its time
    * in completions rather than in audio. */
   {
      size_t total = (size_t)latency * w->rate / 1000 * w->frame_bytes;
      size_t each  = total / WDMKS_PACKETS;
      size_t floor_bytes = (size_t)(w->rate / 1000) * w->frame_bytes;

      if (each < floor_bytes)
         each = floor_bytes;
      each -= each % w->frame_bytes;
      if (!each)
         each = w->frame_bytes;
      w->packet_bytes = each;
   }

   for (i = 0; i < WDMKS_PACKETS; i++)
   {
      w->packets[i].data = (unsigned char*)calloc(1, w->packet_bytes);
      w->packets[i].overlapped.hEvent = CreateEvent(NULL, TRUE, FALSE, NULL);
      if (!w->packets[i].data || !w->packets[i].overlapped.hEvent)
      {
         wdmks_free(w);
         return NULL;
      }
   }

   RARCH_LOG("[WDM-KS] %u packet(s) of %u bytes, %u ms in all.\n",
         (unsigned)WDMKS_PACKETS, (unsigned)w->packet_bytes,
         (unsigned)(w->packet_bytes * WDMKS_PACKETS * 1000
            / (w->frame_bytes * w->rate)));

   if (!wdmks_start(w, false))
   {
      RARCH_ERR("[WDM-KS] The pin would not run.\n");
      wdmks_free(w);
      return NULL;
   }
   return w;
}

audio_driver_t audio_wdmks = {
   wdmks_init,
   wdmks_write,
   wdmks_stop,
   wdmks_start,
   wdmks_alive,
   wdmks_set_nonblock_state,
   wdmks_free,
   wdmks_use_float,
   "wdmks",
   wdmks_device_list_new,
   wdmks_device_list_free,
   wdmks_write_avail,
   wdmks_buffer_size,
   NULL, /* write_raw */
   wdmks_wait_writable,
   wdmks_frames_consumed,
   NULL, /* underruns */
   wdmks_layout,
   NULL, /* frames_consumed_fallback */
   wdmks_device_clock_ppm
};
