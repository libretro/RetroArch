#pragma once
#include "commands.h"
#include "otp.h"
#include <coreinit/filesystem.h>
#include <coreinit/filesystem_fsa.h>
#include <stdint.h>
#include <sysapp/args.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum MochaUtilsStatus {
    MOCHA_RESULT_SUCCESS                 = 0,
    MOCHA_RESULT_INVALID_ARGUMENT        = -0x01,
    MOCHA_RESULT_MAX_CLIENT              = -0x02,
    MOCHA_RESULT_OUT_OF_MEMORY           = -0x03,
    MOCHA_RESULT_ALREADY_EXISTS          = -0x04,
    MOCHA_RESULT_ADD_DEVOPTAB_FAILED     = -0x05,
    MOCHA_RESULT_NOT_FOUND               = -0x06,
    MOCHA_RESULT_UNSUPPORTED_API_VERSION = -0x10,
    MOCHA_RESULT_UNSUPPORTED_COMMAND     = -0x11,
    MOCHA_RESULT_UNSUPPORTED_CFW         = -0x12,
    MOCHA_RESULT_LIB_UNINITIALIZED       = -0x20,
    MOCHA_RESULT_UNKNOWN_ERROR           = -0x100,
} MochaUtilsStatus;

const char *Mocha_GetStatusStr(MochaUtilsStatus status);

/**
 * Initializes the mocha lib. Needs to be called before any other functions can be used
 * @return MOCHA_RESULT_SUCCESS:                Library has been successfully initialized <br>
 *         MOCHA_RESULT_UNSUPPORTED_COMMAND:    Failed to initialize the library caused by an outdated mocha version.
 */
MochaUtilsStatus Mocha_InitLibrary();

/**
 * Deinitializes the mocha lib
 * @return
 */
MochaUtilsStatus Mocha_DeInitLibrary();

/**
 * Retrieves the API Version of the running mocha.
 *
 * @param outVersion pointer to the variable where the version will be stored.
 *
 * @return MOCHA_RESULT_SUCCESS:                    The API version has been store in the version ptr<br>
 *         MOCHA_RESULT_INVALID_ARGUMENT:           Invalid version pointer<br>
 *         MOCHA_RESULT_UNSUPPORTED_API_VERSION:    Failed to get the API version caused by an outdated mocha version.<br>
 *         MOCHA_RESULT_UNSUPPORTED_CFW:            Failed to get the API version caused by not using a (compatible) CFW.
 */
MochaUtilsStatus Mocha_CheckAPIVersion(uint32_t *outVersion);

/**
 * Copies data within IOSU with MCP permission.
 * @param dst - Destination address
 * @param src - Source address
 * @param size - Bytes to copy.
 * @return MOCHA_RESULT_SUCCESS:            The data has been copied successfully<br>
 *         MOCHA_RESULT_INVALID_ARGUMENT:   Invalid dst or src pointer<br>
 *         MOCHA_RESULT_LIB_UNINITIALIZED:  Library was not initialized. Call Mocha_InitLibrary() before using this function.<br>
 *         MOCHA_RESULT_UNKNOWN_ERROR:      Unknown error
 */
MochaUtilsStatus Mocha_IOSUMemoryMemcpy(uint32_t dst, uint32_t src, uint32_t size);

/**
 * Writes data to a given address with MCP permission.
 * @param address - Address where the data will be written to.
 * @param buffer - Pointer to the data which should be written.
 * @param size - Bytes to write.
 * @return MOCHA_RESULT_SUCCESS:            The data has been written successfully<br>
 *         MOCHA_RESULT_INVALID_ARGUMENT:   Invalid address or buffer pointer<br>
 *         MOCHA_RESULT_LIB_UNINITIALIZED:  Library was not initialized. Call Mocha_InitLibrary() before using this function.<br>
 *         MOCHA_RESULT_OUT_OF_MEMORY:      Not enough memory <br>
 *         MOCHA_RESULT_UNKNOWN_ERROR:      Unknown error
 */
MochaUtilsStatus Mocha_IOSUMemoryWrite(uint32_t address, const uint8_t *buffer, uint32_t size);

/**
 * Reads data from a given address with MCP permission.
 * @param address - Address where the data will be read from.
 * @param buffer - Pointer to the buffer where the read will be stored
 * @param size - Bytes to read.
 * @return MOCHA_RESULT_SUCCESS: The data has been read successfully<br>
 *         MOCHA_RESULT_INVALID_ARGUMENT:   Invalid source address<br>
 *         MOCHA_RESULT_LIB_UNINITIALIZED:  Library was not initialized. Call Mocha_InitLibrary() before using this function.<br>
 *         MOCHA_RESULT_UNKNOWN_ERROR:      Unknown error
 */
MochaUtilsStatus Mocha_IOSUMemoryRead(uint32_t address, uint8_t *out_buffer, uint32_t size);

/**
 * Writes 4 bytes with IOSU kernel permission
 * @param address Address where the value will be written.
 * @param value Value that will be written to address.
 * @return MOCHA_RESULT_SUCCESS: The data has been written successfully<br>
 *         MOCHA_RESULT_INVALID_ARGUMENT:   Invalid target address<br>
 *         MOCHA_RESULT_LIB_UNINITIALIZED:  Library was not initialized. Call Mocha_InitLibrary() before using this function.<br>
 *         MOCHA_RESULT_OUT_OF_MEMORY:      Not enough memory <br>
 *         MOCHA_RESULT_UNKNOWN_ERROR:      Unknown error
 */
MochaUtilsStatus Mocha_IOSUKernelWrite32(uint32_t address, uint32_t value);

/**
 * Reads 4 bytes with IOSU kernel permission
 * @param address Address from which the data will be read.
 * @param out_buffer Pointer where the result will be stored
 * @return MOCHA_RESULT_SUCCESS: The data has been read successfully<br>
 *         MOCHA_RESULT_INVALID_ARGUMENT:   Invalid target address<br>
 *         MOCHA_RESULT_LIB_UNINITIALIZED:  Library was not initialized. Call Mocha_InitLibrary() before using this function.<br>
 *         MOCHA_RESULT_UNKNOWN_ERROR:      Unknown error
 */
MochaUtilsStatus Mocha_IOSUKernelRead32(uint32_t address, uint32_t *out_buffer);

/**
 * Reads the consoles OTP into the given buffer.
 *
 * @param out_buffer Buffer where the result will be stored.
 * @return MOCHA_RESULT_SUCCESS: The OTP has been read into the buffer<br>
 *         MOCHA_RESULT_INVALID_ARGUMENT:   Invalid environmentPathBuffer pointer or bufferLen \< 0x100<br>
 *         MOCHA_RESULT_LIB_UNINITIALIZED:  Library was not initialized. Call Mocha_InitLibrary() before using this function.<br>
 *         MOCHA_RESULT_UNKNOWN_ERROR:      Failed to retrieve the environment path.
 */
MochaUtilsStatus Mocha_ReadOTP(WiiUConsoleOTP *out_buffer);

/**
 * Calls an iosu SVC.
 * @param svc_id
 * @param args array of argument with the length arg_cnt
 * @param arg_cnt number of arguments
 * @param outResult
 * @return MOCHA_RESULT_SUCCESS:            The SVC has been called successfully, the result has been stored in outResult.<br>
 *         MOCHA_RESULT_LIB_UNINITIALIZED:  Library was not initialized. Call Mocha_InitLibrary() before using this function.<br>
 *         MOCHA_RESULT_UNKNOWN_ERROR
 */
int Mocha_IOSUCallSVC(uint32_t svc_id, uint32_t *args, uint32_t arg_cnt, int32_t *outResult);

/**
 * Returns the path of the currently loaded environment
 * @param environmentPathBuffer: buffer where the result will be stored
 * @param bufferLen: length of the buffer. Required to be >= 0x100
 * @return MOCHA_RESULT_SUCCESS: The environment path has been stored in environmentPathBuffer<br>
 *         MOCHA_RESULT_INVALID_ARGUMENT:       Invalid environmentPathBuffer pointer or bufferLen \< 0x100<br>
 *         MOCHA_RESULT_LIB_UNINITIALIZED:      Library was not initialized. Call Mocha_InitLibrary() before using this function.<br>
 *         MOCHA_RESULT_UNSUPPORTED_COMMAND:    Command not supported by the currently loaded mocha version.<br>
 *         MOCHA_RESULT_UNKNOWN_ERROR:          Failed to retrieve the environment path.
 */
MochaUtilsStatus Mocha_GetEnvironmentPath(char *environmentPathBuffer, uint32_t bufferLen);

/**
 * Enables logging via USB (FTDI FT232 chipset only) via OSReport and friends. <br>
 * @param notSkipExistingLogs If set to true, existing logs of this session won't be skipped.
 * @return MOCHA_RESULT_SUCCESS:                Logging via USB starts or has already been started<br>
 *         MOCHA_RESULT_LIB_UNINITIALIZED:      Library was not initialized. Call Mocha_InitLibrary() before using this function.<br>
 *         MOCHA_RESULT_UNSUPPORTED_COMMAND:    Command not supported by the currently loaded mocha version.<br>
 *         MOCHA_RESULT_UNKNOWN_ERROR:          Failed to retrieve the environment path.
 */
MochaUtilsStatus Mocha_StartUSBLogging(bool notSkipExistingLogs);

/**
 * Gives a FSClient full permissions. <br>
 * Requires Mocha API Version: 1
 * @param client The FSClient that should have full permission
 * @return MOCHA_RESULT_SUCCESS:                The has been unlocked successfully. <br>
 *         MOCHA_RESULT_INVALID_ARGUMENT:       Client was NULL <br>
 *         MOCHA_RESULT_MAX_CLIENT:             The maximum number of FS Clients have been unlocked.<br>
 *         MOCHA_RESULT_LIB_UNINITIALIZED:      Library was not initialized. Call Mocha_InitLibrary() before using this function.<br>
 *         MOCHA_RESULT_UNSUPPORTED_COMMAND:    Command not supported by the currently loaded mocha version.<br>
 *         MOCHA_RESULT_UNKNOWN_ERROR:          Failed to unlock a given FSClient
 */
MochaUtilsStatus Mocha_UnlockFSClient(FSClient *client);

/**
 * Gives a /dev/fsa handle full permissions. <br>
 * Requires Mocha API Version: 1
 * @param client The /dev/fsa handle that should have full permission
 * @return MOCHA_RESULT_SUCCESS:                The has been unlocked successfully. <br>
 *         MOCHA_RESULT_MAX_CLIENT:             The maximum number of FS Clients have been unlocked.<br>
 *         MOCHA_RESULT_LIB_UNINITIALIZED:      Library was not initialized. Call Mocha_InitLibrary() before using this function.<br>
 *         MOCHA_RESULT_UNSUPPORTED_COMMAND:    Command not supported by the currently loaded mocha version.<br>
 *         MOCHA_RESULT_UNKNOWN_ERROR:          Failed to unlock the given client
 */
MochaUtilsStatus Mocha_UnlockFSClientEx(int clientHandle);

/**
 * Set the .rpx that will be loaded the next time the homebrew wrapper application is launched (e.g. Health & Safety or Daily Log).
 * <br>
 * Loading a .rpx from within a file (archive e.g. a WUHB) is supported. <br>
 * To achieve this, the fileoffset (offset inside file specified via path) and filesize (size of the .rpx) need to be set. <br>
 * If filesize is set to 0, the whole file (starting at fileoffset) will be loaded as .rpx <br>
 * <br>
 * The path is **relative** to the root of the given target device.<br>
 * The target LOAD_RPX_TARGET_EXTRA_REVERT_PREPARE will revert a prepare call. <br>
 * <br>
 * To launch the prepared RPX call Mocha_LaunchHomebrewWrapper if this call was successful.
 *
 * @param loadInfo Information about the .rpx replacement.
 * @return MOCHA_RESULT_SUCCESS:                Loading the next RPX will be redirected. <br>
 *         MOCHA_RESULT_INVALID_ARGUMENT:       The given loadInfo was NULL <br>
 *         MOCHA_RESULT_LIB_UNINITIALIZED:      Library was not initialized. Call Mocha_InitLibrary() before using this function.<br>
 *         MOCHA_RESULT_UNSUPPORTED_COMMAND:    Command not supported by the currently loaded mocha version.<br>
 *         MOCHA_RESULT_UNKNOWN_ERROR:          Failed to setup a redirect of RPX.
 */
MochaUtilsStatus Mocha_PrepareRPXLaunch(MochaRPXLoadInfo *loadInfo);

/**
 * Launches the wrapper app for launching .rpx  <br>
 * To launch a RPX call `Mocha_PrepareRPXLaunch` before this function. <br>
 * <br>
 * see: `Mocha_LaunchRPX` to prepare and launch a RPX in one command.
 *
 * @return MOCHA_RESULT_SUCCESS: App is launching<br>
 *         MOCHA_RESULT_LIB_UNINITIALIZED:      Library was not initialized. Call Mocha_InitLibrary() before using this function.<br>
 *         MOCHA_RESULT_UNSUPPORTED_COMMAND:    Command not supported by the currently loaded mocha version.<br>
 *         MOCHA_RESULT_NOT_FOUND:              Not application that can be used as homebrew wrapper found.
 */
MochaUtilsStatus Mocha_LaunchHomebrewWrapper();

/**
 * Launches the wrapper app for launching .rpx, with custom SYS args.  <br>
 * To launch a RPX call `Mocha_PrepareRPXLaunch` before this function. <br>
 * <br>
 * see: `Mocha_LaunchRPX` to prepare and launch a RPX in one command.
 *
 * @param args sysapp args to pass to the homebrew app.
 * @return MOCHA_RESULT_SUCCESS: App is launching<br>
 *         MOCHA_RESULT_LIB_UNINITIALIZED:      Library was not initialized. Call Mocha_InitLibrary() before using this function.<br>
 *         MOCHA_RESULT_UNSUPPORTED_COMMAND:    Command not supported by the currently loaded mocha version.<br>
 *         MOCHA_RESULT_NOT_FOUND:              Not application that can be used as homebrew wrapper found.
 */
MochaUtilsStatus Mocha_LaunchHomebrewWrapperEx(SYSStandardArgsIn *args);

/**
 * Launches a given RPX by launching a wrapper application and replacing the RPX on the fly. <br>
 * See Mocha_PrepareRPXLaunch for more information. <br>
 *
 * Note: Combines Mocha_PrepareRPXLaunch and Mocha_LaunchHomebrewWrapper.
 * @param loadInfo
 * @return MOCHA_RESULT_SUCCESS: Requested RPX will be launched<br>
 *         MOCHA_RESULT_LIB_UNINITIALIZED:      Library was not initialized. Call Mocha_InitLibrary() before using this function.<br>
 *         MOCHA_RESULT_UNSUPPORTED_COMMAND:    Command not supported by the currently loaded mocha version.<br>
 *         MOCHA_RESULT_INVALID_ARGUMENT:       The given loadInfo was NULL <br>
 *         MOCHA_RESULT_NOT_FOUND:              Not application that can be used as homebrew wrapper found.
 *         MOCHA_RESULT_UNKNOWN_ERROR:          Failed to setup a redirect of RPX.
 */
MochaUtilsStatus Mocha_LaunchRPX(MochaRPXLoadInfo *loadInfo);

typedef struct WUDDiscKey {
    uint8_t key[0x10];
} WUDDiscKey;

/**
 * Reads the disc key (used to decrypt the SI partition) of the inserted disc.
 *
 * @param discKey target buffer where the result will be stored.
 * @return MOCHA_RESULT_SUCCESS: The disc key of the inserted disc has been read into the given buffer.<br>
 *         MOCHA_RESULT_INVALID_ARGUMENT:       The given discKey buffer was NULL <br>
 *         MOCHA_RESULT_LIB_UNINITIALIZED:      Library was not initialized. Call Mocha_InitLibrary() before using this function.<br>
 *         MOCHA_RESULT_UNSUPPORTED_COMMAND:    Command not supported by the currently loaded mocha version.<br>
 *         MOCHA_RESULT_NOT_FOUND:              Failed to obtain disc key, no disc inserted?.
 *         MOCHA_RESULT_UNKNOWN_ERROR:          Unknown error.
 */
MochaUtilsStatus Mocha_ODMGetDiscKey(WUDDiscKey *discKey);

/**
 * Reads *size* bytes from *offset* from the SEEPROM of the console. Total size of SEEPROM is 0x200
 * @param out_buffer buffer where the result will be stored
 * @param offset offset in bytes. Must be an even number.
 * @param size size in bytes
 * @return MOCHA_RESULT_SUCCESS:                The SEEPROM has been read into the given buffer.<br>
 *         MOCHA_RESULT_INVALID_ARGUMENT:       The given out_buffer was NULL or the offset was < 0 or an odd value<br>
 *         MOCHA_RESULT_LIB_UNINITIALIZED:      Library was not initialized. Call Mocha_InitLibrary() before using this function.<br>
 *         MOCHA_RESULT_UNSUPPORTED_COMMAND:    Command not supported by the currently loaded mocha version.<br>
 *         MOCHA_RESULT_UNKNOWN_ERROR:          Failed to read the seeprom.
 */
MochaUtilsStatus Mocha_SEEPROMRead(uint8_t *out_buffer, uint32_t offset, uint32_t size);

/**
 * Mounts a device (dev_path) to a given path (mount_path) and make a accessible via the
 * newlib devoptab (standard POSIX file I/O)
 *
 * Requires Mocha API Version: 1
 * @param virt_name Name which should be used for the devoptab. When choosing e.g. "storage_usb" the mounted device can be accessed via "storage_usb:/".
 * @param dev_path (optional) Cafe OS internal device path (e.g. /dev/slc01). If the given dev_path is NULL, an existing mount will be used (and is expected)
 * @param mount_path Path where CafeOS should mount the device to. Must be globally unique and start with "/vol/storage_"
 * @return MOCHA_RESULT_SUCCESS: The device has been mounted successfully <br>
 *         MOCHA_RESULT_MAX_CLIENT: The maximum number of FSAClients have been registered.<br>
 *         MOCHA_RESULT_LIB_UNINITIALIZED: Library was not initialized. Call Mocha_InitLibrary() before using this function.<br>
 *         MOCHA_RESULT_UNSUPPORTED_COMMAND: Command not supported by the currently loaded mocha version.<br>
 *         MOCHA_RESULT_UNKNOWN_ERROR: Failed to retrieve the environment path.
 */
MochaUtilsStatus Mocha_MountFS(const char *virt_name, const char *dev_path, const char *mount_path);

MochaUtilsStatus Mocha_MountFSEx(const char *virt_name, const char *dev_path, const char *mount_path, FSAMountFlags mountFlags, void *mountArgBuf, int mountArgBufLen);

/**
 * Unmounts a mount by it's name.
 * @param virt_name Name of the mount.
 * @return MOCHA_RESULT_SUCCESS: The unmount was successful <br>
 *         MOCHA_RESULT_INVALID_ARGUMENT: <br>
 *         MOCHA_RESULT_NOT_FOUND: No mount with the given name has been found.
 */
MochaUtilsStatus Mocha_UnmountFS(const char *virt_name);

#ifdef __cplusplus
} // extern "C"
#endif
