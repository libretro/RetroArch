/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#import <CloudKit/CloudKit.h>

#include <time.h>

#include "../cloud_sync_driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"

#define IC_RECORD_TYPE @"cloudsync"

static bool icloud_sync_begin(cloud_sync_complete_handler_t cb, void *user_data)
{
   [CKContainer.defaultContainer accountStatusWithCompletionHandler:^(CKAccountStatus accountStatus, NSError * _Nullable error) {
      BOOL success = (error == nil) && (accountStatus == CKAccountStatusAvailable);
      cb(user_data, NULL, success, NULL);
   }];
   return true;
}

static bool icloud_sync_end(cloud_sync_complete_handler_t cb, void *user_data)
{
   cb(user_data, NULL, true, NULL);
   return true;
}

static CKRecord *icloud_remove_duplicates(NSArray<CKRecord *> *results)
{
   if (!results || ![results count])
      return nil;
   if ([results count] == 1)
      return results[0];

   CKRecord *newest = nil;
   for (CKRecord *rec in results)
   {
      CKRecord *toDelete = rec;
      if (newest == nil || [newest.modificationDate compare:rec.modificationDate] == NSOrderedAscending)
      {
         toDelete = newest;
         newest = rec;
      }
      if (toDelete)
      {
         [CKContainer.defaultContainer.privateCloudDatabase deleteRecordWithID:toDelete.recordID
                                                             completionHandler:^(CKRecordID * _Nullable recordID, NSError * _Nullable error) {
            RARCH_DBG("[iCloud] delete callback for duplicate of %s %s\n",
                  [(NSString *)toDelete[@"path"] UTF8String], error == nil ? "succeeded" : "failed");
         }];
      }
   }
   return newest;
}

static void icloud_query_path(const char *path, void(^cb)(CKRecord * results, NSError * error))
{
   NSPredicate *pred = [NSComparisonPredicate
                        predicateWithLeftExpression:[NSExpression expressionForKeyPath:@"path"]
                        rightExpression:[NSExpression expressionForConstantValue:[NSString stringWithUTF8String:path]]
                        modifier:NSDirectPredicateModifier
                        type:NSEqualToPredicateOperatorType
                        options:0];
   CKQuery *query = [[CKQuery alloc] initWithRecordType:IC_RECORD_TYPE predicate:pred];
   [CKContainer.defaultContainer.privateCloudDatabase performQuery:query
                                                      inZoneWithID:nil
                                                 completionHandler:^(NSArray<CKRecord *> * _Nullable results, NSError * _Nullable error) {
      if (error || ![results count])
      {
         RARCH_DBG("[iCloud] could not find %s (%s)\n", path, error == nil ? "successfully" : "failure");
         if (error)
            RARCH_DBG("[iCloud] error: %s\n", [[error debugDescription] UTF8String]);
         cb(nil, error);
      }
      else
      {
         RARCH_DBG("[iCloud] found %lu results looking for %s\n", (unsigned long)[results count], path);
         cb(icloud_remove_duplicates(results), nil);
      }
   }];
}

/* Cloud sync counts its outstanding requests and waits for each one to
 * report, so every request below reports exactly once, after the work
 * it stands for has finished.  Success with no file from a read means
 * "the server does not have it" and nothing else: a download that
 * cannot be loaded or written is a failure.
 *
 * With destructive sync off, the server's copy of a file is kept, as
 * the other drivers keep it: a delete renames the record's path to
 * <path>-<yymmdd-hhmmss>, and an upload that replaces a record first
 * saves a record under that name holding the old data.  The server
 * manifest is rewritten every sync and is not kept. */

static bool icloud_ck_wants_backup(const char *path)
{
   settings_t *settings = config_get_ptr();
   return settings
       && !settings->bools.cloud_sync_destructive
       && strcmp(path, CLOUD_SYNC_SERVER_MANIFEST) != 0;
}

static NSString *icloud_ck_backup_path(const char *path)
{
   char      ts[32];
   time_t    now = time(NULL);
   struct tm tm_;

   localtime_r(&now, &tm_);
   strftime(ts, sizeof(ts), "-%y%m%d-%H%M%S", &tm_);
   return [NSString stringWithFormat:@"%s%s", path, ts];
}

static bool icloud_read(const char *p, const char *f, cloud_sync_complete_handler_t cb, void *user_data)
{
   char *path = strdup(p);
   char *file = strdup(f);
   icloud_query_path(path, ^(CKRecord *result, NSError *error) {
      if (result)
      {
         [CKContainer.defaultContainer.privateCloudDatabase fetchRecordWithID:result.recordID
                                                            completionHandler:^(CKRecord * _Nullable fetchedRecord, NSError * _Nullable error) {
            RFILE *rfile = NULL;
            bool   ok    = false;

            if (error)
               RARCH_DBG("[iCloud] failed to fetch record for %s\n", path);
            else
            {
               CKAsset *asset = fetchedRecord[@"data"];
               NSData  *data  = asset.fileURL
                  ? [NSData dataWithContentsOfURL:asset.fileURL] : nil;

               if (!data)
                  RARCH_DBG("[iCloud] record for %s has no readable data\n", path);
               else if (!(rfile = filestream_open(file,
                           RETRO_VFS_FILE_ACCESS_READ_WRITE,
                           RETRO_VFS_FILE_ACCESS_HINT_NONE)))
                  RARCH_DBG("[iCloud] could not open %s for writing\n", file);
               else if (filestream_write(rfile, [data bytes], [data length])
                     != (int64_t)[data length])
               {
                  RARCH_DBG("[iCloud] could not write %s\n", file);
                  filestream_close(rfile);
                  rfile = NULL;
               }
               else
               {
                  RARCH_DBG("[iCloud] successfully fetched %s, size %lu\n",
                        path, (unsigned long)[data length]);
                  filestream_seek(rfile, 0, SEEK_SET);
                  ok = true;
               }
            }
            cb(user_data, path, ok, rfile);
            free(path);
            free(file);
         }];
      }
      else
      {
         cb(user_data, path, error == nil, NULL);
         free(path);
         free(file);
      }
   });
   return true;
}

static bool icloud_update(const char *p, RFILE *rfile, cloud_sync_complete_handler_t cb, void *user_data)
{
   char *path = strdup(p);
   icloud_query_path(path, ^(CKRecord *record, NSError *error) {
      CKDatabase *db       = CKContainer.defaultContainer.privateCloudDatabase;
      bool        update   = (record != nil);
      CKAsset    *old_data = record ? record[@"data"] : nil;
      NSString   *fileStr;
      void (^save)(void);

      /* A failed query says nothing about whether the record exists;
       * saving a new one on top would leave a duplicate. */
      if (error)
      {
         RARCH_DBG("[iCloud] not uploading %s: the lookup failed\n", path);
         cb(user_data, path, false, rfile);
         free(path);
         return;
      }

      if (!record)
      {
         record = [[CKRecord alloc] initWithRecordType:IC_RECORD_TYPE];
         record[@"path"] = [NSString stringWithUTF8String:path];
      }
      fileStr = [NSString stringWithUTF8String:filestream_get_path(rfile)];

      save = ^{
         record[@"data"] = [[CKAsset alloc] initWithFileURL:[NSURL fileURLWithPath:fileStr]];
         [db saveRecord:record completionHandler:^(CKRecord * _Nullable newrecord, NSError * _Nullable error) {
            RARCH_DBG("[iCloud] %s %s %s\n", error == nil ? "succeeded" : "failed", update ? "updating" : "creating", path);
            if (error)
               RARCH_DBG("[iCloud] error: %s\n", [[error debugDescription] UTF8String]);
            cb(user_data, path, error == nil, rfile);
            free(path);
         }];
      };

      if (update && icloud_ck_wants_backup(path))
      {
         CKRecord *backup;

         if (!old_data.fileURL)
         {
            RARCH_DBG("[iCloud] not replacing %s: its current data could not be kept\n", path);
            cb(user_data, path, false, rfile);
            free(path);
            return;
         }

         backup          = [[CKRecord alloc] initWithRecordType:IC_RECORD_TYPE];
         backup[@"path"] = icloud_ck_backup_path(path);
         backup[@"data"] = [[CKAsset alloc] initWithFileURL:old_data.fileURL];
         [db saveRecord:backup completionHandler:^(CKRecord * _Nullable saved, NSError * _Nullable error) {
            if (error)
            {
               RARCH_DBG("[iCloud] not replacing %s: the backup failed: %s\n",
                     path, [[error debugDescription] UTF8String]);
               cb(user_data, path, false, rfile);
               free(path);
               return;
            }
            save();
         }];
      }
      else
         save();
   });
   return true;
}

static bool icloud_delete(const char *p, cloud_sync_complete_handler_t cb, void *user_data)
{
   NSString *path = [NSString stringWithUTF8String:p];
   bool      keep = icloud_ck_wants_backup(p);
   NSPredicate *pred = [NSComparisonPredicate
                        predicateWithLeftExpression:[NSExpression expressionForKeyPath:@"path"]
                        rightExpression:[NSExpression expressionForConstantValue:path]
                        modifier:NSDirectPredicateModifier
                        type:NSEqualToPredicateOperatorType
                        options:0];
   CKQuery *query = [[CKQuery alloc] initWithRecordType:IC_RECORD_TYPE predicate:pred];
   [CKContainer.defaultContainer.privateCloudDatabase performQuery:query
                                                      inZoneWithID:nil
                                                 completionHandler:^(NSArray<CKRecord *> * _Nullable results, NSError * _Nullable error) {
      CKDatabase       *db     = CKContainer.defaultContainer.privateCloudDatabase;
      dispatch_group_t  group;
      __block int       failed = 0;
      NSString         *backup_path;

      if (error)
      {
         RARCH_DBG("[iCloud] not deleting %s: the lookup failed\n", [path UTF8String]);
         cb(user_data, [path UTF8String], false, NULL);
         return;
      }

      RARCH_DBG("[iCloud] %s %lu records for %s\n", keep ? "keeping" : "deleting",
            (unsigned long)[results count], [path UTF8String]);
      group       = dispatch_group_create();
      backup_path = keep ? icloud_ck_backup_path(p) : nil;

      /* Report once every record has been dealt with, and report what
       * happened to them: the old code reported success before the
       * deletes had even been sent, and whatever became of them. */
      for (CKRecord *record in results)
      {
         dispatch_group_enter(group);
         if (keep)
         {
            record[@"path"] = backup_path;
            [db saveRecord:record completionHandler:^(CKRecord * _Nullable saved, NSError * _Nullable error) {
               if (error)
               {
                  RARCH_DBG("[iCloud] could not keep %s: %s\n",
                        [path UTF8String], [[error debugDescription] UTF8String]);
                  __sync_fetch_and_add(&failed, 1);
               }
               dispatch_group_leave(group);
            }];
         }
         else
         {
            [db deleteRecordWithID:record.recordID
                 completionHandler:^(CKRecordID * _Nullable recordID, NSError * _Nullable error) {
               if (error)
               {
                  RARCH_DBG("[iCloud] could not delete %s: %s\n",
                        [path UTF8String], [[error debugDescription] UTF8String]);
                  __sync_fetch_and_add(&failed, 1);
               }
               dispatch_group_leave(group);
            }];
         }
      }

      dispatch_group_notify(group, dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
         cb(user_data, [path UTF8String], __sync_fetch_and_add(&failed, 0) == 0, NULL);
      });
   }];
   return true;
}

cloud_sync_driver_t cloud_sync_icloud = {
   icloud_sync_begin,
   icloud_sync_end,
   icloud_read,
   icloud_update,
   icloud_delete,
   "icloud" /* ident */
};
