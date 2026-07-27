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

const RQUOTAPATHLEN = 1024; /* Guess this is max. It is max for mount so probably rquota too */

enum rquotastat {
     RQUOTA_OK		= 1,
     RQUOTA_NOQUOTA	= 2,
     RQUOTA_EPERM	= 3
};

typedef string exportpath<RQUOTAPATHLEN>;

struct GETQUOTA1args {
       exportpath export;
       int uid;
};

enum quotatype {
     RQUOTA_TYPE_UID = 0,
     RQUOTA_TYPE_GID = 1
};

struct GETQUOTA2args {
       exportpath export;
       quotatype type;
       int uid;
};

struct GETQUOTA1res_ok {
       int bsize;
       int active;
       int bhardlimit;
       int bsoftlimit;
       int curblocks;
       int fhardlimit;
       int fsoftlimit;
       int curfiles;
       int btimeleft;
       int ftimeleft;
};

union GETQUOTA1res switch (rquotastat status) {
      case RQUOTA_OK:
            GETQUOTA1res_ok quota;
      default:
            void;
};

program RQUOTA_PROGRAM {
	version RQUOTA_V1 {
		void
		RQUOTA1_NULL(void)                 = 0;

		GETQUOTA1res
		RQUOTA1_GETQUOTA(GETQUOTA1args)    = 1;

		GETQUOTA1res
		RQUOTA1_GETACTIVEQUOTA(GETQUOTA1args)    = 2;
	} = 1;

	version RQUOTA_V2 {
		void
		RQUOTA2_NULL(void)                 = 0;

		GETQUOTA1res
		RQUOTA2_GETQUOTA(GETQUOTA2args)    = 1;

		GETQUOTA1res
		RQUOTA2_GETACTIVEQUOTA(GETQUOTA2args)    = 2;
	} = 2;
} = 100011;

