/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
int main(int argc, char **argv)
{
   STARTUPINFOA startup;
   PROCESS_INFORMATION process;
   HDESK desktop;
   HANDLE job;
   JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits;
   DWORD status = 1, waited;
   char name[64], *command;
   if (argc != 2)
      return 1;
   sprintf(name, "RetroArchStreamTest%lu", (unsigned long)GetCurrentProcessId());
   desktop = CreateDesktopA(name, NULL, NULL, 0, GENERIC_ALL, NULL);
   if (!desktop)
      return 1;
   job = CreateJobObjectA(NULL, NULL);
   limits.BasicLimitInformation.PerProcessUserTimeLimit.QuadPart = 0;
   limits.BasicLimitInformation.PerJobUserTimeLimit.QuadPart = 0;
   limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
   limits.BasicLimitInformation.MinimumWorkingSetSize = 0;
   limits.BasicLimitInformation.MaximumWorkingSetSize = 0;
   limits.BasicLimitInformation.ActiveProcessLimit = 0;
   limits.BasicLimitInformation.Affinity = 0;
   limits.BasicLimitInformation.PriorityClass = 0;
   limits.BasicLimitInformation.SchedulingClass = 0;
   limits.IoInfo.ReadOperationCount = limits.IoInfo.WriteOperationCount = 0;
   limits.IoInfo.OtherOperationCount = limits.IoInfo.ReadTransferCount = 0;
   limits.IoInfo.WriteTransferCount = limits.IoInfo.OtherTransferCount = 0;
   limits.ProcessMemoryLimit = limits.JobMemoryLimit = 0;
   limits.PeakProcessMemoryUsed = limits.PeakJobMemoryUsed = 0;
   if (!job || !SetInformationJobObject(job, JobObjectExtendedLimitInformation,
            &limits, sizeof(limits)))
   {
      if (job) CloseHandle(job);
      CloseDesktop(desktop);
      return 1;
   }
   command = (char*)malloc(strlen(argv[1]) + 1);
   if (!command)
   {
      CloseDesktop(desktop);
      CloseHandle(job);
      return 1;
   }
   strcpy(command, argv[1]);
   startup.cb = sizeof(startup);
   startup.lpReserved = NULL;
   startup.lpDesktop = name;
   startup.lpTitle = NULL;
   startup.dwX = startup.dwY = startup.dwXSize = startup.dwYSize = 0;
   startup.dwXCountChars = startup.dwYCountChars = startup.dwFillAttribute = 0;
   startup.dwFlags = STARTF_USESTDHANDLES;
   startup.wShowWindow = 0;
   startup.cbReserved2 = 0;
   startup.lpReserved2 = NULL;
   startup.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
   startup.hStdOutput = GetStdHandle(STD_OUTPUT_HANDLE);
   startup.hStdError = GetStdHandle(STD_ERROR_HANDLE);
   if (CreateProcessA(NULL, command, NULL, NULL, TRUE, CREATE_NO_WINDOW | CREATE_SUSPENDED,
            NULL, NULL, &startup, &process))
   {
      waited = WAIT_FAILED;
      if (AssignProcessToJobObject(job, process.hProcess)
            && ResumeThread(process.hThread) != (DWORD)-1)
         waited = WaitForSingleObject(process.hProcess, 60000);
      if (waited != WAIT_OBJECT_0)
      {
         TerminateProcess(process.hProcess, 124);
         WaitForSingleObject(process.hProcess, 5000);
      }
      GetExitCodeProcess(process.hProcess, &status);
      CloseHandle(process.hThread);
      CloseHandle(process.hProcess);
   }
   else
      fprintf(stderr, "CreateProcess failed: %lu\n", (unsigned long)GetLastError());
   free(command);
   CloseHandle(job);
   CloseDesktop(desktop);
   return (int)status;
}
