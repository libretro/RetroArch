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
#include "interface/vchi/vchi.h"
#include "interface/vchi/common/endian.h"

#include "vc_vchi_filesys.h"
#include "interface/vmcs_host/vc_vchi_fileservice_defs.h"

/******************************************************************************
Global data.
******************************************************************************/

/******************************************************************************
Local types and defines.
******************************************************************************/
typedef enum {
   VC_SECTOR_IO_NONE,
   VC_SECTOR_IO_READING,
   VC_SECTOR_IO_WRITING
} VC_SECTOR_IO_T;

typedef struct {

   VCHI_SERVICE_HANDLE_T open_handle;

   int32_t               num_connections;

   //Host->2727
   FILESERV_MSG_T        fileserv_msg;

   //2727->Host   XXX
   FILESERV_MSG_T        vc_msg;

   VCOS_THREAD_T         filesys_thread;

   // used to signal response has arrived
   VCOS_EVENT_T          response_event;

   //we lock each vc_filesys function call
   VCOS_MUTEX_T          filesys_lock;

   //used to signal msg arrivals
   VCOS_EVENT_T          filesys_msg_avail;

   // Outstanding transaction's ID
   volatile uint32_t     cur_xid;

   // Copy of the header code from responses
   int32_t      resp_code;
   int32_t      err_no;

   char        *bulk_buffer;
   int32_t      initialised;

} FILESYS_SERVICE_T;

static FILESYS_SERVICE_T vc_filesys_client;


/******************************************************************************
Static functions.
******************************************************************************/

//Lock the host state
static __inline int32_t lock_obtain (void) {
   int ret = -1;
   if(vc_filesys_client.initialised && vcos_mutex_lock(&vc_filesys_client.filesys_lock) == VCOS_SUCCESS) {
      vchi_service_use(vc_filesys_client.open_handle);
      ret = 0;
   }
   return ret;
}

//Unlock the host state
static __inline void lock_release (void) {
   vcos_assert(vc_filesys_client.initialised);
   vchi_service_release(vc_filesys_client.open_handle);
   vcos_mutex_unlock(&vc_filesys_client.filesys_lock);
}

// File Service VCHI functions

static int vchi_msg_stub(FILESERV_MSG_T* msg, uint16_t cmd_id, int msg_len );

static int vchi_msg_stub_noblock(FILESERV_MSG_T* msg, uint16_t cmd_id, int msg_len);

static int vc_fs_message_handler( FILESERV_MSG_T* msg, uint32_t nbytes );

static void *filesys_task_func(void *arg);

static void filesys_callback( void *callback_param, VCHI_CALLBACK_REASON_T reason, void *msg_handle );



#ifdef PRINTF
#ifdef WIN32
#define printf tprintf
#endif
static void showmsg(VC_MSGFIFO_CMD_HEADER_T const * head,
                    struct file_service_msg_body const * body);
#endif
static int fs_host_direntbytestream_create(struct dirent *d, void *buffer);
static void fs_host_direntbytestream_interp(struct dirent *d, void *buffer);

/*---------------------------------------------------------------------------*/

/******************************************************************************
NAME
   vc_filesys_init

SYNOPSIS
   vc_filesys_init (VCHI_INSTANCE_T initialise_instance, VCHI_CONNECTION_T **connections, uint32_t num_connections ) {

FUNCTION
   Initialise the file system for use. A negative return value
   indicates failure (which may mean it has not been started on VideoCore).

RETURNS
   int
******************************************************************************/
int vc_vchi_filesys_init (VCHI_INSTANCE_T initialise_instance, VCHI_CONNECTION_T **connections, uint32_t num_connections )
{
   int32_t success = 0;
   SERVICE_CREATION_T filesys_parameters;
   VCOS_THREAD_ATTR_T attrs;
   VCOS_STATUS_T status;

   // record the number of connections
   memset( &vc_filesys_client, 0, sizeof(FILESYS_SERVICE_T) );
   vc_filesys_client.num_connections = num_connections;

   if(!vcos_verify(vc_filesys_client.num_connections < 2))
      return -1;

   status = vcos_mutex_create(&vc_filesys_client.filesys_lock, "HFilesys");
   vcos_assert(status == VCOS_SUCCESS);

   status = vcos_event_create(&vc_filesys_client.filesys_msg_avail, "HFilesys");
   vcos_assert(status == VCOS_SUCCESS);

   //create sema used to signal cmd response has arrived
   status = vcos_event_create(&vc_filesys_client.response_event, "HFilesys");
   vcos_assert(status == VCOS_SUCCESS);

   vc_filesys_client.bulk_buffer = vcos_malloc_aligned(FILESERV_MAX_BULK, 16, "HFilesys bulk_recv");
   vc_filesys_client.cur_xid = 0;

   memset(&filesys_parameters, 0, sizeof(filesys_parameters));
   filesys_parameters.service_id = FILESERV_4CC;   // 4cc service code
   filesys_parameters.connection = connections[0]; // passed in fn ptrs
   filesys_parameters.rx_fifo_size = 0;            // rx fifo size (unused)
   filesys_parameters.tx_fifo_size = 0;            // tx fifo size (unused)
   filesys_parameters.callback = &filesys_callback;
   filesys_parameters.callback_param = &vc_filesys_client.filesys_msg_avail;
   filesys_parameters.want_unaligned_bulk_rx = 0;
   filesys_parameters.want_unaligned_bulk_tx = 0;
   filesys_parameters.want_crc = 0;
   filesys_parameters.version.version = VC_FILESERV_VER;
   filesys_parameters.version.version_min = VC_FILESERV_VER;

   success = vchi_service_open( initialise_instance, &filesys_parameters, &vc_filesys_client.open_handle );
   vcos_assert( success == 0 );

   vcos_thread_attr_init(&attrs);
   vcos_thread_attr_setstacksize(&attrs, 4000);
   vcos_thread_attr_settimeslice(&attrs, 1);

   vc_filesys_client.initialised = 1;

   status = vcos_thread_create(&vc_filesys_client.filesys_thread, "HFilesys", &attrs, filesys_task_func, NULL);
   vcos_assert(status == VCOS_SUCCESS);

   /* Not using service immediately - release videocore */
   vchi_service_release(vc_filesys_client.open_handle);

   return (int)success;
}

static void *filesys_task_func(void *arg)
{
   int32_t success;
   uint32_t msg_len;

   (void)arg;

   vc_hostfs_init();

   while(1) {
      // wait for the semaphore to say that there is a message
      if (vcos_event_wait(&vc_filesys_client.filesys_msg_avail) != VCOS_SUCCESS || vc_filesys_client.initialised == 0)
         break;

      vchi_service_use(vc_filesys_client.open_handle);
      // read the message - should we really "peek" this
      while (1) {
         success = vchi_msg_dequeue(vc_filesys_client.open_handle, &vc_filesys_client.vc_msg,
                                    sizeof(vc_filesys_client.vc_msg), &msg_len, VCHI_FLAGS_NONE);
         if (!success)
            break;

         /* coverity[tainted_string_argument] */
         success = (int32_t) vc_fs_message_handler(&vc_filesys_client.vc_msg, msg_len);
         (void)success;
      }
      vchi_service_release(vc_filesys_client.open_handle);
   }

   return 0;
}


/******************************************************************************
NAME
   filesys_callback

SYNOPSIS
   void filesys_callback( void *callback_param,
                     const VCHI_CALLBACK_REASON_T reason,
                     const void *msg_handle )

FUNCTION
   VCHI callback

RETURNS
   int
******************************************************************************/
static void filesys_callback( void *callback_param,
                             const VCHI_CALLBACK_REASON_T reason,
                             void *msg_handle )
{
   (void)msg_handle;

   switch( reason ) {

   case VCHI_CALLBACK_MSG_AVAILABLE:
      {
         VCOS_EVENT_T *event = (VCOS_EVENT_T *) callback_param;
         if(event)
            vcos_event_signal(event);
      }
      break;

   case VCHI_CALLBACK_BULK_RECEIVED:
      break;
   case VCHI_CALLBACK_BULK_SENT:
      break;

   default:
      return;
   }
}

/******************************************************************************
NAME
   vc_filesys_stop

SYNOPSIS
   void vc_filesys_stop()

FUNCTION
   This tells us that the file system service has stopped, thereby preventing
   any of the functions from doing anything.

RETURNS
   void
******************************************************************************/

void vc_filesys_stop ()
{
   int32_t result;
   void *dummy;

   if(lock_obtain() != 0)
      return;

   result = vchi_service_close(vc_filesys_client.open_handle);
   vcos_assert(result == 0);

   vc_filesys_client.initialised = 0;

   vcos_event_signal(&vc_filesys_client.filesys_msg_avail);
   vcos_thread_join(&vc_filesys_client.filesys_thread, &dummy);

   vcos_event_delete(&vc_filesys_client.filesys_msg_avail);
   vcos_event_delete(&vc_filesys_client.response_event);
   vcos_mutex_delete(&vc_filesys_client.filesys_lock);

   if(vc_filesys_client.bulk_buffer)
      vcos_free(vc_filesys_client.bulk_buffer);
}

/******************************************************************************
NAME
   vc_filesys_single_param

SYNOPSIS
   int vc_filesys_single_param(uint32_t param, uint32_t fn)

FUNCTION
   Utility function for implementing filesys methods

RETURNS
   void
******************************************************************************/
static int vc_filesys_single_param(uint32_t param, uint32_t fn)
{
   int success = -1;

   if(lock_obtain() == 0)
   {
      vc_filesys_client.fileserv_msg.params[0] = param;
      if (vchi_msg_stub(&vc_filesys_client.fileserv_msg, fn, 4) == FILESERV_RESP_OK)
         success = 0;
      
      lock_release();
   }

   return success;
}

/******************************************************************************
NAME
   vc_filesys_single_string

SYNOPSIS
   int vc_filesys_single_string(uint32_t param, const char *str, uint32_t fn, int return_param)

FUNCTION
   Utility function for implementing filesys methods

RETURNS
   void
******************************************************************************/
static int vc_filesys_single_string(uint32_t param, const char *str, uint32_t fn, int return_param)
{
   int ret = -1;
   int len = strlen(str);

   if(len < FILESERV_MAX_DATA && lock_obtain() == 0)
   {
      vc_filesys_client.fileserv_msg.params[0] = param;
      /* coverity[buffer_size_warning] - the length of str has already been checked */
      strncpy((char*)vc_filesys_client.fileserv_msg.data, str, FILESERV_MAX_DATA);
    
      if (vchi_msg_stub(&vc_filesys_client.fileserv_msg, fn, len+1+16) == FILESERV_RESP_OK)
      {
         if(return_param)
            ret = (int) vc_filesys_client.fileserv_msg.params[0];
         else
            ret = 0;
      }

      lock_release();
   }

   return ret;
}

/* Standard UNIX low-level library functions (declared in unistd.h) */
/******************************************************************************
NAME
   vc_filesys_close

SYNOPSIS
   int vc_filesys_close(int fildes)

FUNCTION
   Deallocates the file descriptor to a file.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_filesys_close(int fildes)
{
   return vc_filesys_single_param((uint32_t) fildes, VC_FILESYS_CLOSE);
}


/******************************************************************************
NAME
   vc_filesys_lseek

SYNOPSIS
   long vc_filesys_lseek(int fildes, long offset, int whence)

FUNCTION
   Sets the file pointer associated with the open file specified by fildes.

RETURNS
   Successful completion: offset
   Otherwise: -1
******************************************************************************/

long vc_filesys_lseek(int fildes, long offset, int whence)
{
   long set = -1;

   if(lock_obtain() == 0)
   {
      vc_filesys_client.fileserv_msg.params[0] = (uint32_t) fildes;
      vc_filesys_client.fileserv_msg.params[1] = (uint32_t) offset;
      vc_filesys_client.fileserv_msg.params[2] = (uint32_t) whence;
      
      if (vchi_msg_stub(&vc_filesys_client.fileserv_msg, VC_FILESYS_LSEEK, 12) == FILESERV_RESP_OK)
         set = (long) vc_filesys_client.fileserv_msg.params[0];
      
      lock_release();
   }

   return set;
}

/******************************************************************************
NAME
   vc_filesys_lseek64

SYNOPSIS
   int64_t vc_filesys_lseek64(int fildes, int64_t offset, int whence)

FUNCTION
   Sets the file pointer associated with the open file specified by fildes.

RETURNS
   Successful completion: file pointer value
   Otherwise: -1
******************************************************************************/

int64_t vc_filesys_lseek64(int fildes, int64_t offset, int whence)
{
   int64_t set = -1;

   if(lock_obtain() == 0)
   {
      vc_filesys_client.fileserv_msg.params[0] = (uint32_t) fildes;
      vc_filesys_client.fileserv_msg.params[1] = (uint32_t) offset;        // LSB
      vc_filesys_client.fileserv_msg.params[2] = (uint32_t)(offset >> 32); // MSB
      vc_filesys_client.fileserv_msg.params[3] = (uint32_t) whence;
      
      if (vchi_msg_stub(&vc_filesys_client.fileserv_msg, VC_FILESYS_LSEEK64, 16) == FILESERV_RESP_OK)
      {
         set = vc_filesys_client.fileserv_msg.params[0];
         set += (int64_t)vc_filesys_client.fileserv_msg.params[1] << 32;
      }
      
      lock_release();
   }

   return set;
}

/******************************************************************************
NAME
   vc_filesys_mount

SYNOPSIS
   int vc_filesys_mount(const char *device, const char *mountpoint, const char *options)

FUNCTION
   Mounts a filesystem at a given location

RETURNS
   Successful completion: 0
******************************************************************************/

int vc_filesys_mount(const char *device, const char *mountpoint, const char *options)
{
   int set = -1, len;
   int a = strlen(device);
   int b = strlen(mountpoint);
   int c = strlen(options);

   if(a + b + c + 3 < FILESERV_MAX_DATA && lock_obtain() == 0)
   {
      char *str = (char *) vc_filesys_client.fileserv_msg.data;

      memcpy(str, device, a);
      str[a] = 0;
      memcpy(str+a+1, mountpoint, b);
      str[a+1+b] = 0;
      memcpy(str+a+b+2, options, c);
      str[a+b+c+2] = 0;
      len = a + b + c + 3 + (int)(((FILESERV_MSG_T *)0)->data);
      len = ((len + (VCHI_BULK_GRANULARITY-1)) & ~(VCHI_BULK_GRANULARITY-1)) + VCHI_BULK_GRANULARITY;
      
      if (vchi_msg_stub(&vc_filesys_client.fileserv_msg, VC_FILESYS_MOUNT, len) == FILESERV_RESP_OK)
         set = (int) vc_filesys_client.fileserv_msg.params[0];

      lock_release();
   }

   return set;
}

/******************************************************************************
NAME
   vc_filesys_umount

SYNOPSIS
   int vc_filesys_mount(const char *mountpoint)

FUNCTION
   Un-mounts a removable device from the location that it has been mounted
   to earlier in the session

RETURNS
   Successful completion: 0
******************************************************************************/

int vc_filesys_umount(const char *mountpoint)
{
   return vc_filesys_single_string(0, mountpoint, VC_FILESYS_UMOUNT, 1);
}

/******************************************************************************
NAME
   vc_filesys_open

SYNOPSIS
   int vc_filesys_open(const char *path, int vc_oflag)

FUNCTION
   Establishes a connection between a file and a file descriptor.

RETURNS
   Successful completion: file descriptor
   Otherwise: -1
******************************************************************************/

int vc_filesys_open(const char *path, int vc_oflag)
{
   return vc_filesys_single_string((uint32_t) vc_oflag, path, VC_FILESYS_OPEN, 1);
}


/******************************************************************************
NAME
   vc_filesys_read

SYNOPSIS
   int vc_filesys_read(int fildes, void *buf, unsigned int nbyte)

FUNCTION
   Attempts to read nbyte bytes from the file associated with the file
   descriptor, fildes, into the buffer pointed to by buf.

RETURNS
   Successful completion: number of bytes read
   Otherwise: -1
******************************************************************************/

/******************************************************************************

FUNCTION


RETURNS
   Successful completion: the number of bytes received
   Otherwise  negative error code
******************************************************************************/
static int vc_vchi_msg_bulk_read(FILESERV_MSG_T* msg, uint16_t cmd_id, uint32_t transfer_len, uint8_t* recv_addr )
{
   uint32_t i;
   int msg_len;
   uint32_t host_align_bytes;
   uint32_t num_bytes_read;
   int32_t success;

   //this is current file_io_buffer size so assuming never more than this
   //otherwise we will split the read into chunks
   if(!vcos_verify(transfer_len <= FILESERV_MAX_BULK))
      return -1;

   //number of bytes required to align recv_addr
   host_align_bytes = VCHI_BULK_ALIGN_NBYTES(recv_addr);

   i = vc_filesys_client.cur_xid + 1;
   i &= 0x7fffffffUL;
   vc_filesys_client.cur_xid = i;

   msg->xid = vc_filesys_client.cur_xid;

   //fill in cmd id: VC_FILESYS_READ etc
   msg->cmd_code = cmd_id;

   msg->params[2] = transfer_len;

   msg->params[3] = host_align_bytes;

   //24 comes from the static size of FILESERV_MSG_T
   msg_len = 24;

   if(vchi_msg_queue( vc_filesys_client.open_handle, msg, (uint32_t)msg_len, VCHI_FLAGS_BLOCK_UNTIL_QUEUED, NULL ) != 0)
      return -1;

   //wait to receive response
   if(vcos_event_wait(&vc_filesys_client.response_event) != VCOS_SUCCESS || msg->cmd_code == FILESERV_RESP_ERROR)
      return -1;

   //total bytes read
   num_bytes_read = msg->params[0];

   if(!vcos_verify(num_bytes_read <= FILESERV_MAX_BULK))
      return -1;

   //everything is in msg->data
   if(msg->cmd_code == FILESERV_RESP_OK) {
      if(!vcos_verify(num_bytes_read <= FILESERV_MAX_DATA))
         return -1;

      memcpy(recv_addr, msg->data, num_bytes_read);
      return (int) num_bytes_read;
   }

// Make this code conditional to stop Coverity complaining about dead code
#if VCHI_BULK_ALIGN > 1
   //copy host_align_bytes bytes to recv_addr, now ready for bulk
   if(host_align_bytes) {
      memcpy(recv_addr, msg->data, host_align_bytes);
      recv_addr += host_align_bytes;
      transfer_len -= host_align_bytes;
   }
#endif

   //receive bulk from host
   if(msg->cmd_code == FILESERV_BULK_WRITE){
      //number of end bytes
      uint32_t end_bytes = msg->params[1];
      //calculate what portion of read transfer by bulk
      uint32_t bulk_bytes = (num_bytes_read-host_align_bytes-end_bytes);

      success = vchi_bulk_queue_receive(vc_filesys_client.open_handle,
                              recv_addr,
                              bulk_bytes,
                              VCHI_FLAGS_BLOCK_UNTIL_OP_COMPLETE,
                              NULL );
      if(!vcos_verify(success == 0))
         return -1;

      recv_addr+=bulk_bytes;

      //copy any left over end bytes from original msg
      if(end_bytes)
         memcpy(recv_addr, &msg->data[host_align_bytes], end_bytes);
   }
   else {
      return -1;
   }

   return (int) num_bytes_read;
}


int vc_filesys_read(int fildes, void *buf, unsigned int nbyte)
{
   int bulk_bytes = 0;
   int actual_read = 0;
   int total_byte = 0;
   uint8_t* ptr = (uint8_t*)buf;

   if (nbyte == 0) {
      return 0;
   }

   if(lock_obtain() == 0)
   {
      do {
         bulk_bytes = nbyte > FILESERV_MAX_BULK ? FILESERV_MAX_BULK : nbyte;
         
         //we overwrite the response here so fill in data again
         vc_filesys_client.fileserv_msg.params[0] = (uint32_t)fildes;
         vc_filesys_client.fileserv_msg.params[1] = 0xffffffffU;        // offset: use -1 to indicate no offset
         
         actual_read = vc_vchi_msg_bulk_read(&vc_filesys_client.fileserv_msg , VC_FILESYS_READ, (uint32_t)bulk_bytes, (uint8_t*)ptr);
         
         if(bulk_bytes != actual_read) {
            if(actual_read < 0)
               total_byte = -1;
            else
               total_byte += actual_read;
            //exit loop
            break;
         }

         ptr+=bulk_bytes;
         nbyte -= actual_read;
         total_byte += actual_read;
      }while(nbyte > 0);

      lock_release();
   }

   return total_byte;
}

/******************************************************************************
NAME
   vc_filesys_write

SYNOPSIS
   int vc_filesys_write(int fildes, const void *buf, unsigned int nbyte)

FUNCTION
   Attempts to write nbyte bytes from the buffer pointed to by buf to file
   associated with the file descriptor, fildes.

RETURNS
   Successful completion: number of bytes written
   Otherwise: -1
******************************************************************************/

/******************************************************************************

FUNCTION


RETURNS
   Successful completion: the number of bytes received
   Otherwise  negative error code
******************************************************************************/
static int vc_vchi_msg_bulk_write(FILESERV_MSG_T* msg, uint16_t cmd_id, uint32_t transfer_len, uint8_t* send_addr )
{
   uint32_t i;
   int msg_len;
   uint32_t align_bytes = 0;
   uint32_t bulk_end_bytes = 0;
   uint32_t bulk_bytes = 0;
   int num_bytes_written = -1;

   //this is current file_io_buffer size so assuming never more than this
   //otherwise we will split the read into chunks
   if(!vcos_verify(transfer_len <= FILESERV_MAX_BULK))
      return -1;

   i = vc_filesys_client.cur_xid + 1;
   i &= 0x7fffffffUL;
   vc_filesys_client.cur_xid = i;

   msg->xid = vc_filesys_client.cur_xid;

   //fill in cmd id VC_FILESYS_OPEN etc
   msg->cmd_code = cmd_id;

   msg->params[2] = transfer_len;

   //24 comes from the static size of FILESERV_MSG_T
   msg_len = 24;

   //put it all in one msg
   if(transfer_len <= FILESERV_MAX_DATA) {
      memcpy(msg->data, send_addr, transfer_len);
      msg->params[3] = 0;
      msg_len += transfer_len;
      //send request to write to host
      if(vchi_msg_queue( vc_filesys_client.open_handle, msg, (uint32_t)msg_len, VCHI_FLAGS_BLOCK_UNTIL_QUEUED, NULL ) != 0)
         return -1;

      // wait to receive response
      if(vcos_event_wait(&vc_filesys_client.response_event) == VCOS_SUCCESS &&
         msg->cmd_code != FILESERV_RESP_ERROR && msg->params[0] == transfer_len)
      {
         //vc_fs_message_handler copies resp into outgoing msg struct
         num_bytes_written = (int)msg->params[0];
      }
   }
   else {
      //required to make send_addr for bulk
      align_bytes = VCHI_BULK_ALIGN_NBYTES(send_addr);

// Make this code conditional to stop Coverity complaining about dead code
#if VCHI_BULK_ALIGN > 1
      //copy vc_align bytes to msg->data, send_addr ready for bulk
      if(align_bytes) {
         msg->params[3] = align_bytes;
         memcpy(msg->data, send_addr, align_bytes);
         transfer_len -= align_bytes;
         send_addr += align_bytes;
         msg_len += align_bytes;
      }
      else
#endif
         msg->params[3] = 0;

      // need to ensure we have the appropriate alignment
      bulk_bytes = (transfer_len)&(~(VCHI_BULK_GRANULARITY-1));

      bulk_end_bytes = transfer_len-bulk_bytes;

      if(bulk_end_bytes) {
         memcpy(msg->data+align_bytes, send_addr+bulk_bytes, bulk_end_bytes);
         msg_len += bulk_end_bytes;
      }

      //send request to write to host
      if(vchi_msg_queue( vc_filesys_client.open_handle, msg, (uint32_t)msg_len, VCHI_FLAGS_BLOCK_UNTIL_QUEUED, NULL ) != 0)
         return -1;

      //send bulk VCHI_FLAGS_BLOCK_UNTIL_QUEUED is ok because we wait for response msg with actual length written
      if(vchi_bulk_queue_transmit( vc_filesys_client.open_handle,
                                   send_addr,
                                   bulk_bytes,
                                   VCHI_FLAGS_BLOCK_UNTIL_QUEUED,
                                   NULL ) != 0)
         return -1;

      // wait to receive response sent by filsys_task_func
      if(vcos_event_wait(&vc_filesys_client.response_event) == VCOS_SUCCESS && msg->cmd_code == FILESERV_BULK_READ)
         num_bytes_written = (int)msg->params[0];
   }

   return num_bytes_written;
}


int vc_filesys_write(int fildes, const void *buf, unsigned int nbyte)
{
   int num_wrt = 0;
   int bulk_bytes = 0;
   int actual_written = 0;
   uint8_t *ptr = (uint8_t*) buf;

   if (nbyte == 0) {
      return 0;
   }

   if(lock_obtain() == 0)
   {
      //will split the read into 4K chunks based on vc fwrite buffer size array size
      //we will make this more dynamic later
      do {
         bulk_bytes = nbyte > FILESERV_MAX_BULK ? FILESERV_MAX_BULK : nbyte;
         
         //we overwrite the response here so fill in data again
         vc_filesys_client.fileserv_msg.params[0] = (uint32_t)fildes;
         vc_filesys_client.fileserv_msg.params[1] = 0xffffffffU;        // offset: use -1 to indicate no offset
         
         actual_written = vc_vchi_msg_bulk_write(&vc_filesys_client.fileserv_msg , VC_FILESYS_WRITE, (uint32_t)bulk_bytes, (uint8_t*)ptr);
         
         if(bulk_bytes != actual_written) {
            if(actual_written < 0)
               num_wrt = -1;
            else
               num_wrt += actual_written;
            break;
         }
         
         ptr+=bulk_bytes;
         nbyte -= actual_written;
         num_wrt += actual_written;
      }while(nbyte > 0);

      lock_release();
   }

   return num_wrt;
}


/* Directory management functions */

/******************************************************************************
NAME
   vc_filesys_closedir

SYNOPSIS
   int vc_filesys_closedir(void *dhandle)

FUNCTION
   Ends a directory list iteration.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_filesys_closedir(void *dhandle)
{
   return vc_filesys_single_param((uint32_t)dhandle, VC_FILESYS_CLOSEDIR);
}


/******************************************************************************
NAME
   vc_filesys_format

SYNOPSIS
   int vc_filesys_format(const char *path)

FUNCTION
   Formats the physical file system that contains path.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_filesys_format(const char *path)
{
   return vc_filesys_single_string(0, path, VC_FILESYS_FORMAT, 0);
}


/******************************************************************************
NAME
   vc_filesys_freespace

SYNOPSIS
   int vc_filesys_freespace(const char *path)

FUNCTION
   Returns the amount of free space on the physical file system that contains
   path.

RETURNS
   Successful completion: free space
   Otherwise: -1
******************************************************************************/

int vc_filesys_freespace(const char *path)
{
   return vc_filesys_single_string(0, path, VC_FILESYS_FREESPACE, 1);
}


/******************************************************************************
NAME
   vc_filesys_freespace64

SYNOPSIS
   int64_t vc_filesys_freespace64(const char *path)

FUNCTION
   Returns the amount of free space on the physical file system that contains
   path.

RETURNS
   Successful completion: free space
   Otherwise: -1
******************************************************************************/

int64_t vc_filesys_freespace64(const char *path)
{
   int64_t freespace = -1LL;

   if(lock_obtain() == 0)
   {
      strncpy((char *)vc_filesys_client.fileserv_msg.data, path, FS_MAX_PATH);
      
      if (vchi_msg_stub(&vc_filesys_client.fileserv_msg, VC_FILESYS_FREESPACE64,
                        (int)(16+strlen((char *)vc_filesys_client.fileserv_msg.data)+1)) == FILESERV_RESP_OK)
      {
         freespace = vc_filesys_client.fileserv_msg.params[0];
         freespace += (int64_t)vc_filesys_client.fileserv_msg.params[1] << 32;
      }

      lock_release();
   }

   return freespace;
}


/******************************************************************************
NAME
   vc_filesys_get_attr

SYNOPSIS
   int vc_filesys_get_attr(const char *path, fattributes_t *attr)

FUNCTION
   Gets the file/directory attributes.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_filesys_get_attr(const char *path, fattributes_t *attr)
{
   int success = -1;

   if(lock_obtain() == 0)
   {
      strncpy((char *)vc_filesys_client.fileserv_msg.data, path, FS_MAX_PATH);
      
      if (vchi_msg_stub(&vc_filesys_client.fileserv_msg, VC_FILESYS_GET_ATTR,
                        (int)(16+strlen((char *)vc_filesys_client.fileserv_msg.data)+1)) == FILESERV_RESP_OK)
      {
         success = 0;
         *attr = (fattributes_t) vc_filesys_client.fileserv_msg.params[0];
      }

      lock_release();
   }

   return success;
}


/******************************************************************************
NAME
   vc_filesys_fstat

SYNOPSIS
   int64_t vc_filesys_fstat(int fildes, FSTAT_T *buf)

FUNCTION
   Returns the file stat info structure for the specified file.
   This structure includes date and size info.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_filesys_fstat(int fildes, FSTAT_T *buf)
{
   int success = -1;

   if (buf != NULL && lock_obtain() == 0)
   {
      vc_filesys_client.fileserv_msg.params[0] = (uint32_t) fildes;
      if ( vchi_msg_stub(&vc_filesys_client.fileserv_msg, VC_FILESYS_FSTAT, 4) == FILESERV_RESP_OK )
      {
         buf->st_size = (int64_t)vc_filesys_client.fileserv_msg.params[0];          // LSB
         buf->st_size |= (int64_t)vc_filesys_client.fileserv_msg.params[1] << 32;    // MSB
         buf->st_modtime = (uint32_t)vc_filesys_client.fileserv_msg.params[2];
         // there's room for expansion here to pass across more elements of the structure if required...
         success = 0;
      }

      lock_release();
   }

   return success;
}


/******************************************************************************
NAME
   vc_filesys_mkdir

SYNOPSIS
   int vc_filesys_mkdir(const char *path)

FUNCTION
   Creates a new directory named by the pathname pointed to by path.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_filesys_mkdir(const char *path)
{
   return vc_filesys_single_string(0, path, VC_FILESYS_MKDIR, 0);
}


/******************************************************************************
NAME
   vc_filesys_opendir

SYNOPSIS
   void *vc_filesys_opendir(const char *dirname)

FUNCTION
   Starts a directory list iteration.

RETURNS
   Successful completion: dhandle (pointer)
   Otherwise: NULL
******************************************************************************/

void *vc_filesys_opendir(const char *dirname)
{
   return (void *) vc_filesys_single_string(0, dirname, VC_FILESYS_OPENDIR, 1);
}


/******************************************************************************
NAME
   vc_filesys_dirsize

SYNOPSIS
   int vc_filesys_dirsize(const char *path)

FUNCTION
   Look through the specified directory tree and sum the file sizes.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int64_t vc_filesys_dirsize(const char *path, uint32_t *num_files, uint32_t *num_dirs)
{
   int64_t ret = -1;

   if(lock_obtain() == 0)
   {
      strncpy((char *)vc_filesys_client.fileserv_msg.data, path, FS_MAX_PATH);
      
      // FIXME: Should probably use a non-blocking call here since it may take a
      // long time to do the operation...
      if ( vchi_msg_stub(&vc_filesys_client.fileserv_msg,
                         VC_FILESYS_DIRSIZE,
                         (int)(16+strlen((char *)vc_filesys_client.fileserv_msg.data)+1)) == FILESERV_RESP_OK )
      {
         ret = vc_filesys_client.fileserv_msg.params[0]; 
         ret += (int64_t)vc_filesys_client.fileserv_msg.params[1] << 32;
         if (num_files)
            *num_files = vc_filesys_client.fileserv_msg.params[2];
         if (num_dirs)
            *num_dirs = vc_filesys_client.fileserv_msg.params[3];
      }

      lock_release();
   }

   return ret;
}


/******************************************************************************
NAME
   vc_filesys_readdir_r

SYNOPSIS
   struct dirent *vc_filesys_readdir_r(void *dhandle, struct dirent *result)

FUNCTION
   Fills in the passed result structure with details of the directory entry
   at the current psition in the directory stream specified by the argument
   dhandle, and positions the directory stream at the next entry.

RETURNS
   Successful completion: result
   End of directory stream: NULL
******************************************************************************/

struct dirent *vc_filesys_readdir_r(void *dhandle, struct dirent *result)
{
   struct dirent *ret = NULL;

   if(lock_obtain() == 0)
   {
      vc_filesys_client.fileserv_msg.params[0] = (uint32_t)dhandle;
      if (vchi_msg_stub(&vc_filesys_client.fileserv_msg, VC_FILESYS_READDIR, 4) == FILESERV_RESP_OK)
      {
         fs_host_direntbytestream_interp(result, (void *)vc_filesys_client.fileserv_msg.data);
         ret = result;
      }

      lock_release();
   }

   return ret;
}


/******************************************************************************
NAME
   vc_filesys_remove

SYNOPSIS
   int vc_filesys_remove(const char *path)

FUNCTION
   Removes a file or a directory. A directory must be empty before it can be
   deleted.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_filesys_remove(const char *path)
{
   return vc_filesys_single_string(0, path, VC_FILESYS_REMOVE, 0);
}


/******************************************************************************
NAME
   vc_filesys_rename

SYNOPSIS
   int vc_filesys_rename(const char *oldfile, const char *newfile)

FUNCTION
   Changes the name of a file. The old and new pathnames must be on the same
   physical file system.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_filesys_rename(const char *oldfile, const char *newfile)
{
   int a, b, success = -1;

   // Ensure the pathnames aren't too long
   if ((a = strlen(oldfile)) < FS_MAX_PATH && (b = strlen(newfile)) < FS_MAX_PATH && lock_obtain() == 0)
   {
      strncpy((char *)vc_filesys_client.fileserv_msg.data, oldfile, FS_MAX_PATH);
      strncpy((char *)&vc_filesys_client.fileserv_msg.data[a+1], newfile, FS_MAX_PATH);
      
      if (vchi_msg_stub(&vc_filesys_client.fileserv_msg, VC_FILESYS_RENAME, 16+a+1+b+1) == FILESERV_RESP_OK)
         success = 0;

      lock_release();
   }

   return success;
}


/******************************************************************************
NAME
   vc_filesys_reset

SYNOPSIS
   int vc_filesys_reset()

FUNCTION
   Send a vc_FILESYS_RESET command. This will return immediately.

RETURNS
   Successful completion: FILESERV_RESP_OK
   Otherwise: -
******************************************************************************/

int vc_filesys_reset()
{
   return vc_filesys_single_param(0, VC_FILESYS_RESET);
}


/******************************************************************************
NAME
   vc_filesys_set_attr

SYNOPSIS
   int vc_filesys_set_attr(const char *path, fattributes_t attr)

FUNCTION
   Sets file/directory attributes.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_filesys_set_attr(const char *path, fattributes_t attr)
{
   return vc_filesys_single_string((uint32_t) attr, path, VC_FILESYS_SET_ATTR, 0);
}


/******************************************************************************
NAME
   vc_filesys_setend

SYNOPSIS
   int vc_filesys_setend(int fildes)

FUNCTION
   Truncates file at current position.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_filesys_setend(int fildes)
{
   return vc_filesys_single_param((uint32_t) fildes, VC_FILESYS_SETEND);
}



/******************************************************************************
NAME
   vc_filesys_scandisk

SYNOPSIS
   void vc_filesys_scandisk(const char *path)

FUNCTION
   Truncates file at current position.

RETURNS
   -
******************************************************************************/

void vc_filesys_scandisk(const char *path)
{
   vc_filesys_single_string(0, path, VC_FILESYS_SCANDISK, 0);
   return;
}

/******************************************************************************
NAME
   vc_filesys_chkdsk

SYNOPSIS
   int vc_filesys_chkdsk(const char *path, int fix_errors)

FUNCTION
   Truncates file at current position.

RETURNS
   -
******************************************************************************/

int vc_filesys_chkdsk(const char *path, int fix_errors)
{
   return vc_filesys_single_string(fix_errors, path, VC_FILESYS_CHKDSK, 1);
}

/******************************************************************************
NAME
   vc_filesys_size

SYNOPSIS
   int vc_filesys_size(const char *path)

FUNCTION
   return size of file

RETURNS
   -
******************************************************************************/

int vc_filesys_size(const char *path)
{
   int fd;
   int end_pos = 0;
   int success = -1;
   if((fd = vc_filesys_open(path, VC_O_RDONLY)) == 0)
   {
      end_pos = vc_filesys_lseek(fd, 0, SEEK_END);
      success = vc_filesys_close(fd);
      (void)success;
   }

   return end_pos;
}
/******************************************************************************
NAME
   vc_filesys_totalspace

SYNOPSIS
   int vc_filesys_totalspace(const char *path)

FUNCTION
   Returns the total amount of space on the physical file system that contains
   path.

RETURNS
   Successful completion: total space
   Otherwise: -1
******************************************************************************/

int vc_filesys_totalspace(const char *path)
{
   return vc_filesys_single_string(0, path, VC_FILESYS_TOTALSPACE, 1);
}


/******************************************************************************
NAME
   vc_filesys_totalspace64

SYNOPSIS
   int64_t vc_filesys_totalspace64(const char *path)

FUNCTION
   Returns the total amount of space on the physical file system that contains
   path.

RETURNS
   Successful completion: total space
   Otherwise: -1
******************************************************************************/

int64_t vc_filesys_totalspace64(const char *path)
{
   int64_t totalspace = -1LL;

   if(lock_obtain() == 0)
   {
      strncpy((char *)vc_filesys_client.fileserv_msg.data, path, FS_MAX_PATH);
      
      if (vchi_msg_stub(&vc_filesys_client.fileserv_msg, VC_FILESYS_TOTALSPACE64,
                        (int)(16+strlen((char *)vc_filesys_client.fileserv_msg.data)+1)) == FILESERV_RESP_OK)
      {
         totalspace = vc_filesys_client.fileserv_msg.params[0];
         totalspace += (int64_t)vc_filesys_client.fileserv_msg.params[1] << 32;
      }

      lock_release();
   }

   return totalspace;
}


/******************************************************************************
NAME
   vc_filesys_diskwritable

SYNOPSIS
   int vc_filesys_diskwritable(const char *path)

FUNCTION
   Return whether the named disk is writable.

RETURNS
   Successful completion: 1 (disk writable) or 0 (disk not writable)
   Otherwise: -1
******************************************************************************/

int vc_filesys_diskwritable(const char *path)
{
   return vc_filesys_single_string(0, path, VC_FILESYS_DISKWRITABLE, 1);
}

/******************************************************************************
NAME
   vc_filesys_fstype

SYNOPSIS
   int vc_filesys_fstype(const char *path)

FUNCTION
   Return the filesystem type of the named disk.

RETURNS
   Successful completion: disk type (see vc_fileservice_defs.h)
   Otherwise: -1
******************************************************************************/

int vc_filesys_fstype(const char *path)
{
   return vc_filesys_single_string(0, path, VC_FILESYS_FSTYPE, 1);
}

/******************************************************************************
NAME
   vc_filesys_open_disk_raw

SYNOPSIS
   int vc_filesys_open_disk_raw(const char *path)

FUNCTION
   Open disk for access in raw mode.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_filesys_open_disk_raw(const char *path)
{
   return vc_filesys_single_string(0, path, VC_FILESYS_OPEN_DISK_RAW, 1);
}

/******************************************************************************
NAME
   vc_filesys_close_disk_raw

SYNOPSIS
   int vc_filesys_close_disk_raw(const char *path)

FUNCTION
   Close disk from access in raw mode.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_filesys_close_disk_raw(const char *path)
{
   return vc_filesys_single_string(0, path, VC_FILESYS_CLOSE_DISK_RAW, 1);
}


/******************************************************************************
NAME
   vc_filesys_open_disk

SYNOPSIS
   int vc_filesys_open_disk(const char *path)

FUNCTION
   Open disk for normal access.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_filesys_open_disk(const char *path)
{
   return vc_filesys_single_string(0, path, VC_FILESYS_OPEN_DISK, 1);
}

/******************************************************************************
NAME
   vc_filesys_close_disk

SYNOPSIS
   int vc_filesys_close_disk(const char *path)

FUNCTION
   Close disk from normal access.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/

int vc_filesys_close_disk(const char *path)
{
   return vc_filesys_single_string(0, path, VC_FILESYS_CLOSE_DISK, 1);
}

/******************************************************************************
NAME
   vc_filesys_numsectors

SYNOPSIS
   int vc_filesys_numsectors(const char *path)

FUNCTION
   Return number of sectors on disk

RETURNS
   Successful completion: greater than 0
   Otherwise: -1
******************************************************************************/

int vc_filesys_numsectors(const char *path)
{
   return vc_filesys_single_string(0, path, VC_FILESYS_NUMSECTORS, 1);
}

/******************************************************************************
NAME
   vc_filesys_read_sectors

SYNOPSIS
   int vc_filesys_read_sectors(const char *path, uint32_t sector_num, char *buffer, uint32_t num_sectors)

FUNCTION
   Start streaming sectors from 2727

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/
int vc_filesys_read_sectors(const char *path, uint32_t sector_num, char *sectors, uint32_t num_sectors, uint32_t *sectors_read)
{
#if VCHI_BULK_ALIGN > 1
   uint32_t align_bytes = 0;
#endif
   char* bulk_addr = sectors;
   int len;
   int ret = -1;

   if((len = (int)strlen(path)) < FS_MAX_PATH && lock_obtain() == 0)
   {
      strncpy((char *)vc_filesys_client.fileserv_msg.data, path, FS_MAX_PATH);
      vc_filesys_client.fileserv_msg.params[0] = sector_num;
      vc_filesys_client.fileserv_msg.params[1] = num_sectors;

#if VCHI_BULK_ALIGN > 1
      //required to make buffer aligned for bulk
      align_bytes = VCHI_BULK_ALIGN_NBYTES(sectors);
#endif
      //note for read we are currently doing memcpy on host to tell 2727 align is 0
      vc_filesys_client.fileserv_msg.params[2] = 0;
      
      //we send read request
      if (vchi_msg_stub_noblock(&vc_filesys_client.fileserv_msg, VC_FILESYS_READ_SECTORS, 16+len+1) == 0)
      {
         while(num_sectors)
         {
            uint32_t bulk_sectors = (num_sectors > FILESERV_MAX_BULK_SECTOR) ? (uint32_t)FILESERV_MAX_BULK_SECTOR : num_sectors;
            if(vchi_bulk_queue_receive( vc_filesys_client.open_handle,
#if VCHI_BULK_ALIGN > 1
                                        align_bytes ? vc_filesys_client.bulk_buffer : bulk_addr,
#else
                                        bulk_addr,
#endif
                                        (bulk_sectors*FILESERV_SECTOR_LENGTH), VCHI_FLAGS_BLOCK_UNTIL_OP_COMPLETE, NULL) != 0)
               break;

#if VCHI_BULK_ALIGN > 1
            if(align_bytes) {
               //this is bad but will do for now..
               memcpy( bulk_addr, vc_filesys_client.bulk_buffer, (bulk_sectors*FILESERV_SECTOR_LENGTH));
            }
#endif
            bulk_addr += (bulk_sectors*FILESERV_SECTOR_LENGTH);
            num_sectors -= bulk_sectors;
         }

         // now wait to receive resp from original msg...
         if(vcos_event_wait(&vc_filesys_client.response_event) == VCOS_SUCCESS && vc_filesys_client.fileserv_msg.cmd_code == FILESERV_RESP_OK)
         {
            *sectors_read = vc_filesys_client.fileserv_msg.params[0];
            ret = 0;
         }
         else
         {
            //error code in [0]
            *sectors_read = vc_filesys_client.fileserv_msg.params[1];
            ret = vc_filesys_client.fileserv_msg.params[0];
         }
      }

      lock_release();
   }

   return ret;
}

/******************************************************************************
NAME
   vc_filesys_write_sectors

SYNOPSIS
   int vc_filesys_write_sectors(const char *path, uint32_t sector_num, char *sectors, uint32_t num_sectors, uint32_t *sectors_written)

FUNCTION
   Start streaming sectors to 2727

RETURNS
   Successful completion: 0
   Otherwise: -error code
******************************************************************************/
int vc_filesys_write_sectors(const char *path, uint32_t sector_num, char *sectors, uint32_t num_sectors, uint32_t *sectors_written)
{
   uint32_t align_bytes = 0;
   char* bulk_addr = sectors;
   int len;
   int ret = -1;

   len = (int) strlen(path);
   if((len = (int) strlen(path)) < FS_MAX_PATH && lock_obtain() == 0)
   {
      vc_filesys_client.fileserv_msg.params[0] = sector_num;
      vc_filesys_client.fileserv_msg.params[1] = num_sectors;
      
      //required to make buffer aligned for bulk
      align_bytes = ((unsigned long)sectors & (VCHI_BULK_ALIGN-1));
      vc_filesys_client.fileserv_msg.params[2] = align_bytes;
      
      //copy path at end of any alignbytes
      strncpy(((char *)vc_filesys_client.fileserv_msg.data), path, FS_MAX_PATH);
      
      //we send write request
      if (vchi_msg_stub_noblock(&vc_filesys_client.fileserv_msg, VC_FILESYS_WRITE_SECTORS, 16+len+1) == 0)
      {
         if(align_bytes) {
            //note we are cheating and going backward to make addr aligned and sending more data...
            bulk_addr -= align_bytes;
         }
         
         while(num_sectors) {
            uint32_t bulk_sectors = (num_sectors > FILESERV_MAX_BULK_SECTOR) ? (uint32_t)FILESERV_MAX_BULK_SECTOR : num_sectors;
            //we send some extra data at the start
            if(vchi_bulk_queue_transmit( vc_filesys_client.open_handle, bulk_addr,
                                         VCHI_BULK_ROUND_UP((bulk_sectors*FILESERV_SECTOR_LENGTH)+align_bytes),
                                         VCHI_FLAGS_BLOCK_UNTIL_DATA_READ, NULL) != 0)
               break;
            
            //go to next ALIGNED address
            bulk_addr += FILESERV_MAX_BULK;
            num_sectors -= bulk_sectors;
         }
         
         // now wait to receive resp from original msg...
         if(vcos_event_wait(&vc_filesys_client.response_event) == VCOS_SUCCESS && vc_filesys_client.fileserv_msg.cmd_code == FILESERV_RESP_OK)
         {
            *sectors_written = vc_filesys_client.fileserv_msg.params[0];
            ret = 0;
         }
         else
         {
            //error code in [0]
            *sectors_written = vc_filesys_client.fileserv_msg.params[1];
            ret = vc_filesys_client.fileserv_msg.params[0];
         }
      }

      lock_release();
   }

   return ret;
}

/******************************************************************************
NAME
   vc_filesys_errno

SYNOPSIS
   int vc_filesys_errno(void)

FUNCTION
   Returns the error code of the last file system error that occurred.

RETURNS
   Error code
******************************************************************************/

int vc_filesys_errno(void)
{
   return (int) vc_filesys_client.err_no;
}


/* File Service Message FIFO functions */

/******************************************************************************
NAME
   vchi_msg_stub

SYNOPSIS
   static int vc_fs_stub(int verb, int ext_len)

FUNCTION
   Generates a request and sends it to the co-processor. It then suspends
   until it receives a reply from the host. The calling task must hold
   the filesys_lock.

RETURNS
   Successful completion: Response code of reply
   Otherwise: -
******************************************************************************/
static int vchi_msg_stub(FILESERV_MSG_T* msg, uint16_t cmd_id, int msg_len )
{
   int ret = -1;
   vchi_msg_stub_noblock(msg, cmd_id, msg_len);
    
   // wait to receive response
   if(vcos_event_wait(&vc_filesys_client.response_event) == VCOS_SUCCESS)
      ret = (int) msg->cmd_code;

   return ret;
}

static int vchi_msg_stub_noblock(FILESERV_MSG_T* msg, uint16_t cmd_id, int msg_len)
{
   uint32_t i;

   if(!vcos_verify(msg_len <= VCHI_MAX_MSG_SIZE))
      return -1;

   //will get changed by response from command
   vc_filesys_client.resp_code = FILESERV_RESP_ERROR;

   //the top bit is used for host/vc
   i = vc_filesys_client.cur_xid + 1;
   i &= 0x7fffffffUL;
   vc_filesys_client.cur_xid = i;

   //fill in transaction id, used for response identification
   vchi_writebuf_uint32( &(msg->xid), vc_filesys_client.cur_xid  );

   //fill in cmd id VC_FILESYS_OPEN etc
   vchi_writebuf_uint32( &(msg->cmd_code), cmd_id );

   //we always have cmd_id, xid
   msg_len += 8;
    
   //return response code
   return (int) vchi_msg_queue( vc_filesys_client.open_handle, msg, (uint32_t)msg_len, VCHI_FLAGS_BLOCK_UNTIL_QUEUED, NULL );
}

static int vc_send_response( FILESERV_MSG_T* msg, uint32_t retval, uint32_t nbytes )
{
   int success = -1;
   //convert all to over the wire values
   vchi_writebuf_uint32(&msg->cmd_code, retval);
   vchi_writebuf_uint32(&msg->xid, msg->xid);
   vchi_writebuf_uint32(&msg->params[0], msg->params[0]);
   vchi_writebuf_uint32(&msg->params[1], msg->params[1]);
   vchi_writebuf_uint32(&msg->params[2], msg->params[2]);
   vchi_writebuf_uint32(&msg->params[3], msg->params[3]);

   //start with 8 because always xid and retval
   nbytes += 8;

   if(vcos_verify(nbytes <= VCHI_MAX_MSG_SIZE))
      success = (int) vchi_msg_queue( vc_filesys_client.open_handle, msg, nbytes, VCHI_FLAGS_BLOCK_UNTIL_QUEUED, NULL );

   return success;
}

/******************************************************************************
NAME
   vc_fs_message_handler

SYNOPSIS
   static int vc_fs_message_handler()

FUNCTION
   Handle messages from the co-processor.

RETURNS
   0 - No message found.
   1 - Request received and actioned.
   2 - Reply received.
******************************************************************************/

static int vc_fs_message_handler( FILESERV_MSG_T* msg, uint32_t nbytes )
{
   int rr = 0;
   uint32_t xid = vchi_readbuf_uint32(&msg->xid);

   if (xid == vc_filesys_client.cur_xid) {

   	  //memcpy reply to msg should really peek before
      vc_filesys_client.fileserv_msg.xid = xid;
      vc_filesys_client.fileserv_msg.cmd_code  = vchi_readbuf_uint32(&msg->cmd_code);
      vc_filesys_client.fileserv_msg.params[0] = vchi_readbuf_uint32(&msg->params[0]);
      vc_filesys_client.fileserv_msg.params[1] = vchi_readbuf_uint32(&msg->params[1]);
      vc_filesys_client.fileserv_msg.params[2] = vchi_readbuf_uint32(&msg->params[2]);
      vc_filesys_client.fileserv_msg.params[3] = vchi_readbuf_uint32(&msg->params[3]);

      //copy any data, 24 is size of header
      if(nbytes >24)
         memcpy(&vc_filesys_client.fileserv_msg.data, msg->data, (nbytes-24));

      vc_filesys_client.resp_code = (int32_t)vc_filesys_client.fileserv_msg.cmd_code;

      if (vc_filesys_client.resp_code == FILESERV_RESP_ERROR) {
         vc_filesys_client.err_no = (int32_t)vc_filesys_client.fileserv_msg.params[0];
      }

      // signal vchi_msg_stub which will be waiting for response
      vcos_event_signal(&vc_filesys_client.response_event);

      rr = 2;
   }
   else if ((xid & 0x80000000UL) == 0x80000000UL) {
      /* Process new requests from the co-processor */

      uint32_t retval = FILESERV_RESP_OK;

      //this is the number of uint32_t param[] + data that we send back to VC in bytes

      uint32_t rlen = 0;
      int i;

      switch (msg->cmd_code) {

      case VC_FILESYS_CLOSE:

         i = vc_hostfs_close((int)msg->params[0]);
         if (i != 0) {
            retval = FILESERV_RESP_ERROR;
         }
         rlen = 0;
         break;

      case VC_FILESYS_CLOSEDIR:

         i = vc_hostfs_closedir((void *)msg->params[0]);
         if (i != 0) {
            retval = FILESERV_RESP_ERROR;
         }
         rlen = 0;
         break;

      case VC_FILESYS_FORMAT:

         i = vc_hostfs_format((const char *)msg->data);
         if (i != 0) {
            retval = FILESERV_RESP_ERROR;
         }
         rlen = 0;
         break;

      case VC_FILESYS_FREESPACE:

         i = vc_hostfs_freespace((const char *)msg->data);
         if (i < 0) {
            retval = FILESERV_RESP_ERROR;
            rlen = 0;
         } else {
            msg->params[0] = (uint32_t)i;
            rlen = 4;
         }
         break;

      case VC_FILESYS_FREESPACE64:
         {
            int64_t freespace;
            freespace = vc_hostfs_freespace64((const char *)msg->data);
            if (freespace < (int64_t)0) {
               retval = FILESERV_RESP_ERROR;
               rlen = 0;
            } else {
               msg->params[0] = (uint32_t)freespace;
               msg->params[1] = (uint32_t)(freespace>>32);
               rlen = 8;
            }
         }
         break;

      case VC_FILESYS_GET_ATTR:
         {
            fattributes_t attr;

            i = vc_hostfs_get_attr((const char *)msg->data,
                                   &attr);
            if (i != 0) {
               retval = FILESERV_RESP_ERROR;
               rlen = 0;
            } else {
               msg->params[0] = (uint32_t) attr;
               rlen = 4;
            }
         }
         break;
      case VC_FILESYS_LSEEK:

         i = vc_hostfs_lseek( (int)msg->params[0],
                              (int)msg->params[1],
                              (int)msg->params[2]);
         if (i < 0) {
            retval = FILESERV_RESP_ERROR;
            rlen = 0;
         } else {
            msg->params[0] = (uint32_t) i;
            rlen = 4;
         }
         break;

      case VC_FILESYS_LSEEK64:
         {
            int64_t offset;
            offset = (((int64_t) msg->params[2]) << 32) + msg->params[1];

            offset = vc_hostfs_lseek64( (int)msg->params[0], offset, (int)msg->params[3]);
            if (offset < (int64_t)0) {
               retval = FILESERV_RESP_ERROR;
               rlen = 0;
            } else {
               msg->params[0] = (uint32_t)offset;
               msg->params[1] = (uint32_t)(offset>>32);
               rlen = 8;
            }
         }
         break;

      case VC_FILESYS_MKDIR:

         i = vc_hostfs_mkdir((const char *)msg->data);
         if (i != 0) {
            retval = FILESERV_RESP_ERROR;
         }
         rlen = 0;
         break;

      case VC_FILESYS_OPEN:

         i = vc_hostfs_open((const char *)msg->data,
                            (int) msg->params[0]);
         if (i < 0) {
            retval = FILESERV_RESP_ERROR;
         } else {
            msg->params[0] = (uint32_t) i;
         }
         rlen = 4;
         break;

      case VC_FILESYS_OPENDIR:

         msg->params[0] = (uint32_t)vc_hostfs_opendir(
                                                      (const char *)msg->data);
         if ((void *)msg->params[0] == NULL) {
            retval = FILESERV_RESP_ERROR;
         }
         rlen = 4;
         break;

      case VC_FILESYS_READ:
         {
            uint32_t fd = msg->params[0];
            uint32_t offset = msg->params[1];
            int total_bytes = (int)msg->params[2];
            uint32_t nalign_bytes = msg->params[3];

            i = 0;

            if(!vcos_verify(((int)vc_filesys_client.bulk_buffer & (VCHI_BULK_ALIGN-1)) == 0 &&
                            total_bytes <= FILESERV_MAX_BULK))
            {
               retval = FILESERV_RESP_ERROR;
               rlen = 4;
               break;
            }

            rlen = 0;

            //perform any seeking
            if ( (uint32_t)0xffffffffUL != offset)
            {
               i = vc_hostfs_lseek( (int)fd,
                                    (long int) offset,
                                    VC_FILESYS_SEEK_SET);
               if ( 0 > i)
               {
                  retval = FILESERV_RESP_ERROR;
                  rlen = 4;
                  break;
               }
            }


            //put it all in one msg
            if(total_bytes <= FILESERV_MAX_DATA) {
               i = vc_hostfs_read( (int)fd,
                                   msg->data,
                                   (unsigned int) total_bytes);

               if(i < 0) {
                  retval = FILESERV_RESP_ERROR;
                  msg->params[0] = 0;
               }
               else {
                  retval = FILESERV_RESP_OK;
                  //send back length of read
                  msg->params[0] = (uint32_t) i;
               }

               msg->params[1] = 0;
               rlen = 16 + (uint32_t) i;
            }
            //bulk transfer required
            else {
               uint32_t end_bytes = 0;
               retval = FILESERV_BULK_WRITE;

               //we send the bytes required for HOST buffer align
               if(nalign_bytes) {
                  i = vc_hostfs_read( (int)fd,
                                      msg->data,
                                      (unsigned int)nalign_bytes);
                  if(i < 0) {
                     retval = FILESERV_RESP_ERROR;
                     rlen = 16;
                     break;
                  }
                  else if(i != (int)nalign_bytes) {
                     //all data will be in one msg
                     retval = FILESERV_RESP_OK;
                     msg->params[0] = (uint32_t) i;
                     msg->params[1] = 0;
                     rlen = 16 + (uint32_t) i;
                     //send response
                     break;
                  }

                  total_bytes -= i;
                  rlen += (uint32_t) i;
               }

               //bulk bytes
               i = vc_hostfs_read((int)fd, vc_filesys_client.bulk_buffer, (unsigned int)total_bytes);

               if(i < 0) {
                  retval = FILESERV_RESP_ERROR;
                  rlen = 16;
                  break;
               }
               else if((i+nalign_bytes) <= FILESERV_MAX_DATA) {
                  retval = FILESERV_RESP_OK;
                  memcpy(&msg->data[nalign_bytes], &vc_filesys_client.bulk_buffer[0], (size_t) i);
                  //read size
                  msg->params[0] = (i + nalign_bytes);
                  msg->params[1] = 0;
                  rlen = i + nalign_bytes + 16;
                  break;
               }

               //copy end unaligned length bytes into msg->data
               end_bytes  = (uint32_t) (i & (VCHI_BULK_GRANULARITY-1));
               if(end_bytes) {
                  int end_index = i - (int) end_bytes;
                  memcpy(&msg->data[nalign_bytes], &vc_filesys_client.bulk_buffer[end_index], end_bytes);
                  rlen += end_bytes;
               }

               //send back total bytes
               msg->params[0] = (uint32_t)(i + nalign_bytes);
               //number of end bytes
               msg->params[1] = end_bytes;
               //number of bulk bytes
               msg->params[2] = (uint32_t)(i - end_bytes);
               //16 for param len
               rlen += 16;

               //queue bulk to be sent
               if(vchi_bulk_queue_transmit( vc_filesys_client.open_handle,
                                            vc_filesys_client.bulk_buffer,
                                            msg->params[2],
                                            VCHI_FLAGS_BLOCK_UNTIL_QUEUED,
                                            NULL ) != 0)
               {
                  retval = FILESERV_RESP_ERROR;
                  rlen = 4;
                  break;
               }
            }
         }
         //send response
         break;

      case VC_FILESYS_READDIR:
         {
            struct dirent result;
            if (vc_hostfs_readdir_r((void *)msg->params[0],
                                    &result) == NULL) {
               retval = FILESERV_RESP_ERROR;
               rlen = 4;
            } else {
               rlen = (uint32_t) (16+fs_host_direntbytestream_create(&result,
                                                                     (void *)msg->data));
            }
         }
         break;

      case VC_FILESYS_REMOVE:

         i = vc_hostfs_remove((const char *)msg->data);
         if (i != 0) {
            retval = FILESERV_RESP_ERROR;
         }
         rlen = 0;
         break;

      case VC_FILESYS_RENAME:

         i = (int) strlen((char *)msg->data);
         if (vc_hostfs_rename((const char *)msg->data,
                              (const char *)&msg->data[i+1])
             != 0) {
            retval = FILESERV_RESP_ERROR;
         }
         rlen = 0;
         break;

      case VC_FILESYS_SETEND:

         i = vc_hostfs_setend( (int)msg->params[0] );
         if (i != 0) {
            retval = FILESERV_RESP_ERROR;
         }
         rlen = 0;
         break;

      case VC_FILESYS_SET_ATTR:

         i = vc_hostfs_set_attr((const char *)msg->data,
                                (fattributes_t)msg->params[0]);
         if (i != 0) {
            retval = FILESERV_RESP_ERROR;
         }
         rlen = 0;
         break;

      case VC_FILESYS_TOTALSPACE:

         i = vc_hostfs_totalspace((const char *)msg->data);
         if (i < 0) {
            retval = FILESERV_RESP_ERROR;
            rlen = 0;
         } else {
            msg->params[0] = (uint32_t) i;
            rlen = 4;
         }
         break;

      case VC_FILESYS_TOTALSPACE64:
         {
            int64_t totalspace;
            totalspace = vc_hostfs_totalspace64((const char *)msg->data);
            if (totalspace < (int64_t)0) {
               retval = FILESERV_RESP_ERROR;
               rlen = 0;
            } else {
               msg->params[0] = (uint32_t)totalspace;
               msg->params[1] = (uint32_t)(totalspace>>32);
               rlen = 8;
            }
         }
         break;
#if 0  // I don't think host systems are ready for these yet
      case VC_FILESYS_SCANDISK:

         vc_hostfs_scandisk((const char *)msg->data);
         rlen = 0;
         break;

      case VC_FILESYS_CHKDSK:

         i = vc_hostfs_chkdsk((const char *)msg->data, msg->params[0]);
         if (i < 0) {
            retval = FILESERV_RESP_ERROR;
            rlen = 0;
         } else {
            msg->params[0] = (uint32_t)i;
            rlen = 4;
         }
         break;
#endif
      case VC_FILESYS_WRITE:
         {
            uint32_t fd = msg->params[0];
            //            uint32_t offset = msg->params[1];
            int total_bytes = (int)msg->params[2];
            uint32_t nalign_bytes = msg->params[3];
            retval = FILESERV_RESP_OK;
            i = 0;

            //everything in one msg
            if(total_bytes <= FILESERV_MAX_DATA)
            {
               i = vc_hostfs_write( (int)fd,
                                    msg->data,
                                    (unsigned int) total_bytes);
               if (i < 0) {
                  retval = FILESERV_RESP_ERROR;
               } else {
                  msg->params[0] = (uint32_t) i;
               }
               rlen = 4;
               //send response
               break;
            }
            else
            {
               uint32_t end_bytes;
               uint32_t bulk_bytes;
               uint32_t total_bytes_written = 0;
               i = 0;
               //one return param
               rlen = 4;
               retval = FILESERV_BULK_READ;

               //write bytes required for VC buffer align
               if(nalign_bytes) {
                  i = vc_hostfs_write( (int)fd,
                                       msg->data,
                                       (unsigned int)nalign_bytes);
                  if(i < 0) {
                     retval = FILESERV_RESP_ERROR;
                     msg->params[0] = 0;
                     //break from switch and send reply
                     break;
                  }
                  total_bytes_written += i;
                  total_bytes -= nalign_bytes;
               }

               //calculate bytes that wil lbe sent by bulk
               bulk_bytes = (uint32_t)((total_bytes)&(~(VCHI_BULK_ALIGN-1)));
            
               end_bytes = total_bytes-bulk_bytes;



               if(vchi_bulk_queue_receive(vc_filesys_client.open_handle,
                                          vc_filesys_client.bulk_buffer,
                                          bulk_bytes,
                                          VCHI_FLAGS_BLOCK_UNTIL_OP_COMPLETE,
                                          NULL) != 0 ||
                  (i = vc_hostfs_write( (int)fd,
                                        vc_filesys_client.bulk_buffer,
                                        (unsigned int) bulk_bytes)) < 0)
               {
                  retval = FILESERV_RESP_ERROR;
                  msg->params[0] = 0;
                  break;
               }
            
               total_bytes_written += i;
            
               if(end_bytes) {
                  i = vc_hostfs_write( (int)fd,
                                       msg->data+nalign_bytes,
                                       (unsigned int)end_bytes);
               
                  total_bytes_written += i;
               }
            
               if(i < 0) {
                  retval = FILESERV_RESP_ERROR;
                  msg->params[0] = 0;
                  break;
               }
            
               msg->params[0] = total_bytes_written;

            }
         }
         break;

      default:
         rlen = 4;
         retval = FILESERV_RESP_ERROR;
         break;
      }

      //convert all to over the wire values and send
      vc_send_response( msg, retval, rlen );

      rr = 1;

   } else {
      /* A message has been left in the fifo and the host side has been reset.
         The message needs to be flushed. It would be better to do this by resetting
         the fifos. */
   }

   return rr;
}


/******************************************************************************
NAME
   showmsg

SYNOPSIS
   static void showmsg(VC_MSGFIFO_CMD_HEADER_T const * head,
                       struct file_service_msg_body const * body)

FUNCTION
   De-bug tool: prints out fifo message.

RETURNS
   void
******************************************************************************/

#ifdef PRINTF
static void showmsg(VC_MSGFIFO_CMD_HEADER_T const * head,
                    struct file_service_msg_body const * body)
{
   unsigned int ael = (head->ext_length + 15) & ~15;
   printf("Sync=%08x XID=%08x Code=%08x Extlen=%08x (%d bytes follow)\n",
          head->sync, head->xid, head->cmd_code, head->ext_length, ael);
   if (ael) {
      unsigned int i;
      printf("Content:");
      for (i = 0; i < 4 && i*4 < head->ext_length; ++i) printf(" %08x", body->params[i]);
      //for(i = 0; i+16 < head->ext_length; ++i) printf(" %02x", body->data[i]);
      if (head->ext_length > 16) printf(" plus %d bytes\n", head->ext_length);
   }
   printf("\n");
}
#endif


/******************************************************************************
NAME
   fs_host_direntbytestream_create

SYNOPSIS
   static int fs_host_direntbytestream_create(struct dirent *d, void *buffer)

FUNCTION
   Turns a variable of type struct dirent into a compiler independent byte
   stream, which is stored in buffer.

RETURNS
   Successful completion: The length of the byte stream
   Otherwise: -1
******************************************************************************/
static void write_bytestream(void *a, char *b, int n)
{
   int i;
   for (i=0;i<n;i++) {
      b[i] = ((char *)a)[i];
   }
   for (;i<4;i++) {
      b[i] = 0;
   }
}

static int fs_host_direntbytestream_create(struct dirent *d, void *buffer)
{
   char *buf = (char*)buffer;

   // Write d_name (D_NAME_MAX_SIZE chars)
   memcpy(buf, &d->d_name, D_NAME_MAX_SIZE);
   buf += D_NAME_MAX_SIZE;

   // Write d_size (unsigned int)
   write_bytestream((void *)&d->d_size, buf, (int)sizeof(d->d_size));
   buf += 4;

   // Write d_attrib (int)
   write_bytestream((void *)&d->d_attrib, buf, (int)sizeof(d->d_attrib));
   buf += 4;

   // Write d_modtime (time_t)
   write_bytestream((void *)&d->d_modtime, buf, (int)sizeof(d->d_modtime));
   buf += 4;

   return (int)(buf-(char *)buffer);
}


/******************************************************************************
NAME
   fs_host_direntbytestream_interp

SYNOPSIS
   static void fs_host_direntbytestream_interp(struct dirent *d, void *buffer)

FUNCTION
   Turns a compiler independent byte stream back into a struct of type dirent.

RETURNS
   Successful completion: 0
   Otherwise: -1
******************************************************************************/
static void read_bytestream(void *a, char *b, int n)
{
   int i;
   for (i=0;i<n;i++) {
      ((char *)a)[i] = b[i];
   }
}

static void fs_host_direntbytestream_interp(struct dirent *d, void *buffer)
{
   char *buf = (char*)buffer;

   // Read d_name (D_NAME_MAX_SIZE chars)
   memcpy(&d->d_name, buf, D_NAME_MAX_SIZE);
   buf += D_NAME_MAX_SIZE;

   // Read d_size (unsigned int)
   read_bytestream((void *)&d->d_size, buf, (int)sizeof(d->d_size));
   d->d_size = VC_VTOH32(d->d_size);
   buf += 4;

   // Read d_attrib (int)
   read_bytestream((void *)&d->d_attrib, buf, (int)sizeof(d->d_attrib));
   d->d_attrib = VC_VTOH32(d->d_attrib);
   buf += 4;

   // Read d_modtime (time_t)
   read_bytestream((void *)&d->d_modtime, buf, (int)sizeof(d->d_modtime));
   d->d_modtime = VC_VTOH32(d->d_modtime);

   return;
}



void vc_filesys_sendreset(void)
{
   //TODO
}
