#ifndef __WEBHOOKS_MACRO_MANAGER_H
#define __WEBHOOKS_MACRO_MANAGER_H

typedef void (*on_macro_downloaded_t)
(
  wb_locals_t* locals,
  const char* macro,
  size_t length
);

void wmm_download_macro
(
  wb_locals_t* locals,
  on_macro_downloaded_t on_macro_downloaded
);

#endif //__WEBHOOKS_MACRO_MANAGER_H
