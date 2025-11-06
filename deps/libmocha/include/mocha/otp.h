#pragma once
#include <assert.h>
#include <stdint.h>
#include <wut.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct WiiUConsoleOTP WiiUConsoleOTP;
typedef struct OTPWiiBank OTPWiiBank;
typedef struct OTPWiiUBank OTPWiiUBank;
typedef struct OTPWiiUNGBank OTPWiiUNGBank;
typedef struct OTPWiiUCertBank OTPWiiUCertBank;
typedef struct OTPWiiCertBank OTPWiiCertBank;
typedef struct OTPMiscBank OTPMiscBank;

typedef uint32_t OTPJTAGStatus;
typedef uint32_t OTPSecurityLevel;

typedef enum OTPSecurityLevelFlags {
    SECURITY_FLAG_UNKNOWN              = 0x40000000, // Unknown, causes error in boot0
    SECURITY_FLAG_CONSOLE_PROGRAMMED   = 0x80000000, // Console type has been programmed
    SECURITY_FLAG_USE_DEBUG_KEY_IMAGE  = 0x08000000, // Use first RSA key and debug ancast images in boot0
    SECURITY_FLAG_USE_RETAIL_KEY_IMAGE = 0x10000000  // Use second RSA key and retail ancast images in boot0
} OTPSecurityLevelFlags;

typedef enum OTPIOStrength {
    IO_HW_IOSTRCTRL0   = 0x00008000,
    IO_HW_IOSTRCTRL1_3 = 0x00002000,
    IO_HW_IOSTRCTRL1_2 = 0x00000800,
    IO_HW_IOSTRCTRL1_1 = 0x00000080,
    IO_HW_IOSTRCTRL1_0 = 0x00000008,
    IO_NONE            = 0x00000000
} OTPIOStrength;

typedef enum OTPPulseLength {
    PULSE_BOOT0 = 0x0000002F,
    PULSE_NONE  = 0x00000000
} OTPPulseLength;

typedef enum OTPJTAGMask {
    JTAG_MASK_DISABLED = 0x80
} OTPJTAGMask;

struct OTPWiiBank {
    uint8_t boot1SHA1Hash[0x14];
    uint8_t commonKey[0x10];
    uint32_t ngId;
    uint8_t ngPrivateKey[0x1C];
    uint8_t nandHMAC[0x14];
    uint8_t nandKey[0x10];
    uint8_t rngKey[0x10];
    WUT_UNKNOWN_BYTES(0x08);
};
WUT_CHECK_SIZE(OTPWiiBank, 0x80);
WUT_CHECK_OFFSET(OTPWiiBank, 0x00, boot1SHA1Hash);
WUT_CHECK_OFFSET(OTPWiiBank, 0x14, commonKey);
WUT_CHECK_OFFSET(OTPWiiBank, 0x24, ngId);
WUT_CHECK_OFFSET(OTPWiiBank, 0x28, ngPrivateKey);
WUT_CHECK_OFFSET(OTPWiiBank, 0x44, nandHMAC);
WUT_CHECK_OFFSET(OTPWiiBank, 0x58, nandKey);
WUT_CHECK_OFFSET(OTPWiiBank, 0x68, rngKey);

struct OTPWiiUBank {
    OTPSecurityLevel securityLevel;
    OTPIOStrength ioStrength;
    OTPPulseLength pulseLength;
    uint32_t signature;
    uint8_t starbuckAncastKey[0x10];
    uint8_t seepromKey[0x10];
    WUT_UNKNOWN_BYTES(0x10);
    WUT_UNKNOWN_BYTES(0x10);
    uint8_t vWiiCommonKey[0x10];
    uint8_t wiiUCommonKey[0x10];
    WUT_UNKNOWN_BYTES(0x10);
    WUT_UNKNOWN_BYTES(0x10);
    WUT_UNKNOWN_BYTES(0x10);
    uint8_t sslRSAKey[0x10];
    uint8_t usbStorageSeedsKey[0x10];
    WUT_UNKNOWN_BYTES(0x10);
    uint8_t xorKey[0x10];
    uint8_t rngKey[0x10];
    uint8_t slcKey[0x10];
    uint8_t mlcKey[0x10];
    uint8_t sshdKey[0x10];
    uint8_t drhWLAN[0x10];
    WUT_UNKNOWN_BYTES(0x30);
    uint8_t slcHmac[0x14];
    WUT_UNKNOWN_BYTES(0x0C);
};
WUT_CHECK_SIZE(OTPWiiUBank, 0x180);
WUT_CHECK_OFFSET(OTPWiiUBank, 0x00, securityLevel);
WUT_CHECK_OFFSET(OTPWiiUBank, 0x04, ioStrength);
WUT_CHECK_OFFSET(OTPWiiUBank, 0x08, pulseLength);
WUT_CHECK_OFFSET(OTPWiiUBank, 0x10, starbuckAncastKey);
WUT_CHECK_OFFSET(OTPWiiUBank, 0x20, seepromKey);
WUT_CHECK_OFFSET(OTPWiiUBank, 0x50, vWiiCommonKey);
WUT_CHECK_OFFSET(OTPWiiUBank, 0x60, wiiUCommonKey);
WUT_CHECK_OFFSET(OTPWiiUBank, 0xA0, sslRSAKey);
WUT_CHECK_OFFSET(OTPWiiUBank, 0xB0, usbStorageSeedsKey);
WUT_CHECK_OFFSET(OTPWiiUBank, 0xD0, xorKey);
WUT_CHECK_OFFSET(OTPWiiUBank, 0xE0, rngKey);
WUT_CHECK_OFFSET(OTPWiiUBank, 0xF0, slcKey);
WUT_CHECK_OFFSET(OTPWiiUBank, 0x100, mlcKey);
WUT_CHECK_OFFSET(OTPWiiUBank, 0x110, sshdKey);
WUT_CHECK_OFFSET(OTPWiiUBank, 0x120, drhWLAN);
WUT_CHECK_OFFSET(OTPWiiUBank, 0x160, slcHmac);

struct OTPWiiUNGBank {
    WUT_UNKNOWN_BYTES(0x10);
    WUT_UNKNOWN_BYTES(0x0C);
    uint32_t ngId;
    uint8_t ngPrivateKey[0x20];
    uint8_t privateNSSDeviceCertKey[0x20];
    uint8_t otpRNGSeed[0x10];
    WUT_UNKNOWN_BYTES(0x10);
};
WUT_CHECK_SIZE(OTPWiiUNGBank, 0x80);
WUT_CHECK_OFFSET(OTPWiiUNGBank, 0x1C, ngId);
WUT_CHECK_OFFSET(OTPWiiUNGBank, 0x20, ngPrivateKey);
WUT_CHECK_OFFSET(OTPWiiUNGBank, 0x40, privateNSSDeviceCertKey);
WUT_CHECK_OFFSET(OTPWiiUNGBank, 0x60, otpRNGSeed);

struct OTPWiiUCertBank {
    uint32_t rootCertMSId;
    uint32_t rootCertCAId;
    uint32_t rootCertNGKeyId;
    uint8_t rootCertNGSignature[0x3C];
    WUT_UNKNOWN_BYTES(0x18);
    WUT_UNKNOWN_BYTES(0x20);
};
WUT_CHECK_SIZE(OTPWiiUCertBank, 0x80);
WUT_CHECK_OFFSET(OTPWiiUCertBank, 0x00, rootCertMSId);
WUT_CHECK_OFFSET(OTPWiiUCertBank, 0x04, rootCertCAId);
WUT_CHECK_OFFSET(OTPWiiUCertBank, 0x08, rootCertNGKeyId);
WUT_CHECK_OFFSET(OTPWiiUCertBank, 0x0C, rootCertNGSignature);

struct OTPWiiCertBank {
    uint32_t rootCertMSId;
    uint32_t rootCertCAId;
    uint32_t rootCertNGKeyId;
    uint8_t rootCertNGSignature[0x3C];
    uint8_t koreanKey[0x10];
    WUT_UNKNOWN_BYTES(0x08);
    uint8_t privateNSSDeviceCertKey[0x20];
};
WUT_CHECK_SIZE(OTPWiiCertBank, 0x80);
WUT_CHECK_OFFSET(OTPWiiCertBank, 0x00, rootCertMSId);
WUT_CHECK_OFFSET(OTPWiiCertBank, 0x04, rootCertCAId);
WUT_CHECK_OFFSET(OTPWiiCertBank, 0x08, rootCertNGKeyId);
WUT_CHECK_OFFSET(OTPWiiCertBank, 0x0C, rootCertNGSignature);
WUT_CHECK_OFFSET(OTPWiiCertBank, 0x48, koreanKey);
WUT_CHECK_OFFSET(OTPWiiCertBank, 0x60, privateNSSDeviceCertKey);

struct OTPMiscBank {
    WUT_UNKNOWN_BYTES(0x20);
    uint8_t boot1Key_protected[0x10];
    WUT_UNKNOWN_BYTES(0x10);
    WUT_UNKNOWN_BYTES(0x20);
    WUT_UNKNOWN_BYTES(0x04);
    uint32_t otpVersionAndRevision;
    uint64_t otpDateCode;
    char otpVersionName[0x08];
    WUT_UNKNOWN_BYTES(0x04);
    OTPJTAGStatus jtagStatus;
};
WUT_CHECK_SIZE(OTPMiscBank, 0x80);
WUT_CHECK_OFFSET(OTPMiscBank, 0x20, boot1Key_protected);
WUT_CHECK_OFFSET(OTPMiscBank, 0x64, otpVersionAndRevision);
WUT_CHECK_OFFSET(OTPMiscBank, 0x68, otpDateCode);
WUT_CHECK_OFFSET(OTPMiscBank, 0x70, otpVersionName);
WUT_CHECK_OFFSET(OTPMiscBank, 0x7C, jtagStatus);

struct WiiUConsoleOTP {
    OTPWiiBank wiiBank;
    OTPWiiUBank wiiUBank;
    OTPWiiUNGBank wiiUNGBank;
    OTPWiiUCertBank wiiUCertBank;
    OTPWiiCertBank wiiCertBank;
    OTPMiscBank miscBank;
};
WUT_CHECK_SIZE(WiiUConsoleOTP, 0x400);
WUT_CHECK_OFFSET(WiiUConsoleOTP, 0x00, wiiBank);
WUT_CHECK_OFFSET(WiiUConsoleOTP, 0x80, wiiUBank);
WUT_CHECK_OFFSET(WiiUConsoleOTP, 0x200, wiiUNGBank);
WUT_CHECK_OFFSET(WiiUConsoleOTP, 0x280, wiiUCertBank);
WUT_CHECK_OFFSET(WiiUConsoleOTP, 0x300, wiiCertBank);
WUT_CHECK_OFFSET(WiiUConsoleOTP, 0x380, miscBank);

#ifdef __cplusplus
} // extern "C"
#endif