/* Hermetic compile-only stand-in for the NDK header: only the names RetroArch uses. */
#ifndef STUB_android_sensor_h
#define STUB_android_sensor_h
#include <android/looper.h>
typedef struct ASensorManager ASensorManager; typedef struct ASensorEventQueue ASensorEventQueue; typedef struct ASensor ASensor; typedef ASensor const *ASensorRef;
typedef struct ASensorVector { float x, y, z; } ASensorVector;
typedef struct ASensorEvent { int version; int sensor; int type; int reserved0; union { ASensorVector vector; ASensorVector acceleration; ASensorVector gyro; float data[16]; }; long long timestamp; } ASensorEvent;
enum { ASENSOR_TYPE_ACCELEROMETER = 1, ASENSOR_TYPE_GYROSCOPE = 4 };
ASensorManager *ASensorManager_getInstance(void); ASensorRef ASensorManager_getDefaultSensor(ASensorManager*, int type); ASensorEventQueue *ASensorManager_createEventQueue(ASensorManager*, ALooper*, int ident, ALooper_callbackFunc, void*); int ASensorManager_destroyEventQueue(ASensorManager*, ASensorEventQueue*);
int ASensorEventQueue_enableSensor(ASensorEventQueue*, ASensorRef); int ASensorEventQueue_disableSensor(ASensorEventQueue*, ASensorRef); int ASensorEventQueue_setEventRate(ASensorEventQueue*, ASensorRef, int); int ASensorEventQueue_getEvents(ASensorEventQueue*, ASensorEvent*, size_t);
#endif
