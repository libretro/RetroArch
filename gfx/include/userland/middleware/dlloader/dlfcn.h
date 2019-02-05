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

#ifndef _DLLOADER_H
#define _DLLOADER_H

#ifdef  __cplusplus
extern "C" {
#endif

   /*
    * Declarations used for dynamic linking support routines.
    */
   extern void dlloader_init(void);
   extern void *dlopen (const char *dllName, int mode);
   extern void *dlopen_at(const char *dllName, void *kept, int keptspace, void *unkept, int unkeptspace);
   extern void *dlopen_pmm(const char *dllName, void *(*pmm_alloc)(void *, unsigned int, unsigned int, const char *, unsigned int), void (*pmm_free)(void *, void *), void *pmm_priv);
   extern void (*dlsym(void *, const char *))();
   extern int dlclose(void *);
   extern const char *dlerror(void);
   extern void dldone(void *handle);
   extern int  dlcheck(const char *pathName);
   extern void *dlshared_vll_load(const char *vll_name,
                        const char **symbols,
                        void *(*pmm_alloc)(void *, unsigned int, unsigned int, const char *, unsigned int),
                        void (*pmm_free)(void *, void *),
                        void *pmm_priv,
                        int *vll_init_required);
   extern void dlshared_vll_init_done(void *vll);
   extern void (*dlshared_get_vll_symbol(void *vll, const char *symbol))();
   extern int dlshared_vll_closing(void *vll);
   extern int dlshared_vll_unload(void *vll);

   /*
    * Valid values for mode argument to dlopen.
    */
#define  RTLD_LAZY      0x00001     /* deferred function binding */
#define  RTLD_NOW       0x00002     /* immediate function binding */

#define  RTLD_GLOBAL    0x00100     /* export symbols to others */
#define  RTLD_LOCAL     0x00000     /* symbols are only available */
   /* to group members */

#ifdef   __cplusplus
}
#endif

#ifdef FOR_VMCS
// Set the path where vlls are to be retrieved from. Return non-zero for success.
int dl_set_vll_dir(char const *dir_name);
#endif

enum dl_pmm_flags {
   DL_PMM_NORMAL = 0,
   DL_PMM_TEMPORARY = 1,
   DL_PMM_DEBUGINFO = 2,
};

enum dlpoolflags {
   DL_POOLFLAGS_EXECUTABLE = 1,
   DL_POOLFLAGS_WRITABLE = 2,
   DL_POOLFLAGS_TEMPORARY = 4,
   DL_POOLFLAGS_DEBUGINFO = 8,
};

struct dlsegmentsizedata {
   int size;
   int align;
   enum dlpoolflags flags;
};

extern int dlgetsegmentsizes(const char *vll, int *nrows, struct dlsegmentsizedata *segdata);

#endif   /* _DLFCN_H */
