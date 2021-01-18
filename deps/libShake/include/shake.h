/** \file shake.h
    \brief libShake public header.
*/

#ifndef _SHAKE_H_
#define _SHAKE_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** \def SHAKE_MAJOR_VERSION
    \brief Version of the program (major).
*/
#define SHAKE_MAJOR_VERSION 0

/** \def SHAKE_MINOR_VERSION
    \brief Version of the program (minor).
*/
#define SHAKE_MINOR_VERSION 3

/** \def SHAKE_PATCH_VERSION
    \brief Version of the program (patch).
*/
#define SHAKE_PATCH_VERSION 2

/** \def SHAKE_ENVELOPE_ATTACK_LENGTH_MAX
    \brief Maximum allowed value of Shake_Envelope attackLength
    \sa Shake_Envelope
*/
#define SHAKE_ENVELOPE_ATTACK_LENGTH_MAX	0x7FFF

/** \def SHAKE_ENVELOPE_FADE_LENGTH_MAX
    \brief Maximum allowed value of Shake_Envelope fadeLength
    \sa Shake_Envelope
*/
#define SHAKE_ENVELOPE_FADE_LENGTH_MAX		0x7FFF

/** \def SHAKE_ENVELOPE_ATTACK_LEVEL_MAX
    \brief Maximum allowed value of Shake_Envelope attackLevel
    \sa Shake_Envelope
*/
#define SHAKE_ENVELOPE_ATTACK_LEVEL_MAX		0x7FFF

/** \def SHAKE_ENVELOPE_FADE_LEVEL_MAX
    \brief Maximum allowed value of Shake_Envelope fadeLevel
    \sa Shake_Envelope
*/
#define SHAKE_ENVELOPE_FADE_LEVEL_MAX		0x7FFF

/** \def SHAKE_RUMBLE_STRONG_MAGNITUDE_MAX
    \brief Maximum allowed value of Shake_EffectRumble strongMagnitude
    \sa Shake_EffectRumble
*/
#define SHAKE_RUMBLE_STRONG_MAGNITUDE_MAX	0x7FFF

/** \def SHAKE_RUMBLE_WEAK_MAGNITUDE_MAX
    \brief Maximum allowed value of Shake_EffectRumble weakMagnitude
    \sa Shake_EffectRumble
*/
#define SHAKE_RUMBLE_WEAK_MAGNITUDE_MAX		0x7FFF

/** \def SHAKE_PERIODIC_PERIOD_MAX
    \brief Maximum allowed value of Shake_EffectPeriodic period
    \sa Shake_EffectPeriodic
*/
#define SHAKE_PERIODIC_PERIOD_MAX		0x7FFF

/** \def SHAKE_PERIODIC_MAGNITUDE_MIN
    \brief Minimum allowed value of Shake_EffectPeriodic magnitude
    \sa Shake_EffectPeriodic
*/
#define SHAKE_PERIODIC_MAGNITUDE_MIN		(-0x8000)

/** \def SHAKE_PERIODIC_MAGNITUDE_MAX
    \brief Maximum allowed value of Shake_EffectPeriodic magnitude
    \sa Shake_EffectPeriodic
*/
#define SHAKE_PERIODIC_MAGNITUDE_MAX		0x7FFF

/** \def SHAKE_PERIODIC_OFFSET_MIN
    \brief Minimum allowed value of Shake_EffectPeriodic offset
    \sa Shake_EffectPeriodic
*/
#define SHAKE_PERIODIC_OFFSET_MIN		(-0x8000)

/** \def SHAKE_PERIODIC_OFFSET_MAX
    \brief Maximum allowed value of Shake_EffectPeriodic offset
    \sa Shake_EffectPeriodic
*/
#define SHAKE_PERIODIC_OFFSET_MAX		0x7FFF

/** \def SHAKE_PERIODIC_PHASE_MAX
    \brief Maximum allowed value of Shake_EffectPeriodic phase
    \sa Shake_EffectPeriodic
*/
#define SHAKE_PERIODIC_PHASE_MAX		0x7FFF

/** \def SHAKE_CONSTANT_LEVEL_MIN
    \brief Minimum allowed value of Shake_EffectConstant level
    \sa Shake_EffectConstant
*/
#define SHAKE_CONSTANT_LEVEL_MIN		(-0x8000)

/** \def SHAKE_CONSTANT_LEVEL_MAX
    \brief Maximum allowed value of Shake_EffectConstant level
    \sa Shake_EffectConstant
*/
#define SHAKE_CONSTANT_LEVEL_MAX		0x7FFF

/** \def SHAKE_RAMP_START_LEVEL_MIN
    \brief Minimum allowed value of Shake_EffectRamp startLevel
    \sa Shake_EffectRamp
*/
#define SHAKE_RAMP_START_LEVEL_MIN		(-0x8000)

/** \def SHAKE_RAMP_START_LEVEL_MAX
    \brief Maximum allowed value of Shake_EffectRamp startLevel
    \sa Shake_EffectRamp
*/
#define SHAKE_RAMP_START_LEVEL_MAX		0x7FFF

/** \def SHAKE_RAMP_END_LEVEL_MIN
    \brief Minimum allowed value of Shake_EffectRamp endLevel
    \sa Shake_EffectRamp
*/
#define SHAKE_RAMP_END_LEVEL_MIN		(-0x8000)

/** \def SHAKE_RAMP_END_LEVEL_MAX
    \brief Maximum allowed value of Shake_EffectRamp endLevel
    \sa Shake_EffectRamp
*/
#define SHAKE_RAMP_END_LEVEL_MAX		0x7FFF

/** \def SHAKE_EFFECT_ID_MIN
    \brief Minimum allowed value of Shake_Effect id
    \sa Shake_Effect
*/
#define SHAKE_EFFECT_ID_MIN			(-0x0001)

/** \def SHAKE_EFFECT_DIRECTION_MAX
    \brief Maximum allowed value of Shake_Effect direction
    \sa Shake_Effect
*/
#define SHAKE_EFFECT_DIRECTION_MAX		0xFFFE

/** \def SHAKE_EFFECT_LENGTH_MAX
    \brief Maximum allowed value of Shake_Effect length
    \sa Shake_Effect
*/
#define SHAKE_EFFECT_LENGTH_MAX			0x7FFF

/** \def SHAKE_EFFECT_DELAY_MAX
    \brief Maximum allowed value of Shake_Effect delay
    \sa Shake_Effect
*/
#define SHAKE_EFFECT_DELAY_MAX			0x7FFF

/** \enum Shake_Status
    \brief Request status.
*/
typedef enum Shake_Status
{
	SHAKE_ERROR	= -1,		/**< Error. */
	SHAKE_OK	= 0		/**< Success. */
} Shake_Status;

/** \enum Shake_Bool
    \brief Boolean type.
*/
typedef enum Shake_Bool
{
	SHAKE_FALSE	= 0,		/**< False. */
	SHAKE_TRUE	= 1		/**< True. */
} Shake_Bool;

/** \enum Shake_ErrorCode
    \brief Information about the error origin.
*/
typedef enum Shake_ErrorCode
{
	SHAKE_EC_UNSET,			/**< No error triggered yet. */
	SHAKE_EC_SUPPORT,		/**< Feature not supported. */
	SHAKE_EC_DEVICE,		/**< Device related. */
	SHAKE_EC_EFFECT,		/**< Effect related. */
	SHAKE_EC_QUERY,			/**< Query related. */
	SHAKE_EC_ARG,			/**< Invalid argument. */
	SHAKE_EC_TRANSFER		/**< Device transfer related. */
} Shake_ErrorCode;

/** \struct Shake_Device
    \brief Haptic device.
*/
struct Shake_Device;
typedef struct Shake_Device Shake_Device;

/** \enum Shake_PeriodicWaveform
    \brief Periodic effect waveform.
*/
typedef enum Shake_PeriodicWaveform
{
	SHAKE_PERIODIC_SQUARE = 0,	/**< Square waveform. */
	SHAKE_PERIODIC_TRIANGLE,	/**< Triangle waveform. */
	SHAKE_PERIODIC_SINE,		/**< Sine waveform. */
	SHAKE_PERIODIC_SAW_UP,		/**< Saw up waveform. */
	SHAKE_PERIODIC_SAW_DOWN,	/**< Saw down waveform. */
	SHAKE_PERIODIC_CUSTOM,		/**< Custom waveform. */
	SHAKE_PERIODIC_COUNT
} Shake_PeriodicWaveform;

/** \enum Shake_EffectType
    \brief Effect type.
*/
typedef enum Shake_EffectType
{
	SHAKE_EFFECT_RUMBLE = 0,	/**< Rumble effect. */
	SHAKE_EFFECT_PERIODIC,		/**< Periodic effect. */
	SHAKE_EFFECT_CONSTANT,		/**< Constant effect. */
	SHAKE_EFFECT_SPRING,		/**< Spring effect. <b>NOTE:</b> Currently not supported. */
	SHAKE_EFFECT_FRICTION,		/**< Friction effect. <b>NOTE:</b> Currently not supported. */
	SHAKE_EFFECT_DAMPER,		/**< Damper effect. <b>NOTE:</b> Currently not supported. */
	SHAKE_EFFECT_INERTIA,		/**< Inertia effect. <b>NOTE:</b> Currently not supported. */
	SHAKE_EFFECT_RAMP,		/**< Ramp effect. */
	SHAKE_EFFECT_COUNT
} Shake_EffectType;

/** \struct Shake_Envelope
    \brief Effect envelope.
*/
typedef struct Shake_Envelope
{
	uint16_t attackLength;		/**< Envelope attack duration. */
	uint16_t attackLevel;		/**< Envelope attack level. */
	uint16_t fadeLength;		/**< Envelope fade duration. */
	uint16_t fadeLevel;		/**< Envelope fade level. */
} Shake_Envelope;

/** \struct Shake_EffectRumble
    \brief Rumble effect structure.

    <b>NOTE:</b> On Linux and OSX backends this effect is internally emulated by averaging the motor values
    with a periodic effect and no direct control over individual motors is available.
*/
typedef struct Shake_EffectRumble
{
	uint16_t strongMagnitude;	/**< Magnitude of the heavy motor. */
	uint16_t weakMagnitude;		/**< Magnitude of the light motor. */
} Shake_EffectRumble;

/** \struct Shake_EffectPeriodic
    \brief Periodic effect structure.
*/
typedef struct Shake_EffectPeriodic
{
	Shake_PeriodicWaveform waveform;/**< Effect waveform. */
	uint16_t period;		/**< Period of the wave (in ms). */
	int16_t magnitude;		/**< Peak value of the wave. */
	int16_t offset;			/**< Mean value of the wave. */
	uint16_t phase;			/**< Horizontal shift of the wave. */
	Shake_Envelope envelope;	/**< Effect envelope. */
} Shake_EffectPeriodic;

/** \struct Shake_EffectConstant
    \brief Constant effect structure.
*/
typedef struct Shake_EffectConstant
{
	int16_t level;			/**< Magnitude of the effect. */
	Shake_Envelope envelope;	/**< Effect envelope. */
} Shake_EffectConstant;

/** \struct Shake_EffectRamp
    \brief Ramp effect structure.
*/
typedef struct Shake_EffectRamp
{
	int16_t startLevel;		/**< Starting magnitude of the effect. */
	int16_t endLevel;		/**< Ending magnitude of the effect. */
	Shake_Envelope envelope;	/**< Effect envelope. */
} Shake_EffectRamp;

/** \struct Shake_Effect
    \brief Effect structure.
*/
typedef struct Shake_Effect
{
	Shake_EffectType type;		/**< Effect type. */
	int16_t id;			/**< Effect id. Value of -1 creates a new effect. Value of 0 or greater modifies existing effect in a device. */
	uint16_t direction;		/**< Direction of the effect. */
	uint16_t length;		/**< Duration of the effect (in ms). */
	uint16_t delay;			/**< Delay before the effect starts playing (in ms). */
	union
	{
		Shake_EffectRumble rumble;
		Shake_EffectPeriodic periodic;
		Shake_EffectConstant constant;
		Shake_EffectRamp ramp;
	} u;				/**< Effect type data container. */
} Shake_Effect;

/** \fn Shake_Status Shake_Init(void)
    \brief Initializes libShake.
    \return On success, SHAKE_OK is returned.
    \return On error, SHAKE_ERROR is returned.
*/
Shake_Status Shake_Init(void);

/** \fn void Shake_Quit(void)
    \brief Uninitializes libShake.
*/
void Shake_Quit(void);

/** \fn int Shake_NumOfDevices(void)
    \brief Lists the number of haptic devices.
    \return On success, number of devices is returned.
    \return On error, SHAKE_ERROR is returned.
*/
int Shake_NumOfDevices(void);

/** \fn Shake_Device *Shake_Open(unsigned int id)
    \brief Opens a Shake device.
    \param id Device id.
    \return On success, pointer to Shake device is returned.
    \return On error, NULL is returned.
*/
Shake_Device *Shake_Open(unsigned int id);

/** \fn Shake_Status Shake_Close(Shake_Device *dev)
    \brief Closes a Shake device.
    \param dev Pointer to Shake device.
    \return On success, SHAKE_OK is returned.
    \return On error, SHAKE_ERROR is returned.
*/
Shake_Status Shake_Close(Shake_Device *dev);

/** \fn int Shake_DeviceId(Shake_Device *dev)
    \brief Lists id of a Shake device.
    \param dev Pointer to Shake device.
    \return On success, id of Shake device is returned.
    \return On error, SHAKE_ERROR is returned.
*/
int Shake_DeviceId(Shake_Device *dev);

/** \fn const char *Shake_DeviceName(Shake_Device *dev)
    \brief Lists name of a Shake device.
    \param dev Pointer to Shake device.
    \return On success, name of Shake device is returned.
    \return On error, NULL is returned.
*/
const char *Shake_DeviceName(Shake_Device *dev);

/** \fn int Shake_DeviceEffectCapacity(Shake_Device *dev)
    \brief Lists effect capacity of a Shake device.
    \param dev Pointer to Shake device.
    \return On success, capacity of Shake device is returned.
    \return On error, SHAKE_ERROR is returned.
*/
int Shake_DeviceEffectCapacity(Shake_Device *dev);

/** \fn Shake_Bool Shake_QueryEffectSupport(Shake_Device *dev, Shake_EffectType type)
    \brief Queries effect support of a Shake device.
    \param dev Pointer to Shake device.
    \param type Effect type to query about.
    \return SHAKE_TRUE if effect is supported.
    \return SHAKE_FALSE if effect is not supported.
*/
Shake_Bool Shake_QueryEffectSupport(Shake_Device *dev, Shake_EffectType type);

/** \fn Shake_Bool Shake_QueryWaveformSupport(Shake_Device *dev, Shake_PeriodicWaveform waveform)
    \brief Queries waveform support of a Shake device.
    \param dev Pointer to Shake device.
    \param waveform Waveform type to query about.
    \return SHAKE_TRUE if waveform is supported.
    \return SHAKE_FALSE if waveform is not supported.
*/
Shake_Bool Shake_QueryWaveformSupport(Shake_Device *dev, Shake_PeriodicWaveform waveform);

/** \fn Shake_Bool Shake_QueryGainSupport(Shake_Device *dev)
    \brief Queries gain adjustment support of a Shake device.
    \param dev Pointer to Shake device.
    \return SHAKE_TRUE if gain adjustment is supported.
    \return SHAKE_FALSE if gain adjustment is not supported.
*/
Shake_Bool Shake_QueryGainSupport(Shake_Device *dev);

/** \fn Shake_Bool Shake_QueryAutocenterSupport(Shake_Device *dev)
    \brief Queries autocenter adjustment support of a Shake device.
    \param dev Pointer to Shake device.
    \return SHAKE_TRUE if autocenter adjustment is supported.
    \return SHAKE_FALSE if autocenter adjustment is not supported.
*/
Shake_Bool Shake_QueryAutocenterSupport(Shake_Device *dev);

/** \fn Shake_Status Shake_SetGain(Shake_Device *dev, int gain)
    \brief Sets gain of a Shake device.
    \param dev Pointer to Shake device.
    \param gain [0-100] Value of a new gain level. Value of 100 means full strength of the device.
    \return On success, SHAKE_OK is returned.
    \return On error, SHAKE_ERROR is returned.
*/
Shake_Status Shake_SetGain(Shake_Device *dev, int gain);

/** \fn Shake_Status Shake_SetAutocenter(Shake_Device *dev, int autocenter)
    \brief Sets autocenter of a Shake device.
    \param dev Pointer to Shake device.
    \param autocenter [0-100] Value of a new autocenter level. Value of 0 means "autocenter disabled".
    \return On success, SHAKE_OK is returned.
    \return On error, SHAKE_ERROR is returned.
*/
Shake_Status Shake_SetAutocenter(Shake_Device *dev, int autocenter);

/** \fn Shake_Status Shake_InitEffect(Shake_Effect *effect, Shake_EffectType type)
    \brief Initializes an effect.
    \param effect Pointer to Effect struct.
    \param type Type of the effect.
    \return On success, SHAKE_OK is returned.
    \return On error, SHAKE_ERROR is returned.
*/
Shake_Status Shake_InitEffect(Shake_Effect *effect, Shake_EffectType type);

/** \fn int Shake_UploadEffect(Shake_Device *dev, Shake_Effect *effect)
    \brief Uploads an effect into a Shake device.
    \param dev Pointer to Shake device.
    \param effect Pointer to Effect struct.
    \return On success, id of the uploaded effect returned.
    \return On error, SHAKE_ERROR is returned.
*/
int Shake_UploadEffect(Shake_Device *dev, Shake_Effect *effect);

/** \fn Shake_Status Shake_EraseEffect(Shake_Device *dev, int id)
    \brief Erases effect from a Shake device.
    \param dev Pointer to Shake device.
    \param id Id of the effect to erase.
    \return On success, SHAKE_OK is returned.
    \return On error, SHAKE_ERROR is returned.
*/
Shake_Status Shake_EraseEffect(Shake_Device *dev, int id);

/** \fn Shake_Status Shake_Play(Shake_Device *dev, int id)
    \brief Starts playback of an effect.
    \param dev Pointer to Shake device.
    \param id Id of the effect to play.
    \return On success, SHAKE_OK is returned.
    \return On error, SHAKE_ERROR is returned.
*/
Shake_Status Shake_Play(Shake_Device *dev, int id);

/** \fn Shake_Status Shake_Stop(Shake_Device *dev, int id)
    \brief Stops playback of an effect.
    \param dev Pointer to Shake device.
    \param id Id of the effect to stop.
    \return On success, SHAKE_OK is returned.
    \return On error, SHAKE_ERROR is returned.
*/
Shake_Status Shake_Stop(Shake_Device *dev, int id);

/** \fn void Shake_SimpleRumble(Shake_Effect *effect, float strongPercent, float weakPercent, float secs)
    \brief Creates a simple Rumble effect.
    \param effect Pointer to Effect struct.
    \param strongPercent [0.0-1.0] Percentage of the strong motor magnitude.
    \param weakPercent [0.0-1.0] Percentage of the weak motor magnitude.
    \param secs Duration of the effect (in sec).

    \sa Shake_Effect
    \sa Shake_EffectRumble
*/
void Shake_SimpleRumble(Shake_Effect *effect, float strongPercent, float weakPercent, float secs);

/** \fn Shake_SimplePeriodic(Shake_Effect *effect, Shake_PeriodicWaveform waveform, float forcePercent, float attackSecs, float sustainSecs, float fadeSecs)
    \brief Creates a simple Periodic effect.
    \param effect Pointer to Effect struct.
    \param waveform Waveform type.
    \param forcePercent [0.0-1.0] Percentage of the effect magnitude.
    \param attackSecs Duration of the effect attack (in sec).
    \param sustainSecs Duration of the effect sustain (in sec).
    \param fadeSecs Duration of the effect fade (in sec).

    \sa Shake_Effect
    \sa Shake_EffectPeriodic
*/
void Shake_SimplePeriodic(Shake_Effect *effect, Shake_PeriodicWaveform waveform, float forcePercent, float attackSecs, float sustainSecs, float fadeSecs);

/** \fn void Shake_SimpleConstant(Shake_Effect *effect, float forcePercent, float attackSecs, float sustainSecs, float fadeSecs)
    \brief Creates a simple Constant effect.
    \param effect Pointer to Effect struct.
    \param forcePercent [0.0-1.0] Percentage of the effect magnitude.
    \param attackSecs Duration of the effect attack (in sec).
    \param sustainSecs Duration of the effect sustain (in sec).
    \param fadeSecs Duration of the effect fade (in sec).

    \sa Shake_Effect
    \sa Shake_EffectConstant
*/
void Shake_SimpleConstant(Shake_Effect *effect, float forcePercent, float attackSecs, float sustainSecs, float fadeSecs);

/** \fn Shake_SimpleRamp(Shake_Effect *effect, float startForcePercent, float endForcePercent, float attackSecs, float sustainSecs, float fadeSecs)
    \brief Creates a simple Ramp effect.
    \param effect Pointer to Effect struct.
    \param startForcePercent [0.0-1.0] Percentage of the effect magnitude at start.
    \param endForcePercent [0.0-1.0] Percentage of the effect magnitude at end.
    \param attackSecs Duration of the effect attack (in sec).
    \param sustainSecs Duration of the effect sustain (in sec).
    \param fadeSecs Duration of the effect fade (in sec).

    \sa Shake_Effect
    \sa Shake_EffectRamp
*/
void Shake_SimpleRamp(Shake_Effect *effect, float startForcePercent, float endForcePercent, float attackSecs, float sustainSecs, float fadeSecs);

/** \fn Shake_ErrorCode Shake_GetErrorCode(void)
    \brief Informs about the error type.
    \return Error code reflecting the last error type.
*/
Shake_ErrorCode Shake_GetErrorCode(void);

#ifdef __cplusplus
}
#endif

#endif /* _SHAKE_H_ */
