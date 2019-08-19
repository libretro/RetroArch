/* RetroArch - A frontend for libretro.
 * Copyright (C) 2010-2018 - Francisco Javier Trujillo Mata - fjtrujy
 *
 * RetroArch is free software: you can redistribute it and/or modify it under the terms
 * of the GNU General Public License as published by the Free Software Found-
 * ation, either version 3 of the License, or (at your option) any later version.
 *
 * RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 * without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 * PURPOSE. See the GNU General Public License for more details.
 * * You should have received a copy of the GNU General Public License along with RetroArch.
 * If not, see <http://www.gnu.org/licenses/>.
 */

#include <ps2_descriptor.h>

#include <stdio.h>
#include <kernel.h>
#include <string.h>
#include <fileXio_rpc.h>

static DescriptorTranslation *__ps2_fdmap[MAX_OPEN_FILES];
static DescriptorTranslation __ps2_fdmap_pool[MAX_OPEN_FILES];
static int _lock_sema_id = -1;

static inline int _lock(void)
{
   return(WaitSema(_lock_sema_id));
}

static inline int _unlock(void)
{
   return(SignalSema(_lock_sema_id));
}

static int __ps2_fd_drop(DescriptorTranslation *map)
{
   _lock();

   if (map->ref_count == 1)
   {
      map->ref_count--;
      map->current_folder_position = -1;
      free(map->FileEntry);
      memset(map, 0, sizeof(DescriptorTranslation));
   }
   else
   {
      map->ref_count--;
   }

   _unlock();
   return 0;
}

int is_fd_valid(int fd)
{
   /* Correct fd value */
   fd = MAX_OPEN_FILES - fd;

   return (fd >= 0) && (fd < MAX_OPEN_FILES) && (__ps2_fdmap[fd] != NULL);
}

void _init_ps2_io(void) {
   ee_sema_t sp;

   memset(__ps2_fdmap, 0, sizeof(__ps2_fdmap));
   memset(__ps2_fdmap_pool, 0, sizeof(__ps2_fdmap_pool));

   sp.init_count = 1;
   sp.max_count = 1;
   sp.option = 0;
   _lock_sema_id = CreateSema(&sp);
}

void _free_ps2_io(void) {
   _lock();
   _unlock();
   if(_lock_sema_id >= 0) DeleteSema(_lock_sema_id);
}

int __ps2_acquire_descriptor(void)
{
   int fd = -1;
   int i = 0;
   _lock();

   /* get free descriptor */
   for (fd = 0; fd < MAX_OPEN_FILES; ++fd)
   {
      if (__ps2_fdmap[fd] == NULL)
      {
         /* get free pool */
         for (i = 0; i < MAX_OPEN_FILES; ++i)
         {
            if (__ps2_fdmap_pool[i].ref_count == 0)
            {
               __ps2_fdmap[fd] = &__ps2_fdmap_pool[i];
               __ps2_fdmap[fd]->ref_count = 1;
               __ps2_fdmap[fd]->current_folder_position = -1;
               __ps2_fdmap[fd]->FileEntry = calloc(sizeof(entries), FILEENTRY_SIZE);
               _unlock();
               return MAX_OPEN_FILES - fd;
            }
         }
      }
   }

   /* no mores descriptors available... */
   _unlock();
   return -1;
}

int __ps2_release_descriptor(int fd)
{
   int res = -1;

   if (is_fd_valid(fd) && __ps2_fd_drop(__ps2_fdmap[MAX_OPEN_FILES - fd]) >= 0)
   {
      _lock();
      /* Correct fd value */
      fd = MAX_OPEN_FILES - fd;
      __ps2_fdmap[fd] = NULL;
      res = 0;
      _unlock();
   }

   return res;
}

DescriptorTranslation *__ps2_fd_grab(int fd)
{
   DescriptorTranslation *map = NULL;

   _lock();

   if (is_fd_valid(fd))
   {
      /* Correct fd value */
      fd = MAX_OPEN_FILES - fd;
      map = __ps2_fdmap[fd];

      if (map)
         map->ref_count++;
   }

   _unlock();
   return map;
}
