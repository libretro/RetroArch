/* 
 * mem_utils.h:
 * Header file for the memory management utilities exposed by mem_utils.c
 */

#ifndef _MEM_UTILS_H_
#define _MEM_UTILS_H_

int vitagl_mem_init(size_t size_ram, size_t size_cdram, size_t size_phycont); // Initialize mempools
void vitagl_mem_term(void); // Terminate both CDRAM and RAM mempools
size_t vitagl_mempool_get_free_space(vglMemType type); // Return free space in bytes for a mempool
void *vitagl_mempool_alloc(size_t size, vglMemType type); // Allocate a memory block on a mempool
void vitagl_mempool_free(void *ptr, vglMemType type); // Free a memory block on a mempool

#endif