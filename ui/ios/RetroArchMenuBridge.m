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

static NSString *ios_menu_tab_title(enum ios_menu_tab tab)
{
   switch (*msg_hash_get_uint(MSG_HASH_USER_LANGUAGE))
   {
      case RETRO_LANGUAGE_ENGLISH:
         return tab == IOS_MENU_TAB_LIBRARY ? @"Game Library" : @"Main Menu";
      case RETRO_LANGUAGE_CHINESE_SIMPLIFIED:
         return tab == IOS_MENU_TAB_LIBRARY ? @"游戏库" : @"主菜单";
      case RETRO_LANGUAGE_CHINESE_TRADITIONAL:
         return tab == IOS_MENU_TAB_LIBRARY ? @"遊戲庫" : @"主選單";
      default:
         return ios_menu_string(msg_hash_to_str(tab == IOS_MENU_TAB_LIBRARY
               ? MENU_ENUM_LABEL_VALUE_PLAYLISTS_TAB : MENU_ENUM_LABEL_VALUE_MAIN_MENU));
   }
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
      if (visible)
      {
         UIView *parent = ios_menu_controller.parentViewController.view;
         UIWindow *window = parent.window;
         if (window)
         {
            /* Navigation must remain outside the game's cropped viewport. */
            CGRect frame = window.bounds;
            UIEdgeInsets extra = ios_menu_controller.additionalSafeAreaInsets;
            UIEdgeInsets current = ios_menu_controller.view.safeAreaInsets;
            UIEdgeInsets target = window.safeAreaInsets;
            if (ios_menu_controller.view.superview != window)
               [window addSubview:ios_menu_controller.view];
            if (!CGRectEqualToRect(ios_menu_controller.view.frame, frame))
               ios_menu_controller.view.frame = frame;
            target.top = MAX(0, target.top - current.top + extra.top);
            target.left = MAX(0, target.left - current.left + extra.left);
            target.bottom = MAX(0, target.bottom - current.bottom + extra.bottom);
            target.right = MAX(0, target.right - current.right + extra.right);
            if (!UIEdgeInsetsEqualToEdgeInsets(extra, target))
               ios_menu_controller.additionalSafeAreaInsets = target;
         }
      }
      if (ios_menu_controller.view.hidden == visible)
      {
         ios_menu_controller.view.hidden = !visible;
         cocoa_input_touches_reset();
         if (visible)
            [ios_menu_controller.view.superview
               bringSubviewToFront:ios_menu_controller.view];
      }
      return true;
   }
   return false;
}

void ios_menu_bridge_begin(uint64_t session, uint64_t revision,
      const char *title, size_t total, size_t offset, size_t depth,
      unsigned tab, unsigned tabs, bool running)
{
   RARCH_RELEASE(ios_menu_snapshot);
   ios_menu_snapshot = [[NSMutableDictionary alloc] initWithDictionary:@{
      @"session": @(session), @"revision": @(revision),
      @"title": ios_menu_string(title), @"total": @(total),
      @"offset": @(offset), @"depth": @(depth), @"running": @(running),
      @"tab": @(tab), @"tabs": @(tabs)
   }];
   ios_menu_snapshot[@"rows"] = [NSMutableArray array];
   ios_menu_snapshot[@"labels"] = @{
      @"settings": ios_menu_string(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SETTINGS_TAB)),
      @"library": ios_menu_tab_title(IOS_MENU_TAB_LIBRARY),
      @"resume": ios_menu_string(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_RESUME_CONTENT)),
      @"home": ios_menu_tab_title(IOS_MENU_TAB_MAIN),
      @"back": ios_menu_string(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_BACK)),
      @"confirm": ios_menu_string(msg_hash_to_str(MENU_ENUM_LABEL_VALUE_BASIC_MENU_CONTROLS_CONFIRM))
   };
}

void ios_menu_bridge_add(size_t index, unsigned type,
      const char *title, const char *value, const char *sublabel,
      bool enabled, bool boolean, bool checked, bool submenu,
      const ios_menu_value_t *input)
{
   [(NSMutableArray*)ios_menu_snapshot[@"rows"] addObject:@{
      @"index": @(index), @"type": @(type),
      @"title": ios_menu_string(title), @"value": ios_menu_string(value),
      @"sublabel": ios_menu_string(sublabel), @"enabled": @(enabled),
      @"boolean": @(boolean), @"checked": @(checked),
      @"submenu": @(submenu), @"inputKind": @(input->kind),
      @"inputValue": ios_menu_string(input->text), @"inputLimit": @(input->limit),
      @"minimum": input->has_minimum ? @(input->minimum) : [NSNull null],
      @"maximum": input->has_maximum ? @(input->maximum) : [NSNull null]
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
