/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "vcfiled_check.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include <sys/stat.h>
#include <fcntl.h>

int vcfiled_lock(const char *lockfile, VCFILED_LOGMSG_T logmsg)
{
   int rc, fd;
   char pidbuf[32];
   char *lockdir = strdup(lockfile);
   char *sep = strrchr(lockdir, '/');
   int ret = -1;
   if (!sep)
   {
      free(lockdir);
      return -1;
   }
   *sep = '\0';

   if (mkdir(lockdir, S_IRWXU | S_IRGRP|S_IXGRP) < 0)
   {
      if (errno != EEXIST)
      {
         logmsg(LOG_CRIT, "could not create %s:%s\n", lockdir,strerror(errno));
         goto finish;
      }
   }
   fd = open(lockfile, O_RDWR | O_CREAT | O_EXCL, 0640);
   if (fd<0)
   {
      if (errno != EEXIST)
      {
         logmsg(LOG_CRIT, "could not create lockfile %s:%s\n", lockfile, strerror(errno));
         goto finish;
      }
      else
      {
         // it already exists - reopen it and try to lock it
         fd = open(lockfile, O_RDWR);
         if (fd<0)
         {
            logmsg(LOG_CRIT, "could not re-open lockfile %s:%s\n", lockfile, strerror(errno));
            goto finish;
         }
      }
   }
   // at this point, we have opened the file, and can use discretionary locking, 
   // which should work even over NFS

   struct flock flock;
   memset(&flock, 0, sizeof(flock));
   flock.l_type   = F_WRLCK;
   flock.l_whence = SEEK_SET;
   flock.l_start  = 0;
   flock.l_len    = 1;
   if (fcntl(fd, F_SETLK, &flock) < 0)
   {
      // if we failed to lock, then it might mean we're already running, or
      // something else bad.
      if (errno == EACCES || errno == EAGAIN)
      {
         // already running
         int pid = 0, rc = read(fd, pidbuf, sizeof(pidbuf));
         if (rc)
            pid = atoi(pidbuf);
         logmsg(LOG_CRIT, "already running at pid %d\n", pid);
         close(fd);
         goto finish;
      }
      else
      {
         logmsg(LOG_CRIT, "could not lock %s:%s\n", lockfile, strerror(errno));
         close(fd);
         goto finish;
      }
   }
   snprintf(pidbuf,sizeof(pidbuf),"%d", getpid());
   rc = write(fd, pidbuf, strlen(pidbuf)+1);
   if (rc<0)
   {
      logmsg(LOG_CRIT, "could not write pid:%s\n", strerror(errno));
      goto finish;
   }
   /* do not close the file, as that will release the lock - it will
    * will close automatically when the program exits.
    */
   ret = 0;
finish:
   free(lockdir);
   /* coverity[leaked_handle] - fd left open on purpose */
   return ret;
}

int vcfiled_is_running(const char *filename)
{
   int ret;
   
   int fd = open(filename, O_RDONLY);
   if (fd < 0)
   {
      // file not there, so filed not running
      ret = 0;
   }

   else
   {
      struct flock flock;
      memset(&flock, 0, sizeof(flock));
      flock.l_type   = F_WRLCK;
      flock.l_whence = SEEK_SET;
      flock.l_start  = 0;
      flock.l_len    = 1;
      int rc = fcntl(fd, F_GETLK, &flock);
      if (rc != 0)
      {
         /* could not access lock info */
         printf("%s: Could not access lockfile %s: %s\n",
            "vmcs_main", filename, strerror(errno));
         ret = 0;
      }
      else if (flock.l_pid == 0)
      {
         /* file is unlocked, so filed not running */
         ret = 0;
      }
      else 
      {
         /* file is locked, so filed is running */
         ret = 1;
      }
   }
   /* coverity[leaked_handle] - fd left open on purpose */
   return ret;
}



