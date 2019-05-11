#ifndef __TRANSLATION_SERVICE__H
#define __TRANSLATION_SERVICE__H

#include "tasks/tasks_internal.h"
void call_translation_server(const char* body);

bool g_translation_service_status;

bool run_translation_service(void);

void handle_translation_cb(retro_task_t *task, void *task_data, void *user_data, const char *error);
#endif
