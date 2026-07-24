/*
FLAC audio decoder. Choice of public domain or MIT-0. See license statements at the end of this file.
rflac - v0.12.42 - 2023-11-02

David Reid - mackron@gmail.com

GitHub: https://github.com/mackron/dr_libs
*/

#ifndef rflac_h
#define rflac_h

#ifdef __cplusplus
extern "C" {
#endif

#include <stddef.h> /* For size_t. */
/* Fixed-width integer types come straight from <stdint.h> rather than a
 * private set of aliases. RetroArch guarantees stdint on every target
 * (via its compat shim for legacy MSVC), so no fallback typedefs are
 * needed here. */
#include <stdint.h>

/*
As data is read from the client it is placed into an internal buffer for fast access. This controls the size of that buffer. Larger values means more speed,
but also more memory. In my testing there is diminishing returns after about 4KB, but you can fiddle with this to suit your own needs. Must be a multiple of 8.
*/
#ifndef RFLAC_BUFFER_SIZE
#define RFLAC_BUFFER_SIZE   4096
#endif

/* Architecture Detection */
#if defined(_WIN64) || defined(_LP64) || defined(__LP64__)
#define RFLAC_64BIT
#endif

#if defined(__x86_64__) || defined(_M_X64)
    #define RFLAC_X64
#elif defined(__i386) || defined(_M_IX86)
    #define RFLAC_X86
#elif defined(__arm__) || defined(_M_ARM) || defined(__arm64) || defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64)
    #define RFLAC_ARM
#endif
/* End Architecture Detection */


/* Bitstream cache word: register-width unsigned, i.e. size_t. */

/* The various metadata block types. */
#define RFLAC_METADATA_BLOCK_TYPE_STREAMINFO       0
#define RFLAC_METADATA_BLOCK_TYPE_PADDING          1
#define RFLAC_METADATA_BLOCK_TYPE_APPLICATION      2
#define RFLAC_METADATA_BLOCK_TYPE_SEEKTABLE        3
#define RFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT   4
#define RFLAC_METADATA_BLOCK_TYPE_CUESHEET         5
#define RFLAC_METADATA_BLOCK_TYPE_PICTURE          6
#define RFLAC_METADATA_BLOCK_TYPE_INVALID          127


typedef enum
{
    rflac_seek_origin_start,
    rflac_seek_origin_current
} rflac_seek_origin;

/* The order of members in this structure is important because we map this directly to the raw data within the SEEKTABLE metadata block. */
typedef struct
{
    uint64_t firstPCMFrame;
    uint64_t flacFrameOffset;   /* The offset from the first byte of the header of the first frame. */
    uint16_t pcmFrameCount;
} rflac_seekpoint;

typedef struct
{
    uint16_t minBlockSizeInPCMFrames;
    uint16_t maxBlockSizeInPCMFrames;
    uint32_t minFrameSizeInPCMFrames;
    uint32_t maxFrameSizeInPCMFrames;
    uint32_t sampleRate;
    uint8_t  channels;
    uint8_t  bitsPerSample;
    uint64_t totalPCMFrameCount;
    uint8_t  md5[16];
} rflac_streaminfo;

typedef struct
{
    /*
    The metadata type. Use this to know how to interpret the data below. Will be set to one of the
    RFLAC_METADATA_BLOCK_TYPE_* tokens.
    */
    uint32_t type;

    /*
    A pointer to the raw data. This points to a temporary buffer so don't hold on to it. It's best to
    not modify the contents of this buffer. Use the structures below for more meaningful and structured
    information about the metadata. It's possible for this to be null.
    */
    const void* pRawData;

    /* The size in bytes of the block and the buffer pointed to by pRawData if it's non-NULL. */
    uint32_t rawDataSize;

    union
    {
        rflac_streaminfo streaminfo;

        struct
        {
            uint32_t id;
            const void* pData;
            uint32_t dataSize;
        } application;

        struct
        {
            uint32_t seekpointCount;
            const rflac_seekpoint* pSeekpoints;
        } seektable;

        struct
        {
            uint32_t vendorLength;
            const char* vendor;
            uint32_t commentCount;
            const void* pComments;
        } vorbis_comment;

        struct
        {
            char catalog[128];
            uint64_t leadInSampleCount;
            uint32_t isCD;
            uint8_t trackCount;
            const void* pTrackData;
        } cuesheet;

        struct
        {
            uint32_t type;
            uint32_t mimeLength;
            const char* mime;
            uint32_t descriptionLength;
            const char* description;
            uint32_t width;
            uint32_t height;
            uint32_t colorDepth;
            uint32_t indexColorCount;
            uint32_t pictureDataSize;
            const uint8_t* pPictureData;
        } picture;
    } data;
} rflac_metadata;


/*
Callback for when data needs to be read from the client.


Parameters
----------
pUserData (in)
    The user data that was passed to rflac_open_memory() and family.

pBufferOut (out)
    The output buffer.

bytesToRead (in)
    The number of bytes to read.


Return Value
------------
The number of bytes actually read.


Remarks
-------
A return value of less than bytesToRead indicates the end of the stream. Do _not_ return from this callback until either the entire bytesToRead is filled or
you have reached the end of the stream.
*/
typedef size_t (* rflac_read_proc)(void* pUserData, void* pBufferOut, size_t bytesToRead);

/*
Callback for when data needs to be seeked.


Parameters
----------
pUserData (in)
    The user data that was passed to rflac_open_memory() and family.

offset (in)
    The number of bytes to move, relative to the origin. Will never be negative.

origin (in)
    The origin of the seek - the current position or the start of the stream.


Return Value
------------
Whether or not the seek was successful.


Remarks
-------
The offset will never be negative. Whether or not it is relative to the beginning or current position is determined by the "origin" parameter which will be
either rflac_seek_origin_start or rflac_seek_origin_current.

When seeking to a PCM frame using rflac_seek_to_pcm_frame(), rflac may call this with an offset beyond the end of the FLAC stream. This needs to be detected
and handled by returning 0.
*/
typedef uint32_t (* rflac_seek_proc)(void* pUserData, int offset, rflac_seek_origin origin);

/*
Callback for when a metadata block is read.


Parameters
----------
pUserData (in)
    The user data that was passed to rflac_open_memory() and family.

pMetadata (in)
    A pointer to a structure containing the data of the metadata block.


Remarks
-------
Use pMetadata->type to determine which metadata block is being handled and how to read the data. This
will be set to one of the RFLAC_METADATA_BLOCK_TYPE_* tokens.
*/
typedef void (* rflac_meta_proc)(void* pUserData, rflac_metadata* pMetadata);


/* Structure for internal use. Only used for decoders opened with rflac_open_memory. */
typedef struct
{
    const uint8_t* data;
    size_t dataSize;
    size_t currentReadPos;
} rflac__memory_stream;

/* Structure for internal use. Used for bit streaming. */
typedef struct
{
    /* The function to call when more data needs to be read. */
    rflac_read_proc onRead;

    /* The function to call when the current read position needs to be moved. */
    rflac_seek_proc onSeek;

    /* The user data to pass around to onRead and onSeek. */
    void* pUserData;


    /*
    The number of unaligned bytes in the L2 cache. This will always be 0 until the end of the stream is hit. At the end of the
    stream there will be a number of bytes that don't cleanly fit in an L1 cache line, so we use this variable to know whether
    or not the bistreamer needs to run on a slower path to read those last bytes. This will never be more than sizeof(size_t).
    */
    size_t unalignedByteCount;

    /* The content of the unaligned bytes. */
    size_t unalignedCache;

    /* The index of the next valid cache line in the "L2" cache. */
    uint32_t nextL2Line;

    /* The number of bits that have been consumed by the cache. This is used to determine how many valid bits are remaining. */
    uint32_t consumedBits;

    /*
    The cached data which was most recently read from the client. There are two levels of cache. Data flows as such:
    Client -> L2 -> L1. The L2 -> L1 movement is aligned and runs on a fast path in just a few instructions.
    */
    size_t cacheL2[RFLAC_BUFFER_SIZE/sizeof(size_t)];
    size_t cache;

    /*
    CRC-16. This is updated whenever bits are read from the bit stream. Manually set this to 0 to reset the CRC. For FLAC, this
    is reset to 0 at the beginning of each frame.
    */
    uint16_t crc16;
    size_t crc16Cache;              /* A cache for optimizing CRC calculations. This is filled when when the L1 cache is reloaded. */
    uint32_t crc16CacheIgnoredBytes;   /* The number of bytes to ignore when updating the CRC-16 from the CRC-16 cache. */
} rflac_bs;

typedef struct
{
    /* The type of the subframe: SUBFRAME_CONSTANT, SUBFRAME_VERBATIM, SUBFRAME_FIXED or SUBFRAME_LPC. */
    uint8_t subframeType;

    /* The number of wasted bits per sample as specified by the sub-frame header. */
    uint8_t wastedBitsPerSample;

    /* The order to use for the prediction stage for SUBFRAME_FIXED and SUBFRAME_LPC. */
    uint8_t lpcOrder;

    /* A pointer to the buffer containing the decoded samples in the subframe. This pointer is an offset from rflac::pExtraData. */
    int32_t* pSamplesS32;
} rflac_subframe;

typedef struct
{
    /*
    If the stream uses variable block sizes, this will be set to the index of the first PCM frame. If fixed block sizes are used, this will
    always be set to 0. This is 64-bit because the decoded PCM frame number will be 36 bits.
    */
    uint64_t pcmFrameNumber;

    /*
    If the stream uses fixed block sizes, this will be set to the frame number. If variable block sizes are used, this will always be 0. This
    is 32-bit because in fixed block sizes, the maximum frame number will be 31 bits.
    */
    uint32_t flacFrameNumber;

    /* The sample rate of this frame. */
    uint32_t sampleRate;

    /* The number of PCM frames in each sub-frame within this frame. */
    uint16_t blockSizeInPCMFrames;

    /*
    The channel assignment of this frame. This is not always set to the channel count. If interchannel decorrelation is being used this
    will be set to RFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE, RFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE or RFLAC_CHANNEL_ASSIGNMENT_MID_SIDE.
    */
    uint8_t channelAssignment;

    /* The number of bits per sample within this frame. */
    uint8_t bitsPerSample;

    /* The frame's CRC. */
    uint8_t crc8;
} rflac_frame_header;

typedef struct
{
    /* The header. */
    rflac_frame_header header;

    /*
    The number of PCM frames left to be read in this FLAC frame. This is initially set to the block size. As PCM frames are read,
    this will be decremented. When it reaches 0, the decoder will see this frame as fully consumed and load the next frame.
    */
    uint32_t pcmFramesRemaining;

    /* The list of sub-frames within the frame. There is one sub-frame for each channel, and there's a maximum of 8 channels. */
    rflac_subframe subframes[8];
} rflac_frame;

typedef struct
{
    /* The function to call when a metadata block is read. */
    rflac_meta_proc onMeta;

    /* The user data posted to the metadata callback function. */
    void* pUserDataMD;


    /* The sample rate. Will be set to something like 44100. */
    uint32_t sampleRate;

    /*
    The number of channels. This will be set to 1 for monaural streams, 2 for stereo, etc. Maximum 8. This is set based on the
    value specified in the STREAMINFO block.
    */
    uint8_t channels;

    /* The bits per sample. Will be set to something like 16, 24, etc. */
    uint8_t bitsPerSample;

    /* The maximum block size, in samples. This number represents the number of samples in each channel (not combined). */
    uint16_t maxBlockSizeInPCMFrames;

    /*
    The total number of PCM Frames making up the stream. Can be 0 in which case it's still a valid stream, but just means
    the total PCM frame count is unknown. Likely the case with streams like internet radio.
    */
    uint64_t totalPCMFrameCount;


    /* The number of seekpoints in the seektable. */
    uint32_t seekpointCount;


    /* Information about the frame the decoder is currently sitting on. */
    rflac_frame currentFLACFrame;


    /* The index of the PCM frame the decoder is currently sitting on. This is only used for seeking. */
    uint64_t currentPCMFrame;

    /* The position of the first FLAC frame in the stream. This is only ever used for seeking. */
    uint64_t firstFLACFramePosInBytes;


    /* A hack to avoid a malloc() when opening a decoder with rflac_open_memory(). */
    rflac__memory_stream memoryStream;


    /* A pointer to the decoded sample data. This is an offset of pExtraData. */
    int32_t* pDecodedSamples;

    /* 64-bit plane for the 33-bit stereo difference channel of 32-bit
     * streams; NULL otherwise. This is an offset of pExtraData. */
    int64_t* pWideSamples;

    /* Subframe index of the wide (33-bit) channel in the current frame, or
     * 0xFF when the current frame has none. */
    uint8_t wideChannelIndex;

    /* A pointer to the seek table. This is an offset of pExtraData, or NULL if there is no seek table. */
    rflac_seekpoint* pSeekpoints;

    /* Internal use only. Used for profiling and testing different seeking modes. */

    /* The bit streamer. The raw FLAC data is fed through this object. */
    rflac_bs bs;

    /* Variable length extra data. We attach this to the end of the object so we can avoid unnecessary mallocs. */
    uint8_t pExtraData[1];
} rflac;



/*
Opens a FLAC decoder and notifies the caller of the metadata chunks (album art, etc.).


Parameters
----------
onRead (in)
    The function to call when data needs to be read from the client.

onSeek (in)
    The function to call when the read position of the client data needs to move.

onMeta (in)
    The function to call for every metadata block.

pUserData (in, optional)
    A pointer to application defined data that will be passed to onRead, onSeek and onMeta.


Return Value
------------
A pointer to an object representing the decoder.


Remarks
-------
Close the decoder with `rflac_close()`.


Internally, this will allocate and free memory on the heap for every
metadata block except for STREAMINFO and PADDING blocks.

The caller is notified of the metadata via the `onMeta` callback. All metadata blocks will be handled before the function returns. This callback takes a
pointer to a `rflac_metadata` object which is a union containing the data of all relevant metadata blocks. Use the `type` member to discriminate against
the different metadata types.

The STREAMINFO block must be present for this to succeed.


Seek Also
---------
rflac_open_memory()
rflac_close()
*/
rflac* rflac_open_with_metadata(rflac_read_proc onRead, rflac_seek_proc onSeek, rflac_meta_proc onMeta, void* pUserData);

/*
Closes the given FLAC decoder.


Parameters
----------
pFlac (in)
    The decoder to close.


Remarks
-------
This will destroy the decoder object.


See Also
--------
rflac_open_memory()
rflac_open_with_metadata()
*/
void rflac_close(rflac* pFlac);

/*
Reads sample data from the given FLAC decoder, output as interleaved signed 16-bit PCM.


Parameters
----------
pFlac (in)
    The decoder.

framesToRead (in)
    The number of PCM frames to read.

pBufferOut (out, optional)
    A pointer to the buffer that will receive the decoded samples.


Return Value
------------
Returns the number of PCM frames actually read. If the return value is less than `framesToRead` it has reached the end.


Remarks
-------
pBufferOut can be null, in which case the call will act as a seek, and the return value will be the number of frames seeked.

Note that this is lossy for streams where the bits per sample is larger than 16.
*/
uint64_t rflac_read_pcm_frames_s16(rflac* pFlac, uint64_t framesToRead, int16_t* pBufferOut);

/*
Reads sample data from the given FLAC decoder, output as interleaved 32-bit floating point PCM.


Parameters
----------
pFlac (in)
    The decoder.

framesToRead (in)
    The number of PCM frames to read.

pBufferOut (out, optional)
    A pointer to the buffer that will receive the decoded samples.


Return Value
------------
Returns the number of PCM frames actually read. If the return value is less than `framesToRead` it has reached the end.


Remarks
-------
pBufferOut can be null, in which case the call will act as a seek, and the return value will be the number of frames seeked.

Note that this should be considered lossy due to the nature of floating point numbers not being able to exactly represent every possible number.
*/
uint64_t rflac_read_pcm_frames_f32(rflac* pFlac, uint64_t framesToRead, float* pBufferOut);

/*
Seeks to the PCM frame at the given index.


Parameters
----------
pFlac (in)
    The decoder.

pcmFrameIndex (in)
    The index of the PCM frame to seek to. See notes below.


Return Value
-------------
1 if successful; 0 otherwise.
*/
uint32_t rflac_seek_to_pcm_frame(rflac* pFlac, uint64_t pcmFrameIndex);

/*
Opens a FLAC decoder from a pre-allocated block of memory


Parameters
----------
pData (in)
    A pointer to the raw encoded FLAC data.

dataSize (in)
    The size in bytes of `data`.


Return Value
------------
A pointer to an object representing the decoder.


Remarks
-------
This does not create a copy of the data. It is up to the application to ensure the buffer remains valid for the lifetime of the decoder.


See Also
--------
rflac_open_memory()
rflac_close()
*/
rflac* rflac_open_memory(const void* pData, size_t dataSize);

/* High Level APIs */

/* Structure representing an iterator for vorbis comments in a VORBIS_COMMENT metadata block. */
typedef struct
{
    uint32_t countRemaining;
    const char* pRunningData;
} rflac_vorbis_comment_iterator;

/* Structure representing an iterator for cuesheet tracks in a CUESHEET metadata block. */
typedef struct
{
    uint32_t countRemaining;
    const char* pRunningData;
} rflac_cuesheet_track_iterator;

/* The order of members here is important because we map this directly to the raw data within the CUESHEET metadata block. */
typedef struct
{
    uint64_t offset;
    uint8_t index;
    uint8_t reserved[3];
} rflac_cuesheet_track_index;

typedef struct
{
    uint64_t offset;
    uint8_t trackNumber;
    char ISRC[12];
    uint8_t isAudio;
    uint8_t preEmphasis;
    uint8_t indexCount;
    const rflac_cuesheet_track_index* pIndexPoints;
} rflac_cuesheet_track;

#ifdef __cplusplus
}
#endif
#endif  /* rflac_h */
