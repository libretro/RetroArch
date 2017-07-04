#ifndef DEBUGGER_PLUGINMAN_H
#define DEBUGGER_PLUGINMAN_H

#include "plugin.h"

extern const debugger_t s_debugger;

void debugger_pluginman_init();
void debugger_pluginman_deinit();
void debugger_pluginman_draw();

#endif /* DEBUGGER_PLUGINMAN_H */
