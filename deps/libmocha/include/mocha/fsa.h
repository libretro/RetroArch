#pragma once

#include <coreinit/filesystem.h>
#include <coreinit/filesystem_fsa.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Opens a device for raw read/write
 * @param client valid FSClient pointer with unlocked permissions
 * @param device_path path of the device. e.g. /dev/sdcard01
 * @param outHandle pointer where the handle of the raw device will be stored
 * @return
 */
FSError FSAEx_RawOpen(FSClient *client, const char *device_path, int32_t *outHandle);

/**
 * Opens a device for raw read/write
 * @param clientHandle valid /dev/fsa handle with unlocked permissions
 * @param device_path path of the device. e.g. /dev/sdcard01
 * @param outHandle pointer where the handle of the raw device will be stored
 * @return
 */
FSError FSAEx_RawOpenEx(FSAClientHandle clientHandle, const char *device_path, int32_t *outHandle);

/**
 * Closes a devices that was previously opened via FSAEx_RawOpen
 * @param client valid FSClient pointer with unlocked permissions
 * @param device_handle device handle
 * @return
 */
FSError FSAEx_RawClose(FSClient *client, int32_t device_handle);

/**
 * Closes a devices that was previously opened via FSAEx_RawOpen
 * @param clientHandle valid /dev/fsa handle with unlocked permissions
 * @param device_handle device handle
 * @return
 */
FSError FSAEx_RawCloseEx(FSAClientHandle clientHandle, int32_t device_handle);

/**
 * Read data from a device handle.
 *
 * @param client valid FSClient pointer with unlocked permissions
 * @param data buffer where the result will be stored. Requires 0x40 alignment for the buffer itself and buffer size.
 * @param size_bytes size of sector.
 * @param cnt number of sectors that should be read.
 * @param blocks_offset read offset in sectors.
 * @param device_handle valid device handle.
 * @return
 */
FSError FSAEx_RawRead(FSClient *client, void *data, uint32_t size_bytes, uint32_t cnt, uint64_t blocks_offset, int device_handle);

/**
 * Read data from a raw device handle.
 *
 * @param clientHandle valid /dev/fsa handle with unlocked permissions
 * @param data buffer where the result will be stored. Requires 0x40 alignment for the buffer itself and buffer size.
 * @param size_bytes size of sector.
 * @param cnt number of sectors that should be read.
 * @param blocks_offset read offset in sectors.
 * @param device_handle valid device handle.
 * @return
 */
FSError FSAEx_RawReadEx(FSAClientHandle clientHandle, void *data, uint32_t size_bytes, uint32_t cnt, uint64_t blocks_offset, int device_handle);


/**
 * Write data to raw device handle
 *
 * @param client valid FSClient pointer with unlocked permissions
 * @param data buffer of data that should be written.. Requires 0x40 alignment for the buffer itself and buffer size.
 * @param size_bytes size of sector.
 * @param cnt number of sectors that should be written.
 * @param blocks_offset write offset in sectors.
 * @param device_handle valid device handle.
 * @return
 */
FSError FSAEx_RawWrite(FSClient *client, const void *data, uint32_t size_bytes, uint32_t cnt, uint64_t blocks_offset, int device_handle);

/**
 * Write data to raw device handle
 *
 * @param clientHandle valid /dev/fsa handle with unlocked permissions
 * @param data buffer of data that should be written.. Requires 0x40 alignment for the buffer itself and buffer size.
 * @param size_bytes size of sector.
 * @param cnt number of sectors that should be written.
 * @param blocks_offset write offset in sectors.
 * @param device_handle valid device handle.
 * @return
 */
FSError FSAEx_RawWriteEx(FSAClientHandle clientHandle, const void *data, uint32_t size_bytes, uint32_t cnt, uint64_t blocks_offset, int device_handle);

#ifdef __cplusplus
} // extern "C"
#endif