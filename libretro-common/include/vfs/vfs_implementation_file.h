/* Copyright  (C) 2010-2020 The RetroArch team
*
* ---------------------------------------------------------------------------------------
* The following license statement only applies to this file (vfs_implementation_file.h).
* ---------------------------------------------------------------------------------------
*
* Permission is hereby granted, free of charge,
* to any person obtaining a copy of this software and associated documentation files (the "Software"),
* to deal in the Software without restriction, including without limitation the rights to
* use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
* and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
*
* The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
*
* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
* INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
* FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
* WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
* OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#ifndef __LIBRETRO_SDK_VFS_IMPLEMENTATION_FILE_H
#define __LIBRETRO_SDK_VFS_IMPLEMENTATION_FILE_H

/* The file handle's layout, for the VFS implementations only: its
 * atomics pull in platform headers (<windows.h> on MSVC) that vfs.h
 * keeps out of every file that opens a stream. */

#include <vfs/vfs.h>
#include <retro_atomic.h>

/* Windows positional I/O keeps a second handle in a pointer atomic; the
 * volatile backend (a PS2 TU forced onto it, built with MinGW in the
 * samples) has none, and positional I/O falls back there. */
#if defined(_WIN32) && !defined(RETRO_ATOMIC_BACKEND_VOLATILE)
#define VFS_HAVE_AT_HANDLE 1
#endif

RETRO_BEGIN_DECLS

#if !(defined(__WINRT__) && defined(__cplusplus_winrt))
#ifdef VFS_FRONTEND
struct retro_vfs_file_handle
#else
struct libretro_vfs_implementation_file
#endif
{
   int64_t size;
   /* where a stdio stream stands, kept here so a write need not ask
    * stdio to keep size; -1 when not known, and stdio is asked */
   int64_t stdio_pos;
   uint64_t mappos;
   uint64_t mapsize;
#if defined(_WIN32) && !defined(_XBOX) && !defined(HAVE_MMAP)
   /* mapped read-ahead: where the next hint is due, the last read's end */
   uint64_t ra_next;
   uint64_t ra_last;
#endif
   FILE *fp;
#ifdef _WIN32
   HANDLE fh;
   /* CreateFileMapping handle backing `mapped`; NULL when unmapped */
   HANDLE map_handle;
   /* A stream on a handle of its own (not VFS_WIN32_NO_NATIVE): its
    * position, the part of the file `buf` holds - read from it, or, with
    * ndirty set, written to it and not yet to the file - and an error */
   uint64_t npos;
   uint64_t nbuf_at;
   size_t   nbuf_len;
   int      nerr;
   int      ndirty;
#if !defined(_XBOX) && !defined(HAVE_MMAP)
   void *warm;   /* mapped read-ahead thread, NULL until used */
#endif
#endif
   char *buf;
   char* orig_path;
   uint8_t *mapped;
   int fd;
   unsigned hints;
   enum vfs_scheme scheme;
#ifdef HAVE_CDROM
   /* Allocated by retro_vfs_file_open_impl() only when the path
    * carries the cdrom:// scheme, and freed by
    * retro_vfs_file_close_impl(). It used to be an inline member at
    * the head of this struct, which is a poor trade: vfs_cdrom_t
    * embeds a 2352-byte sector cache, so every open file handle in a
    * build with HAVE_CDROM cost 2464 bytes instead of 72, and the
    * fields the read/seek paths actually touch (mappos, mapsize,
    * mapped, hints) were pushed past offset 2392 and split across two
    * cache lines instead of sharing one. Every consumer already gates
    * on 'scheme == VFS_SCHEME_CDROM' before reaching for it, so a
    * NULL here is unreachable on those paths. */
   vfs_cdrom_t *cdrom;
#endif
#ifdef HAVE_SMBCLIENT
   intptr_t smb_fh;
   intptr_t smb_ctx;
   intptr_t smb_slot;   /* pool slot held from open to close, 0 if private */
   intptr_t smb_prefetch; /* struct smb_prefetch *, read-only opens with threads */
   intptr_t smb_reopen;   /* struct smb_reopen *: how to open it again */
#endif
#ifdef HAVE_NFSCLIENT
   intptr_t nfs_fh;
   intptr_t nfs_ctx;
   intptr_t nfs_prefetch; /* struct nfs_prefetch *, read-only opens with threads */
#endif
#if defined(HAVE_CDROM) && defined(__APPLE__)
   void *iokit_plugin;   /* IOCFPlugInInterface ** */
   void *iokit_mmc;      /* MMCDeviceInterface ** */
#endif
   /* Positional I/O (retro_vfs_file_read_at_impl, _write_at_impl), from
    * any number of threads at once. append_end: where the next append
    * goes, plus one; 0 until the first. at_handle (Windows): a handle of
    * its own on the file, with a position of its own, made at the first
    * positional call; NULL until then. */
   retro_atomic_size_t append_end;
   /* Set by a positional write that wrote, so the size is asked of the
    * file next time it is wanted; cleared by the query before it asks */
   retro_atomic_int_t  size_grown;
#ifdef VFS_HAVE_AT_HANDLE
   retro_atomic_ptr_t  at_handle;
   /* at_handle is synchronous - Windows' cached-read fast path - and
    * held by one call at a time, at_sync_busy saying so; a call that
    * finds it held takes at_ovl, the same file opened for overlapped
    * I/O, whose calls Windows runs side by side. Each overlapped call
    * waits on an event of its own: one of at_ev, claimed by its bit in
    * at_ev_busy and made at first use, or a passing one when all are in
    * use. A native stream's own handle serves as its at_handle. */
   retro_atomic_ptr_t  at_ovl;
   HANDLE              at_ev[8];
   retro_atomic_int_t  at_ev_busy;
   retro_atomic_int_t  at_sync_busy;
#endif
};
#endif

RETRO_END_DECLS

#endif
