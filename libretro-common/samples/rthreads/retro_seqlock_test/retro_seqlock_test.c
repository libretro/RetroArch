/* rthreads/retro_seqlock.h: a status record one thread updates while
 * others sample it.
 *
 * What is asserted:
 *  - no reader ever gets a torn record (every field of a copy carries
 *    the same update number),
 *  - each reader's copies never go back to an older update,
 *  - the last copy taken after the writer is done is its last update,
 *  - the writer never waits: its updates go through at full rate while
 *    readers loop on the record. */

#include <stdio.h>
#include <stdint.h>
#include <pthread.h>

#include <rthreads/retro_seqlock.h>

#define FIELDS  16
#define UPDATES 2000000
#define READERS 3

static unsigned failures;
#define CHECK(cond, ...) \
   do { if (!(cond)) { printf("FAIL line %d: ", __LINE__); \
        printf(__VA_ARGS__); printf("\n"); failures++; } } while (0)

typedef struct
{
   uint64_t field[FIELDS];
} record_t;

static retro_seqlock_t lock;
static volatile record_t status; /* volatile: copied field by field */
static retro_atomic_int_t running;

typedef struct
{
   unsigned long copies;
   unsigned long torn;
   unsigned long backwards;
} reader_result_t;

static void take(record_t *copy)
{
   int seq;
   unsigned i;
   do
   {
      seq = retro_seqlock_read_begin(&lock);
      for (i = 0; i < FIELDS; i++)
         copy->field[i] = status.field[i];
   } while (retro_seqlock_read_retry(&lock, seq));
}

static void *reader_main(void *arg)
{
   reader_result_t *result = (reader_result_t*)arg;
   uint64_t last           = 0;
   while (retro_atomic_load_acquire_int(&running))
   {
      record_t copy;
      unsigned i;
      take(&copy);
      for (i = 1; i < FIELDS; i++)
         if (copy.field[i] != copy.field[0] * (i + 1))
         {
            result->torn++;
            break;
         }
      if (copy.field[0] < last)
         result->backwards++;
      last = copy.field[0];
      result->copies++;
   }
   return NULL;
}

int main(void)
{
   pthread_t readers[READERS];
   reader_result_t results[READERS] = {{0}};
   record_t final_copy;
   uint64_t n;
   unsigned i;

   retro_seqlock_init(&lock);
   retro_atomic_int_init(&running, 1);
   for (i = 0; i < READERS; i++)
      pthread_create(&readers[i], NULL, reader_main, &results[i]);

   for (n = 1; n <= UPDATES; n++)
   {
      retro_seqlock_write_begin(&lock);
      for (i = 0; i < FIELDS; i++)
         status.field[i] = n * (i + 1);
      retro_seqlock_write_end(&lock);
   }

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

   take(&final_copy);
   CHECK(final_copy.field[0] == UPDATES,
         "last copy is update %llu, not %d",
         (unsigned long long)final_copy.field[0], UPDATES);

   if (failures)
   {
      printf("%u check(s) failed\n", failures);
      return 1;
   }
   printf("all checks passed\n");
   return 0;
}
