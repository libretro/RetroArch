/* Copyright  (C) 2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (cloud_storage.h).
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

#ifndef _CLOUD_STORAGE_H
#define _CLOUD_STORAGE_H

#include <retro_common_api.h>
#include <configuration.h>

#include <boolean.h>
#include <time.h>

#include <lists/linked_list.h>

RETRO_BEGIN_DECLS

#define GAME_STATES_FOLDER_NAME "save_states"

/* Hash types that cloud storage supports for files */
enum cloud_storage_hash_type_t
{
   CLOUD_STORAGE_HASH_MD5
};
typedef enum cloud_storage_hash_type_t cloud_storage_hash_type_t;

/* Metadata for files in a cloud storage provider */
struct cloud_storage_file_t
{
   cloud_storage_hash_type_t hash_type;
   char hash_value[33];
   char *download_url;
};
typedef struct cloud_storage_file_t cloud_storage_file_t;

/* Contains a list of files/folders */
struct cloud_storage_folder_t
{
   linked_list_t *children;
};
typedef struct cloud_storage_folder_t cloud_storage_folder_t;

typedef struct cloud_storage_item_t cloud_storage_item_t;

/* Type of object from the cloud storage provider */
enum cloud_storage_item_type_t
{
   CLOUD_STORAGE_FILE,
   CLOUD_STORAGE_FOLDER
};
typedef enum cloud_storage_item_type_t cloud_storage_item_type_t;

/* Metadat for an object from the cloud storage provider */
struct cloud_storage_item_t
{
   char *id;
   char *name;
   time_t last_sync_time;
   cloud_storage_item_type_t item_type;
   union
   {
      cloud_storage_file_t file;
      cloud_storage_folder_t folder;
   } type_data;
};

/* Category for the folder, examples would be game save, game states, screenshots, etc */
enum folder_type_t
{
   CLOUD_STORAGE_GAME_STATES
};
typedef enum folder_type_t folder_type_t;

/* Wraps the provider specific logic to separate it from the general logic */
struct cloud_storage_provider_t
{
   /**
    * @brief Is the provider fully configured
    *
    * Is the provider fully configured and ready to process operations.
    *
    * @return if the provider is fully configured
    */
   bool (*ready_for_request)(void);

   /**
    * @brief Get the contents of "folder"
    *
    * Retrieve the immediate contents of "folder" from the storage
    * provider. The contents must be added to the "folder" parameter.
    *
    * @param folder folder to get the contents of
    */
   void (*list_files)(cloud_storage_item_t *folder);

   /**
    * @brief Get the contents of a storage provider file
    *
    * Copy the contents of the storage provider file to a local file.
    *
    * @param file_to_download storage provider file (source for the copy)
    * @param local_file local file (destination for the copy)
    * @return if the copy succeeded
    */
   bool (*download_file)(
      cloud_storage_item_t *file_to_download,
      char *local_file
   );

   /**
    * @brief Copy a local file to the storage provider
    *
    * Copy the contents of a local file to a file in the storage provider.
    *
    * @param remote_dir folder in the storage provider that will contain
    *                   "remote_file"
    * @param remote_file object in the storage provider to receive the
    *                    contents of "local_file"
    * @param local_file local file to get the contents from
    * @return if the copy succeeded
    */
   bool (*upload_file)(
      cloud_storage_item_t *remote_dir,
      cloud_storage_item_t *remote_file,
      char *local_file
   );

   /**
    * @brief Get the metadata for a folder in the storage provider
    *
    * Retrieve the metadata for a folder in the storage provider.
    *
    * @param folder_name name of the folder
    * @return a new cloud_storage_item_t containing the metadata for the folder
    */
   cloud_storage_item_t *(*get_folder_metadata)(
      const char *folder_name
   );

   /**
    * @brief Get the metadata for a file in the storage provider
    *
    * Retrieve the metadata for a file in the storage provider. The file
    * is to be identified from metadata in the parameter.
    *
    * @param file for the storage provider to update with the metadata
    * @return the updated cloud_storage_item_t object
    */
   cloud_storage_item_t *(*get_file_metadata)(
      cloud_storage_item_t *file
   );

   /**
    * @brief Get the metadata for a file in the storage provider
    *
    * Retrieve the metadata for a file in the storage provider. The file
    * is to be identified from its name and parent folder.
    *
    * @param folder parent folder of the requested file
    * @param filename name of the file
    * @return the updated cloud_storage_item_t object
    */
   cloud_storage_item_t *(*get_file_metadata_by_name)(
      cloud_storage_item_t *folder,
      char *filename
   );

   /**
    * @brief Deletes a file from the storage provider
    *
    * Deletes a file from the storage provider. The file is to be
    * identified from metadata in the parameter.
    *
    * @param file file to delete
    * @return if the file was deleted
    */
   bool (*delete_file)(
      cloud_storage_item_t *file
   );

   /**
    * @brief Create a folder in the storage provider
    *
    * Create a folder in the storage provider with the given name.
    *
    * @param folder_name name of the folder to create
    * @return metadata for the new folder
    */
   cloud_storage_item_t *(*create_folder)(
      const char *folder_name
   );
};
typedef struct cloud_storage_provider_t cloud_storage_provider_t;

/**
 * @brief Start the cloud storage functionality
 *
 * Initializes the cloud storage system. If a provider is configured, then
 * it will try to sync with the storage provider.
 */
void cloud_storage_init(void);

/**
 * @brief Shuts down the cloud storage functionality
 *
 * Shuts down the cloud storage system. Will complete an in flight operation
 * if there is one and then shutdown.
 */
void cloud_storage_shutdown(void);

/**
 * @brief Sync the local files with the storage provider
 *
 * Syncs files with the storage provider. An operation is added to the
 * operation queue and executed in a different thread. This function
 * returns after adding the operation to the queue.
 *
 * 1. Creates the folders in the storage provider if necessary
 * 2. Upload files that are not in the storage provider
 * 3. Upload files that are local and in the storage provider with different
 *    checksums
 * 4. Download files that do not exist locally
 */
void cloud_storage_sync_files(void);

/**
 * @brief Uploads a file to the storage provider
 *
 * Uploads a file to the storage provider. Will overwrite if it already
 * exists. An operation is added to the operation queue and executed in a different
 * thread. This function returns after adding the operation to the queue.
 *
 * @param folder_type folder category for the file that is to be uploaded
 * @param file_name name of the local file to upload
 */
void cloud_storage_upload_file(folder_type_t folder_type, char *file_name);

/**
 * @brief Release the memory for a cloud_storage_item_t
 *
 * Releases the memory for a cloud_storage_item_t. Will work recursively
 * on folders to release all memory.
 */
void cloud_storage_item_free(cloud_storage_item_t *item);

RETRO_END_DECLS

#endif
