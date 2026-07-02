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

/* Initializes a low level decoder. */
void rmp3dec_init(rmp3dec *dec);

/* Reads a frame from a low level decoder. */
int rmp3dec_decode_frame(rmp3dec *dec, const unsigned char *mp3, int mp3_bytes, short *pcm, rmp3dec_frame_info *info);


/* Main API (Pull API)
 * ===================*/

typedef struct rmp3_src rmp3_src;
typedef uint64_t (* rmp3_src_read_proc)(rmp3_src* pSRC, uint64_t frameCount, void* pFramesOut, void* pUserData); /* Returns the number of frames that were read. */

typedef enum
{
    rmp3_src_algorithm_none,
    rmp3_src_algorithm_linear
} rmp3_src_algorithm;

#define RMP3_SRC_CACHE_SIZE_IN_FRAMES    512
typedef struct
{
    rmp3_src* pSRC;
    float pCachedFrames[2 * RMP3_SRC_CACHE_SIZE_IN_FRAMES];
    uint32_t cachedFrameCount;
    uint32_t iNextFrame;
} rmp3_src_cache;

typedef struct
{
    uint32_t sampleRateIn;
    uint32_t sampleRateOut;
    uint32_t channels;
    rmp3_src_algorithm algorithm;
    uint32_t cacheSizeInFrames;  /* The number of frames to read from the client at a time. */
} rmp3_src_config;

struct rmp3_src
{
    rmp3_src_config config;
    rmp3_src_read_proc onRead;
    void* pUserData;
    float bin[256];
    rmp3_src_cache cache;    /* <-- For simplifying and optimizing client -> memory reading. */
    union
    {
        struct
        {
            float alpha;
            uint32_t isPrevFramesLoaded : 1;
            uint32_t isNextFramesLoaded : 1;
        } linear;
    } algo;
};

typedef enum
{
    rmp3_seek_origin_start,
    rmp3_seek_origin_current
} rmp3_seek_origin;

/* Callback for when data is read. Return value is the number of bytes actually read.
 *
 * pUserData   [in]  The user data that was passed to rmp3_init(), rmp3_open() and family.
 * pBufferOut  [out] The output buffer.
 * bytesToRead [in]  The number of bytes to read.
 *
 * Returns the number of bytes actually read.
 *
 * A return value of less than bytesToRead indicates the end of the stream. Do _not_ return from this callback until
 * either the entire bytesToRead is filled or you have reached the end of the stream.
 */
typedef size_t (* rmp3_read_proc)(void* pUserData, void* pBufferOut, size_t bytesToRead);

/* Callback for when data needs to be seeked.
 *
 * pUserData [in] The user data that was passed to rmp3_init(), rmp3_open() and family.
 * offset    [in] The number of bytes to move, relative to the origin. Will never be negative.
 * origin    [in] The origin of the seek - the current position or the start of the stream.
 *
 * Returns whether or not the seek was successful.
 *
 * Whether or not it is relative to the beginning or current position is determined by the "origin" parameter which
 * will be either rmp3_seek_origin_start or rmp3_seek_origin_current.
 */
typedef uint32_t (* rmp3_seek_proc)(void* pUserData, int offset, rmp3_seek_origin origin);

typedef struct
{
    uint32_t outputChannels;
    uint32_t outputSampleRate;
} rmp3_config;

typedef struct
{
    rmp3dec decoder;
    rmp3dec_frame_info frameInfo;
    uint32_t channels;
    uint32_t sampleRate;
    rmp3_read_proc onRead;
    rmp3_seek_proc onSeek;
    void* pUserData;
    uint32_t frameChannels;     /* The number of channels in the currently loaded MP3 frame. Internal use only. */
    uint32_t frameSampleRate;   /* The sample rate of the currently loaded MP3 frame. Internal use only */
    uint32_t framesConsumed;
    uint32_t framesRemaining;
    int16_t frames[RMP3_MAX_SAMPLES_PER_FRAME];
    rmp3_src src;
    size_t dataSize;
    size_t dataCapacity;
    uint8_t* pData;
    uint32_t atEnd : 1;
    struct
    {
        const uint8_t* pData;
        size_t dataSize;
        size_t currentReadPos;
    } memory;   /* Only used for decoders that were opened against a block of memory. */
} rmp3;

/* Initializes an MP3 decoder.
 *
 * onRead    [in]           The function to call when data needs to be read from the client.
 * onSeek    [in]           The function to call when the read position of the client data needs to move.
 * pUserData [in, optional] A pointer to application defined data that will be passed to onRead and onSeek.
 *
 * Returns true if successful; false otherwise.
 *
 * Close the loader with rmp3_uninit().
 *
 * See also: rmp3_init_file(), rmp3_init_memory(), rmp3_uninit()
 */
uint32_t rmp3_init(rmp3* pMP3, rmp3_read_proc onRead, rmp3_seek_proc onSeek, void* pUserData, const rmp3_config* pConfig);

/* Initializes an MP3 decoder from a block of memory.
 *
 * This does not create a copy of the data. It is up to the application to ensure the buffer remains valid for
 * the lifetime of the rmp3 object.
 *
 * The buffer should contain the contents of the entire MP3 file.
 */
uint32_t rmp3_init_memory(rmp3* pMP3, const void* pData, size_t dataSize, const rmp3_config* pConfig);

/* Uninitializes an MP3 decoder. */
void rmp3_uninit(rmp3* pMP3);

/* Reads PCM frames as interleaved 32-bit IEEE floating point PCM.
 *
 * Note that framesToRead specifies the number of PCM frames to read, _not_ the number of MP3 frames.
 */
uint64_t rmp3_read_f32(rmp3* pMP3, uint64_t framesToRead, float* pBufferOut);

/* Seeks to a specific frame.
 *
 * Note that this is _not_ an MP3 frame, but rather a PCM frame.
 */
uint32_t rmp3_seek_to_frame(rmp3* pMP3, uint64_t frameIndex);


/* Frees any memory that was allocated by a public rmp3 API. */
void rmp3_free(void* p);

#ifdef __cplusplus
}
#endif
#endif  /* rmp3_h */
