#ifndef _SSL_PARSE_MAX_FRAGMENT_LENGTH_EXT_H
#define _SSL_PARSE_MAX_FRAGMENT_LENGTH_EXT_H

static int ssl_parse_max_fragment_length_ext( mbedtls_ssl_context *ssl,
                                              const unsigned char *buf,
                                              size_t len )
{
    if( len != 1 || buf[0] >= MBEDTLS_SSL_MAX_FRAG_LEN_INVALID )
    {
        MBEDTLS_SSL_DEBUG_MSG( 1, ( "bad client hello message" ) );
        mbedtls_ssl_send_alert_message( ssl, MBEDTLS_SSL_ALERT_LEVEL_FATAL,
                                        MBEDTLS_SSL_ALERT_MSG_ILLEGAL_PARAMETER );
        return( MBEDTLS_ERR_SSL_BAD_HS_CLIENT_HELLO );
    }

    ssl->session_negotiate->mfl_code = buf[0];

    return( 0 );
}

#endif