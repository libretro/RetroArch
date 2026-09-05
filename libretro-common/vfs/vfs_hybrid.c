/*
===========================================================================
Hybrid VFS: frontend coverage without giving up direct I/O.

Generic - intended for libretro-common. A core adopts it with one
call at retro_set_environment:

    vfs_hybrid_init(environ_cb, log_cb);

which replaces the wholesale filestream_vfs_init(frontend) pattern.

The tension this resolves: platforms with sandboxed storage (Android
scoped storage / SAF content URIs) can only reach content through the
frontend's VFS callbacks - but frontend VFS handles are opaque, with
no file descriptor and therefore no mmap, and routing everything
through them would kill the mapped-pak architecture (in-place central
directories, stored-entry borrows, GetFileView, OGG borrows) plus add
an indirect call to every read on platforms that never needed it.

Resolution: dispatch per FILE, local-first.

 - Every open tries the LOCAL implementation first (libretro-common's
   own vfs_implementation, the same code the frontend compiles): a
   real fd, mmap intact, zero-copy intact, byte-identical to the
   pre-hybrid behavior.
 - The frontend interface is consulted only when the local open cannot
   possibly succeed or the platform is sandboxed: URI-shaped paths
   ("scheme://...") always go frontend; on sandboxed platforms a local
   failure falls through to the frontend. On desktop a plain-path miss
   NEVER touches the frontend, so the loose-file probe cost is exactly
   what it was.
 - Files that arrive on the frontend branch cannot be mapped - true at
   the OS level, not a limitation of this layer - and every mapped-
   path consumer already has a tested unmapped fallback: the mapped-
   ptr accessor answers NULL for frontend-backed handles and rzip,
   GetFileView, and the borrows degrade to copies for those files
   only.

Installed only when the frontend actually provides a VFS interface;
otherwise filestream keeps its direct local path with zero added
indirection.
===========================================================================
*/

#include <stdlib.h>
#include <string.h>

#include <libretro.h>
#include <vfs/vfs_implementation.h>
#include <streams/file_stream.h>
#include <file/file_path.h>
#include <retro_dirent.h>

enum { HYB_LOCAL = 1, HYB_FRONT = 2 };

typedef struct {
	int   be;
	void *h;          /* libretro_vfs_implementation_file* or frontend handle */
	char *path;
} hyb_file_t;

typedef struct {
	int   be;
	void *h;
} hyb_dir_t;

static struct retro_vfs_interface *hyb_front;
static uint32_t hyb_front_version;

/* A sandboxed platform tries the frontend after ANY local failure;
   elsewhere only URI-shaped paths reach the frontend, so plain-path
   misses cost nothing extra. Overridable per platform/port. */
#ifndef VFS_HYBRID_SANDBOXED
#if defined(ANDROID) || defined(__ANDROID__)
#define VFS_HYBRID_SANDBOXED 1
#else
#define VFS_HYBRID_SANDBOXED 0
#endif
#endif
#define HYB_SANDBOXED VFS_HYBRID_SANDBOXED

static int hyb_is_uri( const char *path ) {
	return path && strstr( path, "://" ) != NULL;
}

/* ---- file ops ---- */

static const char *hyb_get_path( struct retro_vfs_file_handle *fh ) {
	hyb_file_t *f = (hyb_file_t *)fh;
	if ( !f )
		return NULL;
	if ( f->be == HYB_FRONT && hyb_front->get_path )
		return hyb_front->get_path( (struct retro_vfs_file_handle *)f->h );
	return f->path;
}

static struct retro_vfs_file_handle *hyb_open( const char *path, unsigned mode, unsigned hints ) {
	hyb_file_t *f;
	void *h = NULL;
	int be = 0;

	if ( !path )
		return NULL;
	if ( !hyb_is_uri( path ) ) {
		h = retro_vfs_file_open_impl( path, mode, hints );
		if ( h )
			be = HYB_LOCAL;
	}
	if ( !h && hyb_front && hyb_front->open && ( hyb_is_uri( path ) || HYB_SANDBOXED ) ) {
		h = hyb_front->open( path, mode, hints );
		if ( h )
			be = HYB_FRONT;
	}
	if ( !h )
		return NULL;
	f = (hyb_file_t *)calloc( 1, sizeof( *f ) );
	if ( !f ) {
		if ( be == HYB_LOCAL )
			retro_vfs_file_close_impl( (libretro_vfs_implementation_file *)h );
		else
			hyb_front->close( (struct retro_vfs_file_handle *)h );
		return NULL;
	}
	f->be = be;
	f->h = h;
	/* strdup is POSIX, not C89: under a strict-C89 toolchain the
	   prototype is hidden and the implicit int return truncates the
	   pointer on LP64.  Copy manually with the headers we already
	   include. */
	{
		size_t path_len = strlen( path ) + 1;
		f->path = (char *)malloc( path_len );
		if ( f->path )
			memcpy( f->path, path, path_len );
	}
	return (struct retro_vfs_file_handle *)f;
}

static int hyb_close( struct retro_vfs_file_handle *fh ) {
	hyb_file_t *f = (hyb_file_t *)fh;
	int r;
	if ( !f )
		return -1;
	if ( f->be == HYB_LOCAL )
		r = retro_vfs_file_close_impl( (libretro_vfs_implementation_file *)f->h );
	else
		r = hyb_front->close( (struct retro_vfs_file_handle *)f->h );
	free( f->path );
	free( f );
	return r;
}

#define HYB_FWD1(name, ret) \
	static ret hyb_##name( struct retro_vfs_file_handle *fh ) { \
		hyb_file_t *f = (hyb_file_t *)fh; \
		if ( !f ) return (ret)-1; \
		if ( f->be == HYB_LOCAL ) \
			return retro_vfs_file_##name##_impl( (libretro_vfs_implementation_file *)f->h ); \
		return hyb_front->name( (struct retro_vfs_file_handle *)f->h ); \
	}

HYB_FWD1( size, int64_t )
HYB_FWD1( tell, int64_t )
HYB_FWD1( flush, int )

static int64_t hyb_truncate( struct retro_vfs_file_handle *fh, int64_t length ) {
	hyb_file_t *f = (hyb_file_t *)fh;
	if ( !f )
		return -1;
	if ( f->be == HYB_LOCAL )
		return retro_vfs_file_truncate_impl( (libretro_vfs_implementation_file *)f->h, length );
	/* truncate is v2. A frontend that advertised v1 has not filled
	   this member, so it must not be called - unlike the v3 members
	   below, which are unreachable by construction because a
	   frontend-backed dir handle can only be created when the
	   negotiated version is already 3. */
	if ( hyb_front_version < 2 || !hyb_front->truncate )
		return -1;
	return hyb_front->truncate( (struct retro_vfs_file_handle *)f->h, length );
}

static int64_t hyb_seek( struct retro_vfs_file_handle *fh, int64_t offset, int seek_position ) {
	hyb_file_t *f = (hyb_file_t *)fh;
	if ( !f )
		return -1;
	if ( f->be == HYB_LOCAL )
		return retro_vfs_file_seek_impl( (libretro_vfs_implementation_file *)f->h, offset, seek_position );
	return hyb_front->seek( (struct retro_vfs_file_handle *)f->h, offset, seek_position );
}

static int64_t hyb_read( struct retro_vfs_file_handle *fh, void *s, uint64_t len ) {
	hyb_file_t *f = (hyb_file_t *)fh;
	if ( !f )
		return -1;
	if ( f->be == HYB_LOCAL )
		return retro_vfs_file_read_impl( (libretro_vfs_implementation_file *)f->h, s, len );
	return hyb_front->read( (struct retro_vfs_file_handle *)f->h, s, len );
}

static int64_t hyb_write( struct retro_vfs_file_handle *fh, const void *s, uint64_t len ) {
	hyb_file_t *f = (hyb_file_t *)fh;
	if ( !f )
		return -1;
	if ( f->be == HYB_LOCAL )
		return retro_vfs_file_write_impl( (libretro_vfs_implementation_file *)f->h, s, len );
	return hyb_front->write( (struct retro_vfs_file_handle *)f->h, s, len );
}

static int hyb_remove( const char *path ) {
	if ( !hyb_is_uri( path ) ) {
		int r = retro_vfs_file_remove_impl( path );
		if ( r == 0 || !( hyb_front && HYB_SANDBOXED ) )
			return r;
	}
	if ( hyb_front && hyb_front->remove )
		return hyb_front->remove( path );
	return -1;
}

static int hyb_rename( const char *old_path, const char *new_path ) {
	if ( !hyb_is_uri( old_path ) && !hyb_is_uri( new_path ) ) {
		int r = retro_vfs_file_rename_impl( old_path, new_path );
		if ( r == 0 || !( hyb_front && HYB_SANDBOXED ) )
			return r;
	}
	if ( hyb_front && hyb_front->rename )
		return hyb_front->rename( old_path, new_path );
	return -1;
}

/* ---- v3: stat / mkdir / dirent ---- */

static int hyb_stat( const char *path, int32_t *size ) {
	if ( !hyb_is_uri( path ) ) {
		int r = retro_vfs_stat_impl( path, size );
		if ( r != 0 || !( hyb_front && HYB_SANDBOXED ) )
			return r;
	}
	if ( hyb_front && hyb_front_version >= 3 && hyb_front->stat )
		return hyb_front->stat( path, size );
	return 0;
}

/* ---- v4: 64-bit stat ---- */

static int hyb_stat_64( const char *path, int64_t *size ) {
	if ( !hyb_is_uri( path ) ) {
		int r = retro_vfs_stat_64_impl( path, size );
		if ( r != 0 || !( hyb_front && HYB_SANDBOXED ) )
			return r;
	}
	/* stat_64 is v4. A frontend that advertised less has not filled
	   this member, so it must not be called; sandboxed content on
	   such a frontend falls back to the 32-bit stat above, which the
	   callers of the 64-bit path already treat as best-effort. */
	if ( hyb_front && hyb_front_version >= 4 && hyb_front->stat_64 )
		return hyb_front->stat_64( path, size );
	if ( hyb_front && hyb_front_version >= 3 && hyb_front->stat ) {
		int32_t s32 = 0;
		int r = hyb_front->stat( path, size ? &s32 : NULL );
		if ( size )
			*size = (int64_t)s32;
		return r;
	}
	return 0;
}

static int hyb_mkdir( const char *dir ) {
	if ( !hyb_is_uri( dir ) ) {
		int r = retro_vfs_mkdir_impl( dir );
		if ( r != -1 || !( hyb_front && HYB_SANDBOXED ) )
			return r;
	}
	if ( hyb_front && hyb_front_version >= 3 && hyb_front->mkdir )
		return hyb_front->mkdir( dir );
	return -1;
}

static struct retro_vfs_dir_handle *hyb_opendir( const char *dir, bool include_hidden ) {
	hyb_dir_t *d;
	void *h = NULL;
	int be = 0;
	if ( !dir )
		return NULL;
	if ( !hyb_is_uri( dir ) ) {
		h = retro_vfs_opendir_impl( dir, include_hidden );
		if ( h )
			be = HYB_LOCAL;
	}
	if ( !h && hyb_front && hyb_front_version >= 3 && hyb_front->opendir
			&& ( hyb_is_uri( dir ) || HYB_SANDBOXED ) ) {
		h = hyb_front->opendir( dir, include_hidden );
		if ( h )
			be = HYB_FRONT;
	}
	if ( !h )
		return NULL;
	d = (hyb_dir_t *)calloc( 1, sizeof( *d ) );
	if ( !d ) {
		/* the backend handle is already open; hyb_open() releases its
		   own on this path and so must this one */
		if ( be == HYB_LOCAL )
			retro_vfs_closedir_impl( (libretro_vfs_implementation_dir *)h );
		else
			hyb_front->closedir( (struct retro_vfs_dir_handle *)h );
		return NULL;
	}
	d->be = be;
	d->h = h;
	return (struct retro_vfs_dir_handle *)d;
}

static bool hyb_readdir( struct retro_vfs_dir_handle *dh ) {
	hyb_dir_t *d = (hyb_dir_t *)dh;
	if ( !d )
		return false;
	if ( d->be == HYB_LOCAL )
		return retro_vfs_readdir_impl( (libretro_vfs_implementation_dir *)d->h );
	return hyb_front->readdir( (struct retro_vfs_dir_handle *)d->h );
}

static const char *hyb_dirent_get_name( struct retro_vfs_dir_handle *dh ) {
	hyb_dir_t *d = (hyb_dir_t *)dh;
	if ( !d )
		return NULL;
	if ( d->be == HYB_LOCAL )
		return retro_vfs_dirent_get_name_impl( (libretro_vfs_implementation_dir *)d->h );
	return hyb_front->dirent_get_name( (struct retro_vfs_dir_handle *)d->h );
}

static bool hyb_dirent_is_dir( struct retro_vfs_dir_handle *dh ) {
	hyb_dir_t *d = (hyb_dir_t *)dh;
	if ( !d )
		return false;
	if ( d->be == HYB_LOCAL )
		return retro_vfs_dirent_is_dir_impl( (libretro_vfs_implementation_dir *)d->h );
	return hyb_front->dirent_is_dir( (struct retro_vfs_dir_handle *)d->h );
}

static int hyb_closedir( struct retro_vfs_dir_handle *dh ) {
	hyb_dir_t *d = (hyb_dir_t *)dh;
	int r;
	if ( !d )
		return -1;
	if ( d->be == HYB_LOCAL )
		r = retro_vfs_closedir_impl( (libretro_vfs_implementation_dir *)d->h );
	else
		r = hyb_front->closedir( (struct retro_vfs_dir_handle *)d->h );
	free( d );
	return r;
}

/* the zero-copy sideband: local-backed handles expose their mapping,
   frontend-backed ones honestly cannot */
static const uint8_t *hyb_mapped_ptr( void *fh, int64_t *len ) {
	hyb_file_t *f = (hyb_file_t *)fh;
	if ( len )
		*len = 0;
	if ( !f || f->be != HYB_LOCAL )
		return NULL;
	return retro_vfs_file_get_mapped_ptr_impl( (libretro_vfs_implementation_file *)f->h, len );
}

static struct retro_vfs_interface hyb_iface = {
	/* v1 */
	hyb_get_path, hyb_open, hyb_close, hyb_size, hyb_tell, hyb_seek,
	hyb_read, hyb_write, hyb_flush, hyb_remove, hyb_rename,
	/* v2 */
	hyb_truncate,
	/* v3 */
	hyb_stat, hyb_mkdir, hyb_opendir, hyb_readdir,
	hyb_dirent_get_name, hyb_dirent_is_dir, hyb_closedir,
	/* v4 */
	hyb_stat_64
};

void vfs_hybrid_init( retro_environment_t env_cb, retro_log_printf_t log ) {
	struct retro_vfs_interface_info info;

	info.required_interface_version = 4;
	info.iface = NULL;
	if ( !env_cb( RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &info ) || !info.iface ) {
		info.required_interface_version = 3;
		info.iface = NULL;
	}
	if ( !info.iface && ( !env_cb( RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &info ) || !info.iface ) ) {
		info.required_interface_version = 1;
		info.iface = NULL;
		if ( !env_cb( RETRO_ENVIRONMENT_GET_VFS_INTERFACE, &info ) || !info.iface ) {
			/* no frontend VFS: keep filestream's direct local path,
			   zero added indirection - today's exact behavior */
			return;
		}
	}
	hyb_front = info.iface;
	hyb_front_version = info.required_interface_version;

	{
		struct retro_vfs_interface_info ours;
		ours.required_interface_version = 4;
		ours.iface = &hyb_iface;
		filestream_vfs_init( &ours );
		path_vfs_init( &ours );
		dirent_vfs_init( &ours );
	}
	filestream_set_mapped_ptr_cb( hyb_mapped_ptr );

	if ( log )
		log( RETRO_LOG_INFO,
			"[vfs] hybrid: local-first with frontend v%u fallback%s\n",
			hyb_front_version, HYB_SANDBOXED ? " (sandboxed platform: full fallback)" : " (URI paths only)" );
}
