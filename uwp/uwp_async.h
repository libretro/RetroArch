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

#include <ppl.h>
#include <ppltasks.h>
#include "uwp_main.h"
#include "uwp_func.h"

#ifdef __cplusplus
#ifdef __cplusplus_winrt
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
   T RunAsync(std::function<concurrency::task<T>()> func)
   {
      volatile bool finished = false;
      Platform::Exception^ exception = nullptr;
      T result;

      func().then([&finished, &exception, &result](concurrency::task<T> t) {
         try
         {
            result = t.get();
         }
         catch (Platform::Exception ^ exception_)
         {
            exception = exception_;
         }
         finished = true;
         });

      /* Don't stall the UI thread - prevents a deadlock */
      Windows::UI::Core::CoreWindow^ corewindow = Windows::UI::Core::CoreWindow::GetForCurrentThread();
      while (!finished)
      {
         if (corewindow) {
            corewindow->Dispatcher->ProcessEvents(Windows::UI::Core::CoreProcessEventsOption::ProcessAllIfPresent);
         }
      }

      if (exception != nullptr)
         throw exception;
      return result;
   }

   template<typename T>
   T RunAsyncAndCatchErrors(std::function<concurrency::task<T>()> func, T valueOnError)
   {
      try
      {
         return RunAsync<T>(func);
      }
      catch (Platform::Exception ^ e)
      {
         return valueOnError;
      }
   }
}
#endif
#endif
