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

#include "../cloud_sync_driver.h"
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
            RARCH_DBG("[iCloud] delete callback for duplicate of %s %s\n", toDelete[@"path"], error == nil ? "succeeded" : "failed");
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
         RARCH_DBG("[iCloud] found %d results looking for %s\n", [results count], path);
         cb(icloud_remove_duplicates(results), nil);
      }
   }];
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
            if (error)
            {
               RARCH_DBG("[iCloud] failed to fetch record for %s\n", path);
               cb(user_data, path, false, NULL);
            }
            else
            {
               CKAsset *asset = fetchedRecord[@"data"];
               NSData *data = [NSFileManager.defaultManager contentsAtPath:asset.fileURL.path];
               RARCH_DBG("[iCloud] successfully fetched %s, size %d\n", path, [data length]);
               RFILE *rfile = filestream_open(file,
                                              RETRO_VFS_FILE_ACCESS_READ_WRITE,
                                              RETRO_VFS_FILE_ACCESS_HINT_NONE);
               if (rfile)
               {
                  filestream_truncate(rfile, 0);
                  filestream_write(rfile, [data bytes], [data length]);
                  filestream_seek(rfile, 0, SEEK_SET);
               }
               cb(user_data, path, true, rfile);
            }
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
      bool update = true;
      if (error || !record)
      {
         record = [[CKRecord alloc] initWithRecordType:IC_RECORD_TYPE];
         record[@"path"] = [NSString stringWithUTF8String:path];
         update = false;
      }
      NSString *fileStr = [NSString stringWithUTF8String:filestream_get_path(rfile)];
      NSURL *fileURL = [NSURL fileURLWithPath:fileStr];
      record[@"data"] = [[CKAsset alloc] initWithFileURL:fileURL];
      [CKContainer.defaultContainer.privateCloudDatabase saveRecord:record completionHandler:^(CKRecord * _Nullable newrecord, NSError * _Nullable error) {
         RARCH_DBG("[iCloud] %s %s %s\n", error == nil ? "succeeded" : "failed", update ? "updating" : "creating", path);
         if (error)
            RARCH_DBG("[iCloud] error: %s\n", [[error debugDescription] UTF8String]);
         cb(user_data, path, error == nil, rfile);
         free(path);
      }];
   });
   return true;
}

static bool icloud_delete(const char *p, cloud_sync_complete_handler_t cb, void *user_data)
{
   NSString *path = [NSString stringWithUTF8String:p];
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
      RARCH_DBG("[iCloud] deleting %d records for %s\n", [results count], [path UTF8String]);
      for (CKRecord *record in results)
      {
         [CKContainer.defaultContainer.privateCloudDatabase deleteRecordWithID:record.recordID
                                                             completionHandler:^(CKRecordID * _Nullable recordID, NSError * _Nullable error) {
            RARCH_DBG("[iCloud] delete callback for %s %s\n", [path UTF8String], error == nil ? "succeeded" : "failed");
            if (error)
               RARCH_DBG("[iCloud] error: %s\n", [[error debugDescription] UTF8String]);
         }];
      }
      cb(user_data, [path UTF8String], error == nil, NULL);
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
