#ifndef rmp3_h
#define rmp3_h

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h>
#include <stdint.h>
#include <retro_inline.h>

#define RMP3_TRUE       1
#define RMP3_FALSE      0

#define RMP3_MAX_SAMPLES_PER_FRAME (1152*2)


/* Low Level Push API
 * ================== */
typedef struct
{
    int frame_bytes;
    int channels;
    int hz;
    int layer;
    int bitrate_kbps;
} rmp3dec_frame_info;

typedef struct
{
    float mdct_overlap[2][9*32];
    float qmf_state[15*2*32];
    int reserv;
    int free_format_bytes;
    unsigned char header[4];
    unsigned char reserv_buf[511];
} rmp3dec;

/* Main API (Pull API)
 * ===================*/

typedef struct
{
    rmp3dec decoder;
    uint32_t channels;          /* Channel count of the stream (from the first frame). */
    uint32_t sampleRate;        /* Sample rate of the stream (from the first frame). */
    uint32_t frameChannels;     /* Channels in the currently decoded frame. Internal. */
    uint32_t framesConsumed;    /* PCM frames of the current block already returned. Internal. */
    uint32_t framesRemaining;   /* PCM frames of the current block still to return. Internal. */
    uint32_t f32_mode;          /* 0: synthesis outputs s16; 1: native float. Internal. */
    union
    {
        int16_t s16[RMP3_MAX_SAMPLES_PER_FRAME];
        float   f32[RMP3_MAX_SAMPLES_PER_FRAME];
    } frames;                   /* Current decoded frame in the latched format. */
    const uint8_t* pData;       /* Caller's buffer (borrowed, never freed). */
    size_t dataSize;
    size_t readPos;             /* Read cursor into pData. */
    uint32_t atEnd;
} rmp3;


/* Initializes an MP3 decoder from a block of memory.
 *
 * This does not create a copy of the data. It is up to the application to ensure the buffer remains valid for
 * the lifetime of the rmp3 object.
 *
 * The buffer should contain the contents of the entire MP3 file.
 */
uint32_t rmp3_init_memory(rmp3* pMP3, const void* pData, size_t dataSize);

/* Uninitializes an MP3 decoder. */
void rmp3_uninit(rmp3* pMP3);

/* Reads PCM frames as interleaved 32-bit IEEE floating point PCM.
 *
 * Note that framesToRead specifies the number of PCM frames to read, _not_ the number of MP3 frames.
 */
uint64_t rmp3_read_f32(rmp3* pMP3, uint64_t framesToRead, float* pBufferOut);

/* Reads PCM frames as native signed 16-bit; the decoder's synthesis
 * quantises directly, with no float round-trip. A given decoder
 * instance is normally used with one output format; switching formats
 * re-decodes the currently buffered frame. */
uint64_t rmp3_read_s16(rmp3* pMP3, uint64_t framesToRead, int16_t* pBufferOut);

/* Seeks to a specific frame.
 *
 * Note that this is _not_ an MP3 frame, but rather a PCM frame.
 */
uint32_t rmp3_seek_to_frame(rmp3* pMP3, uint64_t frameIndex);


/* Frees any memory that was allocated by a public rmp3 API. */

#ifdef __cplusplus
}
#endif
#endif  /* rmp3_h */
