/****************************************************************************
 * Copyright (C) 2015
 * by Dimok
 *
 * This software is provided 'as-is', without any express or implied
 * warranty. In no event will the authors be held liable for any
 * damages arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any
 * purpose, including commercial applications, and to alter it and
 * redistribute it freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you
 * must not claim that you wrote the original software. If you use
 * this software in a product, an acknowledgment in the product
 * documentation would be appreciated but is not required.
 *
 * 2. Altered source versions must be plainly marked as such, and
 * must not be misrepresented as being the original software.
 *
 * 3. This notice may not be removed or altered from any source
 * distribution.
 ***************************************************************************/
#include "os_functions.h"
#include "curl_functions.h"

unsigned int libcurl_handle __attribute__((section(".data"))) = 0;

EXPORT_DECL(CURLcode, n_curl_global_init, long flags);
EXPORT_DECL(CURL *, n_curl_easy_init, void);
EXPORT_DECL(CURLcode, n_curl_easy_setopt, CURL *curl, CURLoption option, ...);
EXPORT_DECL(CURLcode, n_curl_easy_perform, CURL *curl);
EXPORT_DECL(void, n_curl_easy_cleanup, CURL *curl);
EXPORT_DECL(CURLcode, n_curl_easy_getinfo, CURL *curl, CURLINFO info, ...);

void InitAcquireCurl(void)
{
    OSDynLoad_Acquire("nlibcurl", &libcurl_handle);
}

void InitCurlFunctionPointers(void)
{
    InitAcquireCurl();
    unsigned int *funcPointer = 0;

    OS_FIND_EXPORT_EX(libcurl_handle, curl_global_init, n_curl_global_init);
    OS_FIND_EXPORT_EX(libcurl_handle, curl_easy_init, n_curl_easy_init);
    OS_FIND_EXPORT_EX(libcurl_handle, curl_easy_setopt, n_curl_easy_setopt);
    OS_FIND_EXPORT_EX(libcurl_handle, curl_easy_perform, n_curl_easy_perform);
    OS_FIND_EXPORT_EX(libcurl_handle, curl_easy_cleanup, n_curl_easy_cleanup);
    OS_FIND_EXPORT_EX(libcurl_handle, curl_easy_getinfo, n_curl_easy_getinfo);

    n_curl_global_init(CURL_GLOBAL_ALL);
}
