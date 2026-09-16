/* Hermetic compile-only stand-in for the NDK header: only the names RetroArch uses. */
#ifndef STUB_android_configuration_h
#define STUB_android_configuration_h
typedef struct AConfiguration AConfiguration;
typedef struct AAssetManager AAssetManager;
AConfiguration *AConfiguration_new(void); void AConfiguration_delete(AConfiguration*); void AConfiguration_fromAssetManager(AConfiguration*, AAssetManager*);
#endif
