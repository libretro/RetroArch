/* The part of CoreMIDI and CoreFoundation midi/drivers/coremidi.c calls,
 * for coremidi_driver_test: endpoints the test makes and removes, and a
 * client whose notifications the test sends. Implemented by the test. */
#ifndef COREMIDI_DRIVER_TEST_STUB_H
#define COREMIDI_DRIVER_TEST_STUB_H

#include <stdint.h>

typedef uint8_t  Byte;
typedef uint16_t UInt16;
typedef uint32_t UInt32;
typedef int32_t  SInt32;
typedef uint64_t UInt64;
typedef SInt32   OSStatus;
typedef unsigned long ItemCount;
typedef UInt64   MIDITimeStamp;
typedef UInt32   MIDIObjectRef;
typedef UInt32   MIDIClientRef;
typedef UInt32   MIDIPortRef;
typedef UInt32   MIDIEndpointRef;
typedef const char *CFStringRef;

#define noErr                  0
#define CFSTR(s)               (s)
#define kCFStringEncodingUTF8  0
#define kMIDIPropertyDisplayName ((CFStringRef)"name")

typedef SInt32 MIDINotificationMessageID;
enum
{
   kMIDIMsgSetupChanged  = 1,
   kMIDIMsgObjectAdded   = 2,
   kMIDIMsgObjectRemoved = 3
};

typedef struct MIDINotification
{
   MIDINotificationMessageID messageID;
   UInt32 messageSize;
} MIDINotification;

typedef struct MIDIObjectAddRemoveNotification
{
   MIDINotificationMessageID messageID;
   UInt32 messageSize;
   MIDIObjectRef parent;
   SInt32 parentType;
   MIDIObjectRef child;
   SInt32 childType;
} MIDIObjectAddRemoveNotification;

typedef struct MIDIPacket
{
   MIDITimeStamp timeStamp;
   UInt16 length;
   Byte data[256];
} MIDIPacket;

typedef struct MIDIPacketList
{
   UInt32 numPackets;
   MIDIPacket packet[1];
} MIDIPacketList;

typedef void (*MIDINotifyProc)(const MIDINotification *message, void *refCon);
typedef void (*MIDIReadProc)(const MIDIPacketList *pktlist,
      void *readProcRefCon, void *srcConnRefCon);

OSStatus MIDIClientCreate(CFStringRef name, MIDINotifyProc notify,
      void *refCon, MIDIClientRef *out);
OSStatus MIDIInputPortCreate(MIDIClientRef client, CFStringRef name,
      MIDIReadProc read, void *refCon, MIDIPortRef *out);
OSStatus MIDIOutputPortCreate(MIDIClientRef client, CFStringRef name,
      MIDIPortRef *out);
OSStatus MIDIPortDispose(MIDIPortRef port);
OSStatus MIDIPortConnectSource(MIDIPortRef port, MIDIEndpointRef source,
      void *connRefCon);
OSStatus MIDIPortDisconnectSource(MIDIPortRef port, MIDIEndpointRef source);
ItemCount MIDIGetNumberOfSources(void);
MIDIEndpointRef MIDIGetSource(ItemCount i);
ItemCount MIDIGetNumberOfDestinations(void);
MIDIEndpointRef MIDIGetDestination(ItemCount i);
OSStatus MIDIObjectGetStringProperty(MIDIObjectRef obj, CFStringRef prop,
      CFStringRef *out);
int CFStringGetCString(CFStringRef s, char *buf, long size, int encoding);
void CFRelease(const void *obj);
MIDIPacket *MIDIPacketListInit(MIDIPacketList *list);
MIDIPacket *MIDIPacketListAdd(MIDIPacketList *list, unsigned long size,
      MIDIPacket *cur, MIDITimeStamp time, unsigned long n, const Byte *data);
const MIDIPacket *MIDIPacketNext(const MIDIPacket *pkt);
OSStatus MIDISend(MIDIPortRef port, MIDIEndpointRef dest,
      const MIDIPacketList *list);

#endif
