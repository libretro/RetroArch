#pragma once
#include <wut.h>
#include <nn/result.h>

/**
 * \defgroup nn_ac_c Auto Connect C API
 * \ingroup nn_ac
 * C functions for the Auto Connect API
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif

/**
 * An ID number representing a network configuration. These are the same IDs as
 * shown in System Settings' Connection List.
 */
typedef uint32_t ACConfigId;
typedef uint32_t ACIpAddress;

/**
 * Initialise the Auto Connect library. Call this function before any other AC
 * functions.
 *
 * \return
 * A \link nn_result Result\endlink - see \link NNResult_IsSuccess \endlink
 * and \link NNResult_IsFailure \endlink.
 *
 * \sa
 * - \link ACFinalize \endlink
 */
NNResult
ACInitialize();

/**
 * Cleanup the Auto Connect library. Do not call any AC functions (other than
 * \link ACInitialize \endlink) after calling this function.
 *
 * \sa
 * - \link ACInitialize \endlink
 */
void
ACFinalize();

/**
 * Connects synchronically to a network, using the default configuration
 * May be blocking until the console successfully connects or has an timeout.
 *
 * \return
 * A \link nn_result Result\endlink - see \link NNResult_IsSuccess \endlink
 * and \link NNResult_IsFailure \endlink.
 */
NNResult
ACConnect();

/**
 * Connects asynchronically to a network, using the default configuration
 *
 * \return
 * A \link nn_result Result\endlink - see \link NNResult_IsSuccess \endlink
 * and \link NNResult_IsFailure \endlink.
 */
NNResult
ACConnectAsync();

/**
 * Closes connections made with ACConnect. Use GetCloseStatus to get the status.
 *
 * \return
 * A \link nn_result Result\endlink - see \link NNResult_IsSuccess \endlink
 * and \link NNResult_IsFailure \endlink.
 */
NNResult
ACClose();

NNResult
ACGetCloseStatus();

/**
 * Checks whether the console is currently connected to a network.
 *
 * \return
 * A \link nn_result Result\endlink - see \link NNResult_IsSuccess \endlink
 * and \link NNResult_IsFailure \endlink.
 */
NNResult
ACIsApplicationConnected(BOOL *connected);

/**
 * Gets the default connection configuration id. This is the default as marked
 * in System Settings.
 *
 * \param configId
 * A pointer to an \link ACConfigId \endlink to write the config ID to. Must not
 * be \c NULL.
 *
 * \return
 * A \link nn_result Result\endlink - see \link NNResult_IsSuccess \endlink
 * and \link NNResult_IsFailure \endlink.
 */
NNResult
ACGetStartupId(ACConfigId *configId);

/**
 * Connects to a network, using the configuration represented by the given
 * \link ACConfigId \endlink.
 *
 * \param configId
 * The \link ACConfigId \endlink representing the network to connect to.
 *
 * \return
 * A \link nn_result Result\endlink - see \link NNResult_IsSuccess \endlink
 * and \link NNResult_IsFailure \endlink.
 */
NNResult
ACConnectWithConfigId(ACConfigId configId);

/**
 * Gets the IP address assosciated with the currently active connection.
 *
 * \param ip
 * A pointer to write the IP address to, in
 * <a href="https://en.wikipedia.org/wiki/IPv4#Address_representations"
 * target="_blank">numerical</a> form.
 *
 * \return
 * A \link nn_result Result\endlink - see \link NNResult_IsSuccess \endlink
 * and \link NNResult_IsFailure \endlink.
 */
NNResult
ACGetAssignedAddress(ACIpAddress *ip);

NNResult
ACGetAssignedPreferedDns(ACIpAddress *ip);

NNResult
ACGetAssignedAlternativeDns(ACIpAddress *ip);

NNResult
ACGetAssignedGateway(ACIpAddress *ip);

NNResult
ACGetAssignedSubnet(ACIpAddress *ip);
#ifdef __cplusplus
}
#endif

/** @} */
