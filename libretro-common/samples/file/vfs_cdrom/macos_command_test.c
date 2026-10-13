/* The production macOS mapper, with only the Apple SDK boundary stood in.
 * This tests command/error semantics, not SDK ABI or physical hardware. */
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <stddef.h>

typedef uint32_t UInt32;
typedef unsigned char UInt8;
typedef unsigned short SCSICmdField2Byte;
typedef int IOReturn;
typedef int SCSITaskStatus;
typedef struct { unsigned char bytes[18]; } SCSI_Sense_Data;
typedef struct { unsigned char bytes[36]; } SCSICmd_INQUIRY_StandardData;
enum { kIOReturnSuccess = 0, kSCSITaskStatus_GOOD = 0,
   kSCSITaskStatus_CHECK_CONDITION = 2, DKIOCCDSETSPEED = 1, DKIOCCDREAD = 2 };
enum { kCDSectorAreaSync = 1, kCDSectorAreaHeader = 2,
   kCDSectorAreaSubHeader = 4, kCDSectorAreaUser = 8,
   kCDSectorAreaAuxiliary = 16, kCDSectorAreaErrorFlags = 32,
   kCDSectorAreaSubChannel = 64, kCDSectorAreaSubChannelQ = 128 };
enum { kCDSectorTypeUnknown, kCDSectorTypeCDDA, kCDSectorTypeMode1,
   kCDSectorTypeMode2, kCDSectorTypeMode2Form1, kCDSectorTypeMode2Form2 };
typedef struct { uint64_t offset; UInt8 sectorType, sectorArea;
   uint32_t bufferLength; void *buffer; } dk_cd_read_t;
typedef enum { DIRECTION_NONE, DIRECTION_IN, DIRECTION_OUT } CDROM_CMD_Direction;
typedef struct { void *iokit_mmc; int fd; } libretro_vfs_implementation_file;
typedef struct MMCDeviceInterface MMCDeviceInterface;
struct MMCDeviceInterface
{
   IOReturn (*TestUnitReady)(MMCDeviceInterface**, SCSITaskStatus*, SCSI_Sense_Data*);
   IOReturn (*Inquiry)(MMCDeviceInterface**, SCSICmd_INQUIRY_StandardData*, UInt32, SCSITaskStatus*, SCSI_Sense_Data*);
   IOReturn (*ReadTableOfContents)(MMCDeviceInterface**, int, int, int, void*, SCSICmdField2Byte, SCSITaskStatus*, SCSI_Sense_Data*);
   IOReturn (*GetConfiguration)(MMCDeviceInterface**, int, SCSICmdField2Byte, void*, SCSICmdField2Byte, SCSITaskStatus*, SCSI_Sense_Data*);
   IOReturn (*ReadTrackInformation)(MMCDeviceInterface**, int, UInt32, void*, SCSICmdField2Byte, SCSITaskStatus*, SCSI_Sense_Data*);
   IOReturn (*ModeSense10)(MMCDeviceInterface**, int, int, int, int, void*, SCSICmdField2Byte, SCSITaskStatus*, SCSI_Sense_Data*);
};
static unsigned failures, mmc_calls, ioctl_calls;
static int mmc_error;
#define CHECK(x) do { if (!(x)) { fprintf(stderr, "line %d: %s\n", __LINE__, #x); failures++; } } while (0)
static unsigned cdrom_msf_to_lba(unsigned char m, unsigned char s, unsigned char f)
{ return ((unsigned)m * 60 + s) * 75 + f; }
static int ioctl(int fd, unsigned request, void *arg)
{ (void)fd; (void)request; (void)arg; ioctl_calls++; return -1; }
static IOReturn mode_sense(MMCDeviceInterface **mmc, int l, int d,
      int pc, int page, void *buf, SCSICmdField2Byte len,
      SCSITaskStatus *ts, SCSI_Sense_Data *ss)
{
   (void)mmc; (void)l; (void)d; (void)ts; (void)ss;
   CHECK(pc == 0 && page == 8 && len == 20);
   mmc_calls++;
   ((unsigned char*)buf)[10] = 1;
   return mmc_error;
}
#include "macos_command_body.inc"

int main(void)
{
   MMCDeviceInterface api = {0};
   MMCDeviceInterface *ptr = &api;
   libretro_vfs_implementation_file stream;
   unsigned char cmd[12] = {0x55, 0x10, 0, 0, 0, 0, 0, 0, 20, 0};
   unsigned char payload[20], sense[16];
   unsigned i;
   api.ModeSense10 = mode_sense;
   stream.iokit_mmc = &ptr; stream.fd = 1;
   memset(payload, 0xa5, sizeof(payload));
   memset(sense, 0, sizeof(sense));
   CHECK(cdrom_send_command_macos(&stream, DIRECTION_OUT, payload, 20,
            cmd, 10, sense, sizeof(sense)) == 1);
   CHECK(sense[0] == 0x70 && sense[2] == 5 && sense[7] == 6 && sense[12] == 0x20);
   CHECK(mmc_calls == 0 && ioctl_calls == 0);
   for (i = 0; i < sizeof(payload); i++) CHECK(payload[i] == 0xa5);
   memset(sense, 0xa5, sizeof(sense));
   CHECK(cdrom_send_command_macos(&stream, DIRECTION_OUT, payload, 20,
            cmd, 10, sense, 13) == 1);
   for (i = 0; i < sizeof(sense); i++) CHECK(sense[i] == 0xa5);
   CHECK(cdrom_send_command_macos(&stream, DIRECTION_OUT, NULL, 0,
            cmd, 10, NULL, 0) == 1);
   cmd[0] = 0x5a; cmd[1] = 0; cmd[2] = 8;
   CHECK(cdrom_send_command_macos(&stream, DIRECTION_IN, payload, 20,
            cmd, 10, sense, 16) == 0 && mmc_calls == 1 && payload[10] == 1);
   mmc_error = 1;
   CHECK(cdrom_send_command_macos(&stream, DIRECTION_IN, payload, 20,
            cmd, 10, sense, 16) == 1 && mmc_calls == 2);
   memset(cmd, 0, sizeof(cmd));
   cmd[0] = 0xb9; cmd[4] = 2; cmd[9] = 0x10;
   memset(sense, 0, sizeof(sense));
   CHECK(cdrom_send_command_macos(&stream, DIRECTION_IN, payload, 20,
            cmd, 12, sense, 16) == 1 && ioctl_calls == 1);
   CHECK(sense[0] == 0x70 && sense[2] == 3 && sense[7] == 6 && sense[12] == 0x11);
   stream.fd = -1;
   CHECK(cdrom_send_command_macos(&stream, DIRECTION_IN, payload, 20,
            cmd, 12, sense, 16) == 1 && ioctl_calls == 1);
   stream.iokit_mmc = NULL;
   CHECK(cdrom_send_command_macos(&stream, DIRECTION_IN, payload, 20,
            cmd, 10, sense, 16) == 1 && mmc_calls == 2);
   printf("macOS command stand-in tests: %u failures\n", failures);
   return failures ? 1 : 0;
}
