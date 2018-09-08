#ifndef _SSL_PARSE_RENEGOTIATION_INFO_H
#define _SSL_PARSE_RENEGOTIATION_INFO_H

static int ssl_parse_renegotiation_info( mbedtls_ssl_context *ssl,
                                         const unsigned char *buf,
                                         size_t len )
{
#if defined(MBEDTLS_SSL_RENEGOTIATION)
    if( ssl->renego_status != MBEDTLS_SSL_INITIAL_HANDSHAKE )
    {
        /* Check verify-data in constant-time. The length OTOH is no secret */
        if( len    != 1 + ssl->verify_data_len ||
            buf[0] !=     ssl->verify_data_len ||
            mbedtls_ssl_safer_memcmp( buf + 1, ssl->peer_verify_data,
                          ssl->verify_data_len ) != 0 )
        {
            MBEDTLS_SSL_DEBUG_MSG( 1, ( "non-matching renegotiation info" ) );
            mbedtls_ssl_send_alert_message( ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                            MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE );
            return( MBEDTLS_ERR_SSL_BAD_HS_CLIENT_HELLO );
        }
    }
    else
#endif /* MBEDTLS_SSL_RENEGOTIATION */
    {
        if( len != 1 || buf[0] != 0x0 )
        {
            MBEDTLS_SSL_DEBUG_MSG( 1, ( "non-zero length renegotiation info" ) );
            mbedtls_ssl_send_alert_message( ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                            MBEDTLS_SSL_ALERT_MSG_HANDSHAKE_FAILURE );
            return( MBEDTLS_ERR_SSL_BAD_HS_CLIENT_HELLO );
        }

        ssl->secure_renegotiation = MBEDTLS_SSL_SECURE_RENEGOTIATION;
    }

    return( 0 );
}

#endif