/* Copyright  (C) 2018-2019 The RetroArch team
*
* ---------------------------------------------------------------------------------------
* The following license statement only applies to this file (uwp_async.h).
* ---------------------------------------------------------------------------------------
*
* Permission is hereby granted, free of charge,
* to any person obtaining a copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation the rights to
* use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
* and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <functional>
#include "uwp_main.h"
#include "uwp_func.h"

namespace
{
   /* Dear Microsoft
      * I really appreciate all the effort you took to not provide any
      * synchronous file APIs and block all attempts to synchronously
      * wait for the results of async tasks for "smooth user experience",
      * but I'm not going to run and rewrite all RetroArch cores for
      * async I/O. I hope you like this hack I made instead.
      */
   template<typename T>
   T RunAsync(std::function<winrt::Windows::Foundation::IAsyncOperation<T>()> func)
   {
      volatile bool finished = false;
      std::exception_ptr exception = nullptr;
      T result{};

      auto op = func();
      op.Completed([&finished, &exception, &result](
         winrt::Windows::Foundation::IAsyncOperation<T> const& sender,
         winrt::Windows::Foundation::AsyncStatus status)
      {
         try
         {
            if (status == winrt::Windows::Foundation::AsyncStatus::Completed)
               result = sender.GetResults();
            else if (status == winrt::Windows::Foundation::AsyncStatus::Error)
               winrt::throw_hresult(sender.ErrorCode());
         }
         catch (...)
         {
            exception = std::current_exception();
         }
         finished = true;
      });

      /* Don't stall the UI thread - prevents a deadlock */
      auto corewindow = winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread();
      while (!finished)
      {
         if (corewindow)
            corewindow.Dispatcher().ProcessEvents(winrt::Windows::UI::Core::CoreProcessEventsOption::ProcessAllIfPresent);
      }

      if (exception != nullptr)
         std::rethrow_exception(exception);
      return result;
   }

   /* Overload for IAsyncAction (void return) */
   inline void RunAsync(std::function<winrt::Windows::Foundation::IAsyncAction()> func)
   {
      volatile bool finished = false;
      std::exception_ptr exception = nullptr;

      auto op = func();
      op.Completed([&finished, &exception](
         winrt::Windows::Foundation::IAsyncAction const& sender,
         winrt::Windows::Foundation::AsyncStatus status)
      {
         try
         {
            if (status == winrt::Windows::Foundation::AsyncStatus::Error)
               winrt::throw_hresult(sender.ErrorCode());
         }
         catch (...)
         {
            exception = std::current_exception();
         }
         finished = true;
      });

      /* Don't stall the UI thread - prevents a deadlock */
      auto corewindow = winrt::Windows::UI::Core::CoreWindow::GetForCurrentThread();
      while (!finished)
      {
         if (corewindow)
            corewindow.Dispatcher().ProcessEvents(winrt::Windows::UI::Core::CoreProcessEventsOption::ProcessAllIfPresent);
      }

      if (exception != nullptr)
         std::rethrow_exception(exception);
   }

   template<typename T>
   T RunAsyncAndCatchErrors(std::function<winrt::Windows::Foundation::IAsyncOperation<T>()> func, T valueOnError)
   {
      try
      {
         return RunAsync<T>(func);
      }
      catch (winrt::hresult_error const&)
      {
         return valueOnError;
      }
      catch (...)
      {
         return valueOnError;
      }
   }

   /* Overload for IAsyncAction (void return) */
   inline bool RunAsyncAndCatchErrors(std::function<winrt::Windows::Foundation::IAsyncAction()> func)
   {
      try
      {
         RunAsync(func);
         return true;
      }
      catch (winrt::hresult_error const&)
      {
         return false;
      }
      catch (...)
      {
         return false;
      }
   }
}
