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

const MNTPATHLEN = 1024;  /* Maximum bytes in a path name */
const MNTNAMLEN  = 255;   /* Maximum bytes in a name */
const FHSIZE3    = 64;    /* Maximum bytes in a V3 file handle */


typedef opaque fhandle3<FHSIZE3>;
typedef string dirpath<MNTPATHLEN>;
typedef string name<MNTNAMLEN>;

enum mountstat3 {
	MNT3_OK = 0,                 /* no error */
	MNT3ERR_PERM = 1,            /* Not owner */
	MNT3ERR_NOENT = 2,           /* No such file or directory */
	MNT3ERR_IO = 5,              /* I/O error */
	MNT3ERR_ACCES = 13,          /* Permission denied */
	MNT3ERR_NOTDIR = 20,         /* Not a directory */
	MNT3ERR_INVAL = 22,          /* Invalid argument */
	MNT3ERR_NAMETOOLONG = 63,    /* Filename too long */
	MNT3ERR_NOTSUPP = 10004,     /* Operation not supported */
	MNT3ERR_SERVERFAULT = 10006  /* A failure on the server */
};


typedef struct mountbody *mountlist;

struct mountbody {
	name       ml_hostname;
	dirpath    ml_directory;
	mountlist  ml_next;
};

typedef struct groupnode *groups;

struct groupnode {
	name     gr_name;
	groups   gr_next;
};

typedef struct exportnode *exports;

struct exportnode {
	dirpath  ex_dir;
	groups   ex_groups;
	exports  ex_next;
};

struct mountres3_ok {
	fhandle3   fhandle;
	int        auth_flavors<>;
};

union mountres3 switch (mountstat3 fhs_status) {
	case MNT3_OK:
		mountres3_ok  mountinfo;
	default:
		void;
};


enum mountstat1 {
	MNT1_OK = 0,                 /* no error */
	MNT1ERR_PERM = 1,            /* Not owner */
	MNT1ERR_NOENT = 2,           /* No such file or directory */
	MNT1ERR_IO = 5,              /* I/O error */
	MNT1ERR_ACCES = 13,          /* Permission denied */
	MNT1ERR_NOTDIR = 20,         /* Not a directory */
	MNT1ERR_INVAL = 22,          /* Invalid argument */
	MNT1ERR_NAMETOOLONG = 63,    /* Filename too long */
	MNT1ERR_NOTSUPP = 10004,     /* Operation not supported */
	MNT1ERR_SERVERFAULT = 10006  /* A failure on the server */
};

const FHSIZE = 32;
typedef opaque fhandle1[FHSIZE];

struct mountres1_ok {
	fhandle1   fhandle;
};

union mountres1 switch (mountstat1 fhs_status) {
	case MNT1_OK:
		mountres1_ok  mountinfo;
	default:
		void;
};

typedef dirpath MOUNT1MNTargs;
typedef mountres1 MOUNT1MNTres;
typedef mountbody MOUNT1DUMPres;
typedef mountlist MOUNT1DUMPres_ptr;
typedef dirpath MOUNT1UMNTargs;
typedef struct exportnode MOUNT1EXPORTres;
typedef struct exportnode *MOUNT1EXPORTres_ptr;

typedef dirpath MOUNT3MNTargs;
typedef mountres3 MOUNT3MNTres;
typedef mountbody MOUNT3DUMPres;
typedef mountlist MOUNT3DUMPres_ptr;
typedef dirpath MOUNT3UMNTargs;
typedef struct exportnode MOUNT3EXPORTres;
typedef struct exportnode *MOUNT3EXPORTres_ptr;

program MOUNT_PROGRAM {
	version MOUNT_V1 {
		void
		MOUNT1_NULL(void)           = 0;

		MOUNT1MNTres
		MOUNT1_MNT(MOUNT1MNTargs)   = 1;

		MOUNT1DUMPres
		MOUNT1_DUMP(void)           = 2;

		void
		MOUNT1_UMNT(MOUNT1UMNTargs) = 3;

		void
		MOUNT1_UMNTALL(void)        = 4;

		MOUNT1EXPORTres
		MOUNT1_EXPORT(void)         = 5;
	} = 1;
	version MOUNT_V3 {
		void
		MOUNT3_NULL(void)          = 0;

		MOUNT3MNTres
		MOUNT3_MNT(MOUNT3MNTargs)  = 1;

		MOUNT3DUMPres
		MOUNT3_DUMP(void)          = 2;

		void
		MOUNT3_UMNT(MOUNT3MNTargs) = 3;

		void
		MOUNT3_UMNTALL(void)       = 4;

		MOUNT3EXPORTres
		MOUNT3_EXPORT(void)        = 5;
	} = 3;
} = 100005;
