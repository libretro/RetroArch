/*
libgo2 - Support library for the ODROID-GO Advance
Copyright (C) 2020 OtherCrashOverride

This library is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

This library is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "input.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <signal.h>
#include <dirent.h>
#include <stdbool.h>
#include <pthread.h>

#include <libevdev-1.0/libevdev/libevdev.h>
#include <linux/limits.h>


#define BATTERY_BUFFER_SIZE (128)

static const char* EVDEV_NAME = "/dev/input/by-path/platform-odroidgo2-joypad-event-joystick";
static const char* BATTERY_STATUS_NAME = "/sys/class/power_supply/battery/status";
static const char* BATTERY_CAPACITY_NAME = "/sys/class/power_supply/battery/capacity";


typedef struct go2_input
{
    int fd;
    struct libevdev* dev;
    go2_gamepad_state_t current_state;
    go2_gamepad_state_t pending_state;
    pthread_mutex_t gamepadMutex;
    pthread_t thread_id;
    go2_battery_state_t current_battery;
    pthread_t battery_thread;
    bool terminating;
} go2_input_t;


static void* battery_task(void* arg)
{
    go2_input_t* input = (go2_input_t*)arg;
    int fd;
    void* result = 0;
    char buffer[BATTERY_BUFFER_SIZE + 1];
    go2_battery_state_t battery;


    memset(&battery, 0, sizeof(battery));


    while(!input->terminating)
    {
        fd = open(BATTERY_STATUS_NAME, O_RDONLY);
        if (fd > 0)
        {
            memset(buffer, 0, BATTERY_BUFFER_SIZE + 1);
            ssize_t count = read(fd, buffer, BATTERY_BUFFER_SIZE);
            if (count > 0)
            {
                //printf("BATT: buffer='%s'\n", buffer);

                if (buffer[0] == 'D')
                {
                    battery.status = Battery_Status_Discharging;
                }
                else if (buffer[0] == 'C')
                {
                    battery.status = Battery_Status_Charging;
                }
                else if (buffer[0] == 'F')
                {
                    battery.status = Battery_Status_Full;
                }
                else
                {
                    battery.status = Battery_Status_Unknown;
                }                
            }

            close(fd);
        }

        fd = open(BATTERY_CAPACITY_NAME, O_RDONLY);
        if (fd > 0)
        {
            memset(buffer, 0, BATTERY_BUFFER_SIZE + 1);
            ssize_t count = read(fd, buffer, BATTERY_BUFFER_SIZE);
            if (count > 0)
            {
                battery.level = atoi(buffer);
            }
            else
            {
                battery.level = 0;
            }
            
            close(fd);
        }


        pthread_mutex_lock(&input->gamepadMutex);

        input->current_battery = battery;

        pthread_mutex_unlock(&input->gamepadMutex); 
        
        //printf("BATT: status=%d, level=%d\n", input->current_battery.status, input->current_battery.level);

        sleep(1);      
    }

    //printf("BATT: exit.\n");
    return result;
}




static void* input_task(void* arg)
{
    go2_input_t* input = (go2_input_t*)arg;

    if (!input->dev) return NULL;

    const int abs_x_max = 512; //libevdev_get_abs_maximum(input->dev, ABS_X);
    const int abs_y_max = 512; //libevdev_get_abs_maximum(input->dev, ABS_Y);

    //printf("abs: x_max=%d, y_max=%d\n", abs_x_max, abs_y_max);
    

    // Get current state
    input->pending_state.dpad.up = libevdev_get_event_value(input->dev, EV_KEY, BTN_DPAD_UP) ? ButtonState_Pressed : ButtonState_Released;
    input->pending_state.dpad.down = libevdev_get_event_value(input->dev, EV_KEY, BTN_DPAD_DOWN) ? ButtonState_Pressed : ButtonState_Released;
    input->pending_state.dpad.left = libevdev_get_event_value(input->dev, EV_KEY, BTN_DPAD_LEFT) ? ButtonState_Pressed : ButtonState_Released;
    input->pending_state.dpad.right = libevdev_get_event_value(input->dev, EV_KEY, BTN_DPAD_RIGHT) ? ButtonState_Pressed : ButtonState_Released;

    input->pending_state.buttons.a = libevdev_get_event_value(input->dev, EV_KEY, BTN_EAST) ? ButtonState_Pressed : ButtonState_Released;
    input->pending_state.buttons.b = libevdev_get_event_value(input->dev, EV_KEY, BTN_SOUTH) ? ButtonState_Pressed : ButtonState_Released;
    input->pending_state.buttons.x = libevdev_get_event_value(input->dev, EV_KEY, BTN_NORTH) ? ButtonState_Pressed : ButtonState_Released;
    input->pending_state.buttons.y = libevdev_get_event_value(input->dev, EV_KEY, BTN_WEST) ? ButtonState_Pressed : ButtonState_Released;

    input->pending_state.buttons.top_left = libevdev_get_event_value(input->dev, EV_KEY, BTN_TL) ? ButtonState_Pressed : ButtonState_Released;
    input->pending_state.buttons.top_right = libevdev_get_event_value(input->dev, EV_KEY, BTN_TR) ? ButtonState_Pressed : ButtonState_Released;

    input->current_state.buttons.f1 = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY1) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons.f2 = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY2) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons.f3 = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY3) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons.f4 = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY4) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons.f5 = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY5) ? ButtonState_Pressed : ButtonState_Released;
    input->current_state.buttons.f5 = libevdev_get_event_value(input->dev, EV_KEY, BTN_TRIGGER_HAPPY6) ? ButtonState_Pressed : ButtonState_Released;


    // Events
	while (!input->terminating)
	{
		/* EAGAIN is returned when the queue is empty */
		struct input_event ev;
		int rc = libevdev_next_event(input->dev, LIBEVDEV_READ_FLAG_BLOCKING, &ev);
		if (rc == 0)
		{
#if 0
			printf("Gamepad Event: %s-%s(%d)=%d\n",
			       libevdev_event_type_get_name(ev.type),
			       libevdev_event_code_get_name(ev.type, ev.code), ev.code,
			       ev.value);
#endif

            if (ev.type == EV_KEY)
			{
                go2_button_state_t state = ev.value ? ButtonState_Pressed : ButtonState_Released;

                switch (ev.code)
                {
                    case BTN_DPAD_UP:
                        input->pending_state.dpad.up = state;
                        break;
                    case BTN_DPAD_DOWN:
                        input->pending_state.dpad.down = state;
                        break;
                    case BTN_DPAD_LEFT:
                        input->pending_state.dpad.left = state;
                        break;
                    case BTN_DPAD_RIGHT:
                        input->pending_state.dpad.right = state;
                        break;

                    case BTN_EAST:
                        input->pending_state.buttons.a = state;
                        break;
                    case BTN_SOUTH:
                        input->pending_state.buttons.b = state;
                        break;
                    case BTN_NORTH:
                        input->pending_state.buttons.x = state;
                        break;
                    case BTN_WEST:
                        input->pending_state.buttons.y = state;
                        break;

                    case BTN_TL:
                        input->pending_state.buttons.top_left = state;
                        break;                    
                    case BTN_TR:          
                        input->pending_state.buttons.top_right = state;
                        break;

                    case BTN_TRIGGER_HAPPY1:
                        input->pending_state.buttons.f1 = state;
                        break;
                    case BTN_TRIGGER_HAPPY2:
                        input->pending_state.buttons.f2 = state;
                        break;
                    case BTN_TRIGGER_HAPPY3:
                        input->pending_state.buttons.f3 = state;
                        break;
                    case BTN_TRIGGER_HAPPY4:
                        input->pending_state.buttons.f4 = state;
                        break;
                    case BTN_TRIGGER_HAPPY5:
                        input->pending_state.buttons.f5 = state;
                        break;
                    case BTN_TRIGGER_HAPPY6:
                        input->pending_state.buttons.f6 = state;
                        break;
                }
            }
            else if (ev.type == EV_ABS)
            {
                switch (ev.code)
                {
                    case ABS_X:
                        input->pending_state.thumb.x = ev.value / (float)abs_x_max;
                        break;
                    case ABS_Y:
                        input->pending_state.thumb.y = ev.value / (float)abs_y_max;
                        break;
                }
            }
            else if (ev.type == EV_SYN)
            {
                pthread_mutex_lock(&input->gamepadMutex);
    
                input->current_state = input->pending_state;

                pthread_mutex_unlock(&input->gamepadMutex); 
            }
        }
    }

    return NULL;
}

go2_input_t* go2_input_create()
{
	int rc = 1;

    go2_input_t* result = malloc(sizeof(*result));
    if (!result)
    {
        printf("malloc failed.\n");
        goto out;
    }

    memset(result, 0, sizeof(*result));





    result->fd = open(EVDEV_NAME, O_RDONLY);
    if (result->fd < 0)
    {
        printf("Joystick: No gamepad found.\n");
    }
    else
    {    
        rc = libevdev_new_from_fd(result->fd, &result->dev);
        if (rc < 0) {
            printf("Joystick: Failed to init libevdev (%s)\n", strerror(-rc));
            goto err_00;
        }

        memset(&result->current_state, 0, sizeof(result->current_state));
        memset(&result->pending_state, 0, sizeof(result->pending_state));
    
    
        // printf("Input device name: \"%s\"\n", libevdev_get_name(result->dev));
        // printf("Input device ID: bus %#x vendor %#x product %#x\n",
        //     libevdev_get_id_bustype(result->dev),
        //     libevdev_get_id_vendor(result->dev),
        //     libevdev_get_id_product(result->dev));

        if(pthread_create(&result->thread_id, NULL, input_task, (void*)result) < 0)
        {
            printf("could not create input_task thread\n");
            goto err_01;
        }

        if(pthread_create(&result->battery_thread, NULL, battery_task, (void*)result) < 0)
        {
            printf("could not create battery_task thread\n");
        }

    }

    return result;


err_01:
    libevdev_free(result->dev);

err_00:
    close(result->fd);
    free(result);

out:
    return NULL;
}

void go2_input_destroy(go2_input_t* input)
{
    input->terminating = true;

    pthread_cancel(input->thread_id);

    pthread_join(input->thread_id, NULL);
    pthread_join(input->battery_thread, NULL);

    libevdev_free(input->dev);
    close(input->fd);
    free(input);
}

void go2_input_gamepad_read(go2_input_t* input, go2_gamepad_state_t* outGamepadState)
{
    pthread_mutex_lock(&input->gamepadMutex);
    
    *outGamepadState = input->current_state;        

    pthread_mutex_unlock(&input->gamepadMutex);  
}

void go2_input_battery_read(go2_input_t* input, go2_battery_state_t* outBatteryState)
{
    pthread_mutex_lock(&input->gamepadMutex);

    *outBatteryState = input->current_battery;

    pthread_mutex_unlock(&input->gamepadMutex);
}
