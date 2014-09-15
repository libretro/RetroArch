#ifndef DRIVER_MENU_DISPLAY_H__
#define DRIVER_MENU_DISPLAY_H__

#ifdef __cplusplus
extern "C" {
#endif

typedef struct menu_ctx_driver
{
   void  (*set_texture)(void*);
   void  (*render_messagebox)(const char*);
   void  (*render)(void);
   void  (*frame)(void);
   void* (*init)(void);
   void  (*free)(void*);
   void  (*context_reset)(void*);
   void  (*context_destroy)(void*);
   void  (*populate_entries)(void*, const char *, const char *,
         unsigned);
   void  (*iterate)(void*, unsigned);
   int   (*input_postprocess)(uint64_t);
   void  (*navigation_clear)(void *);
   void  (*navigation_decrement)(void *);
   void  (*navigation_increment)(void *);
   void  (*navigation_set)(void *);
   void  (*navigation_set_last)(void *);
   void  (*navigation_descend_alphabet)(void *, size_t *);
   void  (*navigation_ascend_alphabet)(void *, size_t *);
   void  (*list_insert)(void *, const char *, const char *, size_t);
   void  (*list_delete)(void *, size_t);
   void  (*list_clear)(void *);
   void  (*list_set_selection)(void *);
   void  (*init_core_info)(void *);

   const menu_ctx_driver_backend_t *backend;
   const char *ident;
} menu_ctx_driver_t;

#ifdef __cplusplus
}
#endif

#endif
