/* Wii: the Bluetooth host stack's face toward HID devices.
 *
 * bt.c runs one thread that owns the controller: HCI, the links and
 * their L2CAP HID channels.  A device that connects gets a link number
 * once both its HID channels are configured; everything below is
 * called on that thread. */

#ifndef GEKKO_RVL_BT_H
#define GEKKO_RVL_BT_H

#include <gekko/gekko.h>

#define BT_MAX_LINKS 4

typedef struct bt_hid_ops
{
   void (*connected)(unsigned link);
   void (*disconnected)(unsigned link);
   /* An input report from the interrupt channel, 0xa1 first. */
   void (*input)(unsigned link, const uint8_t *data, unsigned len);
   /* Every pass of the thread, at least every 10 ms. */
   void (*tick)(void);
} bt_hid_ops;

/* Start the stack (once); 0, or a negative errno. */
int  bt_start(const bt_hid_ops *ops);
/* An output report on the interrupt channel, 0xa2 first; 0 or -1. */
int  bt_send(unsigned link, const uint8_t *data, unsigned len);
/* Drop a link. */
void bt_disconnect(unsigned link);
/* From any thread: run the thread's tick soon. */
void bt_wake(void);

#endif
