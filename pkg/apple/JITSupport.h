//
//  JITSupport.h
//  RetroArchiOS
//
//  Created by Yoshi Sugawara on 9/25/21.
//  Copyright © 2021 RetroArch. All rights reserved.
//

#ifndef JITSupport_h
#define JITSupport_h

#include <stdbool.h>
#include <stddef.h>

bool jit_available(void);
bool jit_possible(void);
#if !TARGET_OS_TV
bool jb_enable_ptrace_hack(void);
#endif
bool exec_mem_pool_init(void);
void exec_mem_pool_reset(void);
bool exec_mem_alloc(size_t *size, unsigned *mode, void **rx, void **rw);
void exec_mem_free(void *rx, void *rw, size_t size, bool dual);

#endif /* JITSupport_h */
