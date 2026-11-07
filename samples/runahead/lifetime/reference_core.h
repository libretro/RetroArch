#ifndef SAVESTATE_LIFETIME_REFERENCE_CORE_H
#define SAVESTATE_LIFETIME_REFERENCE_CORE_H

#include <libretro.h>

struct lifetime_counts
{
   unsigned saves, loads, releases, runs, live;
};

const struct lifetime_counts *lifetime_test_counts(void);
void lifetime_test_refuse_save(void);
void lifetime_test_set_usable(bool usable);

#endif
