#ifndef FAKE_LIBUSB_H
#define FAKE_LIBUSB_H

struct fake_dev { int present; };

struct fake_stats
{
   int opens, closes, allocs, frees, freed_in_flight, double_submits, exits;
   int nsent, max_outs_in_flight;
   unsigned char sent[16][8];
};
/* Counters, copied out under the fake's lock. */
struct fake_stats fake_snapshot(void);

struct libusb_device *fake_new_device(void);
void fake_plug(struct libusb_device *dev);
void fake_unplug(struct libusb_device *dev);
int  fake_report(struct libusb_device *dev, const unsigned char *d, int len);
int  fake_ack_out(struct libusb_device *dev);
int  fake_in_flight(struct libusb_device *dev, int in);
void fake_forget_devices(void);

#endif
