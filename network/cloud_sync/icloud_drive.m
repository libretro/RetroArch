#include "../cloud_sync_driver.h"
#include "../../verbosity.h"

#define ICLOUD_CONTAINER_NAME @"iCloud.com.libretro.dist.RetroArch"
#define DOCUMENTS_BASE_FOLDER @"Documents"

static NSURL *icloud_base_url(void)
{
   NSURL *base_url = [[NSFileManager defaultManager] URLForUbiquityContainerIdentifier:ICLOUD_CONTAINER_NAME];
   return [base_url URLByAppendingPathComponent: DOCUMENTS_BASE_FOLDER];
}

static NSURL *icloud_file_url(const char *path)
{
   NSURL *base = icloud_base_url();
   if (!base)
      return nil;

   NSString *relative_path = [NSString stringWithUTF8String:path];
   return [base URLByAppendingPathComponent:relative_path];
}

static bool icloud_drive_sync_begin(cloud_sync_complete_handler_t cb, void *user_data)
{
   BOOL available = [[NSFileManager defaultManager] URLForUbiquityContainerIdentifier:ICLOUD_CONTAINER_NAME] != nil;
   cb(user_data, NULL, available, NULL);
   return true;
}

static bool icloud_drive_sync_end(cloud_sync_complete_handler_t cb, void *user_data)
{
   cb(user_data, NULL, true, NULL);
   return true;
}

static bool icloud_drive_read(const char *p, const char *f, cloud_sync_complete_handler_t cb, void *user_data)
{
    char *path = strdup(p);
    char *file = strdup(f);
    
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        NSURL *url = icloud_file_url(path);

        BOOL is_cloud_file_present = [[NSFileManager defaultManager] fileExistsAtPath: url.path];
        if (is_cloud_file_present)
        {
            NSData *data = [NSData dataWithContentsOfURL:url];
            if (!data) {
                RARCH_DBG("[iCloudDrive] Could not retrieve data for %s \n", path);
                cb(user_data, path, false, NULL);
            }

            RFILE *rfile = filestream_open(file, RETRO_VFS_FILE_ACCESS_READ_WRITE, RETRO_VFS_FILE_ACCESS_HINT_NONE);
            
            if (rfile)
            {
                filestream_truncate(rfile, 0);
                filestream_write(rfile, [data bytes], [data length]);
                filestream_seek(rfile, 0, SEEK_SET);

                cb(user_data, path, true, rfile);
            }
            else
            {
                RARCH_DBG("[iCloudDrive] Could not create RFILE for %s \n", path);
            }
        }
        else
        {
            RARCH_DBG("[iCloudDrive] File %s is not present\n", path);
            cb(user_data, path, true, NULL);
        }

        free(path);
        free(file);
   });
   return true;
}


static bool icloud_drive_update(const char *p, RFILE *rfile, cloud_sync_complete_handler_t cb, void *user_data)
{
    char *path = strdup(p);
    NSURL *url = icloud_file_url(path);
    NSURL *directory_url = [url URLByDeletingLastPathComponent];

    NSString *file_string = [NSString stringWithUTF8String:filestream_get_path(rfile)];
    NSData *local_data = [NSData dataWithContentsOfFile:file_string];

    if (!local_data)
    {
        RARCH_DBG("[iCloudDrive] Failed to read local file: %s\n", [file_string UTF8String]);
        cb(user_data, path, false, rfile);
    }

    dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        NSError *error = nil;
        NSError *directory_error = nil;
        BOOL directory_is_present = [[NSFileManager defaultManager] fileExistsAtPath: directory_url.path];
        
        if (!directory_is_present)
        {
            [[NSFileManager defaultManager] createDirectoryAtPath:directory_url.path withIntermediateDirectories:YES attributes:nil error:&directory_error];
            if (directory_error)
            {
                RARCH_DBG("[iCloudDrive] Failed to create directory: %s\n", [[directory_error debugDescription] UTF8String]);
            }
        }
        
        [local_data writeToURL:url options:NSDataWritingAtomic error:&error];
        RARCH_DBG("[iCloudDrive] %s writing to %s\n", error == nil ? "succeeded" : "failed", [url.absoluteString UTF8String]);
      
        if (error)
        {
            RARCH_DBG("[iCloudDrive] error: %s\n", [[error debugDescription] UTF8String]);
        }
                 
        cb(user_data, path, error == nil, rfile);
        free(path);
    });

   return true;
}

static bool icloud_drive_delete(const char *p, cloud_sync_complete_handler_t cb, void *user_data)
{
    NSString *path_string = [NSString stringWithUTF8String:p];
    NSURL *url = icloud_file_url(p);
    
    dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
        NSError *error = nil;
        if ([[NSFileManager defaultManager] fileExistsAtPath:url.path])
        {
            [[NSFileManager defaultManager] removeItemAtURL:url error:&error];
            RARCH_DBG("[iCloudDrive] delete %s %s\n", p, error == nil ? "succeeded" : "failed");
            if (error)
            {
                RARCH_DBG("[iCloudDrive] error: %s\n", [[error debugDescription] UTF8String]);
            }
        }
       cb(user_data, [path_string UTF8String], error == nil, NULL);
    });
    return true;
}

cloud_sync_driver_t cloud_sync_icloud_drive = {
    icloud_drive_sync_begin,
    icloud_drive_sync_end,
    icloud_drive_read,
    icloud_drive_update,
    icloud_drive_delete,
    "icloud_drive" /* ident */
};