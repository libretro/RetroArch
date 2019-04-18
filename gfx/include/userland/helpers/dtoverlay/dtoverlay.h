/*
Copyright (c) 2016 Raspberry Pi (Trading) Ltd.
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

#ifndef DTOVERLAY_H
#define DTOVERLAY_H

#include <stdarg.h>

#define BE4(x) ((x)>>24)&0xff, ((x)>>16)&0xff, ((x)>>8)&0xff, ((x)>>0)&0xff
#define GETBE4(p, off) ((((unsigned char *)p)[off + 0]<<24) + (((unsigned char *)p)[off + 1]<<16) + \
                  (((unsigned char *)p)[off + 2]<<8) + (((unsigned char *)p)[off + 3]<<0))
#define SETBE4(p, off, x) do { \
   ((unsigned char *)p)[off + 0] = ((x>>24) & 0xff); \
   ((unsigned char *)p)[off + 1] = ((x>>16) & 0xff); \
   ((unsigned char *)p)[off + 2] = ((x>>8) & 0xff); \
   ((unsigned char *)p)[off + 3] = ((x>>0) & 0xff); \
} while (0)

#define NON_FATAL(err) (((err) < 0) ? -(err) : (err))
#define IS_FATAL(err) ((err) < 0)
#define ONLY_FATAL(err) (IS_FATAL(err) ? (err) : 0)

#define DTOVERLAY_PADDING(size) (-(size))

typedef enum
{
   DTOVERLAY_ERROR,
   DTOVERLAY_DEBUG
} dtoverlay_logging_type_t;

typedef struct dtoverlay_struct
{
   const char *param;
   int len;
   const char *b;
} DTOVERLAY_PARAM_T;

typedef struct dtblob_struct
{
   void *fdt;
   char fdt_is_malloced;
   char trailer_is_malloced;
   char fixups_applied;
   int min_phandle;
   int max_phandle;
   void *trailer;
   int trailer_len;
} DTBLOB_T;

typedef void DTOVERLAY_LOGGING_FUNC(dtoverlay_logging_type_t type,
                                    const char *fmt, va_list args);

typedef int (*override_callback_t)(int override_type,
				   DTBLOB_T *dtb, int node_off,
				   const char *prop_name, int target_phandle,
				   int target_off, int target_size,
				   void *callback_value);

uint8_t dtoverlay_read_u8(const void *src, int off);
uint16_t dtoverlay_read_u16(const void *src, int off);
uint32_t dtoverlay_read_u32(const void *src, int off);
uint64_t dtoverlay_read_u64(const void *src, int off);
void dtoverlay_write_u8(void *dst, int off, uint32_t val);
void dtoverlay_write_u16(void *dst, int off, uint32_t val);
void dtoverlay_write_u32(void *dst, int off, uint32_t val);
void dtoverlay_write_u64(void *dst, int off, uint64_t val);

/* Return values: -ve = fatal error, positive = non-fatal error */
int dtoverlay_create_node(DTBLOB_T *dtb, const char *node_name, int path_len);

int dtoverlay_delete_node(DTBLOB_T *dtb, const char *node_name, int path_len);

int dtoverlay_find_node(DTBLOB_T *dtb, const char *node_path, int path_len);

int dtoverlay_set_node_properties(DTBLOB_T *dtb, const char *node_path,
                                  DTOVERLAY_PARAM_T *properties,
                                  unsigned int num_properties);

int dtoverlay_create_prop_fragment(DTBLOB_T *dtb, int idx, int target_phandle,
                                   const char *prop_name, const void *prop_data,
                                   int prop_len);

int dtoverlay_fixup_overlay(DTBLOB_T *base_dtb, DTBLOB_T *overlay_dtb);

int dtoverlay_merge_overlay(DTBLOB_T *base_dtb, DTBLOB_T *overlay_dtb);

int dtoverlay_merge_params(DTBLOB_T *dtb, const DTOVERLAY_PARAM_T *params,
                           unsigned int num_params);

const char *dtoverlay_find_override(DTBLOB_T *dtb, const char *override_name,
                                    int *data_len);

int dtoverlay_override_one_target(int override_type,
                                  DTBLOB_T *dtb, int node_off,
                                  const char *prop_name, int target_phandle,
                                  int target_off, int target_size,
                                  void *callback_value);

int dtoverlay_foreach_override_target(DTBLOB_T *dtb, const char *override_name,
                                      const char *override_data, int data_len,
                                      override_callback_t callback,
                		      void *callback_value);

int dtoverlay_apply_override(DTBLOB_T *dtb, const char *override_name,
                             const char *override_data, int data_len,
                             const char *override_value);

int dtoverlay_override_one_target(int override_type,
				  DTBLOB_T *dtb, int node_off,
				  const char *prop_name, int target_phandle,
				  int target_off, int target_size,
				  void *callback_value);

int dtoverlay_set_synonym(DTBLOB_T *dtb, const char *dst, const char *src);

int dtoverlay_dup_property(DTBLOB_T *dtb, const char *node_name,
                           const char *dst, const char *src);

DTBLOB_T *dtoverlay_create_dtb(int max_size);

DTBLOB_T *dtoverlay_load_dtb_from_fp(FILE *fp, int max_size);

DTBLOB_T *dtoverlay_load_dtb(const char *filename, int max_size);

DTBLOB_T *dtoverlay_import_fdt(void *fdt, int max_size);

int dtoverlay_save_dtb(const DTBLOB_T *dtb, const char *filename);

int dtoverlay_extend_dtb(DTBLOB_T *dtb, int new_size);

int dtoverlay_dtb_totalsize(DTBLOB_T *dtb);

void dtoverlay_pack_dtb(DTBLOB_T *dtb);

void dtoverlay_free_dtb(DTBLOB_T *dtb);

static inline void *dtoverlay_dtb_trailer(DTBLOB_T *dtb)
{
    return dtb->trailer;
}

static inline int dtoverlay_dtb_trailer_len(DTBLOB_T *dtb)
{
    return dtb->trailer_len;
}

static inline void dtoverlay_dtb_set_trailer(DTBLOB_T *dtb,
                                             void *trailer,
                                             int trailer_len)
{
    dtb->trailer = trailer;
    dtb->trailer_len = trailer_len;
    dtb->trailer_is_malloced = 0;
}

int dtoverlay_find_phandle(DTBLOB_T *dtb, int phandle);

int dtoverlay_find_symbol(DTBLOB_T *dtb, const char *symbol_name);

int dtoverlay_find_matching_node(DTBLOB_T *dtb, const char **node_names,
                                 int pos);

int dtoverlay_node_is_enabled(DTBLOB_T *dtb, int pos);

const void *dtoverlay_get_property(DTBLOB_T *dtb, int pos,
                                   const char *prop_name, int *prop_len);

int dtoverlay_set_property(DTBLOB_T *dtb, int pos,
                           const char *prop_name, const void *prop, int prop_len);

const char *dtoverlay_get_alias(DTBLOB_T *dtb, const char *alias_name);

int dtoverlay_set_alias(DTBLOB_T *dtb, const char *alias_name, const char *value);

void dtoverlay_set_logging_func(DTOVERLAY_LOGGING_FUNC *func);

void dtoverlay_enable_debug(int enable);

void dtoverlay_error(const char *fmt, ...);

void dtoverlay_debug(const char *fmt, ...);

#endif
