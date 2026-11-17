/* The macOS/iOS HID driver (input/drivers_hid/iohidmanager_hid.c) for a
 * pad no handler reads, built on the host against stand-ins for IOKit
 * (iokit_sdk/) with the real pad connections behind it. Its element
 * callback is handed button, axis and hat values as IOKit would hand
 * them, and the pad is held to:
 *
 * - what the values said: the first 32 buttons, the 11 axes scaled
 *   from their physical range, the hat's eight ways and centre;
 * - what the snapshot bridge assumes of a driver it serves
 *   (joypad_bridge_checked/bridge_view.h), its view included;
 * - a pad unplugged holding nothing, on the last slot too, and nothing
 *   written past the slot it was in. */
#include "../../../input/drivers_hid/iohidmanager_hid.c"

#include <stdio.h>

#include "../joypad_bridge_checked/bridge_view.h"

/* ---- IOKit's objects, as the stand-in header leaves them opaque ---- */
struct __IOHIDElement
{
   IOHIDElementType type;
   uint32_t page, usage;
   IOHIDElementCookie cookie;
   CFIndex lmin, lmax, pmin, pmax;
};
struct __IOHIDValue
{
   IOHIDElementRef element;
   CFIndex v;
};

IOHIDElementRef IOHIDValueGetElement(IOHIDValueRef v) { return v->element; }
CFIndex IOHIDValueGetIntegerValue(IOHIDValueRef v) { return v->v; }
IOHIDElementType IOHIDElementGetType(IOHIDElementRef e) { return e->type; }
uint32_t IOHIDElementGetUsagePage(IOHIDElementRef e) { return e->page; }
uint32_t IOHIDElementGetUsage(IOHIDElementRef e) { return e->usage; }
IOHIDElementCookie IOHIDElementGetCookie(IOHIDElementRef e) { return e->cookie; }
CFIndex IOHIDElementGetLogicalMin(IOHIDElementRef e) { return e->lmin; }
CFIndex IOHIDElementGetLogicalMax(IOHIDElementRef e) { return e->lmax; }
CFIndex IOHIDElementGetPhysicalMin(IOHIDElementRef e) { return e->pmin; }
CFIndex IOHIDElementGetPhysicalMax(IOHIDElementRef e) { return e->pmax; }

/* the rest of IOKit and CoreFoundation the driver names; the device
 * and manager paths are not run here */
const CFAllocatorRef kCFAllocatorDefault = NULL;
const CFStringRef kCFRunLoopCommonModes  = NULL;
const CFStringRef kCFRunLoopDefaultMode  = NULL;
const CFDictionaryKeyCallBacks kCFTypeDictionaryKeyCallBacks;
const CFDictionaryValueCallBacks kCFTypeDictionaryValueCallBacks;
const CFArrayCallBacks kCFTypeArrayCallBacks;
void CFRelease(CFTypeRef p) { (void)p; }
CFTypeID CFGetTypeID(CFTypeRef p) { (void)p; return 0; }
CFTypeID CFNumberGetTypeID(void) { return 1; }
CFNumberRef CFNumberCreate(CFAllocatorRef a, CFNumberType t, const void *v)
{ (void)a; (void)t; (void)v; return NULL; }
Boolean CFNumberGetValue(CFNumberRef n, CFNumberType t, void *v)
{ (void)n; (void)t; (void)v; return 0; }
Boolean CFStringGetCString(CFStringRef s, char *b, CFIndex l, uint32_t e)
{ (void)s; (void)e; if (l) *b = '\0'; return 0; }
CFRange CFRangeMake(CFIndex a, CFIndex b) { CFRange r; r.location = a; r.length = b; return r; }
CFIndex CFArrayGetCount(CFArrayRef a) { (void)a; return 0; }
const void *CFArrayGetValueAtIndex(CFArrayRef a, CFIndex i) { (void)a; (void)i; return NULL; }
CFMutableArrayRef CFArrayCreateMutable(CFAllocatorRef a, CFIndex n,
      const CFArrayCallBacks *c) { (void)a; (void)n; (void)c; return NULL; }
CFMutableArrayRef CFArrayCreateMutableCopy(CFAllocatorRef a, CFIndex n,
      CFArrayRef s) { (void)a; (void)n; (void)s; return NULL; }
void CFArrayAppendValue(CFMutableArrayRef a, const void *v) { (void)a; (void)v; }
void CFArraySortValues(CFMutableArrayRef a, CFRange r, CFComparatorFunction f,
      void *c) { (void)a; (void)r; (void)f; (void)c; }
CFMutableDictionaryRef CFDictionaryCreateMutable(CFAllocatorRef a, CFIndex n,
      const CFDictionaryKeyCallBacks *k, const CFDictionaryValueCallBacks *v)
{ (void)a; (void)n; (void)k; (void)v; return NULL; }
void CFDictionarySetValue(CFMutableDictionaryRef d, const void *k,
      const void *v) { (void)d; (void)k; (void)v; }
CFRunLoopRef CFRunLoopGetCurrent(void) { return NULL; }
IOHIDManagerRef IOHIDManagerCreate(CFAllocatorRef a, IOOptionBits o)
{ (void)a; (void)o; return NULL; }
IOReturn IOHIDManagerClose(IOHIDManagerRef m, IOOptionBits o) { (void)m; (void)o; return 0; }
void IOHIDManagerScheduleWithRunLoop(IOHIDManagerRef m, CFRunLoopRef r,
      CFStringRef s) { (void)m; (void)r; (void)s; }
void IOHIDManagerUnscheduleFromRunLoop(IOHIDManagerRef m, CFRunLoopRef r,
      CFStringRef s) { (void)m; (void)r; (void)s; }
void IOHIDManagerSetDeviceMatching(IOHIDManagerRef m, CFDictionaryRef d)
{ (void)m; (void)d; }
void IOHIDManagerSetDeviceMatchingMultiple(IOHIDManagerRef m, CFArrayRef a)
{ (void)m; (void)a; }
void IOHIDManagerRegisterDeviceMatchingCallback(IOHIDManagerRef m,
      IOHIDDeviceCallback c, void *d) { (void)m; (void)c; (void)d; }
void IOHIDManagerRegisterDeviceRemovalCallback(IOHIDManagerRef m,
      IOHIDDeviceCallback c, void *d) { (void)m; (void)c; (void)d; }
IOReturn IOHIDDeviceOpen(IOHIDDeviceRef d, IOOptionBits o) { (void)d; (void)o; return 0; }
void IOHIDDeviceScheduleWithRunLoop(IOHIDDeviceRef d, CFRunLoopRef r,
      CFStringRef s) { (void)d; (void)r; (void)s; }
CFTypeRef IOHIDDeviceGetProperty(IOHIDDeviceRef d, CFStringRef k)
{ (void)d; (void)k; return NULL; }
CFArrayRef IOHIDDeviceCopyMatchingElements(IOHIDDeviceRef d, CFDictionaryRef m,
      IOOptionBits o) { (void)d; (void)m; (void)o; return NULL; }
void IOHIDDeviceRegisterInputValueCallback(IOHIDDeviceRef d,
      IOHIDValueCallback c, void *x) { (void)d; (void)c; (void)x; }
void IOHIDDeviceRegisterInputReportCallback(IOHIDDeviceRef d, uint8_t *b,
      CFIndex l, IOHIDReportCallback c, void *x)
{ (void)d; (void)b; (void)l; (void)c; (void)x; }
IOReturn IOHIDDeviceSetReport(IOHIDDeviceRef d, IOHIDReportType t, CFIndex i,
      const uint8_t *b, CFIndex l) { (void)d; (void)t; (void)i; (void)b; (void)l; return 0; }
IOReturn IOHIDDeviceGetReport(IOHIDDeviceRef d, IOHIDReportType t, CFIndex i,
      uint8_t *b, CFIndex *l) { (void)d; (void)t; (void)i; (void)b; (void)l; return 0; }

/* ---- the frontend's ---- */
static iohidmanager_hid_t *test_hid;
void RARCH_LOG(const char *f, ...)  { (void)f; }
void RARCH_DBG(const char *f, ...)  { (void)f; }
void RARCH_ERR(const char *f, ...)  { (void)f; }
void RARCH_WARN(const char *f, ...) { (void)f; }
void input_pad_connect(unsigned port, input_device_driver_t *driver)
{ (void)port; (void)driver; }
bool input_device_ignored(uint16_t vid, uint16_t pid)
{ (void)vid; (void)pid; return false; }
void *hid_driver_get_data(void) { return test_hid; }
bool input_autoconfigure_disconnect(unsigned port, const char *name)
{ (void)port; (void)name; return true; }
bool input_autoconfigure_connect(const char *name, const char *display_name,
      const char *phys, const char *driver, unsigned port, unsigned vid,
      unsigned pid)
{ (void)name; (void)display_name; (void)phys; (void)driver; (void)port;
  (void)vid; (void)pid; return true; }

/* ---- the pad, as the hid joypad driver serves it ---- */
static int32_t t_button(unsigned p, uint16_t k)
{ return iohidmanager_hid_joypad_button(test_hid, p, k); }
static int16_t t_axis(unsigned p, uint32_t a)
{ return iohidmanager_hid_joypad_axis(test_hid, p, a); }
static void t_get_buttons(unsigned p, input_bits_t *s)
{ iohidmanager_hid_joypad_get_buttons(test_hid, p, s); }
static int16_t t_state(rarch_joypad_info_t *i, const uint16_t *k,
      const uint32_t *a, unsigned p)
{ return iohidmanager_hid_joypad_state(test_hid, i, k, a, p); }
static const input_pad_view_t *t_view(unsigned p)
{ return iohidmanager_hid_joypad_view(test_hid, p); }

#define BUTTONS 40
#define AXES    11

static apple_input_rec_t *rec(uint32_t id, uint32_t cookie,
      apple_input_rec_t *next)
{
   apple_input_rec_t *r = (apple_input_rec_t*)calloc(1, sizeof(*r));
   r->id     = id;
   r->cookie = cookie;
   r->next   = next;
   return r;
}

/* a pad no handler reads on @slot, with BUTTONS buttons (cookies 100
 * on), AXES axes (200 on) and a hat (300) */
static struct iohidmanager_hid_adapter *plug(unsigned slot)
{
   int i;
   struct iohidmanager_hid_adapter *a =
      (struct iohidmanager_hid_adapter*)calloc(1, sizeof(*a));
   a->slot   = (int32_t)slot;
   a->handle = (IOHIDDeviceRef)(uintptr_t)(slot + 1);
   for (i = BUTTONS - 1; i >= 0; i--)
      a->buttons = rec((uint32_t)i, (uint32_t)(100 + i), a->buttons);
   for (i = AXES - 1; i >= 0; i--)
      a->axes = rec((uint32_t)i, (uint32_t)(200 + i), a->axes);
   a->hats = rec(0, 300, NULL);
   test_hid->slots[slot].data      = a;
   test_hid->slots[slot].connected = true;
   return a;
}

static void send(struct iohidmanager_hid_adapter *a, IOHIDElementType type,
      uint32_t page, uint32_t usage, uint32_t cookie, CFIndex lmin,
      CFIndex lmax, CFIndex pmin, CFIndex pmax, CFIndex v)
{
   struct __IOHIDElement e;
   struct __IOHIDValue   val;
   e.type   = type;  e.page = page; e.usage = usage; e.cookie = cookie;
   e.lmin   = lmin;  e.lmax = lmax; e.pmin  = pmin;  e.pmax   = pmax;
   val.element = &e;
   val.v       = v;
   iohidmanager_hid_device_input_callback(a, 0, NULL, &val);
}

int main(void)
{
   static const uint16_t ways[9] = { 1, 1 | 8, 8, 2 | 8, 2, 2 | 4, 4, 1 | 4, 0 };
   static const unsigned slots[2] = { 0, MAX_USERS - 1 };
   struct iohidmanager_hid_adapter *ad[2];
   uint32_t want_buttons[2] = { 0, 0 };
   int16_t  want_axes[2][AXES];
   uint16_t want_hat[2]     = { 0, 0 };
   input_device_driver_t drv;
   unsigned n, i, s, bad = 0;

   memset(&drv, 0, sizeof(drv));
   drv.button      = t_button;
   drv.axis        = t_axis;
   drv.get_buttons = t_get_buttons;
   drv.state       = t_state;
   drv.view        = t_view;
   drv.ident       = "hid";

   test_hid        = (iohidmanager_hid_t*)calloc(1, sizeof(*test_hid));
   test_hid->slots = pad_connection_init(MAX_USERS);
   memset(want_axes, 0, sizeof(want_axes));
   for (s = 0; s < 2; s++)
      ad[s] = plug(slots[s]);

   for (n = 0; n < 4000; n++)
   {
      unsigned k    = n & 1;
      unsigned slot = slots[k];
      unsigned what = bv_rand() % 3;
      if (what == 0)
      {
         unsigned b  = bv_rand() % BUTTONS;
         unsigned on = bv_rand() & 1;
         send(ad[k], kIOHIDElementTypeInput_Button, kHIDPage_Button,
               b + 1, 100 + b, 0, 1, 0, 1, on);
         if (b < 32)
            want_buttons[k] = on ? (want_buttons[k] | (1u << b))
                                 : (want_buttons[k] & ~(1u << b));
      }
      else if (what == 1)
      {
         unsigned ax  = bv_rand() % AXES;
         CFIndex  raw = (CFIndex)(bv_rand() % 256);
         float    val = (float)raw / 255.0f;
         send(ad[k], kIOHIDElementTypeInput_Misc, kHIDPage_GenericDesktop,
               kHIDUsage_GD_X, 200 + ax, 0, 255, 0, 255, raw);
         want_axes[k][ax] = (int16_t)(((val * 2.0f) - 1.0f) * 32767.0f);
      }
      else
      {
         unsigned w = bv_rand() % 9; /* 8 is centred */
         send(ad[k], kIOHIDElementTypeInput_Misc, kHIDPage_GenericDesktop,
               kHIDUsage_GD_Hatswitch, 300, 0, 7, 0, 315, w);
         want_hat[k] = ways[w];
      }

      /* what the values said */
      for (i = 0; i < BUTTONS; i++)
         if (     (t_button(slot, (uint16_t)i) != 0)
               != (i < 32 && ((want_buttons[k] >> i) & 1)))
            bad++;
      for (i = 0; i < AXES; i++)
         if ((int16_t)(t_axis(slot, AXIS_POS(i)) + t_axis(slot, AXIS_NEG(i)))
               != want_axes[k][i])
            bad++;
      if (     (t_button(slot, HAT_MAP(0, HAT_UP_MASK))    != 0) != ((want_hat[k] & 1) != 0)
            || (t_button(slot, HAT_MAP(0, HAT_DOWN_MASK))  != 0) != ((want_hat[k] & 2) != 0)
            || (t_button(slot, HAT_MAP(0, HAT_LEFT_MASK))  != 0) != ((want_hat[k] & 4) != 0)
            || (t_button(slot, HAT_MAP(0, HAT_RIGHT_MASK)) != 0) != ((want_hat[k] & 8) != 0))
         bad++;

      bad += bv_compare(&drv, slot);
      bad += bv_compare_view(&drv, slot);
   }

   /* unplugged, the last slot's pad holds nothing: no button, no axis,
    * no hat left held */
   send(ad[1], kIOHIDElementTypeInput_Misc, kHIDPage_GenericDesktop,
         kHIDUsage_GD_Hatswitch, 300, 0, 7, 0, 315, 0);
   send(ad[1], kIOHIDElementTypeInput_Button, kHIDPage_Button, 1, 100,
         0, 1, 0, 1, 1);
   iohidmanager_hid_device_remove(
         (IOHIDDeviceRef)(uintptr_t)(slots[1] + 1), test_hid);
   if (     t_button(slots[1], HAT_MAP(0, HAT_UP_MASK))
         || t_button(slots[1], 0)
         || t_axis(slots[1], AXIS_POS(0)) || t_axis(slots[1], AXIS_NEG(0)))
      bad++;

   if (bad)
   {
      printf("FAIL iohidmanager_hid_test: %u disagreements\n", bad);
      return 1;
   }
   printf("PASS iohidmanager_hid_test: 4000 element values; the pad says"
         " what they said, and the bridge reads it as the driver does\n");
   return 0;
}
