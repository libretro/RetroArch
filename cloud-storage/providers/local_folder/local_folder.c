/* Copyright  (C) 2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (google.c).
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

#include <stdlib.h>

#include <file/file_path.h>

#include "../../cloud_storage.h"
#include "local_folder.h"
#include "local_folder_internal.h"

static char *_base_dir = NULL;

/* Ready if the base folder for local folder provider exists */
static bool _ready_for_request(void)
{
   return _base_dir != NULL;
}

/* Get the base directory for the local folder provider. */
const char *cloud_storage_local_folder_get_base_dir()
{
   return _base_dir;
}

/* Create the storage provider structure for the local folder provider. */
cloud_storage_provider_t *cloud_storage_local_folder_create(void)
{
   cloud_storage_provider_t *provider;

   provider = (cloud_storage_provider_t *)malloc(sizeof(cloud_storage_provider_t));

   /* Assign the function pointers */
   provider->ready_for_request = _ready_for_request;
   provider->list_files = cloud_storage_local_folder_list_files;
   provider->download_file = cloud_storage_local_folder_download_file;
   provider->upload_file = cloud_storage_local_folder_upload_file;
   provider->get_folder_metadata = cloud_storage_local_folder_get_folder_metadata;
   provider->get_file_metadata = cloud_storage_local_folder_get_file_metadata;
   provider->get_file_metadata_by_name = cloud_storage_local_folder_get_file_metadata_by_name;
   provider->delete_file = cloud_storage_local_folder_delete_file;
   provider->create_folder = cloud_storage_local_folder_create_folder;

   /* Create the base directory if it doesn't exist */
   if (!_base_dir)
   {
      _base_dir = getenv("RETRO_CLOUD_STORAGE_TEST");
      if (!_base_dir)
      {
         char *tmpdir;
         size_t tmpdir_len;

         /* Will create in the temp directory. Try to identify an appropriate
          * directory to use for the temp directory.
          */
         tmpdir = getenv("TMPDIR");
         if (!tmpdir){
            tmpdir = getenv("TMP");
         }

         if (!tmpdir)
         {
            if (path_is_directory("/tmp"))
            {
               tmpdir = (char *)"/tmp";
            }
         } else if (!path_is_directory(tmpdir))
         {
            tmpdir = NULL;
         }

         if (tmpdir)
         {
            /* Found a temp directory to use */
            _base_dir = pathname_join(tmpdir, "retro_cloud_storage_test");
            if (!path_is_directory(_base_dir) && !path_mkdir(_base_dir)) {
               free(_base_dir);
               _base_dir = NULL;
            }
         }
      }
   }

   return provider;
}
