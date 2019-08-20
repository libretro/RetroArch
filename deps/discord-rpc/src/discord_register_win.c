#include "discord_rpc.h"
#include "discord_register.h"

#define WIN32_LEAN_AND_MEAN
#define NOMCX
#define NOSERVICE
#define NOIME
#include <windows.h>
#include <psapi.h>
#include <wchar.h>
#include <stdio.h>

/**
 * Updated fixes for MinGW and WinXP
 * This block is written the way it does not involve changing the rest of the code
 * Checked to be compiling
 * 1) strsafe.h belongs to Windows SDK and cannot be added to MinGW
 * #include guarded, functions redirected to <string.h> substitutes
 * 2) RegSetKeyValueW and LSTATUS are not declared in <winreg.h>
 * The entire function is rewritten
 */
#ifdef __MINGW32__
/* strsafe.h fixes */
static HRESULT StringCbPrintfW(LPWSTR pszDest,
      size_t cbDest, LPCWSTR pszFormat, ...)
{
   HRESULT ret;
   va_list va;
   va_start(va, pszFormat);
   cbDest /= 2; /* Size is divided by 2 to convert from bytes to wide characters - causes segfault */
   /* othervise */
   ret = vsnwprintf(pszDest, cbDest, pszFormat, va);
   pszDest[cbDest - 1] = 0; /* Terminate the string in case a buffer overflow; -1 will be returned */
   va_end(va);
   return ret;
}
#else
#include <strsafe.h>
#endif /* __MINGW32__ */

/* winreg.h fixes */
#ifndef LSTATUS
#define LSTATUS LONG
#endif
#ifdef RegSetKeyValueW
#undefine RegSetKeyValueW
#endif
#define RegSetKeyValueW regset
static LSTATUS regset(HKEY hkey,
      LPCWSTR subkey,
      LPCWSTR name,
      DWORD type,
      const void* data,
      DWORD len)
{
    HKEY htkey = hkey, hsubkey = NULL;
    LSTATUS ret;
    if (subkey && subkey[0])
    {
        if ((ret = RegCreateKeyExW(hkey, subkey, 0, 0, 0, KEY_ALL_ACCESS, 0, &hsubkey, 0)) !=
            ERROR_SUCCESS)
            return ret;
        htkey = hsubkey;
    }
    ret = RegSetValueExW(htkey, name, 0, type, (const BYTE*)data, len);
    if (hsubkey && hsubkey != hkey)
        RegCloseKey(hsubkey);
    return ret;
}

static void Discord_RegisterW(
      const wchar_t* applicationId, const wchar_t* command)
{
    /* https://msdn.microsoft.com/en-us/library/aa767914(v=vs.85).aspx
     * we want to register games so we can run them as discord-<appid>://
     * Update the HKEY_CURRENT_USER, because it doesn't seem to require special permissions. */

    DWORD len;
    LSTATUS result;
    wchar_t urlProtocol = 0;
    wchar_t keyName[256];
    wchar_t protocolName[64];
    wchar_t protocolDescription[128];
    wchar_t exeFilePath[MAX_PATH];
    DWORD exeLen = GetModuleFileNameW(NULL, exeFilePath, MAX_PATH);
    wchar_t openCommand[1024];

    if (command && command[0])
        StringCbPrintfW(openCommand, sizeof(openCommand), L"%S", command);
    else
        StringCbPrintfW(openCommand, sizeof(openCommand), L"%S", exeFilePath);

    StringCbPrintfW(protocolName, sizeof(protocolName),
          L"discord-%S", applicationId);
    StringCbPrintfW(
      protocolDescription, sizeof(protocolDescription),
      L"URL:Run game %S protocol", applicationId);
    StringCbPrintfW(keyName, sizeof(keyName), L"Software\\Classes\\%S", protocolName);
    HKEY key;
    LSTATUS status =
      RegCreateKeyExW(HKEY_CURRENT_USER, keyName, 0, NULL, 0, KEY_WRITE, NULL, &key, NULL);
    if (status != ERROR_SUCCESS)
    {
        fprintf(stderr, "Error creating key\n");
        return;
    }
    len = (DWORD)lstrlenW(protocolDescription) + 1;
    result =
      RegSetKeyValueW(key, NULL, NULL, REG_SZ, protocolDescription, len * sizeof(wchar_t));
    if (FAILED(result)) {
        fprintf(stderr, "Error writing description\n");
    }

    len = (DWORD)lstrlenW(protocolDescription) + 1;
    result = RegSetKeyValueW(key, NULL, L"URL Protocol", REG_SZ, &urlProtocol, sizeof(wchar_t));
    if (FAILED(result))
        fprintf(stderr, "Error writing description\n");

    result = RegSetKeyValueW(
      key, L"DefaultIcon", NULL, REG_SZ, exeFilePath, (exeLen + 1) * sizeof(wchar_t));
    if (FAILED(result))
        fprintf(stderr, "Error writing icon\n");

    len = (DWORD)lstrlenW(openCommand) + 1;
    result = RegSetKeyValueW(
      key, L"shell\\open\\command", NULL, REG_SZ, openCommand, len * sizeof(wchar_t));
    if (FAILED(result))
        fprintf(stderr, "Error writing command\n");
    RegCloseKey(key);
}

void Discord_Register(const char* applicationId, const char* command)
{
    wchar_t openCommand[1024];
    const wchar_t* wcommand = NULL;
    wchar_t appId[32];

    MultiByteToWideChar(CP_UTF8, 0, applicationId, -1, appId, 32);
    if (command && command[0])
    {
        const int commandBufferLen = 
           sizeof(openCommand) / sizeof(*openCommand);
        MultiByteToWideChar(CP_UTF8, 0, command, -1,
              openCommand, commandBufferLen);
        wcommand = openCommand;
    }

    Discord_RegisterW(appId, wcommand);
}

void Discord_RegisterSteamGame(
      const char* applicationId,
      const char* steamId)
{
   DWORD pathChars, pathBytes, i;
   HKEY key;
   wchar_t steamPath[MAX_PATH];
   wchar_t command[1024];
   wchar_t appId[32];
   wchar_t wSteamId[32];
   MultiByteToWideChar(CP_UTF8, 0, applicationId, -1, appId, 32);
   MultiByteToWideChar(CP_UTF8, 0, steamId, -1, wSteamId, 32);
   LSTATUS status = RegOpenKeyExW(HKEY_CURRENT_USER,
         L"Software\\Valve\\Steam", 0, KEY_READ, &key);
   if (status != ERROR_SUCCESS)
   {
      fprintf(stderr, "Error opening Steam key\n");
      return;
   }

   pathBytes = sizeof(steamPath);
   status    = RegQueryValueExW(key,
         L"SteamExe", NULL, NULL, (BYTE*)steamPath, &pathBytes);
   RegCloseKey(key);
   if (status != ERROR_SUCCESS || pathBytes < 1) {
      fprintf(stderr, "Error reading SteamExe key\n");
      return;
   }

   pathChars = pathBytes / sizeof(wchar_t);
   for (i = 0; i < pathChars; ++i)
   {
      if (steamPath[i] == L'/')
         steamPath[i] = L'\\';
   }

   StringCbPrintfW(command, sizeof(command),
         L"\"%s\" steam://rungameid/%s", steamPath, wSteamId);

   Discord_RegisterW(appId, command);
}
