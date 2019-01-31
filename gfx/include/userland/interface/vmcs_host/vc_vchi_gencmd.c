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
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <ctype.h>
#include <assert.h>

#include "vchost.h"

#include "interface/vcos/vcos.h"
#include "vcinclude/common.h"
#include "vc_vchi_gencmd.h"
#include "interface/vchi/vchi.h"
#include "interface/vchi/common/endian.h"
#include "interface/vmcs_host/vc_gencmd_defs.h"

#ifdef HAVE_GENCMD_VERSION
extern const char *gencmd_get_build_version(void);
#error
#endif

/******************************************************************************
Local types and defines.
******************************************************************************/
#define GENCMD_MAX_LENGTH 512
typedef struct {
   VCHI_SERVICE_HANDLE_T open_handle[VCHI_MAX_NUM_CONNECTIONS];
   uint32_t              msg_flag[VCHI_MAX_NUM_CONNECTIONS];
   char                  command_buffer[GENCMD_MAX_LENGTH+1];
   char                  response_buffer[GENCMDSERVICE_MSGFIFO_SIZE];
   uint32_t              response_length;  //Length of response minus the error code
   int                   num_connections;
   VCOS_MUTEX_T          lock;
   int                   initialised;
   VCOS_EVENT_T          message_available_event;
} GENCMD_SERVICE_T;

static GENCMD_SERVICE_T gencmd_client;


/******************************************************************************
Static function.
******************************************************************************/
static void gencmd_callback( void *callback_param,
                             VCHI_CALLBACK_REASON_T reason,
                             void *msg_handle );

static __inline int lock_obtain (void) {
   int ret = -1;
   if(gencmd_client.initialised && vcos_mutex_lock(&gencmd_client.lock) == VCOS_SUCCESS)
   {
      ret = 0;
   }

   return ret;
}
static __inline void lock_release (void) {
   vcos_mutex_unlock(&gencmd_client.lock);
}

int use_gencmd_service(void) {
   int ret = 0;
   int i=0;
   for(i = 0; i < gencmd_client.num_connections; i++) {
      ret = (ret == 0) ? vchi_service_use(gencmd_client.open_handle[i]) : ret;
   }
   return ret;
}

int release_gencmd_service(void) {
   int ret = 0;
   int i=0;
   for(i = 0; i < gencmd_client.num_connections; i++) {
      ret = (ret == 0) ? vchi_service_release(gencmd_client.open_handle[i]) : ret;
   }
   return ret;
}


//call vc_vchi_gencmd_init to initialise
int vc_gencmd_init() {
   assert(0);
   return 0;
}

/******************************************************************************
NAME
   vc_vchi_gencmd_init

SYNOPSIS
   void vc_vchi_gencmd_init(VCHI_INSTANCE_T initialise_instance, VCHI_CONNECTION_T **connections, uint32_t num_connections )

FUNCTION
   Initialise the general command service for use. A negative return value
   indicates failure (which may mean it has not been started on VideoCore).

RETURNS
   int
******************************************************************************/

void vc_vchi_gencmd_init (VCHI_INSTANCE_T initialise_instance, VCHI_CONNECTION_T **connections, uint32_t num_connections )
{
   VCOS_STATUS_T status;
   int32_t success;
   int i;

   if (gencmd_client.initialised)
     return;

   // record the number of connections
   memset( &gencmd_client, 0, sizeof(GENCMD_SERVICE_T) );
   gencmd_client.num_connections = (int) num_connections;

   status = vcos_mutex_create(&gencmd_client.lock, "HGencmd");
   vcos_assert(status == VCOS_SUCCESS);
   status = vcos_event_create(&gencmd_client.message_available_event, "HGencmd");
   vcos_assert(status == VCOS_SUCCESS);

   for (i=0; i<gencmd_client.num_connections; i++) {

      // Create a 'LONG' service on the each of the connections
      SERVICE_CREATION_T gencmd_parameters = { VCHI_VERSION(VC_GENCMD_VER),
                                               MAKE_FOURCC("GCMD"),      // 4cc service code
                                               connections[i],           // passed in fn ptrs
                                               0,                        // tx fifo size (unused)
                                               0,                        // tx fifo size (unused)
                                               &gencmd_callback,         // service callback
                                               &gencmd_client.message_available_event, // callback parameter
                                               VC_FALSE,                 // want_unaligned_bulk_rx
                                               VC_FALSE,                 // want_unaligned_bulk_tx
                                               VC_FALSE                  // want_crc
                                               };

      success = vchi_service_open( initialise_instance, &gencmd_parameters, &gencmd_client.open_handle[i] );
      assert( success == 0 );
   }

   gencmd_client.initialised = 1;
   release_gencmd_service();
}

/******************************************************************************
NAME
   gencmd_callback

SYNOPSIS
   void gencmd_callback( void *callback_param,
                     const VCHI_CALLBACK_REASON_T reason,
                     const void *msg_handle )

FUNCTION
   VCHI callback

RETURNS
   int
******************************************************************************/
static void gencmd_callback( void *callback_param,
                             const VCHI_CALLBACK_REASON_T reason,
                             void *msg_handle ) {

   VCOS_EVENT_T *event = (VCOS_EVENT_T *)callback_param;

	(void)msg_handle;
   if ( reason != VCHI_CALLBACK_MSG_AVAILABLE || !event)
      return;

   vcos_event_signal(event);
}

/******************************************************************************
NAME
   vc_gencmd_stop

SYNOPSIS
   int vc_gencmd_stop()

FUNCTION
   This tells us that the generak command service has stopped, thereby preventing
   any of the functions from doing anything.

RETURNS
   int
******************************************************************************/

void vc_gencmd_stop () {
   // Assume a "power down" gencmd has been sent and the lock is held. There will
   // be no response so this should be called instead.
   int32_t success,i;

   if (!gencmd_client.initialised)
      return;

   if(lock_obtain() == 0)
   {
      use_gencmd_service();

      for(i = 0; i< (int32_t)gencmd_client.num_connections; i++) {
         success = vchi_service_close( gencmd_client.open_handle[i]);
         assert(success == 0);
      }

      gencmd_client.initialised = 0;

      lock_release();
            
      vcos_mutex_delete(&gencmd_client.lock);
      vcos_event_delete(&gencmd_client.message_available_event);
   }
}

/******************************************************************************
NAME
   vc_gencmd_send

SYNOPSIS
   int vc_gencmd_send( const char *format, ... )

FUNCTION
   Send a string to general command service.

RETURNS
   int
******************************************************************************/
int vc_gencmd_send_list ( const char *format, va_list a )
{
   int success = -1;

   // Obtain the lock and keep it so no one else can butt in while we await the response.
   if(lock_obtain() == 0)
   {
      int length = vsnprintf( gencmd_client.command_buffer, GENCMD_MAX_LENGTH, format, a );
      
      if (length >= 0 && length < GENCMD_MAX_LENGTH)
      {
         int i;
         use_gencmd_service();
         for( i=0; i<gencmd_client.num_connections; i++ ) {
            success = vchi_msg_queue( gencmd_client.open_handle[i],
                                           gencmd_client.command_buffer,
                                           length+1,
                                           VCHI_FLAGS_BLOCK_UNTIL_QUEUED, NULL );

            if(success == 0)
            { // only want to send on one connection, so break on success
               break;
            }
         }
         release_gencmd_service();
      }

      lock_release();
   }

   return success;
}

int vc_gencmd_send ( const char *format, ... )
{
   va_list a;
   int     rv;

   va_start ( a, format );
   rv = vc_gencmd_send_list( format, a );
   va_end ( a );
   return rv;
}

/******************************************************************************
NAME
   vc_gencmd_read_response

SYNOPSIS
   int vc_gencmd_read_response

FUNCTION
   Block until something comes back

RETURNS
   Error code from dequeue message
******************************************************************************/
int vc_gencmd_read_response (char *response, int maxlen) {
   int i = 0;
   int success = -1;
   int ret_code = 0;
   int32_t sem_ok = 0;

   if(lock_obtain() == 0)
   {
      //Note this will ALWAYS reset response buffer and overwrite any partially read responses
      use_gencmd_service();
      do {
         //TODO : we need to deal with messages coming through on more than one connections properly
         //At the moment it will always try to read the first connection if there is something there
         for(i = 0; i < gencmd_client.num_connections; i++) {
            //Check if there is something in the queue, if so return immediately
            //otherwise wait for the event and read again
            success = (int) vchi_msg_dequeue( gencmd_client.open_handle[i], gencmd_client.response_buffer,
                                              sizeof(gencmd_client.response_buffer), &gencmd_client.response_length, VCHI_FLAGS_NONE);
            if(success == 0) {
               ret_code = VC_VTOH32( *(int *)gencmd_client.response_buffer );
               break;
            } else {
               gencmd_client.response_length = 0;
            }
         }
      } while(!gencmd_client.response_length && vcos_event_wait(&gencmd_client.message_available_event) == VCOS_SUCCESS);
      
      if(gencmd_client.response_length && sem_ok == 0) {
         gencmd_client.response_length -= sizeof(int); //first word is error code
         memcpy(response, gencmd_client.response_buffer+sizeof(int), (size_t) vcos_min((int)gencmd_client.response_length, (int)maxlen));
      }

      release_gencmd_service();
      lock_release();
   }

   // If we read anything, return the VideoCore code. Error codes < 0 mean we failed to
   // read anything...
   //How do we let the caller know the response code of gencmd?
   //return ret_code;

   return success;
}

/******************************************************************************
NAME
   vc_gencmd

SYNOPSIS
   int vc_gencmd(char *response, int maxlen, const char *format, ...)

FUNCTION
   Send a gencmd and receive the response as per vc_gencmd read_response.

RETURNS
   int
******************************************************************************/
int vc_gencmd(char *response, int maxlen, const char *format, ...) {
   va_list args;
   int ret = -1;

   use_gencmd_service();

   va_start(args, format);
   ret = vc_gencmd_send_list(format, args);
   va_end (args);

   if (ret >= 0) {
      ret = vc_gencmd_read_response(response, maxlen);
   }

   release_gencmd_service();

   return ret;
}

/******************************************************************************
NAME
   vc_gencmd_string_property

SYNOPSIS
   int vc_gencmd_string_property(char *text, char *property, char **value, int *length)

FUNCTION
   Given a text string, containing items of the form property=value,
   look for the named property and return the value. The start of the value
   is returned, along with its length. The value may contain spaces, in which
   case it is enclosed in double quotes. The double quotes are not included in
   the return parameters. Return non-zero if the property is found.

RETURNS
   int
******************************************************************************/

int vc_gencmd_string_property(char *text, const char *property, char **value, int *length) {
#define READING_PROPERTY 0
#define READING_VALUE 1
#define READING_VALUE_QUOTED 2
   int state = READING_PROPERTY;
   int delimiter = 1, match = 0, len = (int)strlen(property);
   char *prop_start=text, *value_start=text;
   for (; *text; text++) {
      int ch = *text;
      switch (state) {
      case READING_PROPERTY:
         if (delimiter) prop_start = text;
         if (isspace(ch)) delimiter = 1;
         else if (ch == '=') {
            delimiter = 1;
            match = (text-prop_start==len && strncmp(prop_start, property, (size_t)(text-prop_start))==0);
            state = READING_VALUE;
         }
         else delimiter = 0;
         break;
      case READING_VALUE:
         if (delimiter) value_start = text;
         if (isspace(ch)) {
            if (match) goto success;
            delimiter = 1;
            state = READING_PROPERTY;
         }
         else if (delimiter && ch == '"') {
            delimiter = 1;
            state = READING_VALUE_QUOTED;
         }
         else delimiter = 0;
         break;
      case READING_VALUE_QUOTED:
         if (delimiter) value_start = text;
         if (ch == '"') {
            if (match) goto success;
            delimiter = 1;
            state = READING_PROPERTY;
         }
         else delimiter = 0;
         break;
      }
   }
   if (match) goto success;
   return 0;
success:
   *value = value_start;
   *length = text - value_start;
   return 1;
}

/******************************************************************************
NAME
   vc_gencmd_number_property

SYNOPSIS
   int vc_gencmd_number_property(char *text, char *property, int *number)

FUNCTION
   Given a text string, containing items of the form property=value,
   look for the named property and return the numeric value. If such a numeric
   value is successfully found, return 1; otherwise return 0.

RETURNS
   int
******************************************************************************/

int vc_gencmd_number_property(char *text, const char *property, int *number) {
   char *value, temp;
   int length, retval;
   if (vc_gencmd_string_property(text, property, &value, &length) == 0)
      return 0;
   temp = value[length];
   value[length] = 0;
   /* coverity[secure_coding] - this is not insecure */
   retval = sscanf(value, "0x%x", (unsigned int*)number);
   if (retval != 1)
      /* coverity[secure_coding] - this is not insecure */
      retval = sscanf(value, "%d", number);
   value[length] = temp;
   return retval;

}

/******************************************************************************
NAME
   vc_gencmd_until

SYNOPSIS
   int vc_gencmd_until(const char *cmd, const char *error_string, int timeout);

FUNCTION
   Sends the command repeatedly, until one of the following situations occurs:
   The specified response string is found within the gencmd response.
   The specified error string is found within the gencmd response.
   The timeout is reached.

   The timeout is a rough value, do not use it for precise timing.

RETURNS
   0 if the requested response was detected.
   1 if the error string is detected or the timeout is reached.
******************************************************************************/
int vc_gencmd_until( char        *cmd,
                     const char  *property,
                     char        *value,
                     const char  *error_string,
                     int         timeout) {
   char response[128];
   int length;
   char *ret_value;
   int ret = 1;

   use_gencmd_service();
   for (;timeout > 0; timeout -= 10) {
      vc_gencmd(response, (int)sizeof(response), cmd);
      if (strstr(response,error_string)) {
         ret = 1;
         break;
      }
      else if (vc_gencmd_string_property(response, property, &ret_value, &length) &&
               strncmp(value,ret_value,(size_t)length)==0) {
         ret = 0;
         break;
      }
      vcos_sleep(10);
   }
   release_gencmd_service();

   return ret;
}

