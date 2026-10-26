/****************************************************************************
 * Copyright (C) 2015 Dimok
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 ****************************************************************************/
#ifndef __MEMORY_H_
#define __MEMORY_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <malloc.h>

void memoryInitialize(void);
void memoryRelease(void);

void * MEM2_alloc(unsigned int size, unsigned int align);
void MEM2_free(void *ptr);

void * MEM1_alloc(unsigned int size, unsigned int align);
void MEM1_free(void *ptr);

void * MEMBucket_alloc(unsigned int size, unsigned int align);
void MEMBucket_free(void *ptr);

#ifdef __cplusplus
}
#endif

#endif /* __MEMORY_H_ */
