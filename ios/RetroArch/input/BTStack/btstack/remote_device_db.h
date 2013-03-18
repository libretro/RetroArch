/*
 * Copyright (C) 2009-2012 by Matthias Ringwald
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the copyright holders nor the names of
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 * 4. Any redistribution, use, or modification is done solely for
 *    personal benefit and not for any commercial purpose or for
 *    monetary gain.
 *
 * THIS SOFTWARE IS PROVIDED BY MATTHIAS RINGWALD AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MATTHIAS
 * RINGWALD OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT,
 * INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING,
 * BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS
 * OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED
 * AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Please inquire about commercial licensing options at btstack@ringwald.ch
 *
 */

/**
 * interface to provide link key and remote name storage
 */

#pragma once

#include <btstack/utils.h>

typedef struct {

    // management
    void (*open)(void);
    void (*close)(void);
    
    // link key
    int  (*get_link_key)(bd_addr_t *bd_addr, link_key_t *link_key);
    void (*put_link_key)(bd_addr_t *bd_addr, link_key_t *key);
    void (*delete_link_key)(bd_addr_t *bd_addr);
    
    // remote name
    int  (*get_name)(bd_addr_t *bd_addr, device_name_t *device_name);
    void (*put_name)(bd_addr_t *bd_addr, device_name_t *device_name);
    void (*delete_name)(bd_addr_t *bd_addr);

    // persistent rfcomm channel
    uint8_t (*persistent_rfcomm_channel)(char *servicename);

} remote_device_db_t;

extern remote_device_db_t remote_device_db_iphone;
extern const remote_device_db_t remote_device_db_memory;

// MARK: non-persisten implementation
#include <btstack/linked_list.h>
#define MAX_NAME_LEN 32
typedef struct {
    // linked list - assert: first field
    linked_item_t    item;
    
    bd_addr_t bd_addr;
} db_mem_device_t;

typedef struct {
    db_mem_device_t device;
    link_key_t link_key;
} db_mem_device_link_key_t;

typedef struct {
    db_mem_device_t device;
    char device_name[MAX_NAME_LEN];
} db_mem_device_name_t;

typedef struct {
    // linked list - assert: first field
    linked_item_t    item;
    
    char service_name[MAX_NAME_LEN];
    uint8_t channel;
} db_mem_service_t;
