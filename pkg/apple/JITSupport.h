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

static bool jb_has_debugger_attached(void);
bool jb_enable_ptrace_hack(void);

#endif /* JITSupport_h */
