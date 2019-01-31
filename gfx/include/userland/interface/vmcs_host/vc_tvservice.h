/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

/*
 * TV service host API,
 * See vc_hdmi.h for HDMI related constants
 * See vc_sdtv.h for SDTV related constants
 */

#ifndef _VC_TVSERVICE_H_
#define _VC_TVSERVICE_H_

#include "vcinclude/common.h"
#include "interface/vcos/vcos.h"
#include "interface/vchi/vchi.h"
#include "interface/vmcs_host/vc_tvservice_defs.h"
#include "interface/vmcs_host/vc_hdmi.h"
#include "interface/vmcs_host/vc_sdtv.h"

/**
 * \file
 *
 * This API defines the controls for both HDMI and analogue TVs. It allows
 * the user to dynamically switch between HDMI and SDTV without having
 * to worry about switch one off before turning the other on. It also
 * allows the user to query the supported HDMI resolutions and audio
 * formats and turn on/off copy protection.
 *
 * There are three ways to turn on HDMI: preferred mode; best matched mode
 * and explicit mode. See the three power on functions for details.
 */

/**
 * TVSERVICE_CALLBACK_T is the callback function for host side notification.
 * Host applications register a single callback for all TV related notifications.
 * See <DFN>VC_HDMI_NOTIFY_T</DFN> and <DFN>VC_SDTV_NOTIFY_T</DFN> in vc_hdmi.h and vc_sdtv.h
 * respectively for list of reasons and respective param1 and param2
 *
 * @param callback_data is the context passed in during the call to vc_tv_register_callback
 *
 * @param reason is the notification reason
 *
 * @param param1 is the first optional parameter
 *
 * @param param2 is the second optional parameter
 *
 * @return void
 */
typedef void (*TVSERVICE_CALLBACK_T)(void *callback_data, uint32_t reason, uint32_t param1, uint32_t param2);

/* API at application start time */
/**
 * <DFN>vc_vchi_tv_init</DFN> is called at the beginning of the application
 * to initialise the client to TV service
 *
 * @param initialise_instance is the VCHI instance
 *
 * @param array of pointers of connections
 *
 * @param number of connections (currently this is always <DFN>1</DFN>
 *
 * @return Zero is successful A negative return value indicates failure (which may mean it has not been started on VideoCore).
 */
VCHPRE_ int vc_vchi_tv_init(VCHI_INSTANCE_T initialise_instance, VCHI_CONNECTION_T **connections, uint32_t num_connections );

/**
 * <DFN>vc_vchi_tv_stop</DFN> is called to stop the host side of TV service.
 *
 * @param none
 *
 * @return void
 */
VCHPRE_ void vc_vchi_tv_stop( void );

/**
 * Host applications should call <DFN>vc_tv_register_callback</DFN> at
 * the beginning to register a callback function to handle all notifications.
 * See <DFN>TVSERVICE_CALLBACK_T </DFN>
 *
 * @param callback function
 *
 * @param callback_data is the context to be passed when function is called
 *
 * @return void
 */
VCHPRE_ void vc_tv_register_callback(TVSERVICE_CALLBACK_T callback, void *callback_data);

/**
 * <DFN>vc_tv_unregister_callback</DFN> removes a function registered with
 * <DFN>vc_tv_register_callback</DFN> from the list of callbacks.
 *
 * @param callback function
 *
 * @return void
 */
VCHPRE_ void vc_tv_unregister_callback(TVSERVICE_CALLBACK_T callback);

/**
 * <DFN>vc_tv_unregister_callback</DFN> removes a function registered with
 * <DFN>vc_tv_register_callback</DFN> from the list of callbacks.
 * In contrast to vc_tv_unregister_callback this one matches not only the
 * function pointer but also the data pointer before removal.
 *
 * @param callback function
 *
 * @return void
 */
VCHPRE_ void vc_tv_unregister_callback_full(TVSERVICE_CALLBACK_T callback, void *callback_data);

/**
 * In the following API any functions applying to HDMI only will have hdmi_
 * in the name, ditto for SDTV only will have sdtv_ in the name,
 * Otherwise the function applies to both SDTV and HDMI (e.g. power off)
 */

/**
 * <DFN>vc_tv_get_state</DFN> is used to obtain the current TV state.
 * Host applications should call this function right after registering
 * a callback in case any notifications are missed.
 *
 * Now deprecated - use vc_tv_get_display_state instead
 *
 * @param pointer to TV_GET_STATE_RESP_T
 *
 * @return zero if the command is sent successfully, non zero otherwise
 * If the command fails to be sent, passed in state is unchanged
 *
 */
VCHPRE_ int VCHPOST_ vc_tv_get_state(TV_GET_STATE_RESP_T *tvstate);

/**
 * <DFN>vc_tv_get_display_state</DFN> is used to obtain the current TV display
 * state. This function supersedes vc_tv_get_state (which is only kept for
 * backward compatibility.
 * Host applications should call this function right after registering
 * a callback in case any notifications are missed.
 *
 * @param pointer to TV_DISPLAY_STATE_T
 *
 * @return zero if the command is sent successfully, non zero otherwise
 * If the command fails to be sent, passed in state is unchanged
 *
 */
VCHPRE_ int VCHPOST_ vc_tv_get_display_state(TV_DISPLAY_STATE_T *tvstate);

/**
 * Use <DFN>vc_tv_hdmi_power_on_preferred</DFN> if you don't care what resolutions
 * a TV supports and just want to turn on its native resolution. Analogue TV will
 * be powered down if on (same for the following two HDMI power on functions.)
 * If power on is successful, a host application must wait for the power on complete
 * callback before attempting to open the display.
 *
 * @param none
 *
 * @return single value interpreted as HDMI_RESULT_T (zero means success)
 *         if successful, there will be a callback when the power on is complete
 *
 **/
VCHPRE_ int VCHPOST_ vc_tv_hdmi_power_on_preferred( void );

/**
 * Same as above, but tell the TV to enter 3D mode. The TV will go to 2D mode
 * if the preferred mode doesn't support 3D.
 **/
VCHPRE_ int VCHPOST_ vc_tv_hdmi_power_on_preferred_3d( void );

/**
 * Use <DFN>vc_tv_hdmi_power_on_best</DFN> to power on HDMI at best matched resolution
 * based on passed in parameters. Use HDMI_MODE_MATCH_FRAMERATE if you want to
 * match the frame rate; use HDMI_MODE_MATCH_RESOLUTION if you want to match on
 * screen size; add HDMI_MODE_MATCH_SCANMODE if you want to force
 * interlaced/progressive mode. If no matching mode is found, the native resolution
 * will be used instead.
 *
 * @param width is the desired minimum screen width
 *
 * @param height is the desired minimum screen height
 *
 * @param rate is the desired frame rate
 *
 * @param scan_mode (HDMI_NONINTERLACED / HDMI_INTERLACED) is the desired scan mode
 *
 * @param match flags is the matching flag <DFN>EDID_MODE_MATCH_FLAG_T</DFN>
 *
 * @return same as <DFN>vc_tv_hdmi_power_on_preferred</DFN>
 */
VCHPRE_ int VCHPOST_ vc_tv_hdmi_power_on_best(uint32_t width, uint32_t height, uint32_t frame_rate,
                                              HDMI_INTERLACED_T scan_mode, EDID_MODE_MATCH_FLAG_T match_flags);

/**
 * Same as above, but tell the TV to enter 3D mode. The TV will go to 2D mode
 * if no suitable 3D mode can be found.
 **/
VCHPRE_ int VCHPOST_ vc_tv_hdmi_power_on_best_3d(uint32_t width, uint32_t height, uint32_t frame_rate,
                                              HDMI_INTERLACED_T scan_mode, EDID_MODE_MATCH_FLAG_T match_flags);

/**
 * Use <DFN>vc_tv_hdmi_power_on_explicit</DFN> if you want full control over what mode
 * the TV is driven. This function is used, for example, when the host has the EDID
 * and HDMI middleware does not know. If HDMI middleware has knowledge of EDID, the
 * passed in mode is still subject to TV's supported modes
 *
 * @param mode (HDMI_MODE_HDMI/HDMI_MODE_DVI/HDMI_MODE_3D)
 *
 * @param group (HDMI_RES_GROUP_CEA/HDMI_RES_GROUP_DMT)
 *
 * @param code either <DFN>HDMI_CEA_RES_CODE_T</DFN> or <DFN>HDMI_DMT_RES_CODE_T</DFN>
 *
 * @return same as <DFN>vc_tv_hdmi_power_on_preferred</DFN>
 */
VCHPRE_ int VCHPOST_ vc_tv_hdmi_power_on_explicit_new(HDMI_MODE_T mode, HDMI_RES_GROUP_T group, uint32_t code);

/**
 * <DFN>vc_tv_sdtv_power_on</DFN> is used to turn on analogue TV. HDMI will
 * automatically be powered off if on.
 *
 * @param SDTV mode <DFN>SDTV_MODE_T</DFN>
 *
 * @param options <DFN>SDTV_OPTIONS_T</DFN>
 *
 * @return single value (zero means success) if successful, there will be a callback when the power on is complete
 *
 */
VCHPRE_ int VCHPOST_ vc_tv_sdtv_power_on(SDTV_MODE_T mode, SDTV_OPTIONS_T *options);

/**
 * <DFN>vc_tv_power_off</DFN> is used to turn off either analogue or HDMI output.
 * If HDMI is powered down, there will be a callback with reason UNPLUGGED (if no
 * cable is attached) or STANDBY (if a cable is attached)
 *
 * @param none
 *
 * @return whether command is succcessfully sent
 *
 */
VCHPRE_ int VCHPOST_ vc_tv_power_off( void );

/**
 * <DFN>vc_tv_hdmi_get_supported_modes</DFN> is used to get a list of supported
 * modes for a particular standard (CEA/DMT/CEA3D). Prefer resolution (group and mode)
 * is also returned, if needed. If there are more modes supported than the size of the array
 * supply, only the array will be filled.
 *
 * @param group(HDMI_RES_GROUP_CEA/HDMI_RES_GROUP_DMT)
 *
 * @param array of <DFN>TV_SUPPORT_MODE_T</DFN> struct
 *
 * @param length of array above (in elements, not bytes)
 *
 * @pointer to preferred group (can be NULL)
 *
 * @pointer to prefer mode code (can be NULL)
 *
 * @return the number of modes actually written in the array,
 *         zero means no modes (no EDID or cable unplugged)
 *
 */
VCHPRE_ int VCHPOST_ vc_tv_hdmi_get_supported_modes_new(HDMI_RES_GROUP_T group,
                                                    TV_SUPPORTED_MODE_NEW_T *supported_modes,
                                                    uint32_t max_supported_modes,
                                                    HDMI_RES_GROUP_T *preferred_group,
                                                    uint32_t *preferred_mode);
/**
 * <DFN>vc_tv_hdmi_mode_supported</DFN> is used to query whether a particular mode
 * is supported or not.
 *
 * @param resolution standard (HDMI_RES_GROUP_CEA/HDMI_RES_GROUP_DMT)
 *
 * @param mode code
 *
 * @return > 0 means supported, 0 means unsupported, < 0 means error
 *
 */
VCHPRE_ int VCHPOST_ vc_tv_hdmi_mode_supported(HDMI_RES_GROUP_T group,
                                               uint32_t mode);

/**
 * <DFN>vc_tv_hdmi_audio_supported</DFN> is used to query whether a
 * particular audio format is supported. By default a device has to support
 * 16-bit stereo PCM at 32/44.1/48 kHz if audio is supported at all.
 * Support of other audio formats allow SPDIF to be used.
 * A return value of zero means the audio format is completely supported.
 * Any non-zero values are interpreted as bit mask (EDID_AUDIO_SUPPORT_FLAG_T).
 * For example, if EDID_AUDIO_NO_SUPPORT is set, the audio format is not supported.
 * If EDID_AUDIO_CHAN_UNSUPPORTED is set, the max no. of channels has exceeded.
 *
 * @param audio format supplied as (<DFN>EDID_AudioFormat</DFN> + <DFN>EDID_AudioCodingExtension</DFN>)
 *
 * @param no. of channels (1-8)
 *
 * @param sample rate <DFN>EDID_AudioSampleRate</DFN> but NOT "refer to header"
 *
 * @param bit rate (or sample size if pcm) use <DFN>EDID_AudioSampleSize</DFN> as sample size argument
 *
 * @return: single value return interpreted as flags in <DFN>EDID_AUDIO_SUPPORT_FLAG_T</DFN>
 *
 */
VCHPRE_ int VCHPOST_ vc_tv_hdmi_audio_supported(uint32_t audio_format, uint32_t num_channels,
                                                EDID_AudioSampleRate fs, uint32_t bitrate);

/**
 * Use <DFN>vc_tv_enable_copyprotect</DFN> to turn on copy protection.
 * For HDMI, only HDMI_CP_HDCP is recognised.
 * For SDTV, use one of the values in SDTV_CP_MODE_T
 *
 * @param copy protect mode
 *
 * @param time out in milliseconds (only applicable to HDMI)
 *
 * @return 0 means success, additional result via callback
 *
 */
VCHPRE_ int VCHPOST_ vc_tv_enable_copyprotect(uint32_t cp_mode, uint32_t timeout);

/**
 * Use <DFN>vc_tv_disable_copyprotect</DFN> to turn off copy protection
 *
 * @param none
 *
 * @rturn 0 means success, additional result via callback
 *
 */
VCHPRE_ int VCHPOST_ vc_tv_disable_copyprotect( void );

/**
 * Use <DFN>vc_tv_show_info</DFN> to show or hide info screen.
 * Only usable in HDMI at the moment.
 *
 * @param show (1) or hide (0) info screen
 *
 * @return zero if command is successfully sent
 *
 */
VCHPRE_ int VCHPOST_ vc_tv_show_info(uint32_t show);

/**
 * <DFN>vc_tv_hdmi_get_av_latency</DFN> is used to get the AV latency
 * (in ms) for HDMI (lipsync), only valid if HDMI is currently powered on,
 * otherwise you get zero. The latency is defined as the relative delay
 * of the video stream to the audio stream
 *
 * @param none
 *
 * @return latency (zero if error or latency is not defined),
 *         < 0 if failed to send command)
 *
 */
VCHPRE_ int VCHPOST_ vc_tv_hdmi_get_av_latency( void );

/**
 * Use <DFN>vc_tv_hdmi_set_hdcp_key</DFN> to download HDCP key to HDCP middleware
 *
 * @param AES encrypted key block (328 bytes)
 *
 * @return single value return indicating queued status
 *         Callback indicates the validity of key
 */
VCHPRE_ int VCHPOST_ vc_tv_hdmi_set_hdcp_key(const uint8_t *key);

/**
 * Use <DFN>vc_tv_hdmi_set_hdcp_revoked_list</DFN> to download SRM
 * revoked list
 *
 * @param list
 *
 * @param size of list (no. of keys)
 *
 * @return single value return indicating queued status
 *         Callback indicates the number of keys set (zero if failed, unless you are clearing the list)
 *
 */
VCHPRE_ int VCHPOST_ vc_tv_hdmi_set_hdcp_revoked_list(const uint8_t *list, uint32_t num_keys);

/**
 * <DFN>vc_tv_hdmi_set_spd</DFN> is used to set the SPD infoframe.
 *
 * @param manufacturer (max. 8 characters)
 *
 * @param description (max. 16 characters)
 *
 * @param product type <DFN>HDMI_SPD_TYPE_CODE_T</DFN>
 *
 * @return whether command was sent successfully (zero means success)
 *
 */
VCHPRE_ int VCHPOST_ vc_tv_hdmi_set_spd(const char *manufacturer, const char *description, HDMI_SPD_TYPE_CODE_T type);

/**
 * <DFN>vc_tv_hdmi_set_display_options</DFN> is used to set the
 * active area for HDMI (bar width/height should be set to zero if absent)
 * This information is conveyed in AVI infoframe.
 *
 * @param aspect ratio <DFN>HDMI_ASPECT_T</DFN>
 *
 * @param left bar width
 *
 * @param right bar width
 *
 * @param top bar height
 *
 * @param bottom bar height
 *
 * @return whether command was sent successfully (zero means success)
 *
 */
VCHPRE_ int VCHPOST_ vc_tv_hdmi_set_display_options(HDMI_ASPECT_T aspect, uint32_t left_bar_width, uint32_t right_bar_width, uint32_t top_bar_height, uint32_t bottom_bar_height, uint32_t overscan_flags);

/**
 * Use <DFN>vc_tv_test_mode_start</DFN> to generate test signal.
 * At the moment only DVI test signal is supported.
 * HDMI must be powered off before this function is called.
 *
 * @param 24-bit background colour (if applicable)
 *
 * @param test mode <DFN>TV_TEST_MODE_T</DFN>
 *
 * @return whether command was sent successfully (zero means success)
 *
 */
VCHPRE_ int VCHPOST_ vc_tv_test_mode_start(uint32_t colour, TV_TEST_MODE_T test_mode);

/**
 * Use <DFN>vc_tv_test_mode_stop</DFN> to stop the test signal and power down
 * HDMI.
 *
 * @param none
 *
 * @return whether command was sent successfully (zero means success)
 *
 */
VCHPRE_ int VCHPOST_ vc_tv_test_mode_stop( void );

/**
 * <DFN>vc_tv_hdmi_ddc_read</DFN> allows an host application to read EDID
 * with DDC protocol.
 *
 * @param offset
 *
 * @param length to read (this is typically 128 bytes to coincide with EDID block size)
 *
 * @param pointer to buffer, must be 16 byte aligned
 *
 * @returns length of data read (so zero means error) and the buffer will be filled
 *          only if there is no error
 *
 */
VCHPRE_ int VCHPOST_ vc_tv_hdmi_ddc_read(uint32_t offset, uint32_t length, uint8_t *buffer);

/**
 * Sets the TV state to attached.
 * Required when hotplug interrupt is not handled by VideoCore.
 *
 * @param attached  non-zero if the TV is attached or zero for unplugged.
 */
VCHPRE_ int VCHPOST_ vc_tv_hdmi_set_attached(uint32_t attached);

/**
 * Sets one of the HDMI properties. HDMI properties persist
 * between HDMI power on/off
 *
 * @param property [in]
 *
 * @return zero if successful, non-zero otherwise
 */
VCHPRE_ int VCHPOST_ vc_tv_hdmi_set_property(const HDMI_PROPERTY_PARAM_T *property);

/**
 * Gets the current value of an HDMI property.
 *
 * @param property [in/out]
 *
 * @return zero if success (param1/param2 will be set), non-zero otherwise (param1/param2 will not be set)
 */
VCHPRE_ int VCHPOST_ vc_tv_hdmi_get_property(HDMI_PROPERTY_PARAM_T *property);

/**
 * Converts the notification reason to a string.
 *
 * @param reason is the notification reason
 * @return  The notification reason as a string.
 */
VCHPRE_ const char* vc_tv_notification_name(VC_HDMI_NOTIFY_T reason);

/**
 * Get the unique device ID from the EDID
 * @param pointer to device ID struct
 * @return zero if successful, non-zero if failed.
 */
VCHPRE_ int VCHPOST_  vc_tv_get_device_id(TV_DEVICE_ID_T *id);

// temporary: maintain backwards compatibility
VCHPRE_ int VCHPOST_ vc_tv_hdmi_get_supported_modes(HDMI_RES_GROUP_T group,
                                                    TV_SUPPORTED_MODE_T *supported_modes,
                                                    uint32_t max_supported_modes,
                                                    HDMI_RES_GROUP_T *preferred_group,
                                                    uint32_t *preferred_mode);
// temporary: maintain backwards compatibility
VCHPRE_ int VCHPOST_ vc_tv_hdmi_power_on_explicit(HDMI_MODE_T mode, HDMI_RES_GROUP_T group, uint32_t code);

#endif
