#ifndef __SSNES_GENERAL_H
#define __SSNES_GENERAL_H

#include <stdbool.h>
#include <samplerate.h>

#define SSNES_LOG(msg, args...) do { \
   if (verbose) \
      fprintf(stderr, "SSNES: " msg, ##args); \
   } while(0)

#define SSNES_ERR(msg, args...) do { \
   fprintf(stderr, "SSNES [ERROR] :: " msg, ##args); \
   } while(0)

extern bool verbose;
extern SRC_STATE *source;

#endif
