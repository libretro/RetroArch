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

#ifndef LOGGING_LOGGING_H
#define LOGGING_LOGGING_H

#include <stdarg.h>

#include "vcinclude/vcore.h"

/* Bitfield for indicating the category and level of a logging message. */
#define LOGGING_GENERAL                   (1<<0)  /* for logging general messages */
#define LOGGING_GENERAL_VERBOSE           (2<<0)
#define LOGGING_CODECS                    (1<<2)  /* for codec messages */
#define LOGGING_CODECS_VERBOSE            (2<<2)
#define LOGGING_FILESYSTEM                (1<<4)  /* filesystem messages */
#define LOGGING_FILESYSTEM_VERBOSE        (2<<4)
#define LOGGING_VMCS                      (1<<6)  /* VMCS related messages */
#define LOGGING_VMCS_VERBOSE              (2<<6)
#define LOGGING_DISPMAN2                  (1<<8)  /* Dispman2/scalar logs */
#define LOGGING_DISPMAN2_VERBOSE          (2<<8)
#define LOGGING_GCE                       (1<<8)  /* Re-use Dispman2 for GCE logging */
#define LOGGING_GCE_VERBOSE               (2<<8)
#define LOGGING_CAMPLUS                   (1<<10) /* Camplus logs */
#define LOGGING_CAMPLUS_VERBOSE           (2<<10)
#define LOGGING_APPS                      (1<<12) /* Application logs */
#define LOGGING_APPS_VERBOSE              (2<<12)
#define LOGGING_CLOCKMAN_POWERMAN         (1<<14) /* Clockman + powerman logs */
#define LOGGING_CLOCKMAN_POWERMAN_VERBOSE (2<<14)
#define LOGGING_VCOS                      (1<<16)
#define LOGGING_VCOS_VERBOSE              (2<<16)
#define LOGGING_IMAGE_POOL                (1<<18) /* Image pool messages */
#define LOGGING_IMAGE_POOL_VERBOSE        (2<<18)
#define LOGGING_HDMI                      (1<<20) /* HDMI and HDCP messages */
#define LOGGING_HDMI_VERBOSE              (2<<20)
#define LOGGING_MINIMAL                   (1<<22) /* minimal logging for bandwidth measurement, ie all others off. */
#define LOGGING_MINIMAL_VERBOSE           (2<<22)
#define LOGGING_TUNER                     (1<<24) /* ISP Tuner logs - AGC, AWB etc */
#define LOGGING_TUNER_VERBOSE             (2<<24)
#define LOGGING_VCHI                      (1<<26) /* For all VCHI based services */
#define LOGGING_VCHI_VERBOSE              (2<<26)
#define LOGGING_FOCUS                     (1<<28) /* Focus messages */
#define LOGGING_HANDLERS                  (1<<29) /* For handler messages */
#define LOGGING_VOWIFI                    (1<<28) /* Re-use FOCUS for VOWIFI */
#define LOGGING_VOWIFI_VERBOSE            (2<<28) /* Re-use HANDLERS for VOWIFI */
// add more here
#define LOGGING_USER                      (1<<30) /* only for code under development - do not check in! */
#define LOGGING_USER_VERBOSE              (2<<30)

/* Define some rarely used messages as general messages */
#define LOGGING_DMB            (LOGGING_GENERAL)         /* DMB logs */
#define LOGGING_DMB_VERBOSE    (LOGGING_GENERAL_VERBOSE)
#define LOGGING_MEMORY         (LOGGING_GENERAL)         /* memory task pool logs */
#define LOGGING_MEMORY_VERBOSE (LOGGING_GENERAL_VERBOSE)

#define LOGGING_ALL 0x55555555                /* log all messages */
#define LOGGING_ALL_VERBOSE 0xaaaaaaaa
#define LOGGING_NONE 0                        /* log nothing */

typedef enum {
   LOGGING_FIFO_LOG = 1,
   LOGGING_ASSERTION_LOG,
   LOGGING_TASK_LOG,
} LOG_FORMAT_T;

typedef struct {
   LOG_FORMAT_T type;
   void *log;
} LOG_DESCRIPTOR_T;

// This header prefixes the logged data on every fifo log entry.
typedef struct {
   unsigned long time;
   unsigned short seq_num;    // if two entries have the same timestamp then this seq num differentiates them.
   unsigned short size;       // = size of entire log entry = header + data
} logging_fifo_log_msg_header_t;

// This describes the state of a fifo type log.
typedef struct {
   char name[4];
   unsigned char *start;      // Start and End are defined when the log is created.
   unsigned char *end;
   unsigned char *ptr;        // Always points to where the next contents will be written.
   unsigned char *next_msg;   // Points to the first entry in the fifo. Is updated when the fifo gets full and overwrites the oldest entry.
   logging_fifo_log_msg_header_t msg_header;    // used to keep track of forming a log entry between calls to _start and _end.
} logging_fifo_log_t;

// This describes the state of an array type log.
typedef struct {
   char name[4];
   unsigned short nitems, item_size;
   unsigned long available;
   unsigned char *data;
} logging_array_log_t;

// The header at the start of the log may be one of a number of different types,
// but they all start with a common portion:

#define LOGGING_SYNC 'VLOG'

typedef unsigned long LOGGING_LOG_TYPE_T;
#define LOGGING_LOG_TYPE_VCLIB ((LOGGING_LOG_TYPE_T) 0)
#define LOGGING_LOG_TYPE_VMCS  ((LOGGING_LOG_TYPE_T) 1)

typedef struct {
   unsigned long sync;
   LOGGING_LOG_TYPE_T type;
   unsigned long version;
   void *self;
} logging_common_header_t;

#define N_VCLIB_LOGS  3  //Does not include the task switch log

// This is the top level data structure for the Videocore Logs. It houses several
// logs from different sources as well as extra state info.
typedef struct {
   logging_common_header_t common;
   unsigned long stc;
   int task_switch_log_size;
   unsigned char *task_switch_log;
   unsigned long n_logs;
   LOG_DESCRIPTOR_T assertion_log;
   LOG_DESCRIPTOR_T message_log;
   LOG_DESCRIPTOR_T task_log;

   //LOG_DESCRIPTOR_T logs[N_VCLIB_LOGS - 3];

   //logging_fifo_log_t *assertion_log;
   //logging_fifo_log_t *message_log;
   //logging_array_log_t *task_log;
} logging_header_t;

typedef void (*logging_notify_callback_fn)( void );

#ifdef LOGGING

#ifndef BLOGGING

/* Initialise a log for use, supplying the memory where it starts and the
   number of bytes available. */
void logging_fifo_log_init(logging_fifo_log_t *log, const char *name, void *start, int size);

/* Begin writing a log message. */
void logging_fifo_log_start(logging_fifo_log_t *log);
/* Write nbytes of message data. */
void logging_fifo_log_write(logging_fifo_log_t *log, const void *ptr, int nbytes);
/* Finish a log message. */
void logging_fifo_log_end(logging_fifo_log_t *log);

/* Read a log message */
int logging_read( LOG_FORMAT_T logFormat, void *buf, int bufLen );

/* Initialise an array log. */
void logging_array_log_init(logging_array_log_t *log, char *name, int nitems, int item_size, void *data, int total_size);
/* Add an item to the array log. */
void logging_array_log_add(logging_array_log_t *log, unsigned long key, void *data, int nbytes);
/* Remove an item. */
void logging_array_log_delete(logging_array_log_t *log, unsigned long key);

/* Initialise vclib's logging fifos. */
void logging_init();

/* Reset assert log */
void logging_assert_reset();

/* Reset mesage log */
void logging_message_reset();

/* Setup a fifo in the given block of memory. */
unsigned char *logging_setup_fifo (logging_fifo_log_t **ptr, const char *name, unsigned char *addr, int fifo_size);
/* Set the current logging level. */
void logging_level(int level);
/* Get the current logging level. */
int logging_get_level(void);
/* Test if a logging level will actually produce logging messages. */
int logging_will_log(int level);
/* Log an assertion. */
void logging_assert(const char *file, const char *func, int line, const char *format, ...);
/* dumps registers and stack which get printed by logging_assert */
void logging_assert_dump(void);
/* Log a general message. */
void logging_message(int level, const char *format, ...);
void vlogging_message (int level, const char *format, va_list args);

/* Log a simple string */
void logging_string(int level, const char *string);
/* The log for recording the current tasks. */
logging_array_log_t *logging_task_log(void);

void logging_set_notify_callback( logging_notify_callback_fn fn );

/* Dump log to sdcard, discarding any logging that happens during the file write */
int logging_dump_log(char* filename);

/* Dump log to sdcard. If a sufficiently large temporary buffer is not provided,
logging messages will be discarded during the file write. */
int logging_dump_log_buffered(char* filename, void *buffer, int buffer_size);

/* Parse a fifo log entry and put it into a human readable string. */
int logging_fifo_extract_entry(logging_fifo_log_t *log, const unsigned char *entry_ptr, char *target_buffer, int target_size);

/* Log a hexadecimal representation of the first num_bytes of byte_array,
   formatted to format using logging_message() at logging-level level. */
int logging_message_hex(int level, const char *format, const void *byte_array, int num_bytes);

/* Log a hexadecimal representation of the Vector Register File contents.*/
int logging_message_vrf(int level);

/* Append logging to a file. If flag set, only output to the file */
int logging_set_logfile(const char *file, int only_to_file);

/* Close the file opened above */
void logging_close_logfile(void);

#ifdef WANT_LOGGING_UART
/* Enables/disables outputing log messages to the UART. */
void logging_uart_enable_output(int enable_output);
/* Set the current UART logging level. */
void logging_level_uart(int level);
/* Get the current UART logging level. */
int logging_get_level_uart(void);
#endif

unsigned char* get_message_log_ptr_and_size (int* size);

#else   // BLOGGING

#include "vcfw/blogging/blogging.h"

#define logging_fifo_log_init(log, start, size)
#define logging_fifo_log_start(log)
#define logging_fifo_log_write(log, ptr, nbytes)
#define logging_fifo_log_end(log)

#endif  // BLOGGING

/* Cause the assert macro to log a message if it's defined. Only changed without NDEBUG, though other assert macros still work.*/
#ifdef __VIDEOCORE__
#if !defined(NDEBUG) && defined(assert)
#undef assert
#ifndef ARTS
#define assert(cond) \
   ( (cond) ? (void)0 : (_bkpt(),logging_assert_dump(),logging_assert(__FILE__, __func__, __LINE__, #cond)))
#else //ARTS
#define assert(cond) \
   ( (cond) ? (void)0 : (logging_assert_dump(),logging_assert(__FILE__, __func__, __LINE__, #cond)))
#endif //ARTS
#endif // !defined(NDEBUG) && defined(assert)

/* This will only trigger assert first time it fires */
#define assert_once(cond,comment) \
   do { \
      static char i_asrt=0; \
      if (!(cond)) { \
         if (i_asrt==0) { \
            _bkpt(); \
            i_asrt++; \
            logging_assert_dump(),logging_assert(__FILE__, __func__, __LINE__, (comment)); \
         } \
      } \
   } while (0)

#define assert_comment(cond, comment) if (cond) {} else logging_assert_dump(),logging_assert(__FILE__, __func__, __LINE__,(comment))

#else
   #ifndef ARTS
      #include <assert.h>
   #else
      #undef assert
      #define assert(cond) ( (cond) ? (void)0 : (logging_assert_dump(),logging_assert(__FILE__, __FUNCTION__, __LINE__, #cond)))
   #endif

#define assert_once(cond,comment) \
   do { \
      static char i_asrt=0; \
      if (!(cond)) { \
         if (i_asrt==0) { \
            i_asrt++; \
            logging_assert_dump(),logging_assert(__FILE__, __func__, __LINE__, (comment)); \
         } \
      } \
   } while (0)

#endif // not __VIDEOCORE__

#else   //LOGGING

#define logging_fifo_log_init(log, start, size)

#define logging_fifo_log_start(log)
#define logging_fifo_log_write(log, ptr, nbytes)
#define logging_fifo_log_end(log)

#define logging_array_log_init(log, nitems, item_size, data, total_size)
#define logging_array_log_add(log, key, data, nbytes)
#define logging_array_log_delete(log, key)

#define logging_init()
#define logging_level(level)
#define logging_get_level() 0
#define logging_assert(file, func, line, ...)
#define logging_message_hex(level, format, byte_array, num_bytes)
#define logging_dump_log(filename) -1
#define logging_will_log(level) 0

#ifndef EDID_TOOL //logging_message is mapped to printf for our edid_parser tool
/* Hacky way of getting rid of a var args function with a macro... */
static __inline void logging_message (int level, const char *format, ...) { }
#endif
#define vlogging_message(level, format, args)
#define logging_string(level, string)

#define logging_task_log()

#define assert_periodic(cond,nreps,dt,comment)
#define assert_once(cond,comment)
#define assert_comment(cond, comment)

#define logging_set_notify_callback(fn)

#endif  // LOGGING

#endif // LOGGING_LOGGING_H
