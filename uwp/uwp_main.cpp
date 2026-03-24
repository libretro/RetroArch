/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018 - Krzysztof Haładyn
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.Devices.Enumeration.h>
#include <winrt/Windows.Devices.Input.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Graphics.Display.h>
#include <winrt/Windows.Graphics.Display.Core.h>
#include <winrt/Windows.Storage.h>
#include <winrt/Windows.System.h>
#include <winrt/Windows.System.Profile.h>
#include <winrt/Windows.System.UserProfile.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.UI.ViewManagement.h>

#include <windows.h>
#include <gamingdeviceinformation.h>

#include <boolean.h>
#include <encodings/utf.h>
#include <string/stdstring.h>
#include <lists/string_list.h>
#include <queues/task_queue.h>
#include <retro_timers.h>
#include <sstream>
#include <iomanip>

#include "configuration.h"
#include "paths.h"

#include "uwp_main.h"
#include "../retroarch.h"
#include "../frontend/frontend.h"
#include "../input/input_keymaps.h"
#include "../verbosity.h"
#include "uwp_func.h"
#include "uwp_async.h"

using namespace RetroArchUWP;

using namespace winrt;
using namespace winrt::Windows::ApplicationModel;
using namespace winrt::Windows::ApplicationModel::Core;
using namespace winrt::Windows::ApplicationModel::Activation;
using namespace winrt::Windows::UI::Core;
using namespace winrt::Windows::UI::Input;
using namespace winrt::Windows::UI::ViewManagement;
using namespace winrt::Windows::Devices::Input;
using namespace winrt::Windows::System;
using namespace winrt::Windows::System::Profile;
using namespace winrt::Windows::Foundation;
using namespace winrt::Windows::Foundation::Collections;
using namespace winrt::Windows::Graphics::Display;
using namespace winrt::Windows::Devices::Enumeration;
using namespace winrt::Windows::Storage;

char uwp_dir_install[DIR_MAX_LENGTH]  = { 0 };
char uwp_dir_data[DIR_MAX_LENGTH]     = { 0 };
char uwp_device_family[128]           = { 0 };
char win32_cpu_model_name[128]        = { 0 };

/* Some keys are unavailable in the VirtualKey enum (wtf) but the old-style
 * constants work */
const struct rarch_key_map rarch_key_map_uwp[] = {
   { (unsigned int)VirtualKey::Back, RETROK_BACKSPACE },
   { (unsigned int)VirtualKey::Tab, RETROK_TAB },
   { (unsigned int)VirtualKey::Clear, RETROK_CLEAR },
   { (unsigned int)VirtualKey::Enter, RETROK_RETURN },
   { (unsigned int)VirtualKey::Pause, RETROK_PAUSE },
   { (unsigned int)VirtualKey::Escape, RETROK_ESCAPE },
   { (unsigned int)VirtualKey::ModeChange, RETROK_MODE },
   { (unsigned int)VirtualKey::Space, RETROK_SPACE },
   { (unsigned int)VirtualKey::PageUp, RETROK_PAGEUP },
   { (unsigned int)VirtualKey::PageDown, RETROK_PAGEDOWN },
   { (unsigned int)VirtualKey::End, RETROK_END },
   { (unsigned int)VirtualKey::Home, RETROK_HOME },
   { (unsigned int)VirtualKey::Left, RETROK_LEFT },
   { (unsigned int)VirtualKey::Up, RETROK_UP },
   { (unsigned int)VirtualKey::Right, RETROK_RIGHT },
   { (unsigned int)VirtualKey::Down, RETROK_DOWN },
   { (unsigned int)VirtualKey::Print, RETROK_PRINT },
   { (unsigned int)VirtualKey::Insert, RETROK_INSERT },
   { (unsigned int)VirtualKey::Delete, RETROK_DELETE },
   { (unsigned int)VirtualKey::Help, RETROK_HELP },
   { (unsigned int)VirtualKey::Number0, RETROK_0 },
   { (unsigned int)VirtualKey::Number1, RETROK_1 },
   { (unsigned int)VirtualKey::Number2, RETROK_2 },
   { (unsigned int)VirtualKey::Number3, RETROK_3 },
   { (unsigned int)VirtualKey::Number4, RETROK_4 },
   { (unsigned int)VirtualKey::Number5, RETROK_5 },
   { (unsigned int)VirtualKey::Number6, RETROK_6 },
   { (unsigned int)VirtualKey::Number7, RETROK_7 },
   { (unsigned int)VirtualKey::Number8, RETROK_8 },
   { (unsigned int)VirtualKey::Number9, RETROK_9 },
   { (unsigned int)VirtualKey::A, RETROK_a },
   { (unsigned int)VirtualKey::B, RETROK_b },
   { (unsigned int)VirtualKey::C, RETROK_c },
   { (unsigned int)VirtualKey::D, RETROK_d },
   { (unsigned int)VirtualKey::E, RETROK_e },
   { (unsigned int)VirtualKey::F, RETROK_f },
   { (unsigned int)VirtualKey::G, RETROK_g },
   { (unsigned int)VirtualKey::H, RETROK_h },
   { (unsigned int)VirtualKey::I, RETROK_i },
   { (unsigned int)VirtualKey::J, RETROK_j },
   { (unsigned int)VirtualKey::K, RETROK_k },
   { (unsigned int)VirtualKey::L, RETROK_l },
   { (unsigned int)VirtualKey::M, RETROK_m },
   { (unsigned int)VirtualKey::N, RETROK_n },
   { (unsigned int)VirtualKey::O, RETROK_o },
   { (unsigned int)VirtualKey::P, RETROK_p },
   { (unsigned int)VirtualKey::Q, RETROK_q },
   { (unsigned int)VirtualKey::R, RETROK_r },
   { (unsigned int)VirtualKey::S, RETROK_s },
   { (unsigned int)VirtualKey::T, RETROK_t },
   { (unsigned int)VirtualKey::U, RETROK_u },
   { (unsigned int)VirtualKey::V, RETROK_v },
   { (unsigned int)VirtualKey::W, RETROK_w },
   { (unsigned int)VirtualKey::X, RETROK_x },
   { (unsigned int)VirtualKey::Y, RETROK_y },
   { (unsigned int)VirtualKey::Z, RETROK_z },
   { (unsigned int)VirtualKey::LeftWindows, RETROK_LSUPER },
   { (unsigned int)VirtualKey::RightWindows, RETROK_RSUPER },
   { (unsigned int)VirtualKey::Application, RETROK_MENU },
   { (unsigned int)VirtualKey::NumberPad0, RETROK_KP0 },
   { (unsigned int)VirtualKey::NumberPad1, RETROK_KP1 },
   { (unsigned int)VirtualKey::NumberPad2, RETROK_KP2 },
   { (unsigned int)VirtualKey::NumberPad3, RETROK_KP3 },
   { (unsigned int)VirtualKey::NumberPad4, RETROK_KP4 },
   { (unsigned int)VirtualKey::NumberPad5, RETROK_KP5 },
   { (unsigned int)VirtualKey::NumberPad6, RETROK_KP6 },
   { (unsigned int)VirtualKey::NumberPad7, RETROK_KP7 },
   { (unsigned int)VirtualKey::NumberPad8, RETROK_KP8 },
   { (unsigned int)VirtualKey::NumberPad9, RETROK_KP9 },
   { (unsigned int)VirtualKey::Multiply, RETROK_KP_MULTIPLY },
   { (unsigned int)VirtualKey::Add, RETROK_KP_PLUS },
   { (unsigned int)VirtualKey::Subtract, RETROK_KP_MINUS },
   { (unsigned int)VirtualKey::Decimal, RETROK_KP_PERIOD },
   { (unsigned int)VirtualKey::Divide, RETROK_KP_DIVIDE },
   { (unsigned int)VirtualKey::F1, RETROK_F1 },
   { (unsigned int)VirtualKey::F2, RETROK_F2 },
   { (unsigned int)VirtualKey::F3, RETROK_F3 },
   { (unsigned int)VirtualKey::F4, RETROK_F4 },
   { (unsigned int)VirtualKey::F5, RETROK_F5 },
   { (unsigned int)VirtualKey::F6, RETROK_F6 },
   { (unsigned int)VirtualKey::F7, RETROK_F7 },
   { (unsigned int)VirtualKey::F8, RETROK_F8 },
   { (unsigned int)VirtualKey::F9, RETROK_F9 },
   { (unsigned int)VirtualKey::F10, RETROK_F10 },
   { (unsigned int)VirtualKey::F11, RETROK_F11 },
   { (unsigned int)VirtualKey::F12, RETROK_F12 },
   { (unsigned int)VirtualKey::F13, RETROK_F13 },
   { (unsigned int)VirtualKey::F14, RETROK_F14 },
   { (unsigned int)VirtualKey::F15, RETROK_F15 },
   { (unsigned int)VirtualKey::NumberKeyLock, RETROK_NUMLOCK },
   { (unsigned int)VirtualKey::Scroll, RETROK_SCROLLOCK },
   { (unsigned int)VirtualKey::LeftShift, RETROK_LSHIFT },
   { (unsigned int)VirtualKey::RightShift, RETROK_RSHIFT },
   { (unsigned int)VirtualKey::LeftControl, RETROK_LCTRL },
   { (unsigned int)VirtualKey::RightControl, RETROK_RCTRL },
   { (unsigned int)VirtualKey::LeftMenu, RETROK_LALT },
   { (unsigned int)VirtualKey::RightMenu, RETROK_RALT },
   { (unsigned int)VirtualKey::CapitalLock, RETROK_CAPSLOCK },
   { VK_OEM_1, RETROK_SEMICOLON },
   { VK_OEM_PLUS, RETROK_EQUALS },
   { VK_OEM_COMMA, RETROK_COMMA },
   { VK_OEM_MINUS, RETROK_MINUS },
   { VK_OEM_PERIOD, RETROK_PERIOD },
   { VK_OEM_2, RETROK_SLASH },
   { VK_OEM_3, RETROK_BACKQUOTE },
   { VK_OEM_4, RETROK_LEFTBRACKET },
   { VK_OEM_5, RETROK_BACKSLASH },
   { VK_OEM_6, RETROK_RIGHTBRACKET },
   { VK_OEM_7, RETROK_QUOTE },
   { 0, RETROK_UNKNOWN }
};

#define MAX_TOUCH 16
struct input_pointer
{
   int id;
   int16_t x;
   int16_t y;
   int16_t full_x;
   int16_t full_y;
   bool isInContact;
};

struct uwp_input_state_t
{
   struct input_pointer touch[MAX_TOUCH]; /* int alignment */
   unsigned touch_count;
   int16_t mouse_screen_x;
   int16_t mouse_screen_y;
   int16_t mouse_rel_x;
   int16_t mouse_rel_y;
   int16_t mouse_wheel_left;
   int16_t mouse_wheel_up;
   bool mouse_left;
   bool mouse_right;
   bool mouse_middle;
   bool mouse_button4;
   bool mouse_button5;
};

struct uwp_input_state_t uwp_current_input, uwp_next_input;

/* Taken from DirectX UWP samples - on Xbox, everything is scaled 200%,
 * so getting the DPI calculation correct is crucial */
static inline float ConvertDipsToPixels(float dips, float dpi)
{
   static const float dips_per_inch = 96.0f;
   return floorf(dips * dpi / dips_per_inch + 0.5f);
}

/* The main function is only used to initialize our IFrameworkView class. */
int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int)
{
   winrt::init_apartment(winrt::apartment_type::multi_threaded);

   DWORD dwAttrib;
   char vfs_cache_dir[MAX_PATH];
   hstring local_folder = ApplicationData::Current().LocalFolder().Path();
   hstring install_dir  = Package::Current().InstalledLocation().Path() + L"\\";
   hstring data_dir     = local_folder + L"\\";

   /* Delete VFS cache dir, we do this because this allows a far more
    * concise implementation than manually implementing a function to do this
    * This may be a little slower but shouldn't really matter as the cache dir
    * should never have more than a few items */
   hstring vfs_dir     = local_folder + L"\\VFSCACHE";

   wcstombs(uwp_dir_install, install_dir.c_str(), sizeof(uwp_dir_install));
   wcstombs(uwp_dir_data,    data_dir.c_str(),    sizeof(uwp_dir_data));
   wcstombs(vfs_cache_dir,   vfs_dir.c_str(),     sizeof(vfs_cache_dir));

   dwAttrib = GetFileAttributesA(vfs_cache_dir);
   if ((dwAttrib != INVALID_FILE_ATTRIBUTES) && (dwAttrib & FILE_ATTRIBUTE_DIRECTORY))
   {
      try
      {
         auto vfsdir = StorageFolder::GetFolderFromPathAsync(vfs_dir).get();
         vfsdir.DeleteAsync().get();
      }
      catch (winrt::hresult_error const&)
      {
         /* Ignore errors when deleting VFS cache */
      }
   }

   wcstombs(uwp_device_family,
         AnalyticsInfo::VersionInfo().DeviceFamily().c_str(),
         sizeof(uwp_device_family));

   RARCH_LOG("[UWP] Data dir: %ls\n", data_dir.c_str());
   RARCH_LOG("[UWP] Install dir: %ls\n", install_dir.c_str());

   auto direct3DApplicationSource = winrt::make<Direct3DApplicationSource>();
   CoreApplication::Run(direct3DApplicationSource);
   return 0;
}

IFrameworkView Direct3DApplicationSource::CreateView()
{
   return winrt::make<App>();
}

App* App::m_instance = nullptr;

App::App() :
   m_initialized(false),
   m_windowClosed(false),
   m_windowVisible(true),
   m_windowFocused(true),
   m_windowResized(false),
   m_launchOnExitShutdown(false)
{
   m_instance = this;
}

/* The first method called when the IFrameworkView is being created. */
void App::Initialize(CoreApplicationView const& applicationView)
{
   /* Register event handlers for app lifecycle. This example
    * includes Activated, so that we can make the CoreWindow active and start
    * rendering on the window. */
   m_activatedToken = applicationView.Activated({ this, &App::OnActivated });
   m_suspendingToken = CoreApplication::Suspending({ this, &App::OnSuspending });
   m_resumingToken = CoreApplication::Resuming({ this, &App::OnResuming });
   m_enteredBackgroundToken = CoreApplication::EnteredBackground({ this, &App::OnEnteredBackground });
}

/* Called when the CoreWindow object is created (or re-created). */
void App::SetWindow(CoreWindow const& window)
{
   m_sizeChangedToken = window.SizeChanged({ this, &App::OnWindowSizeChanged });
   m_visibilityChangedToken = window.VisibilityChanged({ this, &App::OnVisibilityChanged });
   m_activatedWindowToken = window.Activated({ this, &App::OnWindowActivated });
   m_closedToken = window.Closed({ this, &App::OnWindowClosed });
   m_pointerPressedToken = window.PointerPressed({ this, &App::OnPointer });
   m_pointerReleasedToken = window.PointerReleased({ this, &App::OnPointer });
   m_pointerMovedToken = window.PointerMoved({ this, &App::OnPointer });
   m_pointerWheelChangedToken = window.PointerWheelChanged({ this, &App::OnPointer });
   m_acceleratorKeyToken = window.Dispatcher().AcceleratorKeyActivated({ this, &App::OnAcceleratorKey });

   DisplayInformation currentDisplayInformation = DisplayInformation::GetForCurrentView();

   m_dpiChangedToken = currentDisplayInformation.DpiChanged({ this, &App::OnDpiChanged });
   m_displayContentsInvalidatedToken = DisplayInformation::DisplayContentsInvalidated({ this, &App::OnDisplayContentsInvalidated });
   m_orientationChangedToken = currentDisplayInformation.OrientationChanged({ this, &App::OnOrientationChanged });

   m_backRequestedToken = SystemNavigationManager::GetForCurrentView().BackRequested({ this, &App::OnBackRequested });
}

/* Initializes scene resources, or loads a previously saved app state. */
void App::Load(hstring const& entryPoint)
{
   auto catalog = PackageCatalog::OpenForCurrentPackage();
   m_packageInstallingToken = catalog.PackageInstalling({ this, &App::OnPackageInstalling });
}

/* This method is called after the window becomes active. */
void App::Run()
{
   bool x = false;
   if (!m_initialized)
   {
      RARCH_WARN("[UWP] Initialization failed, so not running.\n");
      return;
   }

   for (;;)
   {
      int ret;
      CoreWindow::GetForCurrentThread().Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);

      ret = runloop_iterate();

      task_queue_check();

      if (!x)
      {
         /* HACK: I have no idea why is this necessary but
          * it is required to get proper scaling on Xbox *
          * Perhaps PreferredLaunchViewSize is broken and
          * we need to wait until the app starts to call TryResizeView */
         m_windowResized = true;
         x               = true;
      }

      if (ret == -1)
         break;
   }
}

/* Required for IFrameworkView.
 * Terminate events do not cause Uninitialize to be called.
 * It will be called if your IFrameworkView
 * class is torn down while the app is in the foreground. */
void App::Uninitialize()
{
   main_exit(NULL);

   /* If this instance of RetroArch was started from another app/frontend
    * and the frontend passed "launchOnExit" parameter:
    * 1. launch the app specified in "launchOnExit", most likely the
    *    same app that started RetroArch
    * 2. RetroArch goes to background and RunAsyncAndCatchErrors doesn't
    *    return, because the target app is immediately started.
    * 3. Explicitly exit in App::OnEnteredBackground if
    *    m_launchOnExitShutdown is set. Otherwise, RetroArch doesn't
    *    properly shutdown.
    */
   if (!m_launchOnExit.empty())
   {
      try
      {
         /* Launch the target app */
         m_launchOnExitShutdown = true;
         auto ret = RunAsyncAndCatchErrors<bool>([this]() {
               return Launcher::LaunchUriAsync(Uri(m_launchOnExit));
               }, false);
      }
      catch (winrt::hresult_error const&)
      {
      }
   }
}

/* Application lifecycle event handlers. */

void App::OnActivated(CoreApplicationView const& applicationView, IActivatedEventArgs const& args)
{
   int ret;
   int argc = 0;
   std::vector<char*> argv;
   /* using std::string as temp buf instead of char* array
    * to avoid manual char allocations */
   std::vector<std::string> argvTmp;
   ParseProtocolArgs(args, &argc, &argv, &argvTmp);

   /* Start only if not already initialized.
    * If there is a game in progress, just return */
   if (m_initialized)
      return;

   if ((ret = rarch_main(argc, argv.data(), NULL)) != 0)
   {
      RARCH_ERR("[UWP] Init failed.\n");
      CoreApplication::Exit();
      return;
   }
   m_initialized = true;

   if (is_running_on_xbox())
   {
      bool reset                 = false;
      int width                  = uwp_get_width();
      int height                 = uwp_get_height();
      /* Reset driver to D3D11 if set to OpenGL on boot as cores can
       * just set to 'gl' when needed and there is no good reason to
       * use 'gl' for the menus
       * Do not change the default driver if the content is already
       * initialized through arguments, as this would crash RA for
       * cores that use only ANGLE */
      settings_t *settings       = config_get_ptr();
      content_state_t *p_content = content_state_get_ptr();
      char *currentdriver        = settings->arrays.video_driver;
      if (     strcmpi(currentdriver, "gl") == 0
            && !p_content->flags & CONTENT_ST_FLAG_IS_INITED)
      {
         /* Set driver to default */
         configuration_set_string(settings,
               settings->arrays.video_driver,
               config_get_default_video());
         reset = true; /* Reset needed */
      }
      if (     (settings->uints.video_fullscreen_x != width)
            || (settings->uints.video_fullscreen_y != height))
      {
         /* Get width and height from display again */
         configuration_set_int(settings,
               settings->uints.video_fullscreen_x,
               width);
         configuration_set_int(settings,
               settings->uints.video_fullscreen_y,
               height);
         reset = true; /* Reset needed */
      }
      if (reset) /* Restart driver */
         command_event(CMD_EVENT_REINIT, NULL);
   }

   /* Run() won't start until the CoreWindow is activated. */
   CoreWindow::GetForCurrentThread().Activate();
}

void App::OnSuspending(IInspectable const& sender, SuspendingEventArgs const& args)
{
   /* This function will ensure that configs are saved in case the app
    * is sent to background or terminated for saving configs on quit
    * for saving configs on quit now configs will be saved in
    * `retroarch_main_quit` at `retroarch.c`
    * If this function is called because of app closed by quit,
    * the below code must be ignored

   /* Save app state asynchronously after requesting a deferral.
    * Holding a deferral indicates that the application is busy
    * performing suspending operations. Be aware that a deferral may
    * not be held indefinitely. After about five seconds, the app will
    * be forced to exit. */
   SuspendingDeferral deferral = args.SuspendingOperation().GetDeferral();

   std::thread([deferral]()
         {
         /* TODO: Maybe creating a save state here would be a good idea? */
         settings_t* settings     = config_get_ptr();
         bool config_save_on_exit = settings->bools.config_save_on_exit;

         if (config_save_on_exit)
         {
         if (!path_is_empty(RARCH_PATH_CONFIG))
         {
         const char* config_path = path_get(RARCH_PATH_CONFIG);
         bool path_exists        = !string_is_empty(config_path);

         if (path_exists)
         {
         if (config_save_file(config_path))
         {
         RARCH_LOG("[UWP] %s \"%s\".\n",
               msg_hash_to_str(MSG_SAVED_NEW_CONFIG_TO),
               config_path);
         }
         else
         {
            RARCH_ERR("[UWP] %s \"%s\".\n",
                  msg_hash_to_str(MSG_FAILED_SAVING_CONFIG_TO),
                  config_path);
         }
         }

         }
         }

         deferral.Complete();
         }).detach();
}

void App::OnResuming(IInspectable const& sender, IInspectable const& args)
{
   /* Restore any data or state that was unloaded on suspend. By default, data
    * and state are persisted when resuming from suspend. Note that this event
    * does not occur if the app was previously terminated.
    */
}

void App::OnEnteredBackground(IInspectable const& sender, EnteredBackgroundEventArgs const& args)
{
   /* RetroArch entered background because another app/frontend
    * was launched on exit, so properly quit */
   if (m_launchOnExitShutdown)
      CoreApplication::Exit();
}

void App::OnBackRequested(IInspectable const& sender, BackRequestedEventArgs const& args)
{
   /* Prevent the B controller button on Xbox One from quitting the app */
   args.Handled(true);
}

/* Window event handlers. */

void App::OnWindowSizeChanged(CoreWindow const& sender, WindowSizeChangedEventArgs const& args)
{
   m_windowResized = true;
}

void App::OnVisibilityChanged(CoreWindow const& sender, VisibilityChangedEventArgs const& args)
{
   m_windowVisible = args.Visible();
}

void App::OnWindowActivated(CoreWindow const& sender, WindowActivatedEventArgs const& args)
{
   m_windowFocused = args.WindowActivationState() != CoreWindowActivationState::Deactivated;
   if (!m_windowFocused)
   {
      /* When deactivating the window, send key up events for anything still held down */
      for (const rarch_key_map& it : rarch_key_map_uwp)
      {
         if ((sender.GetKeyState((VirtualKey)it.sym) & CoreVirtualKeyStates::Down) == CoreVirtualKeyStates::Down)
            input_keyboard_event(false, it.rk, 0, 0, RETRO_DEVICE_KEYBOARD);
      }
   }
}

void App::OnAcceleratorKey(CoreDispatcher const& sender, AcceleratorKeyEventArgs const& args)
{
   if (args.EventType() != CoreAcceleratorKeyEventType::KeyDown && args.EventType() != CoreAcceleratorKeyEventType::KeyUp &&
       args.EventType() != CoreAcceleratorKeyEventType::SystemKeyDown && args.EventType() != CoreAcceleratorKeyEventType::SystemKeyUp)
      return;

   /* Unlike CoreWindow::KeyDown/KeyUp events, this callback gets called for all keys including
    * F10, Alt and any keys pressed in addition to while Alt is being held down. */
   bool down = !args.KeyStatus().IsKeyReleased;
   bool extended = args.KeyStatus().IsExtendedKey;
   VirtualKey vkey = args.VirtualKey();
   CoreWindow window = CoreWindow::GetForCurrentThread();

   /* Some keys we need to specify via the extended flag */
   unsigned keycode = RETROK_UNKNOWN;
   if (vkey == VirtualKey::Enter)
      keycode = (extended ? RETROK_KP_ENTER : RETROK_RETURN);
   else if (vkey == VirtualKey::Control)
      keycode = (extended ? RETROK_RCTRL : RETROK_LCTRL);
   else if (vkey == VirtualKey::Menu)
      keycode = (extended ? RETROK_RALT : RETROK_LALT);
   else if (vkey == VirtualKey::Shift)
   {
      /* Shift is just sent generic, we need to query for and remember left or right */
      bool left_down = ((window.GetKeyState(VirtualKey::LeftShift) & CoreVirtualKeyStates::Down) == CoreVirtualKeyStates::Down);
      bool right_down = ((window.GetKeyState(VirtualKey::RightShift) & CoreVirtualKeyStates::Down) == CoreVirtualKeyStates::Down);
      static bool had_left_down, had_right_down;
      if (down != had_left_down && down == left_down)
      {
         had_left_down = down;
         keycode = RETROK_LSHIFT;
      }
      else if (down != had_right_down && down == right_down)
      {
         had_right_down = down;
         keycode = RETROK_RSHIFT;
      }
   }
   else
      keycode = input_keymaps_translate_keysym_to_rk((unsigned)vkey);

   if (keycode == RETROK_UNKNOWN)
      return;

   uint16_t mod = 0;
   if ((window.GetKeyState(VirtualKey::Shift) & CoreVirtualKeyStates::Down) == CoreVirtualKeyStates::Down)
      mod |= RETROKMOD_SHIFT;
   if ((window.GetKeyState(VirtualKey::Control) & CoreVirtualKeyStates::Down) == CoreVirtualKeyStates::Down)
      mod |= RETROKMOD_CTRL;
   if ((window.GetKeyState(VirtualKey::Menu) & CoreVirtualKeyStates::Down) == CoreVirtualKeyStates::Down)
      mod |= RETROKMOD_ALT;
   if ((window.GetKeyState(VirtualKey::CapitalLock) & CoreVirtualKeyStates::Locked) == CoreVirtualKeyStates::Locked)
      mod |= RETROKMOD_CAPSLOCK;
   if ((window.GetKeyState(VirtualKey::Scroll) & CoreVirtualKeyStates::Locked) == CoreVirtualKeyStates::Locked)
      mod |= RETROKMOD_SCROLLOCK;
   if ((window.GetKeyState(VirtualKey::NumberKeyLock) & CoreVirtualKeyStates::Locked) == CoreVirtualKeyStates::Locked)
      mod |= RETROKMOD_NUMLOCK;
   if ((window.GetKeyState(VirtualKey::LeftWindows) & CoreVirtualKeyStates::Down) == CoreVirtualKeyStates::Down ||
         (window.GetKeyState(VirtualKey::RightWindows) & CoreVirtualKeyStates::Down) == CoreVirtualKeyStates::Down)
      mod |= RETROKMOD_META;

   input_keyboard_event(down, keycode, 0, mod, RETRO_DEVICE_KEYBOARD);
}

void App::OnPointer(CoreWindow const& sender, PointerEventArgs const& args)
{
   float dpi = DisplayInformation::GetForCurrentView().LogicalDpi();

   if (args.CurrentPoint().PointerDevice().PointerDeviceType() == PointerDeviceType::Mouse)
   {
      uwp_next_input.mouse_left     = args.CurrentPoint().Properties().IsLeftButtonPressed();
      uwp_next_input.mouse_middle   = args.CurrentPoint().Properties().IsMiddleButtonPressed();
      uwp_next_input.mouse_right    = args.CurrentPoint().Properties().IsRightButtonPressed();
      uwp_next_input.mouse_button4  = args.CurrentPoint().Properties().IsXButton1Pressed();
      uwp_next_input.mouse_button5  = args.CurrentPoint().Properties().IsXButton2Pressed();
      uwp_next_input.mouse_screen_x = ConvertDipsToPixels(args.CurrentPoint().Position().X, dpi);
      uwp_next_input.mouse_screen_y = ConvertDipsToPixels(args.CurrentPoint().Position().Y, dpi);
      uwp_next_input.mouse_rel_x    = uwp_next_input.mouse_screen_x - uwp_current_input.mouse_screen_x;
      uwp_next_input.mouse_rel_y    = uwp_next_input.mouse_screen_y - uwp_current_input.mouse_screen_y;
      if (args.CurrentPoint().Properties().IsHorizontalMouseWheel())
         uwp_next_input.mouse_wheel_left += args.CurrentPoint().Properties().MouseWheelDelta();
      else
         uwp_next_input.mouse_wheel_up += args.CurrentPoint().Properties().MouseWheelDelta();
   }
   else
   {
      struct video_viewport vp;
      unsigned i, free_index = MAX_TOUCH; bool found = false;
      int id = args.CurrentPoint().PointerId();

      for (i = 0; i < uwp_next_input.touch_count; i++)
      {
         if (!uwp_next_input.touch[i].isInContact && free_index == MAX_TOUCH)
            free_index = i;
         if (uwp_next_input.touch[i].id == id)
         {
            found = true;
            break;
         }
      }

      if (!found)
      {
         if (free_index >= 0 && free_index < uwp_next_input.touch_count)
            i = free_index;
         else if (uwp_next_input.touch_count + 1 < MAX_TOUCH)
            i = ++uwp_next_input.touch_count;
         else
            return;
      }

      uwp_next_input.touch[i].id = id;

      /* convert from event coordinates to core and screen coordinates */
      vp.x           = 0;
      vp.y           = 0;
      vp.width       = 0;
      vp.height      = 0;
      vp.full_width  = 0;
      vp.full_height = 0;

      video_driver_translate_coord_viewport_wrap(
            &vp,
            ConvertDipsToPixels(args.CurrentPoint().Position().X, dpi),
            ConvertDipsToPixels(args.CurrentPoint().Position().Y, dpi),
            &uwp_next_input.touch[i].x,
            &uwp_next_input.touch[i].y,
            &uwp_next_input.touch[i].full_x,
            &uwp_next_input.touch[i].full_y);

      uwp_next_input.touch[i].isInContact = args.CurrentPoint().IsInContact();

   }
}

void App::OnWindowClosed(CoreWindow const& sender, CoreWindowEventArgs const& args)
{
   m_windowClosed = true;
}

/* DisplayInformation event handlers. */

void App::OnDpiChanged(DisplayInformation const& sender, IInspectable const& args)
{
   m_windowResized = true;
}

void App::OnOrientationChanged(DisplayInformation const& sender, IInspectable const& args)
{
   m_windowResized = true;
}

void App::OnDisplayContentsInvalidated(DisplayInformation const& sender, IInspectable const& args)
{
   /* Probably can be ignored? */
}

void App::OnPackageInstalling(PackageCatalog const& sender, PackageInstallingEventArgs const& args)
{
   /* TODO: This doesn't seem to work even though it's exactly the same as in sample app and it works there */
   if (args.IsComplete())
   {
      char msg[512];
      size_t _len = snprintf(msg, sizeof(msg),
            "Package \"%ls\" installed, a restart may be necessary",
            args.Package().DisplayName().c_str());
      runloop_msg_queue_push(msg, _len, 1, 5 * 60, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
}

void App::ParseProtocolArgs(IActivatedEventArgs const& args, int *argc, std::vector<char*> *argv, std::vector<std::string> *argvTmp)
{
   argvTmp->clear();
   argv->clear();

   /* If the app is activated using protocol,
    * it is expected to be in this format:
    * "retroarch:?cmd=<RetroArch CLI arguments>&launchOnExit=<app to launch on exit>"
    * For example:
    * retroarch:?cmd=retroarch -L cores\core_libretro.dll "c:\mypath\path with spaces\game.rom"&launchOnExit=LaunchApp:
    * "cmd" and "launchOnExit" are optional. If none specified,
    * it will normally launch into menu
    */
   if (args.Kind() == ActivationKind::Protocol)
   {
      unsigned i;
      ProtocolActivatedEventArgs protocolArgs = args.as<ProtocolActivatedEventArgs>();
      WwwFormUrlDecoder query = protocolArgs.Uri().QueryParsed();

      for (i = 0; i < query.Size(); i++)
      {
         IWwwFormUrlDecoderEntry arg = query.GetAt(i);

         /* Parse RetroArch command line string */
         /* This allows a frotend to quit RetroArch, which in turn allows it
          * to launch a different game. */
         if (arg.Name() == L"forceExit")
            CoreApplication::Exit();
         else if (arg.Name() == L"cmd" && !m_initialized)
         {
            std::wstring wsValue(arg.Value().c_str());
            std::string strValue(wsValue.begin(), wsValue.end());
            std::istringstream iss(strValue);
            std::string s;

            /* Set escape character to NULL char to preserve backslashes in
             * paths which are inside quotes, they get stripped by default */
            while (iss >> std::quoted(s, '"', (char)0))
               argvTmp->push_back(s);
         }
         /* If RetroArch UWP app is started using protocol
          * with argument "launchOnExit", this gives an option
          * to launch another app on RA exit,
          * making it easy to integrate RA with other UWP frontends */
         else if (arg.Name() == L"launchOnExit")
            m_launchOnExit = arg.Value();
      }
   }

   if (!m_initialized)
   {
      int i;
      (*argc) = argvTmp->size();
      /* Convert to char* array compatible with argv */
      for (i = 0; i < argvTmp->size(); i++)
         argv->push_back((char*)(argvTmp->at(i)).c_str());
      argv->push_back(nullptr);
   }
}

/* Implement UWP equivalents of various win32_* functions */
extern "C" {

   bool is_running_on_xbox(void)
   {
      hstring device_family = AnalyticsInfo::VersionInfo().DeviceFamily();
      return (device_family == L"Windows.Xbox");
   }

   bool win32_has_focus(void *data)
   {
      return App::GetInstance()->IsWindowFocused();
   }

   bool win32_set_video_mode(void *data, unsigned width, unsigned height, bool fullscreen)
   {
      if (App::GetInstance()->IsInitialized())
      {
         if (fullscreen !=
               ApplicationView::GetForCurrentView().IsFullScreenMode())
         {
            if (fullscreen)
               ApplicationView::GetForCurrentView().TryEnterFullScreenMode();
            else
               ApplicationView::GetForCurrentView().ExitFullScreenMode();
         }
         ApplicationView::GetForCurrentView().TryResizeView(winrt::Windows::Foundation::Size(static_cast<float>(width), static_cast<float>(height)));
      }
      else
      {
         /* In case the window is not activated yet,
          * TryResizeView will fail and we have to set the
          * initial parameters instead
          * Note that these are preserved after restarting the app
          * and used for the UWP splash screen size (!), so they
          * should be set only during init and not changed afterwards */
         ApplicationView::PreferredLaunchViewSize(winrt::Windows::Foundation::Size(static_cast<float>(width), static_cast<float>(height)));
         ApplicationView::PreferredLaunchWindowingMode(fullscreen ? ApplicationViewWindowingMode::FullScreen : ApplicationViewWindowingMode::PreferredLaunchViewSize);
      }

      /* Setting the window size may sometimes fail "because UWP"
       * (i.e. we are on device with no windows, or Windows sandbox decides the window can't be that small)
       * so in case resizing fails we just send a resized event back to RetroArch with old size
       * (and report success because otherwise it bails out hard about failing
       * to set video mode) */
      App::GetInstance()->SetWindowResized();
      return true;
   }

   void win32_show_cursor(void *data, bool state)
   {
      CoreWindow::GetForCurrentThread().PointerCursor(state ? CoreCursor(CoreCursorType::Arrow, 0) : nullptr);
   }

   bool win32_get_client_rect(RECT* rect)
   {
      auto bounds = ApplicationView::GetForCurrentView().VisibleBounds();
      rect->top    = static_cast<LONG>(bounds.Y);
      rect->left   = static_cast<LONG>(bounds.X);
      rect->bottom = static_cast<LONG>(bounds.Y + bounds.Height);
      rect->right  = static_cast<LONG>(bounds.X + bounds.Width);

      return true;
   }

   bool win32_get_metrics(void* data,
         enum display_metric_types type, float* value)
   {
      switch (type)
      {
         case DISPLAY_METRIC_PIXEL_WIDTH:
            *value                 = uwp_get_width();
            return true;
         case DISPLAY_METRIC_PIXEL_HEIGHT:
            *value				 = uwp_get_height();
            return true;
         case DISPLAY_METRIC_MM_WIDTH:
            /* 25.4 mm in an inch. */
            {
               int pixels_x        = DisplayInformation::GetForCurrentView().ScreenWidthInRawPixels();
               int raw_dpi_x       = DisplayInformation::GetForCurrentView().RawDpiX();
               int physical_width  = pixels_x / raw_dpi_x;
               *value              = 254 * physical_width / 10;
            }
            return true;
         case DISPLAY_METRIC_MM_HEIGHT:
            /* 25.4 mm in an inch. */
            {
               int pixels_y        = DisplayInformation::GetForCurrentView().ScreenHeightInRawPixels();
               int raw_dpi_y       = DisplayInformation::GetForCurrentView().RawDpiY();
               int physical_height = pixels_y / raw_dpi_y;
               *value              = 254 * physical_height / 10;
            }
            return true;
         case DISPLAY_METRIC_DPI:
            *value                 = DisplayInformation::GetForCurrentView().RawDpiX();
            return true;
         case DISPLAY_METRIC_NONE:
         default:
            *value                 = 0;
            break;
      }
      return false;
   }

   void win32_check_window(void *data,
         bool *quit, bool *resize, unsigned *width, unsigned *height)
   {
      static bool is_xbox     = is_running_on_xbox();
      *quit                   = App::GetInstance()->IsWindowClosed();
      if (is_xbox)
      {
         settings_t* settings = config_get_ptr();
         *width               = settings->uints.video_fullscreen_x  != 0 ? settings->uints.video_fullscreen_x : uwp_get_width();
         *height              = settings->uints.video_fullscreen_y  != 0 ? settings->uints.video_fullscreen_y : uwp_get_height();
         return;
      }

      *resize = App::GetInstance()->CheckWindowResized();
      if (*resize)
      {
         float dpi = DisplayInformation::GetForCurrentView().LogicalDpi();
         *width    = ConvertDipsToPixels(CoreWindow::GetForCurrentThread().Bounds().Width, dpi);
         *height   = ConvertDipsToPixels(CoreWindow::GetForCurrentThread().Bounds().Height, dpi);
      }
   }

   void* uwp_get_corewindow(void)
   {
      return winrt::get_abi(CoreWindow::GetForCurrentThread());
   }

   int current_height = -1;

   int uwp_get_height(void)
   {
       if (current_height != -1)
           return current_height;

      /* This function must be performed within UI thread,
       * otherwise it will cause a crash in specific cases
       * https://github.com/libretro/RetroArch/issues/13491 */
      float surface_scale    = 0;
      int ret                = -1;
      volatile bool finished = false;
      CoreApplication::MainView().CoreWindow().Dispatcher().RunAsync(
            CoreDispatcherPriority::Normal,
            DispatchedHandler([&surface_scale, &ret, &finished]()
               {
               if (is_running_on_xbox())
               {
               auto hdi = winrt::Windows::Graphics::Display::Core::HdmiDisplayInformation::GetForCurrentView();
               if (hdi)
               ret = hdi.GetCurrentDisplayMode().ResolutionHeightInRawPixels();
               }

               if (ret == -1)
               {
               const LONG32 resolution_scale = static_cast<LONG32>(DisplayInformation::GetForCurrentView().ResolutionScale());
               surface_scale                 = static_cast<float>(resolution_scale) / 100.0f;
               ret                           = static_cast<LONG32>(
                     CoreWindow::GetForCurrentThread().Bounds().Height
                     * surface_scale);
               }
               finished = true;
               }));
      auto corewindow = CoreWindow::GetForCurrentThread();
      while (!finished)
      {
         if (corewindow)
            corewindow.Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
      }
      current_height = ret;
      return ret;
   }

   int current_width = -1;

   int uwp_get_width(void)
   {
       if (current_width != -1)
           return current_width;

      /* This function must be performed within UI thread,
       * otherwise it will cause a crash in specific cases
       * https://github.com/libretro/RetroArch/issues/13491 */
      float surface_scale    = 0;
      int returnValue        = -1;
      volatile bool finished = false;
      CoreApplication::MainView().CoreWindow().Dispatcher().RunAsync(
            CoreDispatcherPriority::Normal,
            DispatchedHandler([&surface_scale, &returnValue, &finished]()
               {
               if (is_running_on_xbox())
               {
               auto hdi = winrt::Windows::Graphics::Display::Core::HdmiDisplayInformation::GetForCurrentView();
               if (hdi)
               returnValue = hdi.GetCurrentDisplayMode().ResolutionWidthInRawPixels();
               }

               if(returnValue == -1)
               {
               const LONG32 resolution_scale = static_cast<LONG32>(DisplayInformation::GetForCurrentView().ResolutionScale());
               surface_scale = static_cast<float>(resolution_scale) / 100.0f;
               returnValue   = static_cast<LONG32>(
                     CoreWindow::GetForCurrentThread().Bounds().Width
                     * surface_scale);
               }
               finished = true;
               }));
      auto corewindow = CoreWindow::GetForCurrentThread();
      while (!finished)
      {
         if (corewindow)
            corewindow.Dispatcher().ProcessEvents(CoreProcessEventsOption::ProcessAllIfPresent);
      }
      current_width = returnValue;
      return returnValue;
   }

   void uwp_fill_installed_core_packages(struct string_list *list)
   {
      for (auto package : Package::Current().Dependencies())
      {
         if (package.IsOptional())
         {
            string_list_elem_attr attr{};
            std::wstring path = std::wstring(package.InstalledLocation().Path().c_str()) + L"\\cores";
            string_list_append(list, utf16_to_utf8_string_alloc(path.c_str()), attr);
         }
      }
   }

   void uwp_input_next_frame(void *data)
   {
      uwp_current_input                = uwp_next_input;
      uwp_next_input.mouse_rel_x       = 0;
      uwp_next_input.mouse_rel_y       = 0;
      uwp_next_input.mouse_wheel_up   %= WHEEL_DELTA;
      uwp_next_input.mouse_wheel_left %= WHEEL_DELTA;
   }

   bool uwp_keyboard_pressed(unsigned key)
   {
      VirtualKey sym = (VirtualKey)rarch_keysym_lut[(enum retro_key)key];

      if (sym == VirtualKey::None)
         return false;

      CoreWindow window = CoreWindow::GetForCurrentThread();

      /* At times CoreWindow will return NULL while running Dolphin core
       * Dolphin core runs on its own CPU thread separate from the UI-thread and so we must do a check for this. */
      if (window)
         return (window.GetKeyState(sym) & CoreVirtualKeyStates::Down) == CoreVirtualKeyStates::Down;
      return false;
   }

   int16_t uwp_mouse_state(unsigned port, unsigned id, bool screen)
   {
      int16_t state = 0;

      switch (id)
      {
         case RETRO_DEVICE_ID_MOUSE_X:
            return screen
               ? uwp_current_input.mouse_screen_x
               : uwp_current_input.mouse_rel_x;
         case RETRO_DEVICE_ID_MOUSE_Y:
            return screen
               ? uwp_current_input.mouse_screen_y
               : uwp_current_input.mouse_rel_y;
         case RETRO_DEVICE_ID_MOUSE_LEFT:
            return uwp_current_input.mouse_left;
         case RETRO_DEVICE_ID_MOUSE_RIGHT:
            return uwp_current_input.mouse_right;
         case RETRO_DEVICE_ID_MOUSE_WHEELUP:
            return uwp_current_input.mouse_wheel_up > WHEEL_DELTA;
         case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
            return uwp_current_input.mouse_wheel_up < -WHEEL_DELTA;
         case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
            return uwp_current_input.mouse_wheel_left > WHEEL_DELTA;
         case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
            return uwp_current_input.mouse_wheel_left < -WHEEL_DELTA;
         case RETRO_DEVICE_ID_MOUSE_MIDDLE:
            return uwp_current_input.mouse_middle;
         case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
            return uwp_current_input.mouse_button4;
         case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
            return uwp_current_input.mouse_button5;
      }

      return 0;
   }

   int16_t uwp_pointer_state(unsigned idx, unsigned id, bool screen)
   {
      switch (id)
      {
         case RETRO_DEVICE_ID_POINTER_X:
            return screen
               ? uwp_current_input.touch[idx].full_x
               : uwp_current_input.touch[idx].x;
         case RETRO_DEVICE_ID_POINTER_Y:
            return screen
               ? uwp_current_input.touch[idx].full_y
               : uwp_current_input.touch[idx].y;
         case RETRO_DEVICE_ID_POINTER_PRESSED:
            return uwp_current_input.touch[idx].isInContact;
         case RETRO_DEVICE_ID_POINTER_COUNT:
            return uwp_current_input.touch_count;
         default:
            break;
      }

      return 0;
   }

   void uwp_open_broadfilesystemaccess_settings(void)
   {
      Launcher::LaunchUriAsync(Uri(L"ms-settings:privacy-broadfilesystemaccess"));
   }

   enum retro_language uwp_get_language(void)
   {
      size_t _len;
      char lang_iso[16];
      auto lang                 = winrt::Windows::System::UserProfile::GlobalizationPreferences::Languages().GetAt(0);
      struct string_list  split = {0};
      char lang_bcp[16]         = {0};

      wcstombs(lang_bcp, lang.c_str(), sizeof(lang_bcp));

      /* Trying to convert BCP 47 language codes to ISO 639 ones */
      string_list_initialize(&split);
      string_split_noalloc(&split, lang_bcp, "-");

      _len = strlcpy(lang_iso, split.elems[0].data, sizeof(lang_iso));

      if (split.size >= 2)
      {
         _len += strlcpy(lang_iso + _len, "_", sizeof(lang_iso) - _len);
         strlcpy(lang_iso       + _len,
               split.elems[split.size >= 3 ? 2 : 1].data,
               sizeof(lang_iso) - _len);
      }
      string_list_deinitialize(&split);
      return retroarch_get_language_from_iso(lang_iso);
   }

   const char* uwp_get_cpu_model_name(void)
   {
      if (is_running_on_xbox())
      {
         /* Use Gaming Device Information API for specific Xbox model */
         GAMING_DEVICE_MODEL_INFORMATION deviceInfo = {};
         HRESULT hr = GetGamingDeviceModelInformation(&deviceInfo);

         if (SUCCEEDED(hr))
         {
            switch (deviceInfo.deviceId)
            {
               case GAMING_DEVICE_DEVICE_ID_XBOX_ONE:
                  return "Xbox One CPU";
               case GAMING_DEVICE_DEVICE_ID_XBOX_ONE_S:
                  return "Xbox One S CPU";
               case GAMING_DEVICE_DEVICE_ID_XBOX_ONE_X:
                  return "Xbox One X CPU";
               case GAMING_DEVICE_DEVICE_ID_XBOX_ONE_X_DEVKIT:
                  return "Xbox One X DevKit CPU";
               case GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_S:
                  return "Xbox Series S CPU";
               case GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_X:
                  return "Xbox Series X CPU";
               case GAMING_DEVICE_DEVICE_ID_XBOX_SERIES_X_DEVKIT:
                  return "Xbox Series X DevKit CPU";
               default:
                  return "Xbox CPU (Unknown Model)";
            }
         }
         return "Xbox CPU";
      }
      else
      {
         hstring cpu_id;
         hstring cpu_name;

         /* GUID_DEVICE_PROCESSOR: {97FADB10-4E33-40AE-359C-8BEF029DBDD0} */
         hstring if_filter = L"System.Devices.InterfaceClassGuid:=\"{97FADB10-4E33-40AE-359C-8BEF029DBDD0}\"";

         /* Enumerate all CPU DeviceInterfaces, and get DeviceInstanceID of the first one. */
         try
         {
            auto collection = DeviceInformation::FindAllAsync(if_filter).get();
            if (collection.Size() > 0)
            {
               auto props = collection.GetAt(0).Properties();
               auto val = props.Lookup(L"System.Devices.DeviceInstanceID");
               if (val)
                  cpu_id = winrt::unbox_value<hstring>(val);
            }
         }
         catch (winrt::hresult_error const&)
         {
            cpu_id = hstring{};
         }

         if (!cpu_id.empty())
         {
            hstring dev_filter = L"System.Devices.DeviceInstanceID:=\"" + cpu_id + L"\"";

            /* Get the Device with the same ID as the DeviceInterface
             * Then get the name (description) of that Device
             * We have to do this because the DeviceInterface we get doesn't have a proper description. */
            try
            {
               auto collection = DeviceInformation::FindAllAsync(dev_filter, {}, DeviceInformationKind::Device).get();
               if (collection.Size() > 0)
                  cpu_name = collection.GetAt(0).Name();
            }
            catch (winrt::hresult_error const&)
            {
               cpu_name = hstring{};
            }
         }

         if (!cpu_name.empty())
         {
            wcstombs(win32_cpu_model_name, cpu_name.c_str(), sizeof(win32_cpu_model_name));
            return win32_cpu_model_name;
         }
      }
      return "Unknown";
   }
}
