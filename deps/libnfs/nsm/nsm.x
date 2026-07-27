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
 * This defines the maximum length of the string
 * identifying the caller.
 */
const NSM_MAXSTRLEN = 1024;

enum nsmstat1 {
    NSM_STAT_SUCC = 0,   /*  NSM agrees to monitor.  */
    NSM_STAT_FAIL = 1    /*  NSM cannot monitor.  */
};

struct nsm_my_id {
    string my_name<NSM_MAXSTRLEN>; /*  hostname  */
    int    my_prog;                /*  RPC program number  */
    int    my_vers;                /*  program version number  */
    int    my_proc;                /*  procedure number  */
};

struct nsm_mon_id {
    string mon_name<NSM_MAXSTRLEN>; /* name of the host to be monitored */
    struct nsm_my_id my_id;
};

struct NSM1_STATres {
    nsmstat1 res;
    int      state;
};

struct NSM1_STATargs {
    string mon_name<NSM_MAXSTRLEN>;
};

struct NSM1_MONres {
    nsmstat1 res;
    int      state;
};

struct NSM1_MONargs {
    struct nsm_mon_id mon_id;
    opaque priv[16];        /*  private information  */
};

struct NSM1_UNMONres {
    int state;    /*  state number of NSM  */
};

struct NSM1_UNMONargs {
    struct nsm_mon_id mon_id;
};

struct NSM1_UNMONALLres {
    int state;    /*  state number of NSM  */
};

struct NSM1_UNMONALLargs {
    struct nsm_my_id my_id;
};

struct NSM1_NOTIFYargs {
    string mon_name<NSM_MAXSTRLEN>;
    int    state;
};

/*
 *  Protocol description for the NSM program.
 */
program NSM_PROGRAM {
    version NSM_V1 {
        void NSM1_NULL(void) = 0;
        struct NSM1_STATres NSM1_STAT(struct NSM1_STATargs) = 1;
        struct NSM1_MONres NSM1_MON(struct NSM1_MONargs) = 2;
        struct NSM1_UNMONres NSM1_UNMON(struct NSM1_UNMONargs) = 3;
        struct NSM1_UNMONALLres NSM1_UNMON_ALL(struct NSM1_UNMONALLargs) = 4; 
        void NSM1_SIMU_CRASH(void) = 5;
        void NSM1_NOTIFY(struct NSM1_NOTIFYargs) = 6;
    } = 1;
} = 100024;


