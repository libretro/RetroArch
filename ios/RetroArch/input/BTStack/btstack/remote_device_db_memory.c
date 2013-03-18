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

#include <string.h>
#include <stdlib.h>

#include "remote_device_db.h"
#include "btstack_memory.h"
#include "debug.h"

#include <btstack/utils.h>
#include <btstack/linked_list.h>

// This lists should be only accessed by tests.
linked_list_t db_mem_link_keys = NULL;
linked_list_t db_mem_names = NULL;
static linked_list_t db_mem_services = NULL;

// Device info
static void db_open(void){
}

static void db_close(void){ 
}

static db_mem_device_t * get_item(linked_list_t list, bd_addr_t *bd_addr) {
    linked_item_t *it;
    for (it = (linked_item_t *) list; it ; it = it->next){
        db_mem_device_t * item = (db_mem_device_t *) it;
        if (BD_ADDR_CMP(item->bd_addr, *bd_addr) == 0) {
            return item;
        }
    }
    return NULL;
}

static int get_name(bd_addr_t *bd_addr, device_name_t *device_name) {
    db_mem_device_name_t * item = (db_mem_device_name_t *) get_item(db_mem_names, bd_addr);
    
    if (!item) return 0;
    
    strncpy((char*)device_name, item->device_name, MAX_NAME_LEN);
    
	linked_list_remove(&db_mem_names, (linked_item_t *) item);
    linked_list_add(&db_mem_names, (linked_item_t *) item);
	
	return 1;
}

static int get_link_key(bd_addr_t *bd_addr, link_key_t *link_key) {
    db_mem_device_link_key_t * item = (db_mem_device_link_key_t *) get_item(db_mem_link_keys, bd_addr);
    
    if (!item) return 0;
    
    memcpy(link_key, item->link_key, LINK_KEY_LEN);
    
	linked_list_remove(&db_mem_link_keys, (linked_item_t *) item);
    linked_list_add(&db_mem_link_keys, (linked_item_t *) item);

	return 1;
}

static void delete_link_key(bd_addr_t *bd_addr){
    db_mem_device_t * item = get_item(db_mem_link_keys, bd_addr);
    
    if (!item) return;
    
    linked_list_remove(&db_mem_link_keys, (linked_item_t *) item);
    btstack_memory_db_mem_device_link_key_free(item);
}


static void put_link_key(bd_addr_t *bd_addr, link_key_t *link_key){
    db_mem_device_link_key_t * existingRecord = (db_mem_device_link_key_t *) get_item(db_mem_link_keys, bd_addr);
    
    if (existingRecord){
        memcpy(existingRecord->link_key, link_key, LINK_KEY_LEN);
        return;
    }
    
    // Record not found, create new one for this device
    db_mem_device_link_key_t * newItem = (db_mem_device_link_key_t*) btstack_memory_db_mem_device_link_key_get();
    if (!newItem){
        newItem = (db_mem_device_link_key_t*)linked_list_get_last_item(&db_mem_link_keys);
    }
    
    if (!newItem) return;
    
    memcpy(newItem->device.bd_addr, bd_addr, sizeof(bd_addr_t));
    memcpy(newItem->link_key, link_key, LINK_KEY_LEN);
    linked_list_add(&db_mem_link_keys, (linked_item_t *) newItem);
}

static void delete_name(bd_addr_t *bd_addr){
    db_mem_device_t * item = get_item(db_mem_names, bd_addr);
    
    if (!item) return;
    
    linked_list_remove(&db_mem_names, (linked_item_t *) item);
    btstack_memory_db_mem_device_name_free(item);    
}

static void put_name(bd_addr_t *bd_addr, device_name_t *device_name){
    db_mem_device_name_t * existingRecord = (db_mem_device_name_t *) get_item(db_mem_names, bd_addr);
    
    if (existingRecord){
        strncpy(existingRecord->device_name, (const char*) device_name, MAX_NAME_LEN);
        return;
    }
    
    // Record not found, create a new one for this device
    db_mem_device_name_t * newItem = (db_mem_device_name_t *) btstack_memory_db_mem_device_name_get();
    if (!newItem) {
        newItem = (db_mem_device_name_t*)linked_list_get_last_item(&db_mem_names);
    };

    if (!newItem) return;
    
    memcpy(newItem->device.bd_addr, bd_addr, sizeof(bd_addr_t));
    strncpy(newItem->device_name, (const char*) device_name, MAX_NAME_LEN);
    linked_list_add(&db_mem_names, (linked_item_t *) newItem);
}


// MARK: PERSISTENT RFCOMM CHANNEL ALLOCATION

static uint8_t persistent_rfcomm_channel(char *serviceName){
    linked_item_t *it;
    db_mem_service_t * item;
    uint8_t max_channel = 1;

    for (it = (linked_item_t *) db_mem_services; it ; it = it->next){
        item = (db_mem_service_t *) it;
        if (strncmp(item->service_name, serviceName, MAX_NAME_LEN) == 0) {
            // Match found
            return item->channel;
        }

        // TODO prevent overflow
        if (item->channel >= max_channel) max_channel = item->channel + 1;
    }

    // Allocate new persistant channel
    db_mem_service_t * newItem = (db_mem_service_t *) btstack_memory_db_mem_service_get();

    if (!newItem) return 0;
    
    strncpy(newItem->service_name, serviceName, MAX_NAME_LEN);
    newItem->channel = max_channel;
    linked_list_add(&db_mem_services, (linked_item_t *) newItem);
    return max_channel;
}


const remote_device_db_t remote_device_db_memory = {
    db_open,
    db_close,
    get_link_key,
    put_link_key,
    delete_link_key,
    get_name,
    put_name,
    delete_name,
    persistent_rfcomm_channel
};
