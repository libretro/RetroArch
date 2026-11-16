/* queues/retro_deque.h against a model that cannot be wrong (an array,
 * shifted for every change):
 *
 * - two million pushes, pops, inserts and erases at either end and in the
 *   middle, the queue the same as the model after each, for a word-sized
 *   element and for a struct;
 * - a queue holds as many as it was made for, refuses the next and is
 *   left as it was; reserve makes room and keeps the order;
 * - none of it allocates after init, but reserve;
 * - a queue marched along (pushed at one end, popped at the other) for a
 *   million elements never leaves its block. */

static unsigned long allocations;
#define RETRO_DEQUE_MALLOC(bytes) (allocations++, malloc(bytes))

#include <queues/retro_deque.h>

#include <stdio.h>
#include <string.h>

struct deadline
{
   unsigned long when;
   void *who;
   unsigned char tag[5];
};

RETRO_DEQUE_DEFINE(words, unsigned long);
RETRO_DEQUE_DEFINE(deadlines, struct deadline);
RETRO_DEQUE_DEFINE(pointers, struct deadline*);

static int failures;
#define CHECK(cond) do { if (!(cond)) { fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, #cond); failures++; } } while (0)

/* 32 bits of it, wherever unsigned long is longer */
static unsigned long rng_state = 2463534242UL;
static unsigned long rng(void)
{
   rng_state ^= (rng_state << 13) & 0xffffffffUL;
   rng_state ^= rng_state >> 17;
   rng_state ^= (rng_state << 5) & 0xffffffffUL;
   return rng_state;
}

#define MODEL_MAX 64

static void against_model(void)
{
   words_t q;
   unsigned long model[MODEL_MAX];
   size_t n = 0, i;
   unsigned long step, wrong = 0, refused = 0, before;

   CHECK(words_init(&q, MODEL_MAX));
   CHECK(words_limit(&q) == MODEL_MAX);
   before = allocations;

   for (step = 0; step < 2000000; step++)
   {
      unsigned long r = rng();
      unsigned long v = r ^ step;
      size_t at       = n ? (size_t)((r >> 12) % (n + 1)) : 0;

      switch (r % 8)
      {
         case 0:
         case 1:
            if (words_push_back(&q, &v))
            {
               if (n == MODEL_MAX) wrong++;
               else model[n++] = v;
            }
            else
            {
               refused++;
               if (n != MODEL_MAX) wrong++;
            }
            break;
         case 2:
            if (words_push_front(&q, &v))
            {
               if (n == MODEL_MAX) wrong++;
               else
               {
                  memmove(model + 1, model, n * sizeof(model[0]));
                  model[0] = v;
                  n++;
               }
            }
            else if (n != MODEL_MAX) wrong++;
            break;
         case 3:
            if (n)
            {
               if (*words_front(&q) != model[0]) wrong++;
               words_pop_front(&q);
               memmove(model, model + 1, (n - 1) * sizeof(model[0]));
               n--;
            }
            break;
         case 4:
            if (n)
            {
               if (*words_back(&q) != model[n - 1]) wrong++;
               words_pop_back(&q);
               n--;
            }
            break;
         case 5:
            if (words_insert(&q, at, &v))
            {
               if (n == MODEL_MAX) wrong++;
               else
               {
                  memmove(model + at + 1, model + at, (n - at) * sizeof(model[0]));
                  model[at] = v;
                  n++;
               }
            }
            else if (n != MODEL_MAX) wrong++;
            break;
         case 6:
            if (n)
            {
               size_t e = at % n;
               words_erase(&q, e);
               memmove(model + e, model + e + 1, (n - e - 1) * sizeof(model[0]));
               n--;
            }
            break;
         default:
            if ((r >> 16) % 4096 == 0)
            {
               words_clear(&q);
               n = 0;
            }
            break;
      }

      if (words_count(&q) != n || words_empty(&q) != (n == 0) || words_full(&q) != (n == MODEL_MAX))
         wrong++;
      for (i = 0; i < n; i++)
         if (*words_at(&q, i) != model[i])
         {
            wrong++;
            break;
         }
   }

   CHECK(wrong == 0);
   CHECK(refused > 0);
   CHECK(allocations == before);
   printf("retro_deque: 2000000 changes against the model, %lu wrong, %lu refused while full, %lu allocations\n",
         wrong, refused, allocations - before);
   words_free(&q);
   words_free(&q);
}

int main(void)
{
   against_model();

   /* A struct; a full queue left as it was; reserve */
   {
      deadlines_t q;
      struct deadline d, extra;
      size_t i;
      unsigned long before;

      memset(&d, 0, sizeof(d));
      CHECK(deadlines_init(&q, 4));
      for (i = 0; i < 4; i++)
      {
         d.when   = 100 + i;
         d.who    = &q;
         d.tag[4] = (unsigned char)i;
         CHECK(deadlines_push_back(&q, &d));
      }
      extra      = d;
      extra.when = 999;
      CHECK(!deadlines_push_back(&q, &extra));
      CHECK(!deadlines_push_front(&q, &extra));
      CHECK(!deadlines_insert(&q, 2, &extra));
      CHECK(deadlines_count(&q) == 4);
      for (i = 0; i < 4; i++)
         CHECK(deadlines_at(&q, i)->when == 100 + i && deadlines_at(&q, i)->tag[4] == i);

      /* round the ring first, so that reserve has a wrapped queue to move */
      deadlines_pop_front(&q);
      d.when = 104;
      CHECK(deadlines_push_back(&q, &d));

      before = allocations;
      CHECK(deadlines_reserve(&q, 4));          /* has it already */
      CHECK(deadlines_reserve(&q, 2));          /* never fewer */
      CHECK(allocations == before && deadlines_limit(&q) == 4);
      CHECK(deadlines_reserve(&q, 9));
      CHECK(allocations == before + 1);
      CHECK(deadlines_limit(&q) == 9 && deadlines_count(&q) == 4);
      for (i = 0; i < 4; i++)
         CHECK(deadlines_at(&q, i)->when == 101 + i);
      CHECK(deadlines_insert(&q, 4, &extra));   /* at the end */
      CHECK(deadlines_back(&q)->when == 999);
      CHECK(!deadlines_insert(&q, 7, &extra));  /* past the end */
      /* It holds nine, in a block of sixteen: the tenth is refused, and
       * a reserve within the block takes no memory */
      while (deadlines_push_back(&q, &extra)) {}
      CHECK(deadlines_count(&q) == 9);
      before = allocations;
      CHECK(deadlines_reserve(&q, 16) && allocations == before);
      CHECK(deadlines_push_back(&q, &extra) && deadlines_count(&q) == 10);
      deadlines_free(&q);
      CHECK(deadlines_limit(&q) == 0 && deadlines_full(&q));
      CHECK(!deadlines_push_back(&q, &extra));  /* freed: holds nothing */
   }

   /* Pointers as elements: what is pushed is the pointer, through a
    * pointer to it that may be to a const one */
   {
      pointers_t q;
      struct deadline a, b;
      struct deadline *pa = &a;
      struct deadline *const pb = &b;
      CHECK(pointers_init(&q, 3));
      CHECK(pointers_push_back(&q, &pa));
      CHECK(pointers_push_back(&q, &pb));
      CHECK(pointers_push_front(&q, &pb));
      CHECK(!pointers_insert(&q, 1, &pa) || pointers_count(&q) == 4);
      CHECK(*pointers_front(&q) == &b && *pointers_at(&q, 1) == &a && *pointers_back(&q) == &b);
      (*pointers_at(&q, 1))->when = 5;          /* the element is not const */
      CHECK(a.when == 5);
      pointers_erase(&q, 0);
      CHECK(*pointers_front(&q) == &a);
      pointers_free(&q);
   }

   /* A queue with no limit of its own: it grows when it has to, in order,
    * and only then; an empty one that was never made grows too */
   {
      words_t q;
      unsigned long i, before, wrong = 0;

      CHECK(words_init(&q, 4));
      before = allocations;
      for (i = 0; i < 4; i++)
         CHECK(words_push_back_growing(&q, &i));
      CHECK(allocations == before);
      words_pop_front(&q);                       /* wrapped when it grows */
      for (i = 4; i < 100; i++)
         CHECK(words_push_back_growing(&q, &i));
      CHECK(allocations == before + 5);          /* 8, 16, 32, 64, 128 */
      CHECK(words_count(&q) == 99 && words_limit(&q) == 128);
      for (i = 0; i < 99; i++)
         wrong += *words_at(&q, i) != i + 1;
      CHECK(wrong == 0);
      words_free(&q);
      i = 7;
      CHECK(words_push_back_growing(&q, &i) && *words_front(&q) == 7 && words_limit(&q) == 8);
      words_free(&q);
   }

   /* The same from the front: newest first, grown in order */
   {
      words_t q;
      unsigned long i, before, wrong = 0;

      CHECK(words_init(&q, 4));
      before = allocations;
      for (i = 0; i < 100; i++)
         CHECK(words_push_front_growing(&q, &i));
      CHECK(allocations == before + 5);
      CHECK(words_count(&q) == 100 && words_limit(&q) == 128);
      for (i = 0; i < 100; i++)
         wrong += *words_at(&q, i) != 99 - i;
      CHECK(wrong == 0);
      words_free(&q);
      i = 3;
      CHECK(words_push_front_growing(&q, &i) && *words_back(&q) == 3 && words_limit(&q) == 8);
      words_free(&q);
   }

   /* Marched along: a few elements, pushed at one end and popped at the
    * other, a million times. One block, the one it was given. */
   {
      words_t q;
      unsigned long i, before, wrong = 0;
      unsigned long *block;

      CHECK(words_init(&q, 16));
      block  = q.data;
      before = allocations;
      for (i = 0; i < 5; i++)
         words_push_back(&q, &i);
      for (i = 5; i < 1000005; i++)
      {
         if (*words_front(&q) != i - 5)
            wrong++;
         words_pop_front(&q);
         if (!words_push_back(&q, &i))
            wrong++;
      }
      CHECK(wrong == 0);
      CHECK(q.data == block && allocations == before);
      words_free(&q);
   }

   printf("retro_deque: %s\n", failures ? "FAIL" : "ok");
   return failures ? 1 : 0;
}
