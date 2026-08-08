/* -*-  mode:c; tab-width:8; c-basic-offset:8; indent-tabs-mode:nil;  -*- */
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

const PMAP_PORT = 111;      /* portmapper port number */

struct pmap2_mapping {
       uint32_t prog;
       uint32_t vers;
       uint32_t prot;
       uint32_t port;
};

struct pmap2_call_args {
       uint32_t prog;
       uint32_t vers;
       uint32_t proc;
       opaque args<>;
};

struct pmap2_call_result {
	uint32_t port;
	opaque res<>;
};

struct pmap2_mapping_list {
       pmap2_mapping map;
       pmap2_mapping_list *next;
};

struct pmap2_dump_result {
       struct pmap2_mapping_list *list;
};

struct pmap3_string_result {
       string addr<>;
};

struct pmap3_mapping {
       uint32_t prog;
       uint32_t vers;
       string netid<>;
       string addr<>;
       string owner<>;
};

struct pmap3_mapping_list {
       pmap3_mapping map;
       pmap3_mapping_list *next;
};

struct pmap3_dump_result {
       struct pmap3_mapping_list *list;
};

struct pmap3_call_args {
       uint32_t prog;
       uint32_t vers;
       uint32_t proc;
       opaque args<>;
};

struct rpcb_rmtcallres {
       string addr<>;
       opaque results<>;
};

struct pmap3_netbuf {
	uint32_t maxlen;
	/* This pretty much contains a sockaddr_storage.
	 * Beware differences in endianess for ss_family
	 * and whether or not ss_len exists.
	 */
	opaque buf<>;
};

struct pmap4_string_result {
       string addr<>;
};

struct pmap4_mapping {
       uint32_t prog;
       uint32_t vers;
       string netid<>;
       string addr<>;
       string owner<>;
};

struct pmap4_mapping_list {
       pmap4_mapping map;
       pmap4_mapping_list *next;
};

struct pmap4_dump_result {
       struct pmap4_mapping_list *list;
};

struct pmap4_bcast_args {
       uint32_t prog;
       uint32_t vers;
       uint32_t proc;
       opaque args<>;
};

struct pmap4_netbuf {
	uint32_t maxlen;
	/* This pretty much contains a sockaddr_storage.
	 * Beware differences in endianess for ss_family
	 * and whether or not ss_len exists.
	 */
	opaque buf<>;
};

struct pmap4_indirect_args {
       uint32_t prog;
       uint32_t vers;
       uint32_t proc;
       opaque args<>;
};

const RPCBSTAT_HIGHPROC = 13; /* # of procs in rpcbind V4 plus one */
const RPCBVERS_STAT     = 3; /* provide only for rpcbind V2, V3 and V4 */
const RPCBVERS_4_STAT   = 2;
const RPCBVERS_3_STAT   = 1;
const RPCBVERS_2_STAT   = 0;

struct rpcbs_addrlist {
	uint32_t prog;
	uint32_t vers;
	int32_t success;
	int32_t failure;
	string netid<>;
	struct rpcbs_addrlist *next;
};

struct rpcbs_rmtcalllist {
	uint32_t prog;
	uint32_t vers;
	uint32_t proc;
	int32_t success;
	int32_t failure;
	int32_t indirect;    /* whether callit or indirect */
	string netid<>;
	struct rpcbs_rmtcalllist *next;
};

typedef int rpcbs_proc[RPCBSTAT_HIGHPROC];
typedef rpcbs_addrlist *rpcbs_addrlist_ptr;
typedef rpcbs_rmtcalllist *rpcbs_rmtcalllist_ptr;

struct rpcb_stat {
	rpcbs_proc              info;
	int32_t                 setinfo;
	int32_t                 unsetinfo;
	rpcbs_addrlist_ptr      addrinfo;
	rpcbs_rmtcalllist_ptr   rmtinfo;
};

typedef rpcb_stat pmap4_stat_byvers[RPCBVERS_STAT];

/* Semantics */
const NC_TPI_CLTS     = 1;
const NC_TPI_COTS     = 2;
const NC_TPI_COTS_ORD = 3;
const NC_TPI_RAW      = 4;

struct rpcb_entry {
	string          r_maddr<>;            /* merged address of service */
	string          r_nc_netid<>;         /* netid field */
	uint32_t        r_nc_semantics;       /* semantics of transport */
	string          r_nc_protofmly<>;     /* protocol family */
	string          r_nc_proto<>;         /* protocol name */
};

struct rpcb_entry_list {
	rpcb_entry rpcb_entry_map;
	struct rpcb_entry_list *next;
};

typedef rpcb_entry_list *pmap4_entry_list_ptr;


typedef pmap2_mapping     PMAP2SETargs;
typedef pmap2_mapping     PMAP2UNSETargs;
typedef pmap2_mapping     PMAP2GETPORTargs;
typedef pmap2_call_args   PMAP2CALLITargs;
typedef pmap2_call_result PMAP2CALLITres;
typedef pmap2_dump_result PMAP2DUMPres;

typedef pmap3_mapping       PMAP3SETargs;
typedef pmap3_mapping       PMAP3UNSETargs;
typedef pmap3_mapping       PMAP3GETADDRargs;
typedef pmap3_string_result PMAP3GETADDRres;
typedef pmap3_dump_result   PMAP3DUMPres;
typedef pmap3_call_args     PMAP3CALLITargs;
typedef rpcb_rmtcallres     PMAP3CALLITres;
typedef pmap3_string_result PMAP3UADDR2TADDRargs;
typedef pmap3_netbuf        PMAP3UADDR2TADDRres;
typedef pmap3_netbuf        PMAP3TADDR2UADDRargs;
typedef pmap3_string_result PMAP3TADDR2UADDRres;

typedef pmap4_mapping         PMAP4SETargs;
typedef pmap4_mapping         PMAP4UNSETargs;
typedef pmap4_mapping         PMAP4GETADDRargs;
typedef pmap4_string_result   PMAP4GETADDRres;
typedef pmap4_dump_result     PMAP4DUMPres;
typedef pmap4_bcast_args      PMAP4BCASTargs;
typedef rpcb_rmtcallres       PMAP4BCASTres;
typedef pmap3_string_result   PMAP4UADDR2TADDRargs;
typedef pmap4_netbuf          PMAP4UADDR2TADDRres;
typedef pmap4_netbuf          PMAP4TADDR2UADDRargs;
typedef pmap4_string_result   PMAP4TADDR2UADDRres;
typedef pmap4_mapping         PMAP4GETVERSADDRargs;
typedef pmap4_string_result   PMAP4GETVERSADDRres;
typedef pmap4_indirect_args   PMAP4INDIRECTargs;
typedef rpcb_rmtcallres       PMAP4INDIRECTres;
typedef pmap4_stat_byvers     PMAP4GETSTATres;
typedef pmap4_mapping         PMAP4GETADDRLISTargs;
typedef pmap4_entry_list_ptr  PMAP4GETADDRLISTres;


program PMAP_PROGRAM {
	version PMAP_V2 {
        	void
		PMAP2_NULL(void)              = 0;

		uint32_t
		PMAP2_SET(PMAP2SETargs)       = 1;

		uint32_t
		PMAP2_UNSET(PMAP2UNSETargs)   = 2;

		uint32_t
		PMAP2_GETPORT(PMAP2GETPORTargs) = 3;

		PMAP2DUMPres
		PMAP2_DUMP(void)               = 4;

		PMAP2CALLITres
		PMAP2_CALLIT(PMAP2CALLITargs)  = 5;
	} = 2;
	version PMAP_V3 {
        	void
		PMAP3_NULL(void)              = 0;

		uint32_t
		PMAP3_SET(PMAP3SETargs)       = 1;

		uint32_t
		PMAP3_UNSET(PMAP3UNSETargs)   = 2;

		PMAP3GETADDRres
		PMAP3_GETADDR(PMAP3GETADDRargs) = 3;

		PMAP3DUMPres
		PMAP3_DUMP(void)              = 4;

		PMAP3CALLITres
		PMAP3_CALLIT(PMAP3CALLITargs) = 5;

		uint32_t
		PMAP3_GETTIME(void)           = 6;

		PMAP3UADDR2TADDRres
		PMAP3_UADDR2TADDR(PMAP3UADDR2TADDRargs) = 7;

		PMAP3TADDR2UADDRres
		PMAP3_TADDR2UADDR(PMAP3TADDR2UADDRargs) = 8;
	} = 3;
	version PMAP_V4 {
		void
		PMAP4_NULL(void)              = 0;

		uint32_t
		PMAP4_SET(PMAP4SETargs)      = 1;

		uint32_t
		PMAP4_UNSET(PMAP4UNSETargs)    = 2;

		PMAP4GETADDRres
		PMAP4_GETADDR(PMAP4GETADDRargs) = 3;

		PMAP4DUMPres
		PMAP4_DUMP(void)              = 4;

		PMAP4BCASTres
		PMAP4_BCAST(PMAP4BCASTargs)  = 5;

		uint32_t
		PMAP4_GETTIME(void)           = 6;

		PMAP4UADDR2TADDRres
		PMAP4_UADDR2TADDR(PMAP4UADDR2TADDRargs) = 7;

		PMAP4TADDR2UADDRres
		PMAP4_TADDR2UADDR(PMAP4TADDR2UADDRargs) = 8;

		PMAP4GETVERSADDRres
		PMAP4_GETVERSADDR(PMAP4GETVERSADDRargs) = 9;
                
		PMAP4INDIRECTres
		PMAP4_INDIRECT(PMAP4INDIRECTargs)  = 10;

                PMAP4GETADDRLISTres
		PMAP4_GETADDRLIST(PMAP4GETADDRLISTargs) = 11;

                PMAP4GETSTATres
		PMAP4_GETSTAT(void)           = 12;
	} = 4;
} = 100000;

