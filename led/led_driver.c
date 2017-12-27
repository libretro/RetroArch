#include <stdio.h>
#include "led_driver.h"
#include "configuration.h"
#include "verbosity.h"

extern led_driver_t *null_led_driver;
#if HAVE_RPILED
extern led_driver_t *rpi_led_driver;
#endif
led_driver_t *current_led_driver = NULL;

bool led_driver_init(void)
{
    char *drivername = NULL;
    settings_t *settings = config_get_ptr();
    drivername = settings->arrays.led_driver;
    
    if(drivername == NULL)
        drivername = "null";

#if HAVE_RPILED      
    if(!strcmp("rpi",drivername))
    {
        current_led_driver = rpi_led_driver;
    }
    else
#endif          
    {
        current_led_driver = null_led_driver;
    }

    RARCH_LOG("[LED]: LED driver = '%s' %p\n",drivername,current_led_driver);
    
    if(current_led_driver != NULL)
    {
        (*current_led_driver->init)();
    }
    
    return true;
}

void led_driver_free(void)
{
    if(current_led_driver != NULL)
    {
        (*current_led_driver->free)();
    }
}

void led_driver_set_led(int led,int value)
{
    if(current_led_driver != NULL)
    {
        (*current_led_driver->set_led)(led,value);
    }
}
