#include "driver.h"
#include <stdlib.h>
#include <rsound.h>

typedef struct rsd
{
   rsound_t *rd;
   int latency;
   int rate;
} rsd_t;

static void* __rsd_init(const char* device, int rate, int latency)
{
   rsd_t *rsd = calloc(1, sizeof(rsd_t));
   if ( rsd == NULL )
      return NULL;

   rsound_t *rd;

   if ( rsd_init(&rd) < 0 )
   {
      free(rsd);
      return NULL;
   }

   int channels = 2;
   int format = RSD_S16_NE;

   rsd_set_param(rd, RSD_CHANNELS, &channels);
   rsd_set_param(rd, RSD_SAMPLERATE, &rate);

   if ( device != NULL )
      rsd_set_param(rd, RSD_HOST, (void*)device);

   rsd_set_param(rd, RSD_FORMAT, &format);

   if ( rsd_start(rd) < 0 )
   {
      free(rsd);
      rsd_free(rd);
      return NULL;
   }

   int min_latency = (rsd_delay_ms(rd) > latency) ? (rsd_delay_ms(rd) * 3 / 2) : latency;

   rsd_set_param(rd, RSD_LATENCY, &min_latency);

   rsd->rd = rd;
   rsd->latency = min_latency;
   rsd->rate = rate;

   return rsd;
}

static ssize_t __rsd_write(void* data, const void* buf, size_t size)
{
   rsd_t *rsd = data;

   if ( size == 0 )
      return 0;

   rsd_delay_wait(rsd->rd);
   if ( rsd_write(rsd->rd, buf, size) == 0 )
      return -1;

   if ( rsd_delay_ms(rsd->rd) < rsd->latency/2 )
   {
      int ms = rsd->latency/2;
      size_t size = (ms * rsd->rate * 4) / 1000;
      void *temp = calloc(1, size);
      rsd_write(rsd->rd, temp, size);
      free(temp);
   }

   return size;
}

static bool __rsd_stop(void *data)
{
   rsd_t *rsd = data;
   rsd_stop(rsd->rd);

   return true;
}

static bool __rsd_start(void *data)
{
   rsd_t *rsd = data;
   if ( rsd_start(rsd->rd) < 0)
      return false;

   return true;
}

static void __rsd_free(void *data)
{
   rsd_t *rsd = data;

   rsd_stop(rsd->rd);
   rsd_free(rsd->rd);
   free(rsd);
}

const audio_driver_t audio_rsound = {
   .init = __rsd_init,
   .write = __rsd_write,
   .stop = __rsd_stop,
   .start = __rsd_start,
   .free = __rsd_free
};

   


   
   
