#ifndef DRIVER_MENU_BACKEND_H__
#define DRIVER_MENU_BACKEND_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct menu_ctx_driver_backend
{
   int      (*iterate)(unsigned);
   unsigned (*type_is)(const char *, unsigned);
   void     (*setting_set_label)(char *, size_t, unsigned *,
         unsigned, const char *, const char *, unsigned);
   const char *ident;
} menu_ctx_driver_backend_t;

#ifdef __cplusplus
}
#endif

#endif
