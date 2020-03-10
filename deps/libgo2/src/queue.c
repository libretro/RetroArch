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

#include "queue.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>


typedef struct go2_queue
{
    int capacity;
    int count;
    void** data;
} go2_queue_t;



go2_queue_t* go2_queue_create(int capacity)
{
    go2_queue_t* result = malloc(sizeof(*result));
    if (!result)
    {
        printf("malloc failed.\n");
        return NULL;
    }

    memset(result, 0, sizeof(*result));


    result->capacity = capacity;
    result->data = malloc(capacity * sizeof(void*));
    if (!result)
    {
        printf("data malloc failed.\n");
        free(result);
        return NULL;
    }

    return result;
}

int go2_queue_count_get(go2_queue_t* queue)
{
    return queue->count;
}

void go2_queue_push(go2_queue_t* queue, void* value)
{
    if (queue->count < queue->capacity)
    {
        queue->data[queue->count] = value;
        queue->count++;
    }
    else
    {
        printf("queue is full.\n");
    }    
}

void* go2_queue_pop(go2_queue_t* queue)
{
    void* result;

    if (queue->count > 0)
    {
        result = queue->data[0];

        for (int i = 0; i < queue->count - 1; ++i)
        {
            queue->data[i] = queue->data[i + 1];
        }

        queue->count--;
    }
    else
    {
        printf("queue is empty.\n");
        result = NULL;
    }
    
    return result;
}

void go2_queue_destroy(go2_queue_t* queue)
{
    free(queue->data);
    free(queue);
}
