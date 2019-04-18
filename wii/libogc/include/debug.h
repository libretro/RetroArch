#ifndef __DEBUG_H__
#define __DEBUG_H__

#include <gctypes.h>

#define GDBSTUB_DEVICE_USB		0			/*!< device type: USBGecko */
#define GDBSTUB_DEVICE_TCP		1			/*!< device type: BBA-TCP */

#define GDBSTUB_DEF_CHANNEL		0			/*!< default EXI channel. channel can be 0 or 1. Note: Used for device type USBGecko  */
#define GDBSTUB_DEF_TCPPORT		2828		/*!< default TCP port. Note: Used for device type TCP */

#ifdef __cplusplus
	extern "C" {
#endif

extern const char *tcp_localip;
extern const char *tcp_netmask;
extern const char *tcp_gateway;

/*!\fn void _break()
 * \brief Stub function to insert the hardware break instruction. This function is used to enter the debug stub and to
 *        connect with the host. The developer is free to insert this function at any position in project's source code.
 *
 * \return none.
 */
void _break();

/*!\fn void DEBUG_Init(s32 device_type,s32 channel_port)
 * \brief Performs the initialization of the debug stub.
 * \param[in] device_type type of device to use. can be either USB or TCP.
 * \param[in] channel_port depending on the used device this can be either the EXI channel or the TCP port.
 *
 * \return none.
 */
void DEBUG_Init(s32 device_type,s32 channel_port);

#ifdef __cplusplus
	}
#endif

#endif
