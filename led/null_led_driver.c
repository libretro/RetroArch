#include "led_driver.h"
#include "verbosity.h"

static void null_init(void)
{
    RARCH_LOG("[LED]: using null LED driver\n");
}
static void null_free(void) {}
static void null_set(int led,int state) {}

static led_driver_t null_led_driver_ins = { null_init, null_free, null_set };
led_driver_t *null_led_driver = &null_led_driver_ins;
