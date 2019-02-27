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

#ifndef VC_CONTAINERS_URI_H
#define VC_CONTAINERS_URI_H

/** \file containers_uri.h
 * API for parsing and building URI strings as described in RFC3986.
 */

#ifdef __cplusplus
extern "C" {
#endif

#include "containers/containers.h"

typedef struct VC_URI_PARTS_T VC_URI_PARTS_T;

/** Create an empty URI structure.
 *
 * \return The new URI structure. */
VC_URI_PARTS_T *vc_uri_create( void );

/** Destroy a URI structure.
 *
 * \param p_uri Pointer to a URI parts structure. */
void vc_uri_release( VC_URI_PARTS_T *p_uri );

/** Clear a URI structure.
 * Any URI component strings held are released, but the structure itself is not.
 *
 * \param p_uri Pointer to a URI parts structure. */
void vc_uri_clear( VC_URI_PARTS_T *p_uri );

/** Parses and unescapes a URI into the component parts.
 *
 * \param p_uri Pointer to a URI parts structure.
 * \param uri Pointer to a URI string to be parsed.
 * \return True if successful, false if not. */
bool vc_uri_parse( VC_URI_PARTS_T *p_uri, const char *uri );

/** Builds the URI component parts into a URI string.
 * If buffer is NULL, or buffer_size is too small, nothing is written to the
 * buffer but the required string length is still returned. buffer_size must be
 * at least one more than the value returned.
 *
 * \param p_uri Pointer to a URI parts structure.
 * \param buffer Pointer to where the URI string is to be built, or NULL.
 * \param buffer_size Number of bytes available in the buffer.
 * \return The length of the URI string. */
uint32_t vc_uri_build( const VC_URI_PARTS_T *p_uri, char *buffer, size_t buffer_size );

/** Retrieves the scheme of the URI.
 * The string is valid until either the scheme is changed or the URI is released.
 *
 * \param p_uri The parsed URI.
 * \return Pointer to the scheme string. */
const char *vc_uri_scheme( const VC_URI_PARTS_T *p_uri );

/** Retrieves the userinfo of the URI.
 * The string is valid until either the userinfo is changed or the URI is released.
 *
 * \param p_uri The parsed URI.
 * \return Pointer to the userinfo string. */
const char *vc_uri_userinfo( const VC_URI_PARTS_T *p_uri );

/** Retrieves the host of the URI.
 * The string is valid until either the host is changed or the URI is released.
 *
 * \param p_uri The parsed URI.
 * \return Pointer to the host string. */
const char *vc_uri_host( const VC_URI_PARTS_T *p_uri );

/** Retrieves the port of the URI.
 * The string is valid until either the port is changed or the URI is released.
 *
 * \param p_uri The parsed URI.
 * \return Pointer to the port string. */
const char *vc_uri_port( const VC_URI_PARTS_T *p_uri );

/** Retrieves the path of the URI.
 * The string is valid until either the path is changed or the URI is released.
 *
 * \param p_uri The parsed URI.
 * \return Pointer to the path string. */
const char *vc_uri_path( const VC_URI_PARTS_T *p_uri );

/** Retrieves the extension part of the path of the URI.
 * The string is valid until either the path is changed or the URI is released.
 *
 * \param p_uri The parsed URI.
 * \return Pointer to the extension string. */
const char *vc_uri_path_extension( const VC_URI_PARTS_T *p_uri );

/** Retrieves the fragment of the URI.
 * The string is valid until either the fragment is changed or the URI is released.
 *
 * \param p_uri The parsed URI.
 * \return Pointer to the fragment string. */
const char *vc_uri_fragment( const VC_URI_PARTS_T *p_uri );

/** Returns the number of query name/value pairs stored.
 *
 * \param p_uri The parsed URI.
 * \return Number of queries stored. */
uint32_t vc_uri_num_queries( const VC_URI_PARTS_T *p_uri );

/** Retrieves a given query's name and value
 * If either p_name or p_value are NULL, that part of the query is not returned,
 * otherwise it is set to the address of the string (which may itself be NULL).
 *
 * \param p_uri The parsed URI.
 * \param index Selects the query to get.
 * \param p_name Address of a string pointer to receive query name, or NULL.
 * \param p_value Address of a string pointer to receive query value, or NULL. */
void vc_uri_query( const VC_URI_PARTS_T *p_uri, uint32_t index, const char **p_name, const char **p_value );

/** Finds a specific query in the array.
 * If p_index is NULL, then it is assumed the search should start at index 0,
 * otherwise the search will start at the specified index and the index will
 * be updated on return to point to the query which has been found.
 * If p_value is NULL, that part of the query is not returned,
 * otherwise it is set to the address of the value string (which may itself be NULL).
 *
 * \param p_uri Pointer to a URI parts structure.
 * \param p_index Index from which to start the search. May be NULL.
 * \param name Unescaped query name.
 * \param value Unescaped query value. May be NULL.
 * \return True if successful, false if not. */
bool vc_uri_find_query( VC_URI_PARTS_T *p_uri, uint32_t *p_index, const char *name, const char **p_value );

/** Sets the scheme of the URI.
 * The string will be copied and stored in the URI, releasing and replacing
 * any existing string. If NULL is passed, any existing string shall simply be
 * released.
 *
 * \param p_uri The parsed URI.
 * \param scheme Pointer to the new scheme string, or NULL.
 * \return True if successful, false on memory allocation failure. */
bool vc_uri_set_scheme( VC_URI_PARTS_T *p_uri, const char *scheme );

/** Sets the userinfo of the URI.
 * The string will be copied and stored in the URI, releasing and replacing
 * any existing string. If NULL is passed, any existing string shall simply be
 * released.
 *
 * \param p_uri The parsed URI.
 * \param userinfo Pointer to the new userinfo string, or NULL.
 * \return True if successful, false on memory allocation failure. */
bool vc_uri_set_userinfo( VC_URI_PARTS_T *p_uri, const char *userinfo );

/** Sets the host of the URI.
 * The string will be copied and stored in the URI, releasing and replacing
 * any existing string. If NULL is passed, any existing string shall simply be
 * released.
 *
 * \param p_uri The parsed URI.
 * \param host Pointer to the new host string, or NULL.
 * \return True if successful, false on memory allocation failure. */
bool vc_uri_set_host( VC_URI_PARTS_T *p_uri, const char *host );

/** Sets the port of the URI.
 * The string will be copied and stored in the URI, releasing and replacing
 * any existing string. If NULL is passed, any existing string shall simply be
 * released.
 *
 * \param p_uri The parsed URI.
 * \param port Pointer to the new port string, or NULL.
 * \return True if successful, false on memory allocation failure. */
bool vc_uri_set_port( VC_URI_PARTS_T *p_uri, const char *port );

/** Sets the path of the URI.
 * The string will be copied and stored in the URI, releasing and replacing
 * any existing string. If NULL is passed, any existing string shall simply be
 * released.
 *
 * \param p_uri The parsed URI.
 * \param path Pointer to the new path string, or NULL.
 * \return True if successful, false on memory allocation failure. */
bool vc_uri_set_path( VC_URI_PARTS_T *p_uri, const char *path );

/** Sets the fragment of the URI.
 * The string will be copied and stored in the URI, releasing and replacing
 * any existing string. If NULL is passed, any existing string shall simply be
 * released.
 *
 * \param p_uri The parsed URI.
 * \param fragment Pointer to the new fragment string, or NULL.
 * \return True if successful, false on memory allocation failure. */
bool vc_uri_set_fragment( VC_URI_PARTS_T *p_uri, const char *fragment );

/** Adds an query to the array.
 * Note that the queries pointer may change after this function is called.
 * May fail due to memory allocation failure or invalid parameters.
 *
 * \param p_uri Pointer to a URI parts structure.
 * \param name Unescaped query name.
 * \param value Unescaped query value. May be NULL.
 * \return True if successful, false if not. */
bool vc_uri_add_query( VC_URI_PARTS_T *p_uri, const char *name, const char *value );

/** Merge a base URI and a relative URI.
 * In general, where the relative URI does not have a given element, the
 * corresponding element from the base URI is used. See RFC1808.
 * The combined URI is left in relative_uri. If the function is unsuccessful,
 * the relative_uri may have been partially modified.
 *
 * \param base_uri Pointer to the base URI parts structure.
 * \param relative_uri Pointer to the relative URI parts structure.
 * \return True if successful, false if not. */
bool vc_uri_merge( const VC_URI_PARTS_T *base_uri, VC_URI_PARTS_T *relative_uri );

#ifdef __cplusplus
}
#endif

#endif /* VC_CONTAINERS_URI_H */
