#ifndef DRIVER_MENU_BACKEND_H__
#define DRIVER_MENU_BACKEND_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct menu_file_list_cbs
{
   int (*action_deferred_push)(void *data, void *userdata, const char
         *path, const char *label, unsigned type);
   int (*action_ok)(const char *path, const char *label, unsigned type,
         size_t idx);
   int (*action_start)(unsigned type,  const char *label, unsigned action);
   int (*action_content_list_switch)(void *data, void *userdata, const char
         *path, const char *label, unsigned type);
   int (*action_toggle)(unsigned type, const char *label, unsigned action);
} menu_file_list_cbs_t;

typedef struct menu_ctx_driver_backend
{
   int      (*iterate)(unsigned);
   const char *ident;
} menu_ctx_driver_backend_t;

#ifdef __cplusplus
}
#endif

#endif
