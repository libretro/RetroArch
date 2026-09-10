/* The driver's copy of the kernel PCM ABI against the kernel's own
 * <sound/asound.h>: every struct the same size with its fields at the
 * same offsets, every constant the same value, every ioctl the same
 * number. The copy exists because an embedded toolchain need not have
 * that header and because alsa-lib cannot share a translation unit
 * with it; this is what keeps the copy honest. */
#include <stdio.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/ioctl.h>
#include <sound/asound.h>

/* the driver's definitions, taken from the file itself */
#define EALSA_ABI_CHECK
#include "ealsa_abi.h"

static unsigned bad;
#define SAME(a, b) do { if ((long)(a) != (long)(b)) { \
   printf("      FAIL: %s (%ld) != %s (%ld)\n", #a, (long)(a), #b, (long)(b)); bad++; } } while (0)

int main(void)
{
   printf("   the ABI copy against <sound/asound.h>\n");
   SAME(sizeof(struct ealsa_interval),  sizeof(struct snd_interval));
   SAME(sizeof(struct ealsa_mask),      sizeof(struct snd_mask));
   SAME(sizeof(struct ealsa_hw_params), sizeof(struct snd_pcm_hw_params));
   SAME(sizeof(struct ealsa_sw_params), sizeof(struct snd_pcm_sw_params));
   SAME(sizeof(struct ealsa_xferi),     sizeof(struct snd_xferi));

   SAME(offsetof(struct ealsa_hw_params, masks),     offsetof(struct snd_pcm_hw_params, masks));
   SAME(offsetof(struct ealsa_hw_params, intervals), offsetof(struct snd_pcm_hw_params, intervals));
   SAME(offsetof(struct ealsa_hw_params, rmask),     offsetof(struct snd_pcm_hw_params, rmask));
   SAME(offsetof(struct ealsa_hw_params, cmask),     offsetof(struct snd_pcm_hw_params, cmask));
   SAME(offsetof(struct ealsa_hw_params, info),      offsetof(struct snd_pcm_hw_params, info));
   SAME(offsetof(struct ealsa_hw_params, fifo_size), offsetof(struct snd_pcm_hw_params, fifo_size));

   SAME(offsetof(struct ealsa_sw_params, avail_min),         offsetof(struct snd_pcm_sw_params, avail_min));
   SAME(offsetof(struct ealsa_sw_params, start_threshold),   offsetof(struct snd_pcm_sw_params, start_threshold));
   SAME(offsetof(struct ealsa_sw_params, stop_threshold),    offsetof(struct snd_pcm_sw_params, stop_threshold));
   SAME(offsetof(struct ealsa_sw_params, silence_threshold), offsetof(struct snd_pcm_sw_params, silence_threshold));
   SAME(offsetof(struct ealsa_sw_params, boundary),          offsetof(struct snd_pcm_sw_params, boundary));

   SAME(offsetof(struct ealsa_xferi, buf),    offsetof(struct snd_xferi, buf));
   SAME(offsetof(struct ealsa_xferi, frames), offsetof(struct snd_xferi, frames));

   SAME(EALSA_MASK_MAX,             SNDRV_MASK_MAX);
   SAME(EALSA_P_ACCESS,             SNDRV_PCM_HW_PARAM_ACCESS);
   SAME(EALSA_P_FORMAT,             SNDRV_PCM_HW_PARAM_FORMAT);
   SAME(EALSA_P_SUBFORMAT,          SNDRV_PCM_HW_PARAM_SUBFORMAT);
   SAME(EALSA_P_SAMPLE_BITS,        SNDRV_PCM_HW_PARAM_SAMPLE_BITS);
   SAME(EALSA_P_CHANNELS,           SNDRV_PCM_HW_PARAM_CHANNELS);
   SAME(EALSA_P_RATE,               SNDRV_PCM_HW_PARAM_RATE);
   SAME(EALSA_P_PERIOD_SIZE,        SNDRV_PCM_HW_PARAM_PERIOD_SIZE);
   SAME(EALSA_P_PERIODS,            SNDRV_PCM_HW_PARAM_PERIODS);
   SAME(EALSA_P_BUFFER_SIZE,        SNDRV_PCM_HW_PARAM_BUFFER_SIZE);
   SAME(EALSA_P_TICK_TIME,          SNDRV_PCM_HW_PARAM_TICK_TIME);
   SAME(EALSA_ACCESS_RW_INTERLEAVED, SNDRV_PCM_ACCESS_RW_INTERLEAVED);
   SAME(EALSA_FMT_S16_LE,           SNDRV_PCM_FORMAT_S16_LE);
   SAME(EALSA_FMT_S16_BE,           SNDRV_PCM_FORMAT_S16_BE);
   SAME(EALSA_FMT_FLOAT_LE,         SNDRV_PCM_FORMAT_FLOAT_LE);
   SAME(EALSA_FMT_FLOAT_BE,         SNDRV_PCM_FORMAT_FLOAT_BE);
   SAME(EALSA_INFO_PAUSE,           SNDRV_PCM_INFO_PAUSE);
   SAME(EALSA_TSTAMP_NONE,          SNDRV_PCM_TSTAMP_NONE);

   SAME(EALSA_IOCTL_HW_REFINE,      SNDRV_PCM_IOCTL_HW_REFINE);
   SAME(EALSA_IOCTL_HW_PARAMS,      SNDRV_PCM_IOCTL_HW_PARAMS);
   SAME(EALSA_IOCTL_SW_PARAMS,      SNDRV_PCM_IOCTL_SW_PARAMS);
   SAME(EALSA_IOCTL_DELAY,          SNDRV_PCM_IOCTL_DELAY);
   SAME(EALSA_IOCTL_PREPARE,        SNDRV_PCM_IOCTL_PREPARE);
   SAME(EALSA_IOCTL_START,          SNDRV_PCM_IOCTL_START);
   SAME(EALSA_IOCTL_DROP,           SNDRV_PCM_IOCTL_DROP);
   SAME(EALSA_IOCTL_PAUSE,          SNDRV_PCM_IOCTL_PAUSE);
   SAME(EALSA_IOCTL_WRITEI_FRAMES,  SNDRV_PCM_IOCTL_WRITEI_FRAMES);

   if (bad) { printf("%u ABI mismatch(es)\n", bad); return 1; }
   printf("      every struct, constant and ioctl matches the kernel's\n");
   return 0;
}
