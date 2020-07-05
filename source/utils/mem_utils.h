/*
 * This file is part of vitaGL
 * Copyright 2017, 2018, 2019, 2020 Rinnegatamante
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published
 * by the Free Software Foundation, version 3 of the License, or (at your
 * option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

/* 
 * mem_utils.h:
 * Header file for the memory management utilities exposed by mem_utils.c
 */

#ifndef _MEM_UTILS_H_
#define _MEM_UTILS_H_

int mem_init(size_t size_ram, size_t size_cdram, size_t size_phycont); // Initialize mempools
void mem_term(void); // Terminate both CDRAM and RAM mempools
size_t mempool_get_free_space(vglMemType type); // Return free space in bytes for a mempool
void *mempool_alloc(size_t size, vglMemType type); // Allocate a memory block on a mempool
void mempool_free(void *ptr, vglMemType type); // Free a memory block on a mempool

#endif