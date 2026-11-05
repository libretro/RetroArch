#ifndef RETROARCH_IOS_MENU_BRIDGE_H
#define RETROARCH_IOS_MENU_BRIDGE_H

#include <stddef.h>
#include <stdint.h>
#include <boolean.h>
#include <retro_common_api.h>

RETRO_BEGIN_DECLS

enum ios_menu_action
{
   IOS_MENU_OK = 0,
   IOS_MENU_LEFT,
   IOS_MENU_RIGHT,
   IOS_MENU_BACK,
   IOS_MENU_RESUME,
   IOS_MENU_PREVIOUS_PAGE,
   IOS_MENU_NEXT_PAGE,
   IOS_MENU_CONFIRM,
   IOS_MENU_TAB
};

enum ios_menu_tab
{
   IOS_MENU_TAB_MAIN = 0,
   IOS_MENU_TAB_LIBRARY,
   IOS_MENU_TAB_SETTINGS
};

/* Main thread only. Requests are consumed by menu iteration. */
bool ios_menu_request(uint64_t session, uint64_t revision,
      size_t index, unsigned type, unsigned action);
void ios_menu_update(void);
unsigned ios_menu_take_action(unsigned action);
bool ios_menu_visible(void);

bool ios_menu_bridge_available(void);
bool ios_menu_bridge_init(uint64_t session);
void ios_menu_bridge_free(void);
bool ios_menu_bridge_visible(bool visible);
void ios_menu_bridge_begin(uint64_t session, uint64_t revision,
      const char *title, size_t total, size_t offset, size_t depth,
      unsigned tab, unsigned tabs, bool running);
void ios_menu_bridge_add(size_t index, unsigned type,
      const char *title, const char *value, const char *sublabel,
      bool enabled, bool boolean, bool checked, bool adjustable);
void ios_menu_bridge_commit(void);
void ios_menu_bridge_status(size_t selection, bool busy, const char *dialog);
void ios_menu_bridge_message(const char *message);

RETRO_END_DECLS

#endif
