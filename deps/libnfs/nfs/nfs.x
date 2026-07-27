/*
Copyright (c) 2014, Ronnie Sahlberg
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this
   list of conditions and the following disclaimer. 
2. Redistributions in binary form must reproduce the above copyright notice,
   this list of conditions and the following disclaimer in the documentation
   and/or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT OWNER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

The views and conclusions contained in the software and documentation are those
of the authors and should not be interpreted as representing official policies, 
either expressed or implied, of the FreeBSD Project.
*/

/*
 * NFS v3 Definitions
 */
const NFS3_FHSIZE    = 64;    /* Maximum bytes in a V3 file handle */
const NFS3_WRITEVERFSIZE = 8;
const NFS3_CREATEVERFSIZE = 8;
const NFS3_COOKIEVERFSIZE = 8;

typedef opaque cookieverf3[NFS3_COOKIEVERFSIZE];

typedef uint64_t cookie3;

struct nfs_fh3 {
	opaque       data<NFS3_FHSIZE>;
};

typedef string filename3<>;

struct diropargs3 {
	nfs_fh3     dir;
	filename3   name;
};

enum ftype3 {
	NF3REG    = 1,
	NF3DIR    = 2,
	NF3BLK    = 3,
	NF3CHR    = 4,
	NF3LNK    = 5,
	NF3SOCK   = 6,
	NF3FIFO   = 7
};

typedef unsigned int mode3;

typedef unsigned int uid3;

typedef unsigned int gid3;

typedef uint64_t size3;

typedef uint64_t fileid3;

struct specdata3 {
	unsigned int specdata1;
	unsigned int specdata2;
};

struct nfstime3 {
	unsigned int seconds;
	unsigned int nseconds;
};

struct fattr3 {
	ftype3       type;
	mode3        mode;
	unsigned int nlink;
	uid3         uid;
	gid3         gid;
	size3        size;
	size3        used;
	specdata3    rdev;
	uint64_t     fsid;
	fileid3      fileid;
	nfstime3     atime;
	nfstime3     mtime;
	nfstime3     ctime;
};

union post_op_attr switch (bool attributes_follow) {
	case TRUE:
		fattr3   attributes;
	case FALSE:
		void;
};


enum nfsstat3 {
	NFS3_OK             = 0,
	NFS3ERR_PERM        = 1,
	NFS3ERR_NOENT       = 2,
	NFS3ERR_IO          = 5,
	NFS3ERR_NXIO        = 6,
	NFS3ERR_ACCES       = 13,
	NFS3ERR_EXIST       = 17,
	NFS3ERR_XDEV        = 18,
	NFS3ERR_NODEV       = 19,
	NFS3ERR_NOTDIR      = 20,
	NFS3ERR_ISDIR       = 21,
	NFS3ERR_INVAL       = 22,
	NFS3ERR_FBIG        = 27,
	NFS3ERR_NOSPC       = 28,
	NFS3ERR_ROFS        = 30,
	NFS3ERR_MLINK       = 31,
	NFS3ERR_NAMETOOLONG = 63,
	NFS3ERR_NOTEMPTY    = 66,
	NFS3ERR_DQUOT       = 69,
	NFS3ERR_STALE       = 70,
	NFS3ERR_REMOTE      = 71,
	NFS3ERR_BADHANDLE   = 10001,
	NFS3ERR_NOT_SYNC    = 10002,
	NFS3ERR_BAD_COOKIE  = 10003,
	NFS3ERR_NOTSUPP     = 10004,
	NFS3ERR_TOOSMALL    = 10005,
	NFS3ERR_SERVERFAULT = 10006,
	NFS3ERR_BADTYPE     = 10007,
	NFS3ERR_JUKEBOX     = 10008
};	

enum stable_how {
	UNSTABLE  = 0,
	DATA_SYNC = 1,
	FILE_SYNC = 2
};

typedef uint64_t offset3;

typedef unsigned int count3;

struct wcc_attr {
	size3       size;
	nfstime3    mtime;
	nfstime3    ctime;
};

union pre_op_attr switch (bool attributes_follow) {
	case TRUE:
		wcc_attr  attributes;
	case FALSE:
		void;
};

struct wcc_data {
	pre_op_attr    before;
	post_op_attr   after;
};

struct WRITE3args {
	nfs_fh3     file;
	offset3     offset;
	count3      count;
	stable_how  stable;
	opaque      data<>;
};

typedef opaque writeverf3[NFS3_WRITEVERFSIZE];

struct WRITE3resok {
	wcc_data    file_wcc;
	count3      count;
	stable_how  committed;
	writeverf3  verf;
};

struct WRITE3resfail {
	wcc_data    file_wcc;
};

union WRITE3res switch (nfsstat3 status) {
	case NFS3_OK:
		WRITE3resok    resok;
	default:
		WRITE3resfail  resfail;
};

struct LOOKUP3args {
	diropargs3  what;
};

struct LOOKUP3resok {
	nfs_fh3      object;
	post_op_attr obj_attributes;
	post_op_attr dir_attributes;
};

struct LOOKUP3resfail {
	post_op_attr dir_attributes;
};



union LOOKUP3res switch (nfsstat3 status) {
	case NFS3_OK:
		LOOKUP3resok    resok;
	default:
		LOOKUP3resfail  resfail;
};

struct COMMIT3args {
	nfs_fh3    file;
	offset3    offset;
	count3     count;
};

struct COMMIT3resok {
	wcc_data   file_wcc;
	writeverf3 verf;
};

struct COMMIT3resfail {
	wcc_data   file_wcc;
};

union COMMIT3res switch (nfsstat3 status) {
	case NFS3_OK:
		COMMIT3resok   resok;
	default:
		COMMIT3resfail resfail;
};

const ACCESS3_READ    = 0x0001;
const ACCESS3_LOOKUP  = 0x0002;
const ACCESS3_MODIFY  = 0x0004;
const ACCESS3_EXTEND  = 0x0008;
const ACCESS3_DELETE  = 0x0010;
const ACCESS3_EXECUTE = 0x0020;

struct ACCESS3args {
     nfs_fh3      object;
     unsigned int access;
};

struct ACCESS3resok {
     post_op_attr obj_attributes;
     unsigned int access;
};

struct ACCESS3resfail {
     post_op_attr   obj_attributes;
};

union ACCESS3res switch (nfsstat3 status) {
case NFS3_OK:
     ACCESS3resok   resok;
default:
     ACCESS3resfail resfail;
};

struct GETATTR3args {
	nfs_fh3  object;
};

struct GETATTR3resok {
        fattr3   obj_attributes;
};

union GETATTR3res switch (nfsstat3 status) {
	case NFS3_OK:
		GETATTR3resok  resok;
	default:
		void;
};



enum time_how {
	DONT_CHANGE        = 0,
	SET_TO_SERVER_TIME = 1,
	SET_TO_CLIENT_TIME = 2
};

union set_mode3 switch (bool set_it) {
	case TRUE:
		mode3    mode;
	default:
	void;
};

union set_uid3 switch (bool set_it) {
	case TRUE:
		uid3     uid;
	default:
		void;
};

union set_gid3 switch (bool set_it) {
	case TRUE:
		gid3     gid;
	default:
		void;
};

union set_size3 switch (bool set_it) {
	case TRUE:
		size3    size;
	default:
		void;
};

union set_atime switch (time_how set_it) {
	case SET_TO_CLIENT_TIME:
		nfstime3  atime;
	default:
		void;
};

union set_mtime switch (time_how set_it) {
	case SET_TO_CLIENT_TIME:
		nfstime3  mtime;
	default:
		void;
};

struct sattr3 {
	set_mode3   mode;
	set_uid3    uid;
	set_gid3    gid;
	set_size3   size;
	set_atime   atime;
	set_mtime   mtime;
};

enum createmode3 {
	UNCHECKED = 0,
	GUARDED   = 1,
	EXCLUSIVE = 2
};


typedef opaque createverf3[NFS3_CREATEVERFSIZE];

union createhow3 switch (createmode3 mode) {
	case UNCHECKED:
		sattr3       obj_attributes;
	case GUARDED:
		sattr3       g_obj_attributes;
	case EXCLUSIVE:
		createverf3  verf;
};

struct CREATE3args {
	diropargs3   where;
	createhow3   how;
};

union post_op_fh3 switch (bool handle_follows) {
	case TRUE:
		nfs_fh3  handle;
	case FALSE:
		void;
};

struct CREATE3resok {
	post_op_fh3   obj;
	post_op_attr  obj_attributes;
	wcc_data      dir_wcc;
};

struct CREATE3resfail {
	wcc_data      dir_wcc;
	};

union CREATE3res switch (nfsstat3 status) {
	case NFS3_OK:
		CREATE3resok    resok;
	default:
		CREATE3resfail  resfail;
};

struct REMOVE3args {
	diropargs3  object;
};

struct REMOVE3resok {
	wcc_data    dir_wcc;
};

struct REMOVE3resfail {
	wcc_data    dir_wcc;
};

union REMOVE3res switch (nfsstat3 status) {
	case NFS3_OK:
		REMOVE3resok   resok;
	default:
	REMOVE3resfail resfail;
};

struct READ3args {
	nfs_fh3  file;
	offset3  offset;
	count3   count;
};

struct READ3resok {
	post_op_attr   file_attributes;
	count3         count;
	bool           eof;
	opaque         data<>;
};

struct READ3resfail {
	post_op_attr   file_attributes;
};

union READ3res switch (nfsstat3 status) {
	case NFS3_OK:
		READ3resok   resok;
	default:
		READ3resfail resfail;
};


const FSF3_LINK        = 0x0001;
const FSF3_SYMLINK     = 0x0002;
const FSF3_HOMOGENEOUS = 0x0008;
const FSF3_CANSETTIME  = 0x0010;

struct FSINFO3args {
	nfs_fh3   fsroot;
};

struct FSINFO3resok {
	post_op_attr obj_attributes;
	unsigned int rtmax;
	unsigned int rtpref;
	unsigned int rtmult;
	unsigned int wtmax;
	unsigned int wtpref;
	unsigned int wtmult;
	unsigned int dtpref;
	size3        maxfilesize;
	nfstime3     time_delta;
	unsigned int properties;
};

struct FSINFO3resfail {
	post_op_attr obj_attributes;
};

union FSINFO3res switch (nfsstat3 status) {
	case NFS3_OK:
		FSINFO3resok   resok;
	default:
		FSINFO3resfail resfail;
};


struct FSSTAT3args {
	nfs_fh3   fsroot;
};

struct FSSTAT3resok {
	post_op_attr obj_attributes;
	size3        tbytes;
	size3        fbytes;
	size3        abytes;
	size3        tfiles;
	size3        ffiles;
	size3        afiles;
	unsigned int invarsec;
};

struct FSSTAT3resfail {
	post_op_attr obj_attributes;
};

union FSSTAT3res switch (nfsstat3 status) {
	case NFS3_OK:
		FSSTAT3resok   resok;
	default:
		FSSTAT3resfail resfail;
};

struct PATHCONF3args {
	nfs_fh3   object;
};

struct PATHCONF3resok {
	post_op_attr obj_attributes;
	unsigned int linkmax;
	unsigned int name_max;
	bool         no_trunc;
	bool         chown_restricted;
	bool         case_insensitive;
	bool         case_preserving;
};

struct PATHCONF3resfail {
	post_op_attr obj_attributes;
};

union PATHCONF3res switch (nfsstat3 status) {
	case NFS3_OK:
		PATHCONF3resok   resok;
	default:
		PATHCONF3resfail resfail;
};

typedef string nfspath3<>;

struct symlinkdata3 {
	sattr3    symlink_attributes;
	nfspath3  symlink_data;
};

struct SYMLINK3args {
	diropargs3    where;
	symlinkdata3  symlink;
};

struct SYMLINK3resok {
	post_op_fh3   obj;
	post_op_attr  obj_attributes;
	wcc_data      dir_wcc;
};

struct SYMLINK3resfail {
	wcc_data      dir_wcc;
};

union SYMLINK3res switch (nfsstat3 status) {
	case NFS3_OK:
		SYMLINK3resok   resok;
	default:
		SYMLINK3resfail resfail;
};


struct READLINK3args {
	nfs_fh3  symlink;
};

struct READLINK3resok {
	post_op_attr   symlink_attributes;
	nfspath3       data;
};

struct READLINK3resfail {
	post_op_attr   symlink_attributes;
};

union READLINK3res switch (nfsstat3 status) {
	case NFS3_OK:
		READLINK3resok   resok;
	default:
	READLINK3resfail resfail;
};


struct devicedata3 {
	sattr3     dev_attributes;
	specdata3  spec;
};

union mknoddata3 switch (ftype3 type) {
	case NF3CHR:
		devicedata3  chr_device;
	case NF3BLK:
		devicedata3  blk_device;
	case NF3SOCK:
		sattr3       sock_attributes;
	case NF3FIFO:
		sattr3       pipe_attributes;
	default:
		void;
};

struct MKNOD3args {
	diropargs3   where;
	mknoddata3   what;
};

struct MKNOD3resok {
	post_op_fh3   obj;
	post_op_attr  obj_attributes;
	wcc_data      dir_wcc;
};

struct MKNOD3resfail {
	wcc_data      dir_wcc;
};

union MKNOD3res switch (nfsstat3 status) {
	case NFS3_OK:
		MKNOD3resok   resok;
	default:
		MKNOD3resfail resfail;
};


struct MKDIR3args {
	diropargs3   where;
	sattr3       attributes;
};

struct MKDIR3resok {
	post_op_fh3   obj;
	post_op_attr  obj_attributes;
	wcc_data      dir_wcc;
};

struct MKDIR3resfail {
	wcc_data      dir_wcc;
};

union MKDIR3res switch (nfsstat3 status) {
	case NFS3_OK:
		MKDIR3resok   resok;
	default:
		MKDIR3resfail resfail;
};

struct RMDIR3args {
	diropargs3  object;
};

struct RMDIR3resok {
	wcc_data    dir_wcc;
};

struct RMDIR3resfail {
	wcc_data    dir_wcc;
};

union RMDIR3res switch (nfsstat3 status) {
	case NFS3_OK:
		RMDIR3resok   resok;
	default:
		RMDIR3resfail resfail;
};

struct RENAME3args {
	diropargs3   from;
	diropargs3   to;
};

struct RENAME3resok {
	wcc_data     fromdir_wcc;
	wcc_data     todir_wcc;
};

struct RENAME3resfail {
	wcc_data     fromdir_wcc;
	wcc_data     todir_wcc;
};

union RENAME3res switch (nfsstat3 status) {
	case NFS3_OK:
		RENAME3resok   resok;
	default:
		RENAME3resfail resfail;
};

struct READDIRPLUS3args {
	nfs_fh3      dir;
	cookie3      cookie;
	cookieverf3  cookieverf;
	count3       dircount;
	count3       maxcount;
};

struct entryplus3 {
	fileid3      fileid;
	filename3    name;
	cookie3      cookie;
	post_op_attr name_attributes;
	post_op_fh3  name_handle;
	entryplus3   *nextentry;
};

struct dirlistplus3 {
	entryplus3   *entries;
	bool         eof;
};

struct READDIRPLUS3resok {
	post_op_attr dir_attributes;
	cookieverf3  cookieverf;
	dirlistplus3 reply;
};


struct READDIRPLUS3resfail {
	post_op_attr dir_attributes;
};

union READDIRPLUS3res switch (nfsstat3 status) {
	case NFS3_OK:
		READDIRPLUS3resok   resok;
	default:
		READDIRPLUS3resfail resfail;
};

struct READDIR3args {
	nfs_fh3      dir;
	cookie3      cookie;
	cookieverf3  cookieverf;
	count3       count;
};


struct entry3 {
	fileid3      fileid;
	filename3    name;
	cookie3      cookie;
	entry3       *nextentry;
};

struct dirlist3 {
	entry3	*entries;
	bool    eof;
};

struct READDIR3resok {
	post_op_attr dir_attributes;
	cookieverf3  cookieverf;
	dirlist3     reply;
};

struct READDIR3resfail {
	post_op_attr dir_attributes;
};

union READDIR3res switch (nfsstat3 status) {
	case NFS3_OK:
		READDIR3resok   resok;
	default:
		READDIR3resfail resfail;
};

struct LINK3args {
	nfs_fh3     file;
	diropargs3  link;
};

struct LINK3resok {
	post_op_attr   file_attributes;
	wcc_data       linkdir_wcc;
};

struct LINK3resfail {
	post_op_attr   file_attributes;
	wcc_data       linkdir_wcc;
};

union LINK3res switch (nfsstat3 status) {
	case NFS3_OK:
		LINK3resok    resok;
	default:
		LINK3resfail  resfail;
};

union sattrguard3 switch (bool check) {
	case TRUE:
		nfstime3  obj_ctime;
	case FALSE:
		void;
};

struct SETATTR3args {
	nfs_fh3      object;
	sattr3       new_attributes;
	sattrguard3  guard;
};

struct SETATTR3resok {
	wcc_data  obj_wcc;
};

struct SETATTR3resfail {
	wcc_data  obj_wcc;
};

union SETATTR3res switch (nfsstat3 status) {
	case NFS3_OK:
		SETATTR3resok   resok;
	default:
		SETATTR3resfail resfail;
};

/*
 * NFS v2 Definitions
 * We share many definitions from v3
 */
const FHSIZE2 = 32;
typedef opaque fhandle2[FHSIZE2];

enum ftype2 {
	NF2NON    = 0,
	NF2REG    = 1,
	NF2DIR    = 2,
	NF2BLK    = 3,
	NF2CHR    = 4,
	NF2LNK    = 5
};

struct fattr2 {
	ftype2       type;
	unsigned int mode;
	unsigned int nlink;
	unsigned int uid;
	unsigned int gid;
	unsigned int size;
	unsigned int blocksize;
	unsigned int rdev;
	unsigned int blocks;
	unsigned int fsid;
	unsigned int fileid;
	nfstime3 atime;
	nfstime3 mtime;
	nfstime3 ctime;
};

struct sattr2 {
	unsigned int mode;
	unsigned int uid;
	unsigned int gid;
	unsigned int size;
	nfstime3     atime;
	nfstime3     mtime;
};

const MAXNAMLEN2 = 255;
typedef string filename2<MAXNAMLEN2>;

const MAXPATHLEN2 = 1024;
typedef string path2<MAXPATHLEN2>;

const NFSMAXDATA2 = 8192;
typedef opaque nfsdata2<NFSMAXDATA2>;

const NFSCOOKIESIZE2 = 4;
typedef opaque nfscookie2[NFSCOOKIESIZE2];

struct entry2 {
	unsigned int fileid;
	filename2 name;
	nfscookie2 cookie;
	entry2 *nextentry;
};

struct diropargs2 {
	fhandle2  dir;
	filename2 name;
};

struct GETATTR2args {
	fhandle2 fhandle;
};

struct GETATTR2resok {
	fattr2 attributes;
};

union GETATTR2res switch (nfsstat3 status) {
	case NFS3_OK:
		GETATTR2resok resok;
	default:
		void;
};

struct SETATTR2args {
	fhandle2 fhandle;
        sattr2 attributes;
};

struct SETATTR2resok {
	fattr2 attributes;
};

union SETATTR2res switch (nfsstat3 status) {
	case NFS3_OK:
		SETATTR2resok resok;
	default:
		void;
};

struct LOOKUP2args {
	diropargs2 what;
};

struct LOOKUP2resok {
	fhandle2 file;
	fattr2   attributes;
};

union LOOKUP2res switch (nfsstat3 status) {
	case NFS3_OK:
		LOOKUP2resok resok;
	default:
		void;
};

struct READLINK2args {
	fhandle2 file;
};

struct READLINK2resok {
	path2 data;
};

union READLINK2res switch (nfsstat3 status) {
	case NFS3_OK:
		READLINK2resok resok;
	default:
		void;
};

struct READ2args {
	fhandle2 file;
	unsigned int offset;
	unsigned int count;
	unsigned int totalcount;
};

struct READ2resok {
	fattr2   attributes;
	nfsdata2 data;
};

union READ2res switch (nfsstat3 status) {
	case NFS3_OK:
		READ2resok resok;
	default:
		void;
};

struct WRITE2args {
	fhandle2 file;
	unsigned int beginoffset;
	unsigned int offset;
	unsigned int totalcount;
	nfsdata2 data;
};

struct WRITE2resok {
	fattr2 attributes;
};

union WRITE2res switch (nfsstat3 status) {
	case NFS3_OK:
		WRITE2resok resok;
	default:
		void;
};

struct CREATE2args {
	diropargs2 where;
        sattr2 attributes;
};

struct CREATE2resok {
	fhandle2 file;
	fattr2   attributes;
};

union CREATE2res switch (nfsstat3 status) {
	case NFS3_OK:
		CREATE2resok resok;
	default:
		void;
};

struct REMOVE2args {
	diropargs2 what;
};

struct REMOVE2res {
	nfsstat3 status;
};

struct RENAME2args {
	diropargs2 from;
	diropargs2 to;
};

struct RENAME2res {
	nfsstat3 status;
};

struct LINK2args {
	fhandle2 from;
	diropargs2 to;
};

struct LINK2res {
	nfsstat3 status;
};

struct SYMLINK2args {
	diropargs2 from;
	path2 to;
        sattr2 attributes;
};

struct SYMLINK2res {
	nfsstat3 status;
};

struct MKDIR2args {
	diropargs2 where;
        sattr2 attributes;
};

struct MKDIR2resok {
	fhandle2 file;
	fattr2   attributes;
};

union MKDIR2res switch (nfsstat3 status) {
	case NFS3_OK:
		MKDIR2resok resok;
	default:
		void;
};

struct RMDIR2args {
	diropargs2 what;
};

struct RMDIR2res {
	nfsstat3 status;
};

struct READDIR2args {
	fhandle2 dir;
	nfscookie2 cookie;
	unsigned int count;
};

struct READDIR2resok {
	entry2 *entries;
	bool    eof;
};

union READDIR2res switch (nfsstat3 status) {
	case NFS3_OK:
		READDIR2resok resok;
	default:
		void;
};

struct STATFS2args {
	fhandle2 dir;
};

struct STATFS2resok {
	unsigned int tsize;
	unsigned int bsize;
	unsigned int blocks;
	unsigned int bfree;
	unsigned int bavail;
};

union STATFS2res switch (nfsstat3 status) {
	case NFS3_OK:
		STATFS2resok resok;
	default:
		void;
};

program NFS_PROGRAM {
	version NFS_V2 {
		void
		NFS2_NULL(void)                    = 0;

		GETATTR2res
		NFS2_GETATTR(GETATTR2args)         = 1;

		SETATTR2res
		NFS2_SETATTR(SETATTR2args)         = 2;

		LOOKUP2res
		NFS2_LOOKUP(LOOKUP2args)           = 4;

		READLINK2res
		NFS2_READLINK(READLINK2args)       = 5;

		READ2res
		NFS2_READ(READ2args)               = 6;

		WRITE2res
		NFS2_WRITE(WRITE2args)             = 8;

		CREATE2res
		NFS2_CREATE(CREATE2args)           = 9;

		REMOVE2res
		NFS2_REMOVE(REMOVE2args)           = 10;

		RENAME2res
		NFS2_RENAME(RENAME2args)           = 11;

		LINK2res
		NFS2_LINK(LINK2args)               = 12;

		SYMLINK2res
		NFS2_SYMLINK(SYMLINK2args)         = 13;

		MKDIR2res
		NFS2_MKDIR(MKDIR2args)             = 14;

		RMDIR2res
		NFS2_RMDIR(RMDIR2args)             = 15;

		READDIR2res
		NFS2_READDIR(READDIR2args)         = 16;

		STATFS2res
		NFS2_STATFS(STATFS2args)           = 17;
	} = 2;

	version NFS_V3 {
		void
		NFS3_NULL(void)                    = 0;

		GETATTR3res
		NFS3_GETATTR(GETATTR3args)         = 1;

		SETATTR3res
		NFS3_SETATTR(SETATTR3args)         = 2;

		LOOKUP3res
		NFS3_LOOKUP(LOOKUP3args)           = 3;

		ACCESS3res
		NFS3_ACCESS(ACCESS3args)           = 4;

		READLINK3res
		NFS3_READLINK(READLINK3args)       = 5;

		READ3res
		NFS3_READ(READ3args)               = 6;

		WRITE3res
		NFS3_WRITE(WRITE3args)             = 7;

		CREATE3res
		NFS3_CREATE(CREATE3args)           = 8;

		MKDIR3res
		NFS3_MKDIR(MKDIR3args)             = 9;

		SYMLINK3res
		NFS3_SYMLINK(SYMLINK3args)         = 10;

		MKNOD3res
		NFS3_MKNOD(MKNOD3args)             = 11;

		REMOVE3res
		NFS3_REMOVE(REMOVE3args)           = 12;

		RMDIR3res
		NFS3_RMDIR(RMDIR3args)             = 13;

		RENAME3res
		NFS3_RENAME(RENAME3args)           = 14;

		LINK3res
		NFS3_LINK(LINK3args)               = 15;

		READDIR3res
		NFS3_READDIR(READDIR3args)         = 16;

		READDIRPLUS3res
		NFS3_READDIRPLUS(READDIRPLUS3args) = 17;

		FSSTAT3res
		NFS3_FSSTAT(FSSTAT3args)           = 18;

		FSINFO3res
		NFS3_FSINFO(FSINFO3args)           = 19;

		PATHCONF3res
		NFS3_PATHCONF(PATHCONF3args)       = 20;

		COMMIT3res
		NFS3_COMMIT(COMMIT3args)           = 21;
	} = 3;
} = 100003;



/* NFS ACL definitions based on wireshark souces and network traces */
/* NFSACL interface. Uses same port/process as NFS */

enum nfsacl_type {
     NFSACL_TYPE_USER_OBJ	   = 0x0001,
     NFSACL_TYPE_USER		   = 0x0002,
     NFSACL_TYPE_GROUP_OBJ	   = 0x0004,
     NFSACL_TYPE_GROUP		   = 0x0008,
     NFSACL_TYPE_CLASS_OBJ	   = 0x0010,
     NFSACL_TYPE_CLASS		   = 0x0020,
     NFSACL_TYPE_DEFAULT	   = 0x1000,
     NFSACL_TYPE_DEFAULT_USER_OBJ  = 0x1001, 
     NFSACL_TYPE_DEFAULT_USER      = 0x1002,
     NFSACL_TYPE_DEFAULT_GROUP_OBJ = 0x1004,
     NFSACL_TYPE_DEFAULT_GROUP     = 0x1008,
     NFSACL_TYPE_DEFAULT_CLASS_OBJ = 0x1010,
     NFSACL_TYPE_DEFAULT_OTHER_OBJ = 0x1020
};

const NFSACL_PERM_READ  = 0x04;
const NFSACL_PERM_WRITE = 0x02;
const NFSACL_PERM_EXEC  = 0x01;

struct nfsacl_ace {
       enum nfsacl_type type;
       unsigned int id;
       unsigned int perm;
};

const NFSACL_MASK_ACL_ENTRY         = 0x0001;
const NFSACL_MASK_ACL_COUNT         = 0x0002;
const NFSACL_MASK_ACL_DEFAULT_ENTRY = 0x0004;
const NFSACL_MASK_ACL_DEFAULT_COUNT = 0x0008;

struct GETACL3args {
	nfs_fh3      dir;
	unsigned int mask;
};

struct GETACL3resok {
	post_op_attr       attr;
	unsigned int       mask;
	unsigned int       ace_count;
	struct nfsacl_ace  ace<>;
	unsigned int       default_ace_count;
	struct nfsacl_ace  default_ace<>;
};

union GETACL3res switch (nfsstat3 status) {
case NFS3_OK:
     GETACL3resok   resok;
default:
     void;
};

struct SETACL3args {
	nfs_fh3            dir;
	unsigned int       mask;
	unsigned int       ace_count;
	struct nfsacl_ace  ace<>;
	unsigned int       default_ace_count;
	struct nfsacl_ace  default_ace<>;
};

struct SETACL3resok {
	post_op_attr   attr;
};

union SETACL3res switch (nfsstat3 status) {
case NFS3_OK:
     SETACL3resok   resok;
default:
     void;
};

program NFSACL_PROGRAM {
	version NFSACL_V3 {
		void
		NFSACL3_NULL(void)                    = 0;

		GETACL3res
		NFSACL3_GETACL(GETACL3args)           = 1;

		SETACL3res
		NFSACL3_SETACL(SETACL3args)           = 2;
	} = 3;
} = 100227;

