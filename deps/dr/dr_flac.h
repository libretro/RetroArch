/*
FLAC audio decoder. Choice of public domain or MIT-0. See license statements at the end of this file.
dr_flac - v0.12.42 - 2023-11-02

David Reid - mackron@gmail.com

GitHub: https://github.com/mackron/dr_libs
*/

#ifndef DR_FLAC_H
#define DR_FLAC_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h> /* For size_t. */

#define DRFLAC_STRINGIFY(x)      #x
#define DRFLAC_XSTRINGIFY(x)     DRFLAC_STRINGIFY(x)

#define DRFLAC_VERSION_MAJOR     0
#define DRFLAC_VERSION_MINOR     12
#define DRFLAC_VERSION_REVISION  42
#define DRFLAC_VERSION_STRING    DRFLAC_XSTRINGIFY(DRFLAC_VERSION_MAJOR) "." DRFLAC_XSTRINGIFY(DRFLAC_VERSION_MINOR) "." DRFLAC_XSTRINGIFY(DRFLAC_VERSION_REVISION)

/* Sized Types */
typedef   signed char           drflac_int8;
typedef unsigned char           drflac_uint8;
typedef   signed short          drflac_int16;
typedef unsigned short          drflac_uint16;
typedef   signed int            drflac_int32;
typedef unsigned int            drflac_uint32;
#if defined(_MSC_VER) && !defined(__clang__)
    typedef   signed __int64    drflac_int64;
    typedef unsigned __int64    drflac_uint64;
#else
    #if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
        #pragma GCC diagnostic push
        #pragma GCC diagnostic ignored "-Wlong-long"
        #if defined(__clang__)
            #pragma GCC diagnostic ignored "-Wc++11-long-long"
        #endif
    #endif
    typedef   signed long long  drflac_int64;
    typedef unsigned long long  drflac_uint64;
    #if defined(__clang__) || (defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 6)))
        #pragma GCC diagnostic pop
    #endif
#endif
#if defined(__LP64__) || defined(_WIN64) || (defined(__x86_64__) && !defined(__ILP32__)) || defined(_M_X64) || defined(__ia64) || defined(_M_IA64) || defined(__aarch64__) || defined(_M_ARM64) || defined(__powerpc64__)
    typedef drflac_uint64       drflac_uintptr;
#else
    typedef drflac_uint32       drflac_uintptr;
#endif
typedef drflac_uint8            drflac_bool8;
typedef drflac_uint32           drflac_bool32;
#define DRFLAC_TRUE             1
#define DRFLAC_FALSE            0
/* End Sized Types */

/*
BIT READING ATTEMPT #2

This uses a 32- or 64-bit bit-shifted cache - as bits are read, the cache is shifted such that the first valid bit is sitting
on the most significant bit. It uses the notion of an L1 and L2 cache (borrowed from CPU architecture), where the L1 cache
is a 32- or 64-bit unsigned integer (depending on whether or not a 32- or 64-bit build is being compiled) and the L2 is an
array of "cache lines", with each cache line being the same size as the L1. The L2 is a buffer of about 4KB and is where data
from onRead() is read into.
*/
#define DRFLAC_CACHE_L1_SIZE_BYTES(bs)                      (sizeof((bs)->cache))
#define DRFLAC_CACHE_L1_SIZE_BITS(bs)                       (sizeof((bs)->cache)*8)
#define DRFLAC_CACHE_L1_BITS_REMAINING(bs)                  (DRFLAC_CACHE_L1_SIZE_BITS(bs) - (bs)->consumedBits)
#define DRFLAC_CACHE_L1_SELECTION_MASK(_bitCount)           (~((~(drflac_cache_t)0) >> (_bitCount)))
#define DRFLAC_CACHE_L1_SELECTION_SHIFT(bs, _bitCount)      (DRFLAC_CACHE_L1_SIZE_BITS(bs) - (_bitCount))
#define DRFLAC_CACHE_L1_SELECT(bs, _bitCount)               (((bs)->cache) & DRFLAC_CACHE_L1_SELECTION_MASK(_bitCount))
#define DRFLAC_CACHE_L1_SELECT_AND_SHIFT(bs, _bitCount)     (DRFLAC_CACHE_L1_SELECT((bs), (_bitCount)) >>  DRFLAC_CACHE_L1_SELECTION_SHIFT((bs), (_bitCount)))
#define DRFLAC_CACHE_L1_SELECT_AND_SHIFT_SAFE(bs, _bitCount)(DRFLAC_CACHE_L1_SELECT((bs), (_bitCount)) >> (DRFLAC_CACHE_L1_SELECTION_SHIFT((bs), (_bitCount)) & (DRFLAC_CACHE_L1_SIZE_BITS(bs)-1)))
#define DRFLAC_CACHE_L2_SIZE_BYTES(bs)                      (sizeof((bs)->cacheL2))
#define DRFLAC_CACHE_L2_LINE_COUNT(bs)                      (DRFLAC_CACHE_L2_SIZE_BYTES(bs) / sizeof((bs)->cacheL2[0]))
#define DRFLAC_CACHE_L2_LINES_REMAINING(bs)                 (DRFLAC_CACHE_L2_LINE_COUNT(bs) - (bs)->nextL2Line)

/* Allocation Callbacks */
typedef struct
{
    void* pUserData;
    void* (* onMalloc)(size_t sz, void* pUserData);
    void* (* onRealloc)(void* p, size_t sz, void* pUserData);
    void  (* onFree)(void* p, void* pUserData);
} drflac_allocation_callbacks;
/* End Allocation Callbacks */

/*
As data is read from the client it is placed into an internal buffer for fast access. This controls the size of that buffer. Larger values means more speed,
but also more memory. In my testing there is diminishing returns after about 4KB, but you can fiddle with this to suit your own needs. Must be a multiple of 8.
*/
#ifndef DR_FLAC_BUFFER_SIZE
#define DR_FLAC_BUFFER_SIZE   4096
#endif


/* Architecture Detection */
#if defined(_WIN64) || defined(_LP64) || defined(__LP64__)
#define DRFLAC_64BIT
#endif

#if defined(__x86_64__) || defined(_M_X64)
    #define DRFLAC_X64
#elif defined(__i386) || defined(_M_IX86)
    #define DRFLAC_X86
#elif defined(__arm__) || defined(_M_ARM) || defined(__arm64) || defined(__arm64__) || defined(__aarch64__) || defined(_M_ARM64)
    #define DRFLAC_ARM
#endif
/* End Architecture Detection */


#ifdef DRFLAC_64BIT
typedef drflac_uint64 drflac_cache_t;
#else
typedef drflac_uint32 drflac_cache_t;
#endif

/* The various metadata block types. */
#define DRFLAC_METADATA_BLOCK_TYPE_STREAMINFO       0
#define DRFLAC_METADATA_BLOCK_TYPE_PADDING          1
#define DRFLAC_METADATA_BLOCK_TYPE_APPLICATION      2
#define DRFLAC_METADATA_BLOCK_TYPE_SEEKTABLE        3
#define DRFLAC_METADATA_BLOCK_TYPE_VORBIS_COMMENT   4
#define DRFLAC_METADATA_BLOCK_TYPE_CUESHEET         5
#define DRFLAC_METADATA_BLOCK_TYPE_PICTURE          6
#define DRFLAC_METADATA_BLOCK_TYPE_INVALID          127

/* The various picture types specified in the PICTURE block. */
#define DRFLAC_PICTURE_TYPE_OTHER                   0
#define DRFLAC_PICTURE_TYPE_FILE_ICON               1
#define DRFLAC_PICTURE_TYPE_OTHER_FILE_ICON         2
#define DRFLAC_PICTURE_TYPE_COVER_FRONT             3
#define DRFLAC_PICTURE_TYPE_COVER_BACK              4
#define DRFLAC_PICTURE_TYPE_LEAFLET_PAGE            5
#define DRFLAC_PICTURE_TYPE_MEDIA                   6
#define DRFLAC_PICTURE_TYPE_LEAD_ARTIST             7
#define DRFLAC_PICTURE_TYPE_ARTIST                  8
#define DRFLAC_PICTURE_TYPE_CONDUCTOR               9
#define DRFLAC_PICTURE_TYPE_BAND                    10
#define DRFLAC_PICTURE_TYPE_COMPOSER                11
#define DRFLAC_PICTURE_TYPE_LYRICIST                12
#define DRFLAC_PICTURE_TYPE_RECORDING_LOCATION      13
#define DRFLAC_PICTURE_TYPE_DURING_RECORDING        14
#define DRFLAC_PICTURE_TYPE_DURING_PERFORMANCE      15
#define DRFLAC_PICTURE_TYPE_SCREEN_CAPTURE          16
#define DRFLAC_PICTURE_TYPE_BRIGHT_COLORED_FISH     17
#define DRFLAC_PICTURE_TYPE_ILLUSTRATION            18
#define DRFLAC_PICTURE_TYPE_BAND_LOGOTYPE           19
#define DRFLAC_PICTURE_TYPE_PUBLISHER_LOGOTYPE      20

typedef enum
{
    drflac_container_native,
    drflac_container_ogg,
    drflac_container_unknown
} drflac_container;

typedef enum
{
    drflac_seek_origin_start,
    drflac_seek_origin_current
} drflac_seek_origin;

/* The order of members in this structure is important because we map this directly to the raw data within the SEEKTABLE metadata block. */
typedef struct
{
    drflac_uint64 firstPCMFrame;
    drflac_uint64 flacFrameOffset;   /* The offset from the first byte of the header of the first frame. */
    drflac_uint16 pcmFrameCount;
} drflac_seekpoint;

typedef struct
{
    drflac_uint16 minBlockSizeInPCMFrames;
    drflac_uint16 maxBlockSizeInPCMFrames;
    drflac_uint32 minFrameSizeInPCMFrames;
    drflac_uint32 maxFrameSizeInPCMFrames;
    drflac_uint32 sampleRate;
    drflac_uint8  channels;
    drflac_uint8  bitsPerSample;
    drflac_uint64 totalPCMFrameCount;
    drflac_uint8  md5[16];
} drflac_streaminfo;

typedef struct
{
    /*
    The metadata type. Use this to know how to interpret the data below. Will be set to one of the
    DRFLAC_METADATA_BLOCK_TYPE_* tokens.
    */
    drflac_uint32 type;

    /*
    A pointer to the raw data. This points to a temporary buffer so don't hold on to it. It's best to
    not modify the contents of this buffer. Use the structures below for more meaningful and structured
    information about the metadata. It's possible for this to be null.
    */
    const void* pRawData;

    /* The size in bytes of the block and the buffer pointed to by pRawData if it's non-NULL. */
    drflac_uint32 rawDataSize;

    union
    {
        drflac_streaminfo streaminfo;

        struct
        {
            int unused;
        } padding;

        struct
        {
            drflac_uint32 id;
            const void* pData;
            drflac_uint32 dataSize;
        } application;

        struct
        {
            drflac_uint32 seekpointCount;
            const drflac_seekpoint* pSeekpoints;
        } seektable;

        struct
        {
            drflac_uint32 vendorLength;
            const char* vendor;
            drflac_uint32 commentCount;
            const void* pComments;
        } vorbis_comment;

        struct
        {
            char catalog[128];
            drflac_uint64 leadInSampleCount;
            drflac_bool32 isCD;
            drflac_uint8 trackCount;
            const void* pTrackData;
        } cuesheet;

        struct
        {
            drflac_uint32 type;
            drflac_uint32 mimeLength;
            const char* mime;
            drflac_uint32 descriptionLength;
            const char* description;
            drflac_uint32 width;
            drflac_uint32 height;
            drflac_uint32 colorDepth;
            drflac_uint32 indexColorCount;
            drflac_uint32 pictureDataSize;
            const drflac_uint8* pPictureData;
        } picture;
    } data;
} drflac_metadata;


/*
Callback for when data needs to be read from the client.


Parameters
----------
pUserData (in)
    The user data that was passed to drflac_open() and family.

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
typedef size_t (* drflac_read_proc)(void* pUserData, void* pBufferOut, size_t bytesToRead);

/*
Callback for when data needs to be seeked.


Parameters
----------
pUserData (in)
    The user data that was passed to drflac_open() and family.

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
either drflac_seek_origin_start or drflac_seek_origin_current.

When seeking to a PCM frame using drflac_seek_to_pcm_frame(), dr_flac may call this with an offset beyond the end of the FLAC stream. This needs to be detected
and handled by returning DRFLAC_FALSE.
*/
typedef drflac_bool32 (* drflac_seek_proc)(void* pUserData, int offset, drflac_seek_origin origin);

/*
Callback for when a metadata block is read.


Parameters
----------
pUserData (in)
    The user data that was passed to drflac_open() and family.

pMetadata (in)
    A pointer to a structure containing the data of the metadata block.


Remarks
-------
Use pMetadata->type to determine which metadata block is being handled and how to read the data. This
will be set to one of the DRFLAC_METADATA_BLOCK_TYPE_* tokens.
*/
typedef void (* drflac_meta_proc)(void* pUserData, drflac_metadata* pMetadata);


/* Structure for internal use. Only used for decoders opened with drflac_open_memory. */
typedef struct
{
    const drflac_uint8* data;
    size_t dataSize;
    size_t currentReadPos;
} drflac__memory_stream;

/* Structure for internal use. Used for bit streaming. */
typedef struct
{
    /* The function to call when more data needs to be read. */
    drflac_read_proc onRead;

    /* The function to call when the current read position needs to be moved. */
    drflac_seek_proc onSeek;

    /* The user data to pass around to onRead and onSeek. */
    void* pUserData;


    /*
    The number of unaligned bytes in the L2 cache. This will always be 0 until the end of the stream is hit. At the end of the
    stream there will be a number of bytes that don't cleanly fit in an L1 cache line, so we use this variable to know whether
    or not the bistreamer needs to run on a slower path to read those last bytes. This will never be more than sizeof(drflac_cache_t).
    */
    size_t unalignedByteCount;

    /* The content of the unaligned bytes. */
    drflac_cache_t unalignedCache;

    /* The index of the next valid cache line in the "L2" cache. */
    drflac_uint32 nextL2Line;

    /* The number of bits that have been consumed by the cache. This is used to determine how many valid bits are remaining. */
    drflac_uint32 consumedBits;

    /*
    The cached data which was most recently read from the client. There are two levels of cache. Data flows as such:
    Client -> L2 -> L1. The L2 -> L1 movement is aligned and runs on a fast path in just a few instructions.
    */
    drflac_cache_t cacheL2[DR_FLAC_BUFFER_SIZE/sizeof(drflac_cache_t)];
    drflac_cache_t cache;

    /*
    CRC-16. This is updated whenever bits are read from the bit stream. Manually set this to 0 to reset the CRC. For FLAC, this
    is reset to 0 at the beginning of each frame.
    */
    drflac_uint16 crc16;
    drflac_cache_t crc16Cache;              /* A cache for optimizing CRC calculations. This is filled when when the L1 cache is reloaded. */
    drflac_uint32 crc16CacheIgnoredBytes;   /* The number of bytes to ignore when updating the CRC-16 from the CRC-16 cache. */
} drflac_bs;

typedef struct
{
    /* The type of the subframe: SUBFRAME_CONSTANT, SUBFRAME_VERBATIM, SUBFRAME_FIXED or SUBFRAME_LPC. */
    drflac_uint8 subframeType;

    /* The number of wasted bits per sample as specified by the sub-frame header. */
    drflac_uint8 wastedBitsPerSample;

    /* The order to use for the prediction stage for SUBFRAME_FIXED and SUBFRAME_LPC. */
    drflac_uint8 lpcOrder;

    /* A pointer to the buffer containing the decoded samples in the subframe. This pointer is an offset from drflac::pExtraData. */
    drflac_int32* pSamplesS32;
} drflac_subframe;

typedef struct
{
    /*
    If the stream uses variable block sizes, this will be set to the index of the first PCM frame. If fixed block sizes are used, this will
    always be set to 0. This is 64-bit because the decoded PCM frame number will be 36 bits.
    */
    drflac_uint64 pcmFrameNumber;

    /*
    If the stream uses fixed block sizes, this will be set to the frame number. If variable block sizes are used, this will always be 0. This
    is 32-bit because in fixed block sizes, the maximum frame number will be 31 bits.
    */
    drflac_uint32 flacFrameNumber;

    /* The sample rate of this frame. */
    drflac_uint32 sampleRate;

    /* The number of PCM frames in each sub-frame within this frame. */
    drflac_uint16 blockSizeInPCMFrames;

    /*
    The channel assignment of this frame. This is not always set to the channel count. If interchannel decorrelation is being used this
    will be set to DRFLAC_CHANNEL_ASSIGNMENT_LEFT_SIDE, DRFLAC_CHANNEL_ASSIGNMENT_RIGHT_SIDE or DRFLAC_CHANNEL_ASSIGNMENT_MID_SIDE.
    */
    drflac_uint8 channelAssignment;

    /* The number of bits per sample within this frame. */
    drflac_uint8 bitsPerSample;

    /* The frame's CRC. */
    drflac_uint8 crc8;
} drflac_frame_header;

typedef struct
{
    /* The header. */
    drflac_frame_header header;

    /*
    The number of PCM frames left to be read in this FLAC frame. This is initially set to the block size. As PCM frames are read,
    this will be decremented. When it reaches 0, the decoder will see this frame as fully consumed and load the next frame.
    */
    drflac_uint32 pcmFramesRemaining;

    /* The list of sub-frames within the frame. There is one sub-frame for each channel, and there's a maximum of 8 channels. */
    drflac_subframe subframes[8];
} drflac_frame;

typedef struct
{
    /* The function to call when a metadata block is read. */
    drflac_meta_proc onMeta;

    /* The user data posted to the metadata callback function. */
    void* pUserDataMD;

    /* Memory allocation callbacks. */
    drflac_allocation_callbacks allocationCallbacks;


    /* The sample rate. Will be set to something like 44100. */
    drflac_uint32 sampleRate;

    /*
    The number of channels. This will be set to 1 for monaural streams, 2 for stereo, etc. Maximum 8. This is set based on the
    value specified in the STREAMINFO block.
    */
    drflac_uint8 channels;

    /* The bits per sample. Will be set to something like 16, 24, etc. */
    drflac_uint8 bitsPerSample;

    /* The maximum block size, in samples. This number represents the number of samples in each channel (not combined). */
    drflac_uint16 maxBlockSizeInPCMFrames;

    /*
    The total number of PCM Frames making up the stream. Can be 0 in which case it's still a valid stream, but just means
    the total PCM frame count is unknown. Likely the case with streams like internet radio.
    */
    drflac_uint64 totalPCMFrameCount;


    /* The container type. This is set based on whether or not the decoder was opened from a native or Ogg stream. */
    drflac_container container;

    /* The number of seekpoints in the seektable. */
    drflac_uint32 seekpointCount;


    /* Information about the frame the decoder is currently sitting on. */
    drflac_frame currentFLACFrame;


    /* The index of the PCM frame the decoder is currently sitting on. This is only used for seeking. */
    drflac_uint64 currentPCMFrame;

    /* The position of the first FLAC frame in the stream. This is only ever used for seeking. */
    drflac_uint64 firstFLACFramePosInBytes;


    /* A hack to avoid a malloc() when opening a decoder with drflac_open_memory(). */
    drflac__memory_stream memoryStream;


    /* A pointer to the decoded sample data. This is an offset of pExtraData. */
    drflac_int32* pDecodedSamples;

    /* A pointer to the seek table. This is an offset of pExtraData, or NULL if there is no seek table. */
    drflac_seekpoint* pSeekpoints;

    /* Internal use only. Only used with Ogg containers. Points to a drflac_oggbs object. This is an offset of pExtraData. */
    void* _oggbs;

    /* Internal use only. Used for profiling and testing different seeking modes. */
    drflac_bool32 _noSeekTableSeek    : 1;
    drflac_bool32 _noBinarySearchSeek : 1;
    drflac_bool32 _noBruteForceSeek   : 1;

    /* The bit streamer. The raw FLAC data is fed through this object. */
    drflac_bs bs;

    /* Variable length extra data. We attach this to the end of the object so we can avoid unnecessary mallocs. */
    drflac_uint8 pExtraData[1];
} drflac;

/* Structure representing an iterator for vorbis comments in a VORBIS_COMMENT metadata block. */
typedef struct
{
    drflac_uint32 countRemaining;
    const char* pRunningData;
} drflac_vorbis_comment_iterator;

/* Structure representing an iterator for cuesheet tracks in a CUESHEET metadata block. */
typedef struct
{
    drflac_uint32 countRemaining;
    const char* pRunningData;
} drflac_cuesheet_track_iterator;

/* The order of members here is important because we map this directly to the raw data within the CUESHEET metadata block. */
typedef struct
{
    drflac_uint64 offset;
    drflac_uint8 index;
    drflac_uint8 reserved[3];
} drflac_cuesheet_track_index;

typedef struct
{
    drflac_uint64 offset;
    drflac_uint8 trackNumber;
    char ISRC[12];
    drflac_bool8 isAudio;
    drflac_bool8 preEmphasis;
    drflac_uint8 indexCount;
    const drflac_cuesheet_track_index* pIndexPoints;
} drflac_cuesheet_track;

/*
Opens a FLAC decoder.


Parameters
----------
onRead (in)
    The function to call when data needs to be read from the client.

onSeek (in)
    The function to call when the read position of the client data needs to move.

pUserData (in, optional)
    A pointer to application defined data that will be passed to onRead and onSeek.

pAllocationCallbacks (in, optional)
    A pointer to application defined callbacks for managing memory allocations.


Return Value
------------
Returns a pointer to an object representing the decoder.


Remarks
-------
Close the decoder with `drflac_close()`.

`pAllocationCallbacks` can be NULL in which case it will use `DRFLAC_MALLOC`, `DRFLAC_REALLOC` and `DRFLAC_FREE`.

This function will automatically detect whether or not you are attempting to open a native or Ogg encapsulated FLAC, both of which should work seamlessly
without any manual intervention. Ogg encapsulation also works with multiplexed streams which basically means it can play FLAC encoded audio tracks in videos.

This is the lowest level function for opening a FLAC stream. You can also use `drflac_open_memory()` to open the stream from a block of memory.

The STREAMINFO block must be present for this to succeed.

Use `drflac_open_with_metadata()` if you need access to metadata.


Seek Also
---------
drflac_open_memory()
drflac_open_with_metadata()
drflac_close()
*/
drflac* drflac_open(drflac_read_proc onRead, drflac_seek_proc onSeek, void* pUserData, const drflac_allocation_callbacks* pAllocationCallbacks);

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

pAllocationCallbacks (in, optional)
    A pointer to application defined callbacks for managing memory allocations.


Return Value
------------
A pointer to an object representing the decoder.


Remarks
-------
Close the decoder with `drflac_close()`.

`pAllocationCallbacks` can be NULL in which case it will use `DRFLAC_MALLOC`, `DRFLAC_REALLOC` and `DRFLAC_FREE`.

This is slower than `drflac_open()`, so avoid this one if you don't need metadata. Internally, this will allocate and free memory on the heap for every
metadata block except for STREAMINFO and PADDING blocks.

The caller is notified of the metadata via the `onMeta` callback. All metadata blocks will be handled before the function returns. This callback takes a
pointer to a `drflac_metadata` object which is a union containing the data of all relevant metadata blocks. Use the `type` member to discriminate against
the different metadata types.

The STREAMINFO block must be present for this to succeed.

Note that this will behave inconsistently with `drflac_open()` if the stream is an Ogg encapsulated stream and a metadata block is corrupted. This is due to
the way the Ogg stream recovers from corrupted pages. When `drflac_open_with_metadata()` is being used, the open routine will try to read the contents of the
metadata block, whereas `drflac_open()` will simply seek past it (for the sake of efficiency). This inconsistency can result in different samples being
returned depending on whether or not the stream is being opened with metadata.


Seek Also
---------
drflac_open()
drflac_close()
*/
drflac* drflac_open_with_metadata(drflac_read_proc onRead, drflac_seek_proc onSeek, drflac_meta_proc onMeta, void* pUserData, const drflac_allocation_callbacks* pAllocationCallbacks);

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
drflac_open()
drflac_open_with_metadata()
drflac_open_memory()
*/
void drflac_close(drflac* pFlac);

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
drflac_uint64 drflac_read_pcm_frames_s16(drflac* pFlac, drflac_uint64 framesToRead, drflac_int16* pBufferOut);

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
drflac_uint64 drflac_read_pcm_frames_f32(drflac* pFlac, drflac_uint64 framesToRead, float* pBufferOut);

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
`DRFLAC_TRUE` if successful; `DRFLAC_FALSE` otherwise.
*/
uint32_t drflac_seek_to_pcm_frame(drflac* pFlac, uint64_t pcmFrameIndex);

/*
Opens a FLAC decoder from a pre-allocated block of memory


Parameters
----------
pData (in)
    A pointer to the raw encoded FLAC data.

dataSize (in)
    The size in bytes of `data`.

pAllocationCallbacks (in)
    A pointer to application defined callbacks for managing memory allocations.


Return Value
------------
A pointer to an object representing the decoder.


Remarks
-------
This does not create a copy of the data. It is up to the application to ensure the buffer remains valid for the lifetime of the decoder.


See Also
--------
drflac_open()
drflac_close()
*/
drflac* drflac_open_memory(const void* pData, size_t dataSize, const drflac_allocation_callbacks* pAllocationCallbacks);

/* High Level APIs */

/*
Frees memory that was allocated internally by dr_flac.

Set pAllocationCallbacks to the same object that was passed to drflac_open_*_and_read_pcm_frames_*(). If you originally passed in NULL, pass in NULL for this.
*/
void drflac_free(void* p, const drflac_allocation_callbacks* pAllocationCallbacks);

#ifdef __cplusplus
}
#endif

#endif  /* DR_FLAC_H */
