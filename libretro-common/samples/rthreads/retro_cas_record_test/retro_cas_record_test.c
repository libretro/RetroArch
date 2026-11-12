/* rthreads/retro_cas_record.h: a record several threads change while
 * others read it, with no lock.
 *
 * What is asserted:
 *  - no change is lost: every thread's count in the record ends at the
 *    number of changes it made, and the total at their sum, half the
 *    threads changing it in place in a spare slot (begin, commit) and
 *    half on a copy of their own (read_versioned, replace),
 *  - no reader ever gets a torn record (every field of a copy carries
 *    the same total), and no reader's copies go back,
 *  - an aborted change leaves the record as it was,
 *  - more threads than there are spare slots still all get through,
 *  - every slot but the record's is spare again at the end. */

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

#include <rthreads/retro_cas_record.h>

#define FIELDS   16
#define WRITERS  20 /* more than RETRO_CAS_RECORD_SLOTS - 1 */
#define CHANGES  100000
#define READERS  3

static unsigned failures;
#define CHECK(cond, ...) \
   do { if (!(cond)) { printf("FAIL line %d: ", __LINE__); \
        printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

typedef struct
{
   uint64_t field[FIELDS];    /* field[i] = total * (i + 1) */
   uint64_t by_writer[WRITERS];
} record_t;

static record_t storage[RETRO_CAS_RECORD_SLOTS];
static retro_cas_record_t rec;
static retro_atomic_int_t running;

typedef struct
{
   unsigned long copies;
   unsigned long torn;
   unsigned long backwards;
} reader_result_t;

static void *reader_main(void *arg)
{
   reader_result_t *result = (reader_result_t*)arg;
   uint64_t last           = 0;
   while (retro_atomic_load_acquire_int(&running))
   {
      record_t copy;
      uint64_t sum = 0;
      unsigned i;
      retro_cas_record_read(&rec, &copy);
      for (i = 1; i < FIELDS; i++)
         if (copy.field[i] != copy.field[0] * (i + 1))
            break;
      if (i == FIELDS)
      {
         for (i = 0; i < WRITERS; i++)
            sum += copy.by_writer[i];
         if (sum != copy.field[0])
            i = 0;
      }
      if (i != WRITERS)
         result->torn++;
      if (copy.field[0] < last)
         result->backwards++;
      last = copy.field[0];
      result->copies++;
   }
   return NULL;
}

static void *writer_main(void *arg)
{
   unsigned id = (unsigned)(uintptr_t)arg;
   unsigned n;
   for (n = 0; n < CHANGES; n++)
   {
      retro_cas_record_txn_t txn;
      record_t *r;
      unsigned i;

      /* Now and then a look that changes nothing */
      if ((n & 63) == id)
      {
         r = (record_t*)retro_cas_record_begin(&rec, &txn);
         r->field[0] = 0xdead; /* thrown away */
         retro_cas_record_abort(&rec, &txn);
      }

      if (id & 1)
      {
         /* The other way to change it: on a copy of this thread's own,
          * put back if the record is still the one that was read */
         record_t copy;
         retro_cas_record_word_t version;
         do
         {
            uint64_t total;
            version = retro_cas_record_read_versioned(&rec, &copy);
            total   = copy.field[0] + 1;
            for (i = 0; i < FIELDS; i++)
               copy.field[i] = total * (i + 1);
            copy.by_writer[id]++;
         } while (!retro_cas_record_replace(&rec, version, &copy));
         continue;
      }

      do
      {
         uint64_t total;
         r     = (record_t*)retro_cas_record_begin(&rec, &txn);
         total = r->field[0] + 1;
         for (i = 0; i < FIELDS; i++)
            r->field[i] = total * (i + 1);
         r->by_writer[id]++;
      } while (!retro_cas_record_commit(&rec, &txn));
   }
   return NULL;
}

int main(void)
{
   pthread_t readers[READERS], writers[WRITERS];
   reader_result_t results[READERS] = {{0}};
   record_t final_copy;
   unsigned i;

   retro_cas_record_init(&rec, storage, sizeof(record_t));
   retro_atomic_int_init(&running, 1);

   for (i = 0; i < READERS; i++)
      pthread_create(&readers[i], NULL, reader_main, &results[i]);
   for (i = 0; i < WRITERS; i++)
      pthread_create(&writers[i], NULL, writer_main, (void*)(uintptr_t)i);
   for (i = 0; i < WRITERS; i++)
      pthread_join(writers[i], NULL);

   retro_atomic_store_release_int(&running, 0);
   for (i = 0; i < READERS; i++)
   {
      pthread_join(readers[i], NULL);
      printf("reader %u: %lu copies\n", i, results[i].copies);
      CHECK(results[i].torn == 0, "reader %u: %lu torn copies", i,
            results[i].torn);
      CHECK(results[i].backwards == 0, "reader %u: went back %lu times", i,
            results[i].backwards);
      CHECK(results[i].copies > 0, "reader %u: no copy taken", i);
   }

   retro_cas_record_read(&rec, &final_copy);
   CHECK(final_copy.field[0] == (uint64_t)WRITERS * CHANGES,
         "total %llu, not %llu: changes lost",
         (unsigned long long)final_copy.field[0],
         (unsigned long long)WRITERS * CHANGES);
   for (i = 0; i < WRITERS; i++)
      CHECK(final_copy.by_writer[i] == CHANGES, "writer %u: %llu of %u", i,
            (unsigned long long)final_copy.by_writer[i], CHANGES);
   for (i = 1; i < FIELDS; i++)
      CHECK(final_copy.field[i] == final_copy.field[0] * (i + 1),
            "field %u of the last record", i);

   /* One slot in use: the record's */
   {
      unsigned busy = (unsigned)retro_atomic_load_acquire_int(&rec.busy);
      CHECK(busy && !(busy & (busy - 1)), "slots left taken: 0x%x", busy);
   }

   printf("%u threads, %u changes each: total %llu\n", WRITERS, CHANGES,
         (unsigned long long)final_copy.field[0]);
   if (failures)
   {
      printf("%u check(s) failed\n", failures);
      return 1;
   }
   printf("ALL OK\n");
   return 0;
}
