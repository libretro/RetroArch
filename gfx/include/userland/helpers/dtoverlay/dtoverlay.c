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

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <stdint.h>
#include <libfdt.h>
#include <assert.h>

#include "dtoverlay.h"

#define ARRAY_SIZE(a)   (sizeof(a) / sizeof(a[0]))

typedef enum
{
   FIXUP_ABSOLUTE,
   FIXUP_RELATIVE
} fixup_type_t;

#define DTOVERRIDE_END     0
#define DTOVERRIDE_INTEGER 1
#define DTOVERRIDE_BOOLEAN 2
#define DTOVERRIDE_STRING  3
#define DTOVERRIDE_OVERLAY 4

static int dtoverlay_extract_override(const char *override_name,
                                      int *phandle_ptr,
                                      const char **data_ptr, int *len_ptr,
                                      const char **namep, int *namelenp,
                                      int *offp, int *sizep);

static int dtoverlay_set_node_name(DTBLOB_T *dtb, int node_off,
				   const char *name);

static void dtoverlay_stdio_logging(dtoverlay_logging_type_t type,
                                    const char *fmt, va_list args);

#define phandle_debug if (0) dtoverlay_debug

static DTOVERLAY_LOGGING_FUNC *dtoverlay_logging_func = dtoverlay_stdio_logging;
static int dtoverlay_debug_enabled = 0;

uint8_t dtoverlay_read_u8(const void *src, int off)
{
   const unsigned char *p = src;
   return (p[off + 0] << 0);
}

uint16_t dtoverlay_read_u16(const void *src, int off)
{
   const unsigned char *p = src;
   return (p[off + 0] << 8) | (p[off + 1] << 0);
}

uint32_t dtoverlay_read_u32(const void *src, int off)
{
   const unsigned char *p = src;
   return (p[off + 0] << 24) | (p[off + 1] << 16) |
          (p[off + 2] << 8)  | (p[off + 3] << 0);
}

uint64_t dtoverlay_read_u64(const void *src, int off)
{
   const unsigned char *p = src;
   return ((uint64_t)p[off + 0] << 56) | ((uint64_t)p[off + 1] << 48) |
          ((uint64_t)p[off + 2] << 40)  | ((uint64_t)p[off + 3] << 32) |
          (p[off + 4] << 24) | (p[off + 5] << 16) |
          (p[off + 6] << 8)  | (p[off + 7] << 0);
}

void dtoverlay_write_u8(void *dst, int off, uint32_t val)
{
   unsigned char *p = dst;
   p[off] = (val >> 0) & 0xff;
}

void dtoverlay_write_u16(void *dst, int off, uint32_t val)
{
   unsigned char *p = dst;
   p[off + 0] = (val >> 8) & 0xff;
   p[off + 1] = (val >> 0) & 0xff;
}

void dtoverlay_write_u32(void *dst, int off, uint32_t val)
{
   unsigned char *p = dst;
   p[off + 0] = (val >> 24) & 0xff;
   p[off + 1] = (val >> 16) & 0xff;
   p[off + 2] = (val >> 8) & 0xff;
   p[off + 3] = (val >> 0) & 0xff;
}

void dtoverlay_write_u64(void *dst, int off, uint64_t val)
{
   unsigned char *p = dst;
   p[off + 0] = (val >> 56) & 0xff;
   p[off + 1] = (val >> 48) & 0xff;
   p[off + 2] = (val >> 40) & 0xff;
   p[off + 3] = (val >> 32) & 0xff;
   p[off + 4] = (val >> 24) & 0xff;
   p[off + 5] = (val >> 16) & 0xff;
   p[off + 6] = (val >> 8) & 0xff;
   p[off + 7] = (val >> 0) & 0xff;
}

// Returns the offset of the node indicated by the absolute path, creating
// it and any intermediates as necessary, or a negative error code.
int dtoverlay_create_node(DTBLOB_T *dtb, const char *node_path, int path_len)
{
   const char *path_ptr;
   const char *path_end;
   int node_off = 0;

   if (!path_len)
      path_len = strlen(node_path);

   path_ptr = node_path;
   path_end = node_path + path_len;

   if ((path_len > 0) && (path_ptr[path_len - 1] == '/'))
       path_end--;

   while (path_ptr < path_end)
   {
      const char *path_next;
      int subnode_off;

      if (*path_ptr != '/')
         return -FDT_ERR_BADPATH;

      // find the next path separator (or the end of the string)
      path_ptr++;
      for (path_next = path_ptr;
           (path_next != path_end) && (*path_next != '/');
           path_next++)
         continue;

      subnode_off = fdt_subnode_offset_namelen(dtb->fdt, node_off, path_ptr,
                                               path_next - path_ptr);
      if (subnode_off >= 0)
         node_off = subnode_off;
      else
         node_off = fdt_add_subnode_namelen(dtb->fdt, node_off, path_ptr,
                                            path_next - path_ptr);
      if (node_off < 0)
         break;

      path_ptr = path_next;
   }

   if ((node_off >= 0) && (path_ptr != path_end))
      return -FDT_ERR_BADPATH;

   return node_off;
}

// Returns 0 on success, otherwise <0 error code
int dtoverlay_delete_node(DTBLOB_T *dtb, const char *node_path, int path_len)
{
   int node_off = 0;
   if (!path_len)
      path_len = strlen(node_path);

   dtoverlay_debug("delete_node(%.*s)", path_len, node_path);
   node_off = fdt_path_offset_namelen(dtb->fdt, node_path, path_len);
   if (node_off < 0)
      return node_off;
   return fdt_del_node(dtb->fdt, node_off);
}

// Returns the offset of the node indicated by the absolute path or a negative
// error code.
int dtoverlay_find_node(DTBLOB_T *dtb, const char *node_path, int path_len)
{
   if (!path_len)
      path_len = strlen(node_path);
   return fdt_path_offset_namelen(dtb->fdt, node_path, path_len);
}

// Returns 0 on success, otherwise <0 error code
int dtoverlay_set_node_properties(DTBLOB_T *dtb, const char *node_path,
                                  DTOVERLAY_PARAM_T *properties,
                                  unsigned int num_properties)
{
   int err = 0;
   int node_off;

   node_off = fdt_path_offset(dtb->fdt, node_path);
   if (node_off < 0)
      node_off = dtoverlay_create_node(dtb, node_path, 0);
   if (node_off >= 0)
   {
      int i;
      for (i = 0; (i < num_properties) && (err == 0); i++)
      {
         DTOVERLAY_PARAM_T *p;

         p = properties + i;
         err = fdt_setprop(dtb->fdt, node_off, p->param, p->b, p->len);
      }
   }
   else
      err = node_off;
   return err;
}

struct dynstring
{
   char *buf;
   int size;
   int len;
};

static void dynstring_init(struct dynstring *ds)
{
   ds->size = 0;
   ds->len = 0;
   ds->buf = NULL;
}

static int dynstring_init_size(struct dynstring *ds, int initial_size)
{
   if (initial_size < 32)
      initial_size = 32;
   ds->size = initial_size;
   ds->len = 0;
   ds->buf = malloc(initial_size);
   if (!ds->buf)
   {
      dtoverlay_error("  out of memory");
      return -FDT_ERR_NOSPACE;
   }
   return 0;
}

static int dynstring_set_size(struct dynstring *ds, int size)
{
   if (size > ds->size)
   {
      size = (size * 5)/4; // Add a 25% headroom
      ds->buf = realloc(ds->buf, size);
      if (!ds->buf)
      {
         dtoverlay_error("  out of memory");
         return -FDT_ERR_NOSPACE;
      }
      ds->size = size;
   }
   return 0;
}

static int dynstring_dup(struct dynstring *ds, const char *src, int len)
{
   int err = 0;

   if (!len)
      len = strlen(src);

   err = dynstring_set_size(ds, len + 1);
   if (!err)
   {
      memcpy(ds->buf, src, len + 1);
      ds->len = len;
   }

   return err;
}

static int dynstring_patch(struct dynstring *ds, int pos, int width,
                           const char *src, int len)
{
   int newlen = ds->len + (len - width);
   int err = dynstring_set_size(ds, newlen + 1);
   if (!err)
   {
      if (width != len)
      {
         // Move any data following the patch
         memmove(ds->buf + pos + len, ds->buf + pos + width,
                 ds->len + 1 - (pos + width));
         ds->len = newlen;
      }
      memcpy(ds->buf + pos, src, len);
   }
   return err;
}

static int dynstring_grow(struct dynstring *ds)
{
   return dynstring_set_size(ds, (3*ds->size)/2);
}

static void dynstring_free(struct dynstring *ds)
{
   free(ds->buf);
   dynstring_init(ds);
}

static int dtoverlay_set_node_name(DTBLOB_T *dtb, int node_off,
				   const char *name)
{
   struct dynstring path_buf;
   struct dynstring prop_buf;
   char *old_path;
   const char *old_name;
   const char *fixup_nodes[] =
   {
      "/__fixups__",
      "/__local_fixups__", // For old-style dtbos
      "/__symbols__"       // Just in case the kernel cares
   };
   int old_name_len;
   int old_path_len; // All of it
   int dir_len; // Excluding the node name, but with the trailling slash
   int name_len;
   int offset;
   int fixup_idx;
   int err = 0;

   // Fixups and local-fixups both use node names, so this
   // function must be patch them up when a node is renamed
   // unless the fixups have already been applied.
   // Calculating a node's name is expensive, so only do it if
   // necessary. Since renaming a node can move things around,
   // don't use node_off afterwards.
   err = dynstring_init_size(&path_buf, 100);
   if (err)
      return err;

   if (!dtb->fixups_applied)
   {
      while (1)
      {
         err = fdt_get_path(dtb->fdt, node_off, path_buf.buf, path_buf.size);
         if (!err)
            break;
         if (err != -FDT_ERR_NOSPACE)
            return err;
         dynstring_grow(&path_buf);
      }
   }
   old_path = path_buf.buf;

   err = fdt_set_name(dtb->fdt, node_off, name);
   if (err || dtb->fixups_applied)
      goto clean_up;

   // Find the node name in old_path
   old_name = strrchr(old_path, '/');
   assert(old_name);
   if (!old_name)
      return -FDT_ERR_INTERNAL;
   old_name++;
   old_name_len = strlen(old_name);
   dir_len = old_name - old_path;
   old_path_len = dir_len + old_name_len;

   // Short-circuit the case where the name isn't changing
   if (strcmp(name, old_name) == 0)
      goto clean_up;

   name_len = strlen(name);

   // Search the fixups and symbols for the old path (including as
   // a parent)  and replace with the new name

   dynstring_init(&prop_buf);
   for (fixup_idx = 0; fixup_idx < ARRAY_SIZE(fixup_nodes); fixup_idx++)
   {
      int prop_off;

      offset = fdt_path_offset(dtb->fdt, fixup_nodes[fixup_idx]);
      if (offset > 0)
      {

         // Iterate through the properties
         for (prop_off = fdt_first_property_offset(dtb->fdt, offset);
              (prop_off >= 0) && (err == 0);
              prop_off = fdt_next_property_offset(dtb->fdt, prop_off))
         {
            const char *prop_name;
            const char *prop_val;
            int prop_len;
            int pos;
            int changed = 0;

            prop_val = fdt_getprop_by_offset(dtb->fdt, prop_off,
                                             &prop_name, &prop_len);
            err = dynstring_dup(&prop_buf, prop_val, prop_len);
            if (err)
               break;

            // Scan each property for matching paths
            pos = 0;
            while (pos < prop_len)
            {
               if ((pos + old_path_len < prop_len) &&
                   (memcmp(prop_buf.buf + pos, old_path, old_path_len) == 0) &&
                   ((prop_buf.buf[pos + old_path_len] == ':') ||
                    (prop_buf.buf[pos + old_path_len] == '/') ||
                    (prop_buf.buf[pos + old_path_len] == '\0')))
               {
                  // Patch the string, replacing old name with new
                  err = dynstring_patch(&prop_buf, pos + dir_len, old_name_len,
                                        name, name_len);
                  if (err)
                     break;

                  prop_len += name_len - old_name_len;
                  changed = 1;
               }
               pos += strlen(prop_buf.buf + pos) + 1;
            }

            if (!err && changed)
            {
               // Caution - may change offsets, but only by shuffling everything
               // afterwards, i.e. the offset to this node or property does not
               // change.
               err = fdt_setprop(dtb->fdt, offset, prop_name, prop_buf.buf,
                                 prop_len);
            }
         }
      }
   }

   dynstring_free(&prop_buf);

   if (err)
      goto clean_up;

   // Then look for a "/__local_fixups__<old_path>" node, and rename
   // that as well.
   offset = fdt_path_offset(dtb->fdt, "/__local_fixups__");
   if (offset > 0)
   {
      const char *p, *end;

      p = old_path;
      end = old_path + old_path_len;
      while (p < end)
      {
         const char *q;

         while (*p == '/') {
            p++;
            if (p == end)
               break;
         }
         q = memchr(p, '/', end - p);
         if (! q)
            q = end;

         offset = fdt_subnode_offset_namelen(dtb->fdt, offset, p, q-p);
         if (offset < 0)
            break;

         p = q;
      }

      if (offset > 0)
         err = fdt_set_name(dtb->fdt, offset, name);
   }

   // __overrides__ don't need patching because nodes are identified
   // using phandles, which are unaffected by renaming and resizing nodes.

clean_up:
   dynstring_free(&path_buf);

   return err;
}

// Returns 0 on success, otherwise <0 error code
int dtoverlay_create_prop_fragment(DTBLOB_T *dtb, int idx, int target_phandle,
                                   const char *prop_name, const void *prop_data,
                                   int prop_len)
{
	char fragment_name[20];
	int frag_off, ovl_off;
	int ret;
	snprintf(fragment_name, sizeof(fragment_name), "fragment-%u", idx);
	frag_off = fdt_add_subnode(dtb->fdt, 0, fragment_name);
	if (frag_off < 0)
		return frag_off;
	ret = fdt_setprop_u32(dtb->fdt, frag_off, "target", target_phandle);
	if (ret < 0)
		return ret;
	ovl_off = fdt_add_subnode(dtb->fdt, frag_off, "__overlay__");
	if (ovl_off < 0)
		return ovl_off;
	return fdt_setprop(dtb->fdt, ovl_off, prop_name, prop_data, prop_len);
}

// Returns 0 on success, otherwise <0 error code
static int dtoverlay_merge_fragment(DTBLOB_T *base_dtb, int target_off,
                                    const DTBLOB_T *overlay_dtb,
                                    int overlay_off, int depth)
{
   int prop_off, subnode_off;
   int err = 0;

   if (dtoverlay_debug_enabled)
   {
      char base_path[256];
      char overlay_path[256];
      fdt_get_path(base_dtb->fdt, target_off, base_path, sizeof(base_path));
      fdt_get_path(overlay_dtb->fdt, overlay_off, overlay_path,
                   sizeof(overlay_path));

      dtoverlay_debug("merge_fragment(%s,%s)", base_path,
                      overlay_path);
   }

   // Merge each property of the node
   for (prop_off = fdt_first_property_offset(overlay_dtb->fdt, overlay_off);
        (prop_off >= 0) && (err == 0);
        prop_off = fdt_next_property_offset(overlay_dtb->fdt, prop_off))
   {
      const char *prop_name;
      const void *prop_val;
      int prop_len;
      struct fdt_property *target_prop;
      int target_len;

      prop_val = fdt_getprop_by_offset(overlay_dtb->fdt, prop_off,
                                       &prop_name, &prop_len);

      /* Skip these system properties (only phandles in the first level) */
      if ((strcmp(prop_name, "name") == 0) ||
          ((depth == 0) && ((strcmp(prop_name, "phandle") == 0) ||
			    (strcmp(prop_name, "linux,phandle") == 0))))
          continue;

      dtoverlay_debug("  +prop(%s)", prop_name);

      if ((strcmp(prop_name, "bootargs") == 0) &&
         ((target_prop = fdt_get_property_w(base_dtb->fdt, target_off, prop_name, &target_len)) != NULL) &&
         (target_len > 0) && *target_prop->data)
      {
         target_prop->data[target_len - 1] = ' ';
         err = fdt_appendprop(base_dtb->fdt, target_off, prop_name, prop_val, prop_len);
      }
      else
         err = fdt_setprop(base_dtb->fdt, target_off, prop_name, prop_val, prop_len);
   }

   // Merge each subnode of the node
   for (subnode_off = fdt_first_subnode(overlay_dtb->fdt, overlay_off);
        (subnode_off >= 0) && (err == 0);
        subnode_off = fdt_next_subnode(overlay_dtb->fdt, subnode_off))
   {
      const char *subnode_name;
      int name_len;
      int subtarget_off;

      subnode_name = fdt_get_name(overlay_dtb->fdt, subnode_off, &name_len);

      subtarget_off = fdt_subnode_offset_namelen(base_dtb->fdt, target_off,
                                                 subnode_name, name_len);
      if (subtarget_off < 0)
         subtarget_off = fdt_add_subnode_namelen(base_dtb->fdt, target_off,
                                                 subnode_name, name_len);

      if (subtarget_off >= 0)
      {
         err = dtoverlay_merge_fragment(base_dtb, subtarget_off,
                                        overlay_dtb, subnode_off,
                                        depth + 1);
      }
      else
      {
         err = subtarget_off;
      }
   }

   dtoverlay_debug("merge_fragment() end");

   return err;
}

static int dtoverlay_phandle_relocate(DTBLOB_T *dtb, int node_off,
                                      const char *prop_name,
                                      uint32_t phandle_increment)
{
   int len;
   const fdt32_t *prop_val = fdt_getprop(dtb->fdt, node_off, prop_name, &len);
   int err = 0; // The absence of the property is not an error

   if (prop_val)
   {
      uint32_t phandle;

      if (len < 4)
      {
         dtoverlay_error("%s property too small", prop_name);
         return -FDT_ERR_BADSTRUCTURE;
      }

      phandle = fdt32_to_cpu(*prop_val) + phandle_increment;
      phandle_debug("  phandle_relocate %d->%d", fdt32_to_cpu(*prop_val), phandle);

      err = fdt_setprop_inplace_u32(dtb->fdt, node_off, prop_name, phandle);
   }

   return err;
}

// Returns 0 on success, or an FDT error code
static int dtoverlay_apply_fixups(DTBLOB_T *dtb, const char *fixups_stringlist,
                                  uint32_t phandle, fixup_type_t type)
{
   // The fixups arrive as a sequence of NUL-terminated strings, of the form:
   //   "path:property:offset"
   // Use an empty string as an end marker, since:
   // 1) all tags begin 0x00 0x00 0x00,
   // 2) all string properties must be followed by a tag,
   // 3) an empty string is not a valid fixup, and
   // 4) the code is simpler as a result.

   const char *fixup = fixups_stringlist;

   while  (fixup[0])
   {
      const char *prop_name, *offset_str;
      char *offset_end;
      const void *prop_ptr;
      int prop_len;
      int node_off;
      unsigned long offset;
      uint32_t patch;

      prop_name = strchr(fixup, ':');
      if (!prop_name)
         return -FDT_ERR_BADSTRUCTURE;
      prop_name++;

      offset_str = strchr(prop_name, ':');
      if (!offset_str)
         return -FDT_ERR_BADSTRUCTURE;
      offset_str++;

      offset = strtoul(offset_str, &offset_end, 10);
      if ((offset_end == offset_str) || (offset_end[0] != 0))
         return -FDT_ERR_BADSTRUCTURE;

      node_off = fdt_path_offset_namelen(dtb->fdt, fixup, prop_name - 1 - fixup);
      if (node_off < 0)
         return node_off;

      prop_ptr = fdt_getprop_namelen(dtb->fdt, node_off, prop_name,
                                     offset_str - 1 - prop_name, &prop_len);
      if (!prop_ptr)
         return prop_len;
      if (offset > (prop_len - 4))
         return -FDT_ERR_BADSTRUCTURE;

      // Now apply the patch. Yes, prop_ptr is a const void *, but the
      // alternative (copying the whole property, patching, then updating as
      // a whole) is ridiculous.
      if (type == FIXUP_RELATIVE)
      {
         patch = phandle + dtoverlay_read_u32(prop_ptr, offset);
         phandle_debug("  phandle fixup %d+%d->%d", phandle, patch - phandle, patch);
      }
      else
      {
         patch = phandle;
         phandle_debug("  phandle ref '%s'->%d", prop_name, patch);
      }

      dtoverlay_write_u32((void *)prop_ptr, offset, patch);

      fixup = offset_end + 1;
   }

   return 0;
}

// Returns 0 on success, or an FDT error code
static int dtoverlay_apply_fixups_node(DTBLOB_T *dtb, int fix_off,
                                       int target_off, uint32_t phandle_offset)
{
   // The fixups are arranged as a subtree mirroring the structure of the
   // overall tree. Walk this tree in order. Each property is an array of cells
   // containing offsets to patch within the corresponding node/property of
   // the target tree.
   int err = 0;
   int prop_off;
   int subfix_off;

   // Merge each property of the node
   for (prop_off = fdt_first_property_offset(dtb->fdt, fix_off);
        (prop_off >= 0) && (err == 0);
        prop_off = fdt_next_property_offset(dtb->fdt, prop_off))
   {
      const char *prop_name;
      const void *prop_val;
      int prop_len;
      void *target_ptr;
      int target_len;
      int off;

      prop_val = fdt_getprop_by_offset(dtb->fdt, prop_off,
                                       &prop_name, &prop_len);
      if (!prop_val)
         return -FDT_ERR_INTERNAL;

      target_ptr = fdt_getprop_w(dtb->fdt, target_off, prop_name, &target_len);
      if (!target_ptr)
         return -FDT_ERR_BADSTRUCTURE;

      for (off = 0; (off + 4) <= prop_len; off += 4)
      {
         uint32_t patch;
         int patch_offset = dtoverlay_read_u32(prop_val, off);
         if ((patch_offset + 4) > target_len)
            return -FDT_ERR_BADSTRUCTURE;

         patch = phandle_offset + dtoverlay_read_u32(target_ptr, patch_offset);
         phandle_debug("  phandle fixup %d+%d->%d", phandle_offset, patch - phandle_offset, patch);

         dtoverlay_write_u32(target_ptr, patch_offset, patch);
      }
   }

   // Merge each subnode of the node
   for (subfix_off = fdt_first_subnode(dtb->fdt, fix_off);
        (subfix_off >= 0) && (err == 0);
        subfix_off = fdt_next_subnode(dtb->fdt, subfix_off))
   {
      const char *subnode_name;
      int name_len;
      int subtarget_off;

      subnode_name = fdt_get_name(dtb->fdt, subfix_off, &name_len);

      subtarget_off = fdt_subnode_offset_namelen(dtb->fdt, target_off,
                                                 subnode_name, name_len);

      if (subtarget_off >= 0)
      {
         err = dtoverlay_apply_fixups_node(dtb, subfix_off, subtarget_off,
                                           phandle_offset);
      }
      else
      {
         err = subtarget_off;
      }
   }

   return err;
}

// Returns 0 on success, or a negative FDT error.
static int dtoverlay_resolve_phandles(DTBLOB_T *base_dtb, DTBLOB_T *overlay_dtb)
{
   int local_fixups_off;
   int node_off;
   int err = 0;

   // First find and update the phandles in the overlay

   for (node_off = 0;
        node_off >= 0;
        node_off = fdt_next_node(overlay_dtb->fdt, node_off, NULL))
   {
      dtoverlay_phandle_relocate(overlay_dtb, node_off, "phandle",
                                 base_dtb->max_phandle);
      dtoverlay_phandle_relocate(overlay_dtb, node_off, "linux,phandle",
                                 base_dtb->max_phandle);
   }

   local_fixups_off = fdt_path_offset(overlay_dtb->fdt, "/__local_fixups__");
   if (local_fixups_off >= 0)
   {
      const char *fixups_stringlist;

      // Update the references to local phandles using the local fixups.
      // The property name is "fixup".
      // The value is a NUL-separated stringlist of descriptors of the form:
      //    path:property:offset
      fixups_stringlist =
         fdt_getprop(overlay_dtb->fdt, local_fixups_off, "fixup", &err);
      if (fixups_stringlist)
      {
         // Relocate the overlay phandle references
         err = dtoverlay_apply_fixups(overlay_dtb, fixups_stringlist,
                                      base_dtb->max_phandle, FIXUP_RELATIVE);
      }
      else
      {
          err = dtoverlay_apply_fixups_node(overlay_dtb, local_fixups_off,
                                            0, base_dtb->max_phandle);
      }
      if (err < 0)
      {
         dtoverlay_error("error applying local fixups");
         return err;
      }
   }

   overlay_dtb->max_phandle += base_dtb->max_phandle;
   phandle_debug("  +overlay max phandle +%d -> %d", base_dtb->max_phandle, overlay_dtb->max_phandle);

   return err;
}

// Returns 0 on success, or an FDT error code
static int dtoverlay_resolve_fixups(DTBLOB_T *base_dtb, DTBLOB_T *overlay_dtb)
{
   int fixups_off;
   int err = 0;

   fixups_off = fdt_path_offset(overlay_dtb->fdt, "/__fixups__");

   if (fixups_off >= 0)
   {
      int fixup_off, symbols_off = -1;

      fixup_off = fdt_first_property_offset(overlay_dtb->fdt, fixups_off);

      if (fixup_off >= 0)
      {
         // Find the symbols, which will be needed to resolve the fixups
         symbols_off = fdt_path_offset(base_dtb->fdt, "/__symbols__");

         if (symbols_off < 0)
         {
            dtoverlay_error("No symbols found");
            return -FDT_ERR_NOTFOUND;
         }
      }

      for (;
           fixup_off >= 0;
           fixup_off = fdt_next_property_offset(overlay_dtb->fdt, fixup_off))
      {
         const char *fixups_stringlist, *symbol_name, *target_path;
         const char *ref_type;
         int target_off;
         uint32_t target_phandle;

         // The property name identifies a symbol (or alias) in the base.
         // The value is a comma-separated list of descriptors of the form:
         //    path:property:offset
         fixups_stringlist = fdt_getprop_by_offset(overlay_dtb->fdt, fixup_off,
                                                   &symbol_name, &err);
         if (!fixups_stringlist)
         {
            dtoverlay_error("__fixups__ are borked");
            break;
         }

         // 1) Find the target node.
         if (symbol_name[0] == '/')
         {
            /* This is a new-style path reference */
            target_path = symbol_name;
            ref_type = "path";
         }
         else
         {
            target_path = fdt_getprop(base_dtb->fdt, symbols_off, symbol_name,
                                      &err);
            if (!target_path)
            {
               dtoverlay_error("can't find symbol '%s'", symbol_name);
               break;
            }

            ref_type = "symbol";
         }

         target_off = fdt_path_offset(base_dtb->fdt, target_path);
         if (target_off < 0)
         {
            dtoverlay_error("%s '%s' is invalid", ref_type, symbol_name);
            err = target_off;
            break;
         }

         // 2) Ensure that the target node has a phandle.
         target_phandle = fdt_get_phandle(base_dtb->fdt, target_off);
         if (!target_phandle)
         {
            // It doesn't, so give it one
            fdt32_t temp;
            target_phandle = ++base_dtb->max_phandle;
            temp = cpu_to_fdt32(target_phandle);

            err = fdt_setprop(base_dtb->fdt, target_off, "phandle",
                              &temp, 4);

            if (err != 0)
            {
               dtoverlay_error("failed to add a phandle");
               break;
            }
            phandle_debug("  phandle '%s'->%d", target_path, target_phandle);

            // The symbols may have moved, so recalculate
            symbols_off = fdt_path_offset(base_dtb->fdt, "/__symbols__");
         }

         // Now apply the valid target_phandle to the items in the fixup string

         err = dtoverlay_apply_fixups(overlay_dtb, fixups_stringlist,
                                      target_phandle, FIXUP_ABSOLUTE);
         if (err)
            break;
      }
   }

   return err;
}

// Returns 0 on success, -ve for fatal errors and +ve for non-fatal errors
int dtoverlay_merge_overlay(DTBLOB_T *base_dtb, DTBLOB_T *overlay_dtb)
{
   // Merge each fragment node
   int frag_off;

   for (frag_off = fdt_first_subnode(overlay_dtb->fdt, 0);
        frag_off >= 0;
        frag_off = fdt_next_subnode(overlay_dtb->fdt, frag_off))
   {
      const char *node_name, *target_path;
      const char *frag_name;
      int target_off, overlay_off;
      int len, err;

      node_name = fdt_get_name(overlay_dtb->fdt, frag_off, NULL);

      if (strncmp(node_name, "fragment@", 9) != 0 &&
          strncmp(node_name, "fragment-", 9) != 0)
         continue;
      frag_name = node_name + 9;

      dtoverlay_debug("Found fragment %s (offset %d)", frag_name, frag_off);

      // Find the target and overlay nodes
      overlay_off = fdt_subnode_offset(overlay_dtb->fdt, frag_off, "__overlay__");
      if (overlay_off < 0)
      {
         if (fdt_subnode_offset(overlay_dtb->fdt, frag_off, "__dormant__"))
            dtoverlay_debug("fragment %s disabled", frag_name);
         else
            dtoverlay_error("no overlay in fragment %s", frag_name);
         continue;
      }

      target_path = fdt_getprop(overlay_dtb->fdt, frag_off, "target-path", &len);
      if (target_path)
      {
         if (len && (target_path[len - 1] == '\0'))
            len--;
         target_off = fdt_path_offset_namelen(base_dtb->fdt, target_path, len);
         if (target_off < 0)
         {
            dtoverlay_error("invalid target-path '%.*s'", len, target_path);
            return NON_FATAL(target_off);
         }
      }
      else
      {
         const void *target_prop;
         target_prop = fdt_getprop(overlay_dtb->fdt, frag_off, "target", &len);
         if (!target_prop)
         {
            dtoverlay_error("no target or target-path");
            return NON_FATAL(len);
         }

         if (len != 4)
            return NON_FATAL(FDT_ERR_BADSTRUCTURE);

         target_off =
            fdt_node_offset_by_phandle(base_dtb->fdt,
                                       fdt32_to_cpu(*(fdt32_t *)target_prop));
         if (target_off < 0)
         {
            dtoverlay_error("invalid target");
            return NON_FATAL(target_off);
         }
      }

      // Now do the merge
      err = dtoverlay_merge_fragment(base_dtb, target_off, overlay_dtb,
                                     overlay_off, 0);
      if (err != 0)
      {
         dtoverlay_error("merge failed");
         return err;
      }
   }

   base_dtb->max_phandle = overlay_dtb->max_phandle;

   return 0;
}

// Returns 0 on success, -ve for fatal errors and +ve for non-fatal errors
int dtoverlay_fixup_overlay(DTBLOB_T *base_dtb, DTBLOB_T *overlay_dtb)
{
   int err;

   // To do: Check the "compatible" string?

   err = dtoverlay_resolve_fixups(base_dtb, overlay_dtb);

   if (err >= 0)
      err = dtoverlay_resolve_phandles(base_dtb, overlay_dtb);

   overlay_dtb->fixups_applied = 1;

   return NON_FATAL(err);
}

// Returns 0 on success, -ve for fatal errors and +ve for non-fatal errors
int dtoverlay_merge_params(DTBLOB_T *dtb, const DTOVERLAY_PARAM_T *params,
                           unsigned int num_params)
{
   int err = 0;
   unsigned int i;

   for (i=0; (i<num_params) && (err == 0); i++) {
      const DTOVERLAY_PARAM_T *p;
      const char *node_name, *slash;
      int node_off, path_len;

      p = params + i;
      node_name = p->param;
      slash = strrchr(node_name, '/');

      if (!slash)
      {
         err = NON_FATAL(FDT_ERR_BADPATH);
         break;
      }

      // Ensure that root properties ("/xxx") work
      if (slash == node_name)
         path_len = 1;
      else
         path_len = slash - node_name;

      // find node, create if it does not exist yet
      node_off = dtoverlay_create_node(dtb, node_name, path_len);
      if (node_off >= 0)
      {
         const char *prop_name = slash + 1;
         int prop_len;
         struct fdt_property *prop;

         if ((strcmp(prop_name, "bootargs") == 0) &&
            ((prop = fdt_get_property_w(dtb->fdt, node_off, prop_name, &prop_len)) != NULL) &&
            (prop_len > 0) && *prop->data)
         {
            prop->data[prop_len - 1] = ' ';
            err = fdt_appendprop(dtb->fdt, node_off, prop_name, p->b, p->len);
         }
         else
            err = fdt_setprop(dtb->fdt, node_off, prop_name, p->b, p->len);
      }
      else
         err = node_off;
   }

   return err;
}

/* Returns a pointer to the override data and (through data_len) its length.
   On error, sets *data_len to be the error code. */
const char *dtoverlay_find_override(DTBLOB_T *dtb, const char *override_name,
                                    int *data_len)
{
   int overrides_off;
   const char *data;
   int len;

   // Find the table of overrides
   overrides_off = fdt_path_offset(dtb->fdt, "/__overrides__");

   if (overrides_off < 0)
   {
      dtoverlay_debug("/__overrides__ node not found");
      *data_len = overrides_off;
      return NULL;
   }

   // Locate the property
   data = fdt_getprop(dtb->fdt, overrides_off, override_name, &len);
   *data_len = len;
   if (data)
      dtoverlay_debug("Found override %s", override_name);
   else
      dtoverlay_debug("/__overrides__ has no %s property", override_name);

   return data;
}

int dtoverlay_override_one_target(int override_type,
				  DTBLOB_T *dtb, int node_off,
				  const char *prop_name, int target_phandle,
				  int target_off, int target_size,
				  void *callback_value)
{
   const char *override_value = callback_value;
   int err = 0;

   if (override_type == DTOVERRIDE_STRING)
   {
      char *prop_val;
      int prop_len;

      /* Replace the whole property with the string */
      if (strcmp(prop_name, "status") == 0)
      {
	 /* Convert booleans to okay/disabled */
	 if ((strcmp(override_value, "y") == 0) ||
	     (strcmp(override_value, "yes") == 0) ||
	     (strcmp(override_value, "on") == 0) ||
	     (strcmp(override_value, "true") == 0) ||
	     (strcmp(override_value, "enable") == 0) ||
	     (strcmp(override_value, "1") == 0))
		 override_value = "okay";
	 else if ((strcmp(override_value, "n") == 0) ||
                     (strcmp(override_value, "no") == 0) ||
                     (strcmp(override_value, "off") == 0) ||
                     (strcmp(override_value, "false") == 0) ||
                     (strcmp(override_value, "0") == 0))
		 override_value = "disabled";
      }

      if ((strcmp(prop_name, "bootargs") == 0) &&
	  ((prop_val = fdt_getprop_w(dtb->fdt, node_off, prop_name,
				     &prop_len)) != NULL) &&
	  (prop_len > 0) && prop_val[0])
      {
	 prop_val[prop_len - 1] = ' ';
	 err = fdt_appendprop_string(dtb->fdt, node_off, prop_name, override_value);
      }
      else if (strcmp(prop_name, "name") == 0) // "name" is a pseudo-property
      {
         err = dtoverlay_set_node_name(dtb, node_off, override_value);
      }
      else
	 err = fdt_setprop_string(dtb->fdt, node_off, prop_name, override_value);
   }
   else if (override_type != DTOVERRIDE_END)
   {
      const char *p;
      char *end;
      char *prop_val;
      char *prop_buf = NULL;
      int prop_len;
      int new_prop_len;
      uint64_t override_int;
      uint32_t frag_num;

      /* Parse as an integer */
      override_int = strtoull(override_value, &end, 0);
      if (end[0] != '\0')
      {
	 if ((strcmp(override_value, "y") == 0) ||
	     (strcmp(override_value, "yes") == 0) ||
	     (strcmp(override_value, "on") == 0) ||
	     (strcmp(override_value, "true") == 0) ||
	     (strcmp(override_value, "down") == 0))
	    override_int = 1;
	 else if ((strcmp(override_value, "n") == 0) ||
		  (strcmp(override_value, "no") == 0) ||
		  (strcmp(override_value, "off") == 0) ||
		  (strcmp(override_value, "false") == 0))
	    override_int = 0;
	 else if (strcmp(override_value, "up") == 0)
	    override_int = 2;
	 else
	 {
	    dtoverlay_error("invalid override value '%s' - ignored",
			    override_value);
	    return NON_FATAL(FDT_ERR_INTERNAL);
	 }
      }

      switch (override_type)
      {
      case DTOVERRIDE_INTEGER:
	 /* Patch a word within the property */
	 prop_val = (void *)fdt_getprop(dtb->fdt, node_off, prop_name,
					&prop_len);
	 new_prop_len = target_off + target_size;
	 if (prop_len < new_prop_len)
	 {
	     /* This property either doesn't exist or isn't long enough.
		Create a buffer containing any existing property data
		with zero padding, which will later be patched and written
		back. */
	     prop_buf = calloc(1, new_prop_len);
	     if (!prop_buf)
	     {
		 dtoverlay_error("  out of memory");
		 return NON_FATAL(FDT_ERR_NOSPACE);
	     }
	     if (prop_len > 0)
		 memcpy(prop_buf, prop_val, prop_len);
	     prop_val = prop_buf;
	 }

	 switch (target_size)
	 {
	 case 1:
	     dtoverlay_write_u8(prop_val, target_off, (uint32_t)override_int);
	     break;
	 case 2:
	     dtoverlay_write_u16(prop_val, target_off, (uint32_t)override_int);
	     break;
	 case 4:
	     dtoverlay_write_u32(prop_val, target_off, (uint32_t)override_int);
	     break;
	 case 8:
	     dtoverlay_write_u64(prop_val, target_off, override_int);
	     break;
	 default:
	     break;
	 }

	 if (prop_buf && (strcmp(prop_name, "reg") != 0)) // Don't create or extend "reg" - it must be a pseudo-property
	 {
	     /* Add/extend the property by setting it */
             err = fdt_setprop(dtb->fdt, node_off, prop_name, prop_buf, new_prop_len);
	     free(prop_buf);
	 }

         if (strcmp(prop_name, "reg") == 0 && target_off == 0)
         {
            const char *old_name = fdt_get_name(dtb->fdt, node_off, NULL);
            const char *atpos = strchr(old_name, '@');
            if (atpos)
            {
               int name_len = (atpos - old_name);
               char *new_name = malloc(name_len + 1 + 16 + 1);
               if (!new_name)
                  return -FDT_ERR_NOSPACE;
               sprintf(new_name, "%.*s@%x", name_len, old_name, (uint32_t)override_int);
               err = dtoverlay_set_node_name(dtb, node_off, new_name);
               free(new_name);
            }
         }
	 break;

      case DTOVERRIDE_BOOLEAN:
	 /* This is a boolean property (present->true, absent->false) */
	 if (override_int)
	    err = fdt_setprop(dtb->fdt, node_off, prop_name, NULL, 0);
	 else
	 {
	    err = fdt_delprop(dtb->fdt, node_off, prop_name);
	    if (err == -FDT_ERR_NOTFOUND)
	       err = 0;
	 }
	 break;

      case DTOVERRIDE_OVERLAY:
	 /* Apply the overlay-wide override. The supported options are (<frag> is a fragment number):
	    +<frag>    Enable a fragment
	    -<frag>    Disable a fragment
	    =<frag>    Enable/disable the fragment according to the override value
	    !<frag>    Disable/enable the fragment according to the inverse of the override value */
	 p = prop_name;
	 while (*p && !err)
	 {
	    char type = *p;
	    int frag_off;
	    switch (type)
	    {
	    case '+':
	    case '-':
	    case '=':
	    case '!':
	       frag_num = strtoul(p + 1, &end, 0);
	       if (end != p)
	       {
		  /* Change fragment@<frag_num>/__overlay__<->__dormant__ as necessary */
		  const char *states[2] = { "__dormant__", "__overlay__" };
		  char node_name[24];
		  int active = (type == '+') ||
			  ((type == '=') && (override_int != 0)) ||
			  ((type == '!') && (override_int == 0));
		  snprintf(node_name, sizeof(node_name), "/fragment@%u", frag_num);
		  frag_off = fdt_path_offset(dtb->fdt, node_name);
		  if (frag_off < 0)
		  {
		      snprintf(node_name, sizeof(node_name), "/fragment-%u", frag_num);
		      frag_off = fdt_path_offset(dtb->fdt, node_name);
		  }
		  if (frag_off >= 0)
		  {
		     frag_off = fdt_subnode_offset(dtb->fdt, frag_off, states[!active]);
		     if (frag_off >= 0)
			(void)dtoverlay_set_node_name(dtb, frag_off, states[active]);
		  }
		  else
		  {
		     dtoverlay_error("  fragment %u not found", frag_num);
		     err = NON_FATAL(frag_off);
		  }
		  p = end;
	       }
	       else
	       {
		  dtoverlay_error("  invalid overlay override '%s'", prop_name);
		  err = NON_FATAL(FDT_ERR_BADVALUE);
	       }
	       break;
	    }
	 }
	 break;
      }
   }

   return err;
}

// Returns 0 on success, -ve for fatal errors and +ve for non-fatal errors
// After calling this, assume all node offsets are no longer valid
int dtoverlay_foreach_override_target(DTBLOB_T *dtb, const char *override_name,
				      const char *override_data, int data_len,
				      override_callback_t callback,
				      void *callback_value)
{
   int err = 0;
   int target_phandle = 0;
   char *data;

   /* Short-circuit the degenerate case of an empty parameter, avoiding an
      apparent memory allocation failure. */
   if (!data_len)
      return 0;

   /* Copy the override data in case it moves */
   data = malloc(data_len);
   if (!data)
   {
      dtoverlay_error("  out of memory");
      return NON_FATAL(FDT_ERR_NOSPACE);
   }

   memcpy(data, override_data, data_len);
   override_data = data;

   while (err == 0)
   {
      const char *target_prop = NULL;
      char *prop_name = NULL;
      int name_len = 0;
      int target_off = 0;
      int target_size = 0;
      int override_type;
      int node_off = 0;

      override_type = dtoverlay_extract_override(override_name,
                                                &target_phandle,
                                                &override_data, &data_len,
                                                &target_prop, &name_len,
                                                &target_off, &target_size);

      /* Pass DTOVERRIDE_END to the callback, in case it is interested */

      if (target_phandle != 0)
      {
         node_off = fdt_node_offset_by_phandle(dtb->fdt, target_phandle);
         if (node_off < 0)
         {
            dtoverlay_error("  phandle %d not found", target_phandle);
            err = NON_FATAL(node_off);
            break;
         }
      }

      if (target_prop)
      {
         /* Sadly there are no '_namelen' setprop variants, so a copy is required */
         prop_name = malloc(name_len + 1);
         if (!prop_name)
         {
            dtoverlay_error("  out of memory");
            err = NON_FATAL(FDT_ERR_NOSPACE);
            break;
         }
         memcpy(prop_name, target_prop, name_len);
         prop_name[name_len] = '\0';
      }

      err = callback(override_type, dtb, node_off, prop_name,
		     target_phandle, target_off, target_size,
		     callback_value);

      if (prop_name)
         free(prop_name);

      if (override_type == DTOVERRIDE_END)
         break;
   }

   free(data);

   return err;
}

// Returns 0 on success, -ve for fatal errors and +ve for non-fatal errors
int dtoverlay_apply_override(DTBLOB_T *dtb, const char *override_name,
                             const char *override_data, int data_len,
                             const char *override_value)
{
   return dtoverlay_foreach_override_target(dtb, override_name,
					    override_data, data_len,
					    dtoverlay_override_one_target,
					    (void *)override_value);
}

/* Returns an override type (DTOVERRIDE_INTEGER, DTOVERRIDE_BOOLEAN, DTOVERRIDE_STRING, DTOVERRIDE_OVERLAY),
   DTOVERRIDE_END (0) at the end, or an error code (< 0) */
static int dtoverlay_extract_override(const char *override_name,
                                      int *phandle_ptr,
                                      const char **data_ptr, int *len_ptr,
                                      const char **namep, int *namelenp,
				      int *offp, int *sizep)
{
   const char *data;
   const char *prop_name, *override_end;
   int len, override_len, name_len, phandle;
   const char *offset_seps = ".;:#?";
   int type;

   data = *data_ptr;
   len = *len_ptr;
   if (len <= 0)
   {
      if (len < 0)
	 return len;
      *phandle_ptr = 0;
      *namep = NULL;
      return DTOVERRIDE_END;
   }

   // Check for space for a phandle, a terminating NUL and at least one char
   if (len < (sizeof(fdt32_t) + 1 + 1))
   {
      dtoverlay_error("  override %s: data is truncated or mangled",
                      override_name);
      return -FDT_ERR_BADSTRUCTURE;
   }

   phandle = dtoverlay_read_u32(data, 0);
   *phandle_ptr = phandle;

   data += sizeof(fdt32_t);
   len -= sizeof(fdt32_t);

   override_end = memchr(data, 0, len);
   if (!override_end)
   {
      dtoverlay_error("  override %s: string is not NUL-terminated",
                      override_name);
      return -FDT_ERR_BADSTRUCTURE;
   }

   prop_name = data;

   override_len = override_end - prop_name;
   *data_ptr = data + (override_len + 1);
   *len_ptr = len - (override_len + 1);

   if (phandle <= 0)
   {
      if (phandle < 0)
         return -FDT_ERR_BADPHANDLE;
      /* This is an "overlay" override, signalled using <0> as the phandle. */
      *namep = prop_name;
      *namelenp = override_len;
      return DTOVERRIDE_OVERLAY;
   }

   name_len = strcspn(prop_name, offset_seps);

   *namep = prop_name;
   *namelenp = name_len;
   if (name_len < override_len)
   {
      /* There is a separator specified */
      char sep = prop_name[name_len];
      if (sep == '?')
      {
         /* The target is a boolean parameter (present->true, absent->false) */
         *offp = 0;
         *sizep = 0;
         type = DTOVERRIDE_BOOLEAN;
         dtoverlay_debug("  override %s: boolean target %.*s",
                         override_name, name_len, prop_name);
      }
      else
      {
         /* The target is a cell/integer */
         *offp = atoi(prop_name + name_len + 1);
         *sizep = 1 << (strchr(offset_seps, sep) - offset_seps);
         type = DTOVERRIDE_INTEGER;
         dtoverlay_debug("  override %s: cell target %.*s @ offset %d (size %d)",
                         override_name, name_len, prop_name, *offp, *sizep);
      }
   }
   else
   {
      *offp = -1;
      *sizep = 0;
      type = DTOVERRIDE_STRING;
      dtoverlay_debug("  override %s: string target '%.*s'",
                      override_name, name_len, prop_name);
   }

   return type;
}

int dtoverlay_set_synonym(DTBLOB_T *dtb, const char *dst, const char *src)
{
   /* Add/update all aliases, symbols and overrides named dst
      to be equivalent to those named src.
      An absent src is ignored.
   */
   int err;

   err = dtoverlay_dup_property(dtb, "/aliases", dst, src);
   if (err == 0)
      err = dtoverlay_dup_property(dtb, "/__symbols__", dst, src);
   if (err == 0)
      dtoverlay_dup_property(dtb, "/__overrides__", dst, src);
   return err;
}

int dtoverlay_dup_property(DTBLOB_T *dtb, const char *node_name,
                           const char *dst, const char *src)
{
   /* Find the node and src property */
   const DTBLOB_T *src_prop;
   int node_off;
   int prop_len = 0;
   int err = 0;

   node_off = fdt_path_offset(dtb->fdt, node_name);
   if (node_off < 0)
      return 0;

   src_prop = fdt_getprop(dtb->fdt, node_off, src, &prop_len);
   if (!src_prop)
      return 0;

   err = fdt_setprop_inplace(dtb->fdt, node_off, dst, src_prop, prop_len);
   if (err != 0)
   {
      void *prop_data;
      /* Copy the src property, just in case things move */
      prop_data = malloc(prop_len);
      memcpy(prop_data, src_prop, prop_len);

      err = fdt_setprop(dtb->fdt, node_off, dst, prop_data, prop_len);

      free(prop_data);
   }

   if (err == 0)
      dtoverlay_debug("%s:%s=%s", node_name, dst, src);
   return err;
}

DTBLOB_T *dtoverlay_create_dtb(int max_size)
{
   DTBLOB_T *dtb = NULL;
   void *fdt = NULL;

   fdt = malloc(max_size);
   if (!fdt)
   {
      dtoverlay_error("out of memory");
      goto error_exit;
   }

   if (fdt_create_empty_tree(fdt, max_size) != 0)
   {
      dtoverlay_error("failed to create empty dtb");
      goto error_exit;
   }

   dtb = calloc(1, sizeof(DTBLOB_T));
   if (!dtb)
   {
      dtoverlay_error("out of memory");
      goto error_exit;
   }

   dtb->fdt = fdt;
   dtb->max_phandle = 0; // Not a valid phandle

   return dtb;

error_exit:
   free(fdt);
   if (dtb)
      free(dtb->trailer);
   free(dtb);
   return NULL;

}

DTBLOB_T *dtoverlay_load_dtb_from_fp(FILE *fp, int max_size)
{
   DTBLOB_T *dtb = NULL;
   void *fdt = NULL;

   if (fp)
   {
      long len;
      long bytes_read;
      int dtb_len;

      fseek(fp, 0, SEEK_END);
      len = ftell(fp);
      fseek(fp, 0, SEEK_SET);
      if (max_size > 0)
      {
         if (max_size < len)
         {
            dtoverlay_error("file too large (%d bytes) for max_size", len);
            goto error_exit;
         }
      }
      else if (max_size < 0)
      {
         max_size = len - max_size;
      }
      else
      {
         max_size = len;
      }

      fdt = malloc(max_size);
      if (!fdt)
      {
         dtoverlay_error("out of memory");
         goto error_exit;
      }

      bytes_read = fread(fdt, 1, len, fp);
      fclose(fp);

      if (bytes_read != len)
      {
         dtoverlay_error("fread failed");
         goto error_exit;
      }

      // Record the total size before any expansion
      dtb_len = fdt_totalsize(fdt);

      dtb = dtoverlay_import_fdt(fdt, max_size);
      if (!dtb)
         goto error_exit;

      dtb->fdt_is_malloced = 1;

      if (len > dtb_len)
      {
         /* Load the trailer */
         dtb->trailer_len = len - dtb_len;
         dtb->trailer = malloc(dtb->trailer_len);
         if (!dtb->trailer)
         {
            dtoverlay_error("out of memory");
            goto error_exit;
         }
         dtb->trailer_is_malloced = 1;
         memcpy(dtb->trailer, (char *)fdt + dtb_len, dtb->trailer_len);
      }
   }

   return dtb;

error_exit:
   free(fdt);
   if (dtb)
      free(dtb->trailer);
   free(dtb);
   return NULL;
}

DTBLOB_T *dtoverlay_load_dtb(const char *filename, int max_size)
{
   FILE *fp = fopen(filename, "rb");
   if (fp)
      return dtoverlay_load_dtb_from_fp(fp, max_size);
   dtoverlay_error("Failed to open '%s'", filename);
   return NULL;
}

DTBLOB_T *dtoverlay_import_fdt(void *fdt, int buf_size)
{
   DTBLOB_T *dtb = NULL;
   int node_off;
   int dtb_len;
   int err;

   err = fdt_check_header(fdt);
   if (err != 0)
   {
      dtoverlay_error("not a valid FDT - err %d", err);
      goto error_exit;
   }

   dtb_len = fdt_totalsize(fdt);

   if (buf_size < dtb_len)
   {
      dtoverlay_error("fdt is too large");
      err = -FDT_ERR_NOSPACE;
      goto error_exit;
   }

   if (buf_size > dtb_len)
         fdt_set_totalsize(fdt, buf_size);

   dtb = calloc(1, sizeof(DTBLOB_T));
   if (!dtb)
   {
      dtoverlay_error("out of memory");
      goto error_exit;
   }

   dtb->fdt = fdt;
   dtb->max_phandle = 0; // Not a valid phandle

   // Find the minimum and maximum phandles, in case it is necessary to
   // relocate existing ones or create new ones.

   for (node_off = 0;
        node_off >= 0;
        node_off = fdt_next_node(fdt, node_off, NULL))
   {
      uint32_t phandle = fdt_get_phandle(fdt, node_off);
      if (phandle > dtb->max_phandle)
         dtb->max_phandle = phandle;
   }

error_exit:
   return dtb;
}

int dtoverlay_save_dtb(const DTBLOB_T *dtb, const char *filename)
{
   FILE *fp = fopen(filename, "wb");
   int err = 0;

   if (fp)
   {
      long len = fdt_totalsize(dtb->fdt);
      if (len != fwrite(dtb->fdt, 1, len, fp))
      {
         dtoverlay_error("fwrite failed");
         err = -2;
         goto error_exit;
      }
      if (dtb->trailer_len &&
          (fwrite(dtb->trailer, 1, dtb->trailer_len, fp) != dtb->trailer_len))
      {
         dtoverlay_error("fwrite failed");
         err = -2;
         goto error_exit;
      }

      dtoverlay_debug("Wrote %ld bytes to '%s'", len, filename);
      fclose(fp);
   }
   else
   {
      dtoverlay_debug("Failed to create '%s'", filename);
      err = -1;
   }

error_exit:
   return err;
}

int dtoverlay_extend_dtb(DTBLOB_T *dtb, int new_size)
{
   int size = fdt_totalsize(dtb->fdt);
   int err = 0;

   if (new_size < 0)
      new_size = size - new_size;

   if (new_size > size)
   {
      void *fdt;
      fdt = malloc(new_size);
      if (fdt)
      {
         memcpy(fdt, dtb->fdt, size);
         fdt_set_totalsize(fdt, new_size);

         if (dtb->fdt_is_malloced)
            free(dtb->fdt);

         dtb->fdt = fdt;
         dtb->fdt_is_malloced = 1;
      }
      else
      {
         err = -FDT_ERR_NOSPACE;
      }
   }
   else if (new_size < size)
   {
      /* Can't shrink it */
      err = -FDT_ERR_NOSPACE;
   }

   return err;
}

int dtoverlay_dtb_totalsize(DTBLOB_T *dtb)
{
   return fdt_totalsize(dtb->fdt);
}

void dtoverlay_pack_dtb(DTBLOB_T *dtb)
{
   fdt_pack(dtb->fdt);
}

void dtoverlay_free_dtb(DTBLOB_T *dtb)
{
   if (dtb)
   {
      if (dtb->fdt_is_malloced)
         free(dtb->fdt);
      if (dtb->trailer_is_malloced)
         free(dtb->trailer);
      free(dtb);
   }
}

int dtoverlay_find_phandle(DTBLOB_T *dtb, int phandle)
{
   return fdt_node_offset_by_phandle(dtb->fdt, phandle);
}

int dtoverlay_find_symbol(DTBLOB_T *dtb, const char *symbol_name)
{
   int symbols_off, path_len;
   const char *node_path;

   node_path = dtoverlay_get_alias(dtb, symbol_name);

   if (node_path)
   {
      path_len = strlen(node_path);
   }
   else
   {
      symbols_off = fdt_path_offset(dtb->fdt, "/__symbols__");

      if (symbols_off < 0)
      {
         dtoverlay_error("No symbols found");
         return -FDT_ERR_NOTFOUND;
      }

      node_path = fdt_getprop(dtb->fdt, symbols_off, symbol_name, &path_len);
      if (path_len < 0)
         return -FDT_ERR_NOTFOUND;
   }
   return fdt_path_offset_namelen(dtb->fdt, node_path, path_len);
}

int dtoverlay_find_matching_node(DTBLOB_T *dtb, const char **node_names,
                                 int pos)
{
   while (1)
   {
      const char *node_name;
      pos = fdt_next_node(dtb->fdt, pos, NULL);
      if (pos < 0)
         break;
      node_name = fdt_get_name(dtb->fdt, pos, NULL);
      if (node_name)
      {
         int i;
         for (i = 0; node_names[i]; i++)
         {
            const char *node = node_names[i];
            int matchlen = strlen(node);
            if ((strncmp(node_name, node, matchlen) == 0) &&
                ((node[matchlen] == '\0') ||
                 (node[matchlen] == '@')))
               return pos;
         }
      }
   }
   return -1;
}

int dtoverlay_node_is_enabled(DTBLOB_T *dtb, int pos)
{
   if (pos >= 0)
   {
      const void *prop = dtoverlay_get_property(dtb, pos, "status", NULL);
      if (prop &&
          ((strcmp((const char *)prop, "okay") == 0) ||
           (strcmp((const char *)prop, "ok") == 0)))
         return 1;
   }
   return 0;
}

const void *dtoverlay_get_property(DTBLOB_T *dtb, int pos, const char *prop_name, int *prop_len)
{
   return fdt_getprop(dtb->fdt, pos, prop_name, prop_len);
}

int dtoverlay_set_property(DTBLOB_T *dtb, int pos,
                           const char *prop_name, const void *prop, int prop_len)
{
   int err = fdt_setprop(dtb->fdt, pos, prop_name, prop, prop_len);
   if (err < 0)
      dtoverlay_error("Failed to set property '%s'", prop_name);
   return err;
}

const char *dtoverlay_get_alias(DTBLOB_T *dtb, const char *alias_name)
{
   int node_off;
   int prop_len;
   const char *alias;

   node_off = fdt_path_offset(dtb->fdt, "/aliases");

   alias = fdt_getprop(dtb->fdt, node_off, alias_name, &prop_len);
   if (alias && !prop_len)
       alias = "";
   return alias;
}

int dtoverlay_set_alias(DTBLOB_T *dtb, const char *alias_name, const char *value)
{
   int node_off;

   node_off = fdt_path_offset(dtb->fdt, "/aliases");
   if (node_off < 0)
       node_off = fdt_add_subnode(dtb->fdt, 0, "aliases");

   return fdt_setprop_string(dtb->fdt, node_off, alias_name, value);
}

void dtoverlay_set_logging_func(DTOVERLAY_LOGGING_FUNC *func)
{
    dtoverlay_logging_func = func;
}

void dtoverlay_enable_debug(int enable)
{
   dtoverlay_debug_enabled = enable;
}

void dtoverlay_error(const char *fmt, ...)
{
   va_list args;
   va_start(args, fmt);
   (*dtoverlay_logging_func)(DTOVERLAY_ERROR, fmt, args);
   va_end(args);
}

void dtoverlay_debug(const char *fmt, ...)
{
   va_list args;
   if (dtoverlay_debug_enabled)
   {
      va_start(args, fmt);
      (*dtoverlay_logging_func)(DTOVERLAY_DEBUG, fmt, args);
      va_end(args);
   }
}

static void dtoverlay_stdio_logging(dtoverlay_logging_type_t type,
                                    const char *fmt, va_list args)
{
   const char *type_str;

   switch (type)
   {
   case DTOVERLAY_ERROR:
      type_str = "error";
      break;

   case DTOVERLAY_DEBUG:
      type_str = "debug";
      break;

   default:
      type_str = "?";
   }

   fprintf(stderr, "DTOVERLAY[%s]: ", type_str);
   vfprintf(stderr, fmt, args);
   fprintf(stderr, "\n");
}
