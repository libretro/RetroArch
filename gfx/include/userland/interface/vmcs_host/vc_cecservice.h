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
 * CEC service host API,
 * See vc_cec.h and vc_cecservice_defs.h for related constants
 */

#ifndef _VC_CECSERVICE_H_
#define _VC_CECSERVICE_H_

#include "vcinclude/common.h"
#include "interface/vcos/vcos.h"
#include "interface/vchi/vchi.h"
#include "interface/vmcs_host/vc_cecservice_defs.h"
#include "interface/vmcs_host/vc_cec.h"

/**
 * \file
 * This API defines the controls for CEC. HDMI must be powered on before
 * CEC is available (subject to CEC support in TV).
 *
 * In general, a zero return value indicates success; a negative return
 * value indicates error in VCHI layer; a positive return value indicates
 * alternative return value from the server
 */

/**
 * Callback function for host side notification
 * This is the SAME as the callback function type defined in vc_cec.h
 * Host applications register a single callback for all CEC related notifications.
 * See vc_cec.h for meanings of all parameters
 *
 * @param callback_data is the context passed in by user in <DFN>vc_cec_register_callback</DFN>
 *
 * @param reason bits 15-0 is VC_CEC_NOTIFY_T in vc_cec.h;
 *               bits 23-16 is the valid length of message in param1 to param4 (LSB of param1 is the byte0, MSB of param4 is byte15), little endian
 *               bits 31-24 is the return code (if any)
 *
 * @param param1 is the first parameter
 *
 * @param param2 is the second parameter
 *
 * @param param3 is the third parameter
 *
 * @param param4 is the fourth parameter
 *
 * @return void
 */
typedef void (*CECSERVICE_CALLBACK_T)(void *callback_data, uint32_t reason, uint32_t param1, uint32_t param2, uint32_t param3, uint32_t param4);

//API at application start time
/**
 * Call <DFN>vc_vchi_cec_init</DFN> to initialise the CEC service for use.
 *
 * @param initialise_instance is the VCHI instance
 * @param connections are array of pointers to VCHI connections
 * @param num_connections is the number of connections in array
 * @return void
 **********************************************************/
VCHPRE_ void vc_vchi_cec_init(VCHI_INSTANCE_T initialise_instance, VCHI_CONNECTION_T **connections, uint32_t num_connections );

/**
 * Call <DFN>vc_vchi_cec_stop</DFN> to stop host side CEC service.
 *
 * @param none
 * @return void
 **********************************************************/
VCHPRE_ void vc_vchi_cec_stop( void );

/**
 * Host applications use <DFN>vc_cec_register_callaback</DFN> to register
 * callback to handle all CEC notifications. If more than one applications
 * need to use CEC, there should be ONE central application which acts on
 * behalf of all clients and handles all communications with CEC services.
 *
 * @param callback function
 * @param context to be passed when function is called
 * @return void
 ***********************************************************/
VCHPRE_ void vc_cec_register_callback(CECSERVICE_CALLBACK_T callback, void *callback_data);

//Service API
/**
 * Use <DFN>vc_cec_register_command</DFN> to register an opcode to
 * to forwarded to the host application. By default <Feature Abort>
 * is always forwarded. Once an opcode is registered, it is left to
 * the host application to reply to a CEC message (where appropriate).
 * It is recommended NOT to register the following commands as they
 * are replied to automatically by CEC middleware:
 * <Give Physical Address>, <Give Device Vendor ID>, <Give OSD Name>,
 * <Get CEC Version>, <Give Device Power Status>, <Menu Request>,
 * and <Get Menu Language>
 * In addition, the following opcodes cannot be registered:
 * <User Control Pressed>, <User Control Released>,
 * <Vendor Remote Button Down>, <Vendor Remote Button Up>,
 * and <Abort>.
 * <Feature Abort> is always forwarded if it is the reply
 * of a command the host sent.
 *
 * @param opcode to be registered.
 *
 * @return zero if the command is successful, non-zero otherwise
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_register_command(CEC_OPCODE_T opcode);

/**
 * <DFN>vc_cec_register_all</DFN> registers all opcodes except <Abort>
 *  to be forwarded as CEC_RX notification.
 * Button presses <User Control Pressed>, etc. will still be forwarded
 * separately as VC_CEC_BUTTON_PRESSED etc. notification.
 *
 * @param None
 *
 * @return zero if the command is successful, non-zero otherwise
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_register_all( void );

/**
 * Use <DFN>vc_cec_deregister_command</DFN> to remove an opcode from
 * the filter for forwarding. By default <Feature Abort> is always forwarded.
 * The following opcode cannot be deregistered:
 * <User Control Pressed>, <User Control Released>,
 * <Vendor Remote Button Down>, <Vendor Remote Button Up>,
 * and <Abort>.
 *
 * @param opcode to be deregistered
 *
 * @return zero if the command is successful, non-zero otherwise
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_deregister_command(CEC_OPCODE_T opcode);

/**
 * <DFN>vc_cec_deregister_all</DFN> removes all registered opcodes,
 * except the ones (e.g. button presses) which are always forwarded.
 *
 * @param None
 *
 * @return zero if the command is successful, non-zero otherwise
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_deregister_all( void );

/**
 * <DFN>vc_cec_send_message</DFN> allows a host application to
 * send a CEC message to another device. There are several
 * built-in functions for sending command messages. The host
 * application MUST have a valid logical address (between 1 and
 * 14 inclusive) before it can send a message.
 * (For poll message set payload to NULL and length to zero).
 *
 * @param Follower's logical address
 *
 * @param Message payload WITHOUT the header byte (can be NULL)
 *
 * @param Payload length WITHOUT the header byte (can be zero)
 *
 * @param VC_TRUE if the message is a reply to an incoming message
 *
 * @return zero if the command is successful, non-zero otherwise
 *         If the command is successful, there will be a Tx callback
 *         in due course to indicate whether the message has been
 *         acknowledged by the recipient or not
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_send_message(const uint32_t follower,
                                         const uint8_t *payload,
                                         uint32_t length,
                                         vcos_bool_t is_reply);
/**
 * <DFN>vc_cec_get_logical_address</DFN> gets the logical address,
 * If one is being allocated 0xF (unregistered) will be set.
 * A address value of 0xF also means CEC system is not yet ready
 * to send or receive any messages.
 *
 * @param pointer to logical address (set to allocated address)
 *
 * @return zero if the command is successful, non-zero otherwise
 *         logical_address is not modified if command failed
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_get_logical_address(CEC_AllDevices_T *logical_address);

/**
 * <DFN>vc_cec_alloc_logical_address</DFN> starts the allocation
 * of a logical address. Logical address is automatically allocated
 * after HDMI power on is complete and AV mute is deassert.
 * The host only needs to call this if the
 * initial allocation failed (logical address being 0xF and
 * physical address is NOT 0xFFFF from <DFN>VC_CEC_LOGICAL_ADDR</DFN>
 * notification), or if the host explicitly released its logical
 * address.
 *
 * @param none
 *
 * @return zero if the command is successful, non-zero otherwise
 *         If successful, there will be a callback notification
 *         <DFN>VC_CEC_LOGICAL_ADDR</DFN>.
 *         The host should wait for this before calling this
 *         function again.
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_alloc_logical_address( void );

/**
 * Normally <DFN>vc_cec_release_logical_address</DFN> will not
 * be called by the host application. It is used to release
 * our logical address. This effectively disables CEC.
 * The host will need to allocate a new logical address before
 * doing any CEC calls (send/receive message, get topology, etc.).
 *
 * @param none
 *
 * @return zero if the command is successful, non-zero otherwise
 *         The host should get a callback <DFN>VC_CEC_LOGICAL_ADDR</DFN>
 *         with 0xF being the logical address and 0xFFFF
 *         being the physical address.
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_release_logical_address( void );

/**
 * Use <DFN>vc_cec_get_topology</DFN> to get the topology.
 *
 * @param pointer to <DFN>VC_CEC_TOPOLOGY_T</DFN>
 *
 * @return zero if the command is successful, non-zero otherwise
 *         If successful, the topology will be set, otherwise it is unchanged
 *         A topology with only 1 device (us) means CEC is not supported.
 *         If there is no topology available, this also returns a failure.
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_get_topology( VC_CEC_TOPOLOGY_T* topology);

/**
 * Use <DFN>vc_cec_set_vendor_id</DFN> to
 * set the response to <Give Device Vendor ID>
 *
 * @param 24-bit IEEE vendor id
 *
 * @return zero if the command is successful, non-zero otherwise
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_set_vendor_id( const uint32_t id );

/**
 * Use <DFN>vc_cec_set_osd_name</DFN> to
 * set the response to <Give OSD Name>
 *
 * @param OSD name (14 byte char array)
 *
 * @return zero if the command is successful, non-zero otherwise
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_set_osd_name( const char* name );

/**
 * <DFN>vc_cec_get_physical_address</DFN> gets our physical address
 *
 * @param pointer to physical address (returned as 16-bit packed value)
 *
 * @return zero if the command is successful, non-zero otherwise
 *          If failed, physical address argument will not be changed
 *          A physical address of 0xFFFF means CEC is not supported
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_get_physical_address(uint16_t *physical_address);

/**
 * <DFN>vc_cec_get_vendor_id(</DFN> gets the vendor id of a particular logical address
 *
 * @param logical_address is the logical address of the device [in]
 *
 * @param vendorid is the pointer to vendor ID (24-bit IEEE OUI value) [out]
 *
 * @return zero if the command is successful, non-zero otherwise
 *         If failed, vendor id argument will not be changed
 *         A vendor ID of 0xFFFFFF means the device does not exist
 *         A vendor ID of 0x0 means vendor ID is not known and
 *         the application can send <Give Device Vendor ID> to that device
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_get_vendor_id(const CEC_AllDevices_T logical_address, uint32_t *vendor_id);

/**
 * <DFN>vc_cec_device_type(</DFN> returns the default device type of a particular
 * logical address, which can be used as the argument to vc_cec_send_ReportPhysicalAddress.
 *
 * @param logical address
 *
 * @return the default device type, if there is any error, the return device
 *         type will be CEC_DeviceType_Invalid
 *
 ************************************************************/
VCHPRE_ CEC_DEVICE_TYPE_T VCHPOST_ vc_cec_device_type(const CEC_AllDevices_T logical_address);

/**
 * These couple of functions are provided for host application's convenience:
 * If the xmit message is encapsulate in a VC_CEC_MESSAGE_T
 * then it can be sent as a normal message (not as a reply)
 * and the initiator field is ignored with vc_cec_send_message2
 * and return zero for success
 *
 * Applications can call vc_cec_param2message to turn the callback parameters
 * into a VC_CEC_MESSAGE_T (not for LOGICAL_ADDR and TOPOLOGY callbacks).
 * It also returns zero for success.
 */
VCHPRE_ int VCHPOST_ vc_cec_send_message2(const VC_CEC_MESSAGE_T *message);

VCHPRE_ int VCHPOST_ vc_cec_param2message( const uint32_t reason, const uint32_t param1,
                                           const uint32_t param2, const uint32_t param3,
                                           const uint32_t param4, VC_CEC_MESSAGE_T *message);

//Extra API if CEC is running in passive mode
//If CEC is not in passive mode the following 3 functions always
//return failure
/**
 * <DFN> vc_cec_poll_address </DFN> sets and polls a particular address to find out
 * its availability in the CEC network. Only available when CEC is running in passive
 * mode. The host can only call this function during logical address allocation stage.
 *
 * @param logical address to try
 *
 * @return 0 if poll is successful (address is occupied)
 *        >0 if poll is unsuccessful (Error code is in VC_CEC_ERROR_T in vc_cec.h)
 *        <0 VCHI errors
 */
VCHPRE_ int VCHPOST_ vc_cec_poll_address(const CEC_AllDevices_T logical_address);

/**
 * <DFN> vc_cec_set_logical_address </DFN> sets the logical address, device type
 * and vendor ID to be in use. Only available when CEC is running in passive
 * mode. It is the responsibility of the host to make sure the logical address
 * is actually free to be used. Physical address will be what is read from EDID.
 *
 * @param logical address
 *
 * @param device type
 *
 * @param vendor ID
 *
 * @return 0 if successful, non-zero otherwise
 */
VCHPRE_ int VCHPOST_ vc_cec_set_logical_address(const CEC_AllDevices_T logical_address,
                                                const CEC_DEVICE_TYPE_T device_type,
                                                const uint32_t vendor_id);

/**
 * <DFN> vc_cec_add_device </DFN> adds a new device to topology.
 * Only available when CEC is running in passive mode. Device will be
 * automatically removed from topology if a failed xmit is detected.
 * If last_device is true, it will trigger a topology computation
 * (and may trigger a topology callback).
 *
 * @param logical address
 *
 * @param physical address
 *
 * @param device type
 *
 * @param true if this is the last device, false otherwise
 *
 * @return 0 if successful, non-zero otherwise
 */
VCHPRE_ int VCHPOST_ vc_cec_add_device(const CEC_AllDevices_T logical_address,
                                       const uint16_t physical_address,
                                       const CEC_DEVICE_TYPE_T device_type,
                                       vcos_bool_t last_device);

/**
 * <DFN> vc_cec_set_passive </DFN> enables and disables passive mode.
 * Call this function first (with VC_TRUE as the argument) to enable
 * passive mode before calling any of the above passive API functions
 *
 * @param TRUE to enable passive mode, FALSE to disable
 *
 * @return 0 if successful, non-zero otherwise
 */
VCHPRE_ int VCHPOST_ vc_cec_set_passive(vcos_bool_t enabled);

//API for some common CEC messages
/**
 * Functions beginning with vc_cec_send_xxx make it easier for the
 * host application to send CEC message xxx to other devices
 */
/**
 * <DFN>vc_cec_send_FeatureAbort</DFN> sends <Feature Abort>
 * for a received command.
 *
 * @param follower (cannot be 0xF)
 *
 * @param rejected opcode
 *
 * @param reject reason <DFN>CEC_ABORT_REASON_T</DFN>
 *
 * @return zero if the command is successful, non-zero otherwise
 *         Tx callback if successful
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_send_FeatureAbort(uint32_t follower,
                                              CEC_OPCODE_T opcode,
                                              CEC_ABORT_REASON_T reason);

/**
 * <DFN>vc_cec_send_ActiveSource</DFN> broadcasts
 * <Active Source> to all devices
 *
 * @param physical address (16-bit packed)
 *
 * @param reply or not (normally not)
 *
 * @return zero if the command is successful, non-zero otherwise
 *         Tx callback if successful
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_send_ActiveSource(uint16_t physical_address, vcos_bool_t is_reply);

/**
 * <DFN>vc_cec_send_ImageViewOn</DFN> sends <Image View On>
 *
 * @param follower
 *
 * @param reply or not (normally not)
 *
 * @return zero if the command is successful, non-zero otherwise
 *         Tx callback if successful
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_send_ImageViewOn(uint32_t follower, vcos_bool_t is_reply);

/**
 * <DFN>vc_cec_send_SetOSDString</DFN> sends <Set OSD String>
 *
 * @param follower
 *
 * @param string (char[13])
 *
 * @param display control <DFN>CEC_DISPLAY_CONTROL_T</DFN>
 *
 * @param reply or not (normally not)
 *
 * @return zero if the command is successful, non-zero otherwise
 *         Tx callback if successful
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_send_SetOSDString(uint32_t follower,
                                              CEC_DISPLAY_CONTROL_T disp_ctrl,
                                              const char* string,
                                              vcos_bool_t is_reply);

/**
 * <DFN>vc_cec_send_Standby</DFN> sends <Standby>.
 * This will put any/all devices to standby if they support
 * this CEC message.
 *
 * @param follower (can be 0xF)
 *
 * @param reply or not (normally not)
 *
 * @return zero if the command is successful, non-zero otherwise
 *         Tx callback if successful
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_send_Standby(uint32_t follower, vcos_bool_t is_reply);

/**
 * <DFN>vc_cec_send_MenuStatus</DFN> sends <Menu Status>
 * (response to <Menu Request>)
 *
 * @param follower
 *
 * @param menu state <DFN>CEC_MENU_STATE_T</DFN> but NOT CEC_MENU_STATE_QUERY
 *
 * @param reply or not (should always be yes)
 *
 * @return zero if the command is successful, non-zero otherwise
 *         Tx callback if successful
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_send_MenuStatus(uint32_t follower,
                                            CEC_MENU_STATE_T menu_state,
                                            vcos_bool_t is_reply);

/**
 * <DFN>vc_cec_send_ReportPhysicalAddress</DFN> broadcasts
 * <Report Physical Address> to all devices. Note although
 * the passed in device type can be override the default one
 * associated the allocated logical address, it is not
 * recommended to do so. One can use <DFN>vc_cec_device_type</DFN>
 * to get the default device type associated with the logical
 * address returned via VC_CEC_LOGICAL_ADDR callback.
 *
 * @param physical address (16-bit packed)
 *
 * @param device type to be broadcasted
 *
 * @param reply or not (normally not)
 *
 * @return zero if the command is successful, non-zero otherwise
 *         Tx callback if successful
 ***********************************************************/
VCHPRE_ int VCHPOST_ vc_cec_send_ReportPhysicalAddress(uint16_t physical_address,
                                                       CEC_DEVICE_TYPE_T device_type,
                                                       vcos_bool_t is_reply);

#endif //_VC_CECSERVICE_H_
