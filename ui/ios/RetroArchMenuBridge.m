/* RetroArch - A frontend for libretro.
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#import "RetroArchMenuBridge.h"
#import "../drivers/cocoa/cocoa_common.h"
#include "../../menu/menu_driver.h"
#include "../../input/drivers/cocoa_input.h"
#import "RetroArch-Swift.h"

static RetroArchNativeMenuController *ios_menu_controller API_AVAILABLE(ios(16.0));
static NSMutableDictionary *ios_menu_snapshot;

static NSString *ios_menu_string(const char *value)
{
   return value ? ([NSString stringWithUTF8String:value] ?: @"") : @"";
}

bool ios_menu_bridge_available(void)
{
   if (@available(iOS 16.0, *))
      return true;
   return false;
}

bool ios_menu_bridge_init(uint64_t session)
{
   if (![NSThread isMainThread])
      return false;
   if (@available(iOS 16.0, *))
   {
      UIViewController *parent = [CocoaView get];
      if (!parent || !parent.view)
         return false;
      ios_menu_controller = [[RetroArchNativeMenuController alloc] initWithSession:session];
      if (!ios_menu_controller)
         return false;
      [parent addChildViewController:ios_menu_controller];
      ios_menu_controller.view.frame = parent.view.bounds;
      ios_menu_controller.view.autoresizingMask =
         UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight;
      ios_menu_controller.view.hidden = YES;
      [parent.view addSubview:ios_menu_controller.view];
      [ios_menu_controller didMoveToParentViewController:parent];
      return true;
   }
   return false;
}

void ios_menu_bridge_free(void)
{
   if (@available(iOS 16.0, *))
   {
      [ios_menu_controller willMoveToParentViewController:nil];
      [ios_menu_controller.view removeFromSuperview];
      [ios_menu_controller removeFromParentViewController];
      RARCH_RELEASE(ios_menu_controller);
      ios_menu_controller = nil;
   }
   RARCH_RELEASE(ios_menu_snapshot);
   ios_menu_snapshot = nil;
}

bool ios_menu_bridge_visible(bool visible)
{
   if (@available(iOS 16.0, *))
   {
      if (!ios_menu_controller || !ios_menu_controller.parentViewController)
         return false;
      if (ios_menu_controller.view.hidden == visible)
      {
         ios_menu_controller.view.hidden = !visible;
         cocoa_input_touches_reset();
         if (visible)
            [ios_menu_controller.parentViewController.view
               bringSubviewToFront:ios_menu_controller.view];
      }
      return true;
   }
   return false;
}

void ios_menu_bridge_begin(uint64_t session, uint64_t revision,
      const char *title, size_t total, size_t offset, bool can_go_back,
      const char *driver, bool pointer, unsigned tab, unsigned tabs)
{
   NSMutableArray *drivers = [NSMutableArray array];
   size_t i;
   RARCH_RELEASE(ios_menu_snapshot);
   ios_menu_snapshot = [[NSMutableDictionary alloc] initWithDictionary:@{
      @"session": @(session), @"revision": @(revision),
      @"title": ios_menu_string(title), @"total": @(total),
      @"offset": @(offset), @"back": @(can_go_back),
      @"driver": ios_menu_string(driver), @"pointer": @(pointer),
      @"tab": @(tab), @"tabs": @(tabs)
   }];
   ios_menu_snapshot[@"rows"] = [NSMutableArray array];
   ios_menu_snapshot[@"labels"] = @{
      @"settings": ios_menu_string(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS_TAB)),
      @"library": ios_menu_string(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB)),
      @"history": ios_menu_string(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_HISTORY_TAB)),
      @"favorites": ios_menu_string(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_FAVORITES_TAB)),
      @"quick": ios_menu_string(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_CONTENT_SETTINGS)),
      @"home": ios_menu_string(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MAIN_MENU)),
      @"driver": ios_menu_string(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_MENU_DRIVER)),
      @"touch": ios_menu_string(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_POINTER_ENABLE)),
      @"save": ios_menu_string(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SAVE_CURRENT_CONFIG)),
      @"back": ios_menu_string(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK)),
      @"confirm": ios_menu_string(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM))
   };
   for (i = 0; menu_ctx_drivers[i]; i++)
      if (strcmp(menu_ctx_drivers[i]->ident, "null"))
         [drivers addObject:@{ @"index": @(i),
            @"name": ios_menu_string(menu_ctx_drivers[i]->ident) }];
   ios_menu_snapshot[@"drivers"] = drivers;
}

void ios_menu_bridge_add(size_t index, unsigned type,
      const char *title, const char *value, const char *sublabel,
      bool enabled, bool boolean, bool checked, bool adjustable)
{
   [(NSMutableArray*)ios_menu_snapshot[@"rows"] addObject:@{
      @"index": @(index), @"type": @(type),
      @"title": ios_menu_string(title), @"value": ios_menu_string(value),
      @"sublabel": ios_menu_string(sublabel), @"enabled": @(enabled),
      @"boolean": @(boolean), @"checked": @(checked),
      @"adjustable": @(adjustable)
   }];
}

void ios_menu_bridge_commit(void)
{
   if (@available(iOS 16.0, *))
      [ios_menu_controller publish:ios_menu_snapshot];
}

void ios_menu_bridge_status(size_t selection, bool busy, const char *dialog)
{
   if (@available(iOS 16.0, *))
      [ios_menu_controller updateSelection:selection busy:busy dialog:ios_menu_string(dialog)];
}

void ios_menu_bridge_message(const char *message)
{
   /* Task completions are delivered by the main run loop. */
   if (@available(iOS 16.0, *))
      if ([NSThread isMainThread])
         [ios_menu_controller showMessage:ios_menu_string(message)];
}

void ios_menu_bridge_close_settings(void)
{
   if (@available(iOS 16.0, *))
      [ios_menu_controller closeSettings];
}
