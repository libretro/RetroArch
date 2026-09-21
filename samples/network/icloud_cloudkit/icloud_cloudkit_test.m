/* Harness for the iCloud (CloudKit) cloud sync driver.
 *
 * What the driver got wrong, and what this checks:
 *
 * - A read whose local file could not be written, or whose record held
 *   no readable data, reported success - with no file (which cloud sync
 *   takes as "not on the server" and drops from the server manifest)
 *   or with an empty one (which it takes as the save).  Both must fail.
 * - An upload whose lookup failed saved a brand new record anyway,
 *   leaving a duplicate; it must fail instead.
 * - A delete reported success before the deletes were even sent, and
 *   whatever became of them; it must report once they are done, with
 *   what happened.
 * - Destructive sync off was ignored: deletes removed records for good
 *   and uploads replaced their data.  A delete must now rename the
 *   record to <path>-<yymmdd-hhmmss>, and an upload must first save a
 *   record under that name holding the old data - and not replace
 *   anything if that backup fails.  Destructive sync and the server
 *   manifest skip the backup.
 * - Every request reports exactly once.
 *
 * CloudKit is replaced by an in-memory database: +[CKContainer
 * defaultContainer] is overridden in a category to return a fake
 * container, so the driver's own code runs unchanged.  Real CKRecord,
 * CKAsset and CKQuery objects are used; the fake keeps its own copy of
 * each saved asset, as the server would.
 *
 * Includes the driver's translation unit so it is tested as shipped.
 */

#import <Foundation/Foundation.h>
#import <CloudKit/CloudKit.h>

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>

#include <streams/file_stream.h>

#include "../../../network/cloud_sync/icloud.m"

void RARCH_LOG(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); }
void RARCH_WARN(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); printf("WARN: "); vprintf(fmt, ap); va_end(ap); }
void RARCH_ERR(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); printf("ERR: "); vprintf(fmt, ap); va_end(ap); }
void RARCH_DBG(const char *fmt, ...)
{ va_list ap; va_start(ap, fmt); vprintf(fmt, ap); va_end(ap); }

settings_t *config_get_ptr(void)
{
   static settings_t settings;
   return &settings;
}

static unsigned failures = 0;

#define CHECK(cond, ...) \
   do { \
      if (!(cond)) \
      { \
         printf("FAIL %s:%d: ", __FILE__, __LINE__); \
         printf(__VA_ARGS__); \
         printf("\n"); \
         failures++; \
      } \
   } while (0)

static char store_dir[256];

/* ---- the fake CloudKit database ---- */

/* Ivars and @synthesize spelled out so the harness also builds for a
 * fragile-ABI runtime. */
@interface FakeDatabase : NSObject
{
   NSMutableArray *_records;
   BOOL _failQuery, _failFetch, _failDelete;
   int  _failSaveNumber, _saves;
}
@property (strong) NSMutableArray *records;
@property BOOL     failQuery;
@property BOOL     failFetch;
@property BOOL     failDelete;
@property int      failSaveNumber;   /* 1-based: which save fails */
@property int      saves;
@end

@implementation FakeDatabase
@synthesize records = _records, failQuery = _failQuery, failFetch = _failFetch,
            failDelete = _failDelete, failSaveNumber = _failSaveNumber, saves = _saves;

- (instancetype)init
{
   if ((self = [super init]))
      self.records = [NSMutableArray array];
   return self;
}

- (NSError *)error
{
   return [NSError errorWithDomain:@"FakeCloudKit" code:1 userInfo:nil];
}

- (CKRecord *)recordWithID:(CKRecordID *)recordID
{
   for (CKRecord *r in self.records)
      if ([r.recordID isEqual:recordID])
         return r;
   return nil;
}

/* Keep a private copy of the asset's bytes, as the server does. */
- (CKAsset *)storedAsset:(CKAsset *)asset
{
   static int n;
   NSString  *copy;
   NSData    *data = asset.fileURL ? [NSData dataWithContentsOfURL:asset.fileURL] : nil;
   if (!data)
      return nil;
   copy = [NSString stringWithFormat:@"%s/asset-%d", store_dir, ++n];
   [data writeToFile:copy atomically:YES];
   return [[CKAsset alloc] initWithFileURL:[NSURL fileURLWithPath:copy]];
}

- (void)performQuery:(CKQuery *)query inZoneWithID:(CKRecordZoneID *)zoneID
   completionHandler:(void (^)(NSArray<CKRecord *> *results, NSError *error))completionHandler
{
   NSString       *want    = [((NSComparisonPredicate *)query.predicate).rightExpression constantValue];
   NSMutableArray *matches = [NSMutableArray array];
   BOOL            fail    = self.failQuery;
   (void)zoneID;

   for (CKRecord *r in self.records)
      if ([(NSString *)r[@"path"] isEqualToString:want])
         [matches addObject:r];
   dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
      completionHandler(fail ? nil : matches, fail ? [self error] : nil);
   });
}

- (void)fetchRecordWithID:(CKRecordID *)recordID
        completionHandler:(void (^)(CKRecord *record, NSError *error))completionHandler
{
   CKRecord *r    = [self recordWithID:recordID];
   BOOL      fail = self.failFetch || !r;
   dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
      completionHandler(fail ? nil : r, fail ? [self error] : nil);
   });
}

- (void)saveRecord:(CKRecord *)record
 completionHandler:(void (^)(CKRecord *record, NSError *error))completionHandler
{
   BOOL fail;
   @synchronized (self)
   {
      self.saves++;
      fail = (self.failSaveNumber && self.saves == self.failSaveNumber);
      if (!fail)
      {
         CKAsset *asset = record[@"data"];
         if (asset)
            record[@"data"] = [self storedAsset:asset];
         if (![self recordWithID:record.recordID])
            [self.records addObject:record];
      }
   }
   dispatch_async(dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
      completionHandler(fail ? nil : record, fail ? [self error] : nil);
   });
}

- (void)deleteRecordWithID:(CKRecordID *)recordID
         completionHandler:(void (^)(CKRecordID *recordID, NSError *error))completionHandler
{
   BOOL fail = self.failDelete;
   @synchronized (self)
   {
      CKRecord *r = [self recordWithID:recordID];
      if (!fail && r)
         [self.records removeObject:r];
   }
   /* Late, so a driver that reports before the delete lands is caught. */
   dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 200 * NSEC_PER_MSEC),
         dispatch_get_global_queue(QOS_CLASS_DEFAULT, 0), ^{
      completionHandler(recordID, fail ? [self error] : nil);
   });
}

@end

@interface FakeContainer : NSObject
{
   FakeDatabase *_privateCloudDatabase;
}
@property (strong) FakeDatabase *privateCloudDatabase;
@end
@implementation FakeContainer
@synthesize privateCloudDatabase = _privateCloudDatabase;
@end

static FakeContainer *fake_container;

#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wobjc-protocol-method-implementation"
@implementation CKContainer (ICloudTest)
+ (CKContainer *)defaultContainer
{
   return (CKContainer *)fake_container;
}
@end
#pragma clang diagnostic pop

static FakeDatabase *db(void) { return fake_container.privateCloudDatabase; }

/* ---- helpers ---- */

typedef struct
{
   volatile int calls;
   volatile int success;
   RFILE *volatile file;
} report_t;

static void on_report(void *user_data, const char *path, bool success,
      RFILE *file)
{
   report_t *r = (report_t*)user_data;
   (void)path;
   r->success = success;
   r->file    = file;
   __sync_synchronize();
   __sync_fetch_and_add(&r->calls, 1);
}

/* Up to 5 s for the first report, then 500 ms more for a second. */
static void wait_reports(report_t *r)
{
   int i;
   for (i = 0; i < 500 && !__sync_fetch_and_add(&r->calls, 0); i++)
      usleep(10000);
   usleep(500000);
   __sync_synchronize();
}

static void check_once(const char *what, report_t *r, bool success)
{
   CHECK(r->calls == 1, "%s: must report once, reported %d time(s)", what, r->calls);
   if (r->calls >= 1)
      CHECK((bool)r->success == success, "%s: expected %s", what,
            success ? "success" : "failure");
}

static void fresh_db(void)
{
   fake_container.privateCloudDatabase = [[FakeDatabase alloc] init];
}

static void write_text(const char *path, const char *text)
{
   FILE *f = fopen(path, "wb");
   if (f)
   {
      fputs(text, f);
      fclose(f);
   }
}

/* Put a record on the "server" holding @text. */
static void server_put(const char *path, const char *text)
{
   char tmp[320];
   CKRecord *r = [[CKRecord alloc] initWithRecordType:IC_RECORD_TYPE];
   snprintf(tmp, sizeof(tmp), "%s/seed", store_dir);
   write_text(tmp, text);
   r[@"path"] = [NSString stringWithUTF8String:path];
   r[@"data"] = [db() storedAsset:[[CKAsset alloc] initWithFileURL:
         [NSURL fileURLWithPath:[NSString stringWithUTF8String:tmp]]]];
   [db().records addObject:r];
}

static NSString *text_of(CKRecord *r)
{
   CKAsset *a    = r[@"data"];
   NSData  *data = a.fileURL ? [NSData dataWithContentsOfURL:a.fileURL] : nil;
   return data ? [[NSString alloc] initWithData:data encoding:NSUTF8StringEncoding] : nil;
}

static unsigned records_at(const char *path, CKRecord **first)
{
   unsigned n = 0;
   *first = nil;
   for (CKRecord *r in db().records)
      if (!strcmp([(NSString *)r[@"path"] UTF8String], path))
      {
         if (!n++)
            *first = r;
      }
   return n;
}

/* Records named <path>-yymmdd-hhmmss. */
static unsigned backups_of(const char *path, CKRecord **first)
{
   unsigned n = 0;
   size_t   len = strlen(path);
   *first = nil;
   for (CKRecord *r in db().records)
   {
      const char *name = [(NSString *)r[@"path"] UTF8String];
      const char *p;
      bool ok = true;
      int  i;
      if (strncmp(name, path, len) || name[len] != '-')
         continue;
      p = name + len + 1;
      for (i = 0; i < 13; i++)
         if (i == 6 ? p[i] != '-' : !isdigit((unsigned char)p[i]))
            ok = false;
      if (!ok || p[13])
         continue;
      if (!n++)
         *first = r;
   }
   return n;
}

static RFILE *local_upload(const char *text)
{
   char tmp[320];
   snprintf(tmp, sizeof(tmp), "%s/upload", store_dir);
   write_text(tmp, text);
   return filestream_open(tmp, RETRO_VFS_FILE_ACCESS_READ,
         RETRO_VFS_FILE_ACCESS_HINT_NONE);
}

/* ---- reads ---- */

static void test_reads(void)
{
   report_t r;
   char local[320], bad[320];
   CKRecord *rec;

   snprintf(local, sizeof(local), "%s/local", store_dir);
   snprintf(bad, sizeof(bad), "%s/no/such/dir/local", store_dir);

   fresh_db();
   server_put("saves/a.srm", "cloud-copy");

   memset(&r, 0, sizeof(r));
   icloud_read("saves/a.srm", local, on_report, &r);
   wait_reports(&r);
   check_once("read present", &r, true);
   if (r.file)
   {
      char buf[64] = {0};
      filestream_read(r.file, buf, sizeof(buf) - 1);
      CHECK(!strcmp(buf, "cloud-copy"), "read present: got \"%s\"", buf);
      filestream_close(r.file);
   }

   memset(&r, 0, sizeof(r));
   icloud_read("saves/missing.srm", local, on_report, &r);
   wait_reports(&r);
   check_once("read missing", &r, true);
   CHECK(!r.file, "read missing: must hand back no file");

   memset(&r, 0, sizeof(r));
   icloud_read("saves/a.srm", bad, on_report, &r);
   wait_reports(&r);
   check_once("read, local file cannot be created", &r, false);
   if (r.file)
      filestream_close(r.file);

   records_at("saves/a.srm", &rec);
   unlink([((CKAsset *)rec[@"data"]).fileURL.path UTF8String]);
   memset(&r, 0, sizeof(r));
   icloud_read("saves/a.srm", local, on_report, &r);
   wait_reports(&r);
   check_once("read, record data unreadable", &r, false);
   if (r.file)
      filestream_close(r.file);
}

/* ---- uploads ---- */

static void test_upload_keeps_old(void)
{
   report_t  r = {0};
   CKRecord *rec, *bak;
   RFILE    *rfile;

   fresh_db();
   config_get_ptr()->bools.cloud_sync_destructive = false;
   server_put("saves/a.srm", "OLD");
   rfile = local_upload("NEW");

   icloud_update("saves/a.srm", rfile, on_report, &r);
   wait_reports(&r);
   check_once("non-destructive upload", &r, true);
   CHECK(records_at("saves/a.srm", &rec) == 1 && [text_of(rec) isEqualToString:@"NEW"],
         "non-destructive upload must put the new data in place");
   CHECK(backups_of("saves/a.srm", &bak) == 1 && [text_of(bak) isEqualToString:@"OLD"],
         "non-destructive upload must keep the old data as saves/a.srm-<yymmdd-hhmmss>");
   filestream_close(rfile);
}

static void test_upload_no_backup(void)
{
   report_t  r;
   CKRecord *rec, *bak;
   RFILE    *rfile = local_upload("NEW");

   fresh_db();
   config_get_ptr()->bools.cloud_sync_destructive = true;
   server_put("saves/a.srm", "OLD");
   memset(&r, 0, sizeof(r));
   icloud_update("saves/a.srm", rfile, on_report, &r);
   wait_reports(&r);
   check_once("destructive upload", &r, true);
   CHECK(records_at("saves/a.srm", &rec) == 1 && [text_of(rec) isEqualToString:@"NEW"]
         && !backups_of("saves/a.srm", &bak),
         "destructive upload must replace without a backup");

   fresh_db();
   config_get_ptr()->bools.cloud_sync_destructive = false;
   server_put(CLOUD_SYNC_SERVER_MANIFEST, "OLD");
   memset(&r, 0, sizeof(r));
   icloud_update(CLOUD_SYNC_SERVER_MANIFEST, rfile, on_report, &r);
   wait_reports(&r);
   check_once("manifest upload", &r, true);
   CHECK(!backups_of(CLOUD_SYNC_SERVER_MANIFEST, &bak),
         "the server manifest must be replaced without a backup");

   fresh_db();
   memset(&r, 0, sizeof(r));
   icloud_update("saves/new.srm", rfile, on_report, &r);
   wait_reports(&r);
   check_once("new file upload", &r, true);
   CHECK(records_at("saves/new.srm", &rec) == 1 && !backups_of("saves/new.srm", &bak),
         "a new file must be created without inventing a backup");
   filestream_close(rfile);
}

static void test_upload_failures(void)
{
   report_t  r;
   CKRecord *rec, *bak;
   RFILE    *rfile = local_upload("NEW");

   config_get_ptr()->bools.cloud_sync_destructive = false;

   fresh_db();
   server_put("saves/a.srm", "OLD");
   db().failQuery = YES;
   memset(&r, 0, sizeof(r));
   icloud_update("saves/a.srm", rfile, on_report, &r);
   wait_reports(&r);
   check_once("upload, lookup failed", &r, false);
   CHECK(records_at("saves/a.srm", &rec) == 1 && [text_of(rec) isEqualToString:@"OLD"],
         "a failed lookup must not create a duplicate record");

   fresh_db();
   server_put("saves/a.srm", "OLD");
   db().failSaveNumber = 1; /* the backup */
   memset(&r, 0, sizeof(r));
   icloud_update("saves/a.srm", rfile, on_report, &r);
   wait_reports(&r);
   check_once("upload, backup failed", &r, false);
   CHECK(records_at("saves/a.srm", &rec) == 1 && [text_of(rec) isEqualToString:@"OLD"]
         && !backups_of("saves/a.srm", &bak),
         "a failed backup must leave the old data in place");
   filestream_close(rfile);
}

/* ---- deletes ---- */

static void test_deletes(void)
{
   report_t  r;
   CKRecord *rec, *bak;

   fresh_db();
   config_get_ptr()->bools.cloud_sync_destructive = true;
   server_put("saves/a.srm", "OLD");
   memset(&r, 0, sizeof(r));
   icloud_delete("saves/a.srm", on_report, &r);
   wait_reports(&r);
   check_once("destructive delete", &r, true);
   CHECK(!records_at("saves/a.srm", &rec) && !backups_of("saves/a.srm", &bak),
         "destructive delete must remove the record");

   fresh_db();
   config_get_ptr()->bools.cloud_sync_destructive = false;
   server_put("saves/a.srm", "OLD");
   memset(&r, 0, sizeof(r));
   icloud_delete("saves/a.srm", on_report, &r);
   wait_reports(&r);
   check_once("non-destructive delete", &r, true);
   CHECK(!records_at("saves/a.srm", &rec)
         && backups_of("saves/a.srm", &bak) == 1 && [text_of(bak) isEqualToString:@"OLD"],
         "non-destructive delete must keep the data as saves/a.srm-<yymmdd-hhmmss>");

   fresh_db();
   config_get_ptr()->bools.cloud_sync_destructive = true;
   server_put("saves/a.srm", "OLD");
   db().failDelete = YES;
   memset(&r, 0, sizeof(r));
   icloud_delete("saves/a.srm", on_report, &r);
   wait_reports(&r);
   check_once("delete, server refused", &r, false);

   fresh_db();
   server_put("saves/a.srm", "OLD");
   db().failQuery = YES;
   memset(&r, 0, sizeof(r));
   icloud_delete("saves/a.srm", on_report, &r);
   wait_reports(&r);
   check_once("delete, lookup failed", &r, false);

   fresh_db();
   memset(&r, 0, sizeof(r));
   icloud_delete("saves/missing.srm", on_report, &r);
   wait_reports(&r);
   check_once("delete of a missing file", &r, true);
}

int main(void)
{
   @autoreleasepool
   {
      char cmd[300];
      snprintf(store_dir, sizeof(store_dir), "/tmp/icloud_ck_test_%ld", (long)getpid());
      snprintf(cmd, sizeof(cmd), "rm -rf '%s' && mkdir -p '%s'", store_dir, store_dir);
      system(cmd);

      fake_container = [[FakeContainer alloc] init];

      test_reads();
      test_upload_keeps_old();
      test_upload_no_backup();
      test_upload_failures();
      test_deletes();

      snprintf(cmd, sizeof(cmd), "rm -rf '%s'", store_dir);
      system(cmd);
   }

   if (failures)
   {
      printf("\n%u check(s) failed\n", failures);
      return 1;
   }
   printf("\nall iCloud (CloudKit) driver checks passed\n");
   return 0;
}
