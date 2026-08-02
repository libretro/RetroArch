// SPDX-License-Identifier: GPL-2.0
/*
 * Copyright (c) 2023-24 Raspberry Pi Ltd.
 * All rights reserved.
 */
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#include "piolib.h"
#include "piolib_priv.h"

#define PIO_MAX_INSTANCES 4

extern PIO_CHIP_T *__start_piochips;
extern PIO_CHIP_T *__stop_piochips;

static __thread PIO __pio;

static PIO pio_instances[PIO_MAX_INSTANCES];
static uint num_instances;
static pthread_mutex_t pio_handle_lock;

void pio_select(PIO pio)
{
    __pio = pio;
}

PIO pio_get_current(void)
{
    PIO pio = __pio;
    check_pio_param(pio);
    return pio;
}

int pio_get_index(PIO pio)
{
    int i;
    for (i = 0; i < PIO_MAX_INSTANCES; i++)
    {
        if (pio == pio_instances[i])
            return i;
    }
    return -1;
}

int pio_init(void)
{
    static bool initialised;
    const PIO_CHIP_T * const *p;
    uint i = 0;
    int err;

    if (initialised)
        return 0;
    num_instances = 0;
    p = &__start_piochips;
    while (p < &__stop_piochips && num_instances < PIO_MAX_INSTANCES)
    {
        PIO_CHIP_T *chip = *p;
        PIO pio = chip->create_instance(chip, i);
        if (pio && !PIO_IS_ERR(pio)) {
            pio_instances[num_instances++] = pio;
            i++;
        } else {
            p++;
            i = 0;
        }
    }

    err = pthread_mutex_init(&pio_handle_lock, NULL);
    if (err)
        return err;

    initialised = true;
    return 0;
}

PIO pio_open(uint idx)
{
    PIO pio = NULL;
    int err;

    err = pio_init();
    if (err)
        return PIO_ERR(err);

    if (idx >= num_instances)
        return PIO_ERR(-EINVAL);

    pthread_mutex_lock(&pio_handle_lock);

    pio = pio_instances[idx];
    if (pio) {
        if (pio->in_use)
            err = -EBUSY;
        else
            pio->in_use = 1;
    }

    pthread_mutex_unlock(&pio_handle_lock);

    if (err)
        return PIO_ERR(err);

    err = pio->chip->open_instance(pio);
    if (err) {
        pio->in_use = 0;
        return PIO_ERR(err);
    }

    pio_select(pio);

    return pio;
}

PIO pio_open_by_name(const char *name)
{
    int err = -ENOENT;
    uint i;

    err = pio_init();
    if (err)
        return PIO_ERR(err);

    for (i = 0; i < num_instances; i++) {
        PIO p = pio_instances[i];
        if (!strcmp(name, p->chip->name))
            break;
    }

    if (i == num_instances)
        return PIO_ERR(-ENOENT);

    return pio_open(i);
}

PIO pio_open_helper(uint idx)
{
    PIO pio = pio_instances[idx];
    if (!pio || !pio->in_use) {
        pio = pio_open(idx);
        if (PIO_IS_ERR(pio)) {
            printf("* Failed to open PIO device %d (error %d)\n",
                   idx, PIO_ERR_VAL(pio));
            exit(1);
        }
    }
    return pio;
}

void pio_close(PIO pio)
{
    pio->chip->close_instance(pio);
    pthread_mutex_lock(&pio_handle_lock);
    pio->in_use = 0;
    pthread_mutex_unlock(&pio_handle_lock);
}

void pio_panic(const char *msg)
{
    fprintf(stderr, "PANIC: %s\n", msg);
    exit(1);
}

void sleep_us(uint64_t us) {
    const struct timespec tv = {
        .tv_sec = (us / 1000000),
        .tv_nsec = 1000ull * (us % 1000000)
    };
    nanosleep(&tv, NULL);
}
