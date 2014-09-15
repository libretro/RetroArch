#ifndef DRIVER_MENU_BACKEND_H__
#define DRIVER_MENU_BACKEND_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct menu_ctx_driver_backend
{
   int      (*iterate)(unsigned);
   void     (*shader_manager_init)(menu_handle_t *);
   void     (*shader_manager_get_str)(struct gfx_shader *, char *,
         size_t, const char *, const char *, unsigned);
   void     (*shader_manager_set_preset)(struct gfx_shader *,
         unsigned, const char*);
   void     (*shader_manager_save_preset)(const char *, bool);
   unsigned (*shader_manager_get_type)(const struct gfx_shader *);
   int      (*shader_manager_setting_toggle)(unsigned, const char *, unsigned);
   unsigned (*type_is)(const char *, unsigned);
   void     (*setting_set_label)(char *, size_t, unsigned *,
         unsigned, const char *, const char *, unsigned);
   const char *ident;
} menu_ctx_driver_backend_t;

#ifdef __cplusplus
}
#endif

#endif
