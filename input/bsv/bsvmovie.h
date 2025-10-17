#ifndef __BSV_MOVIE__H
#define __BSV_MOVIE__H

#include <sys/types.h>
#include "../input_driver.h"
#include <retro_common_api.h>

#define REPLAY_HEADER_MAGIC_INDEX        0
#define REPLAY_HEADER_VERSION_INDEX      1
#define REPLAY_HEADER_CRC_INDEX          2
#define REPLAY_HEADER_STATE_SIZE_INDEX   3
/* Identifier is int64_t, so takes up two slots */
#define REPLAY_HEADER_IDENTIFIER_INDEX   4
#define REPLAY_HEADER_FRAME_COUNT_INDEX  6
#define REPLAY_HEADER_BLOCK_SIZE_INDEX   7
#define REPLAY_HEADER_SUPERBLOCK_SIZE_INDEX 8
#define REPLAY_HEADER_CHECKPOINT_CONFIG_INDEX 9
#define REPLAY_HEADER_LEN                10
#define REPLAY_HEADER_LEN_BYTES          (REPLAY_HEADER_LEN*4)

#define REPLAY_HEADER_V0V1_LEN           6
#define REPLAY_HEADER_V0V1_LEN_BYTES     (REPLAY_HEADER_V0V1_LEN*4)

#define REPLAY_FORMAT_VERSION            2
#define REPLAY_MAGIC                     0x42535632


RETRO_BEGIN_DECLS

void bsv_movie_poll(input_driver_state_t *input_st);
int16_t bsv_movie_read_state(input_driver_state_t *input_st,
                             unsigned port, unsigned device,
                             unsigned idx, unsigned id);
void bsv_movie_push_key_event(bsv_movie_t *movie,
                              uint8_t down, uint16_t mod, uint32_t code, uint32_t character);
void bsv_movie_push_input_event(bsv_movie_t *movie,
                                uint8_t port, uint8_t dev, uint8_t idx, uint16_t id, int16_t val);

bool bsv_movie_load_checkpoint(bsv_movie_t *movie,
      uint8_t compression, uint8_t encoding,
      replay_checkpoint_behavior cpbehavior);
int64_t bsv_movie_write_checkpoint(bsv_movie_t *movie,
      uint8_t compression, uint8_t encoding);

RETRO_END_DECLS

#endif /* __BSV_MOVIE__H */
