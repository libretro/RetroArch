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

/*
 *  linked_list.c
 *
 *  Created by Matthias Ringwald on 7/13/09.
 */

#include "linked_list.h"
#include <stdlib.h>
/**
 * tests if list is empty
 */
int  linked_list_empty(linked_list_t * list){
    return *list == (void *) 0;
}

/**
 * linked_list_get_last_item
 */
linked_item_t * linked_list_get_last_item(linked_list_t * list){        // <-- find the last item in the list
    linked_item_t *lastItem = NULL;
    linked_item_t *it;
    for (it = *list; it ; it = it->next){
        if (it) {
            lastItem = it;
        }
    }
    return lastItem;
}


/**
 * linked_list_add
 */
void linked_list_add(linked_list_t * list, linked_item_t *item){        // <-- add item to list
    // check if already in list
    linked_item_t *it;
    for (it = *list; it ; it = it->next){
        if (it == item) {
            return;
        }
    }
    // add first
    item->next = *list;
    *list = item;
}

void linked_list_add_tail(linked_list_t * list, linked_item_t *item){   // <-- add item to list as last element
    // check if already in list
    linked_item_t *it;
    for (it = (linked_item_t *) list; it->next ; it = it->next){
        if (it->next == item) {
            return;
        }
    }
    item->next = (linked_item_t*) 0;
    it->next = item;
}

/**
 * Remove data_source from run loop
 *
 * @note: assumes that data_source_t.next is first element in data_source
 */
int  linked_list_remove(linked_list_t * list, linked_item_t *item){    // <-- remove item from list
    linked_item_t *it;
    for (it = (linked_item_t *) list; it ; it = it->next){
        if (it->next == item){
            it->next =  item->next;
            return 0;
        }
    }
    return -1;
}

void linked_item_set_user(linked_item_t *item, void *user_data){
    item->next = (linked_item_t *) 0;
    item->user_data = user_data;
}

void * linked_item_get_user(linked_item_t *item) {
    return item->user_data;
}
