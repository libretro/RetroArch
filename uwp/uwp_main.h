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

#pragma once

#include <winrt/Windows.ApplicationModel.h>
#include <winrt/Windows.ApplicationModel.Activation.h>
#include <winrt/Windows.ApplicationModel.Core.h>
#include <winrt/Windows.UI.Core.h>
#include <winrt/Windows.UI.Input.h>
#include <winrt/Windows.UI.ViewManagement.h>
#include <winrt/Windows.Graphics.Display.h>
#include <winrt/Windows.Graphics.Display.Core.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.System.h>

namespace RetroArchUWP
{
   /* Main entry point for our app. Connects the app with the Windows shell and handles application lifecycle events. */
   struct App : winrt::implements<App, winrt::Windows::ApplicationModel::Core::IFrameworkView>
   {
   public:
      App();

      /* IFrameworkView methods. */
      void Initialize(winrt::Windows::ApplicationModel::Core::CoreApplicationView const& applicationView);
      void SetWindow(winrt::Windows::UI::Core::CoreWindow const& window);
      void Load(winrt::hstring const& entryPoint);
      void Run();
      void Uninitialize();

      bool IsInitialized() { return m_initialized; }
      bool IsWindowClosed() { return m_windowClosed; }
      bool IsWindowVisible() { return m_windowVisible; }
      bool IsWindowFocused() { return m_windowFocused; }
      bool CheckWindowResized() { bool resized = m_windowResized; m_windowResized = false; return resized; }
      void SetWindowResized() { m_windowResized = true; }
      static App* GetInstance() { return m_instance; }

   private:
      /* Application lifecycle event handlers. */
      void OnActivated(winrt::Windows::ApplicationModel::Core::CoreApplicationView const& applicationView,
                       winrt::Windows::ApplicationModel::Activation::IActivatedEventArgs const& args);
      void OnSuspending(winrt::Windows::Foundation::IInspectable const& sender,
                        winrt::Windows::ApplicationModel::SuspendingEventArgs const& args);
      void OnResuming(winrt::Windows::Foundation::IInspectable const& sender,
                      winrt::Windows::Foundation::IInspectable const& args);
      void OnEnteredBackground(winrt::Windows::Foundation::IInspectable const& sender,
                               winrt::Windows::ApplicationModel::EnteredBackgroundEventArgs const& args);

      void OnBackRequested(winrt::Windows::Foundation::IInspectable const& sender,
                           winrt::Windows::UI::Core::BackRequestedEventArgs const& args);

      /* Window event handlers. */
      void OnWindowSizeChanged(winrt::Windows::UI::Core::CoreWindow const& sender,
                               winrt::Windows::UI::Core::WindowSizeChangedEventArgs const& args);
      void OnVisibilityChanged(winrt::Windows::UI::Core::CoreWindow const& sender,
                               winrt::Windows::UI::Core::VisibilityChangedEventArgs const& args);
      void OnWindowClosed(winrt::Windows::UI::Core::CoreWindow const& sender,
                          winrt::Windows::UI::Core::CoreWindowEventArgs const& args);
      void OnWindowActivated(winrt::Windows::UI::Core::CoreWindow const& sender,
                             winrt::Windows::UI::Core::WindowActivatedEventArgs const& args);
      void OnAcceleratorKey(winrt::Windows::UI::Core::CoreDispatcher const& sender,
                            winrt::Windows::UI::Core::AcceleratorKeyEventArgs const& args);
      void OnPointer(winrt::Windows::UI::Core::CoreWindow const& sender,
                     winrt::Windows::UI::Core::PointerEventArgs const& args);

      /* DisplayInformation event handlers. */
      void OnDpiChanged(winrt::Windows::Graphics::Display::DisplayInformation const& sender,
                        winrt::Windows::Foundation::IInspectable const& args);
      void OnOrientationChanged(winrt::Windows::Graphics::Display::DisplayInformation const& sender,
                                winrt::Windows::Foundation::IInspectable const& args);
      void OnDisplayContentsInvalidated(winrt::Windows::Graphics::Display::DisplayInformation const& sender,
                                        winrt::Windows::Foundation::IInspectable const& args);

      void OnPackageInstalling(winrt::Windows::ApplicationModel::PackageCatalog const& sender,
                               winrt::Windows::ApplicationModel::PackageInstallingEventArgs const& args);

      void ParseProtocolArgs(winrt::Windows::ApplicationModel::Activation::IActivatedEventArgs const& args,
                             int *argc, std::vector<char*> *argv, std::vector<std::string> *argvTmp);

      bool m_initialized;
      bool m_windowClosed;
      bool m_windowVisible;
      bool m_windowFocused;
      bool m_windowResized;
      winrt::hstring m_launchOnExit;
      bool m_launchOnExitShutdown;
      static App* m_instance;

      /* Event tokens for cleanup */
      winrt::event_token m_activatedToken;
      winrt::event_token m_suspendingToken;
      winrt::event_token m_resumingToken;
      winrt::event_token m_enteredBackgroundToken;
      winrt::event_token m_sizeChangedToken;
      winrt::event_token m_visibilityChangedToken;
      winrt::event_token m_activatedWindowToken;
      winrt::event_token m_closedToken;
      winrt::event_token m_pointerPressedToken;
      winrt::event_token m_pointerReleasedToken;
      winrt::event_token m_pointerMovedToken;
      winrt::event_token m_pointerWheelChangedToken;
      winrt::event_token m_acceleratorKeyToken;
      winrt::event_token m_dpiChangedToken;
      winrt::event_token m_displayContentsInvalidatedToken;
      winrt::event_token m_orientationChangedToken;
      winrt::event_token m_backRequestedToken;
      winrt::event_token m_packageInstallingToken;
   };
}

struct Direct3DApplicationSource : winrt::implements<Direct3DApplicationSource, winrt::Windows::ApplicationModel::Core::IFrameworkViewSource>
{
   winrt::Windows::ApplicationModel::Core::IFrameworkView CreateView();
};
