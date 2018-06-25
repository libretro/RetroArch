/*
  libco.win (2016-09-06)
  authors: frangarcj
  license: public domain
*/

#define LIBCO_C
#include <libco.h>
#include <stdlib.h>
#include <psp2/sysmodule.h>

#ifdef __cplusplus
extern "C" {
#endif

static thread_local cothread_t co_active_ = 0;

typedef struct SceFiber {
	char reserved[128];
} SceFiber __attribute__( ( aligned ( 8 ) ) ) ;

int32_t _sceFiberInitializeImpl(SceFiber* fiber, char* name, void* entry, uint32_t argOnInitialize, void* addrContext, int32_t sizeContext, void* params);

int32_t sceFiberFinalize(SceFiber* fiber);

int32_t sceFiberRun(SceFiber* fiber, uint32_t argOnRunTo, uint32_t* argOnRun);

int32_t sceFiberSwitch(SceFiber* fiber, uint32_t argOnRunTo, uint32_t* argOnRun);

int32_t sceFiberReturnToThread(uint32_t argOnReturn, uint32_t* argOnRun);

static void co_thunk(uint32_t argOnInitialize, uint32_t argOnRun)
{
   ((void (*)(void))argOnInitialize)();
}

cothread_t co_active(void)
{
   if(!co_active_)
   {
		  sceSysmoduleLoadModule(SCE_SYSMODULE_FIBER);
      co_active_ = (cothread_t)1;
   }
   return co_active_;
}

cothread_t co_create(unsigned int heapsize, void (*coentry)(void))
{
   SceFiber* tailFiber = malloc(sizeof(SceFiber));
	 char * m_contextBuffer = malloc(sizeof(char)*heapsize);
	 if(!co_active_)
   {
      sceSysmoduleLoadModule(SCE_SYSMODULE_FIBER);
      co_active_ = (cothread_t)1;
   }

   //_sceFiberInitializeImpl
   int ret = _sceFiberInitializeImpl(tailFiber, "tailFiber", co_thunk, (uint32_t)coentry, (void*) m_contextBuffer, heapsize, NULL);
   if(ret==0){
     return (cothread_t)tailFiber;
   }else{
     return (cothread_t)ret;
   }

}

void co_delete(cothread_t cothread)
{
	 if(cothread == (cothread_t)1){
		 return;
	 }
   sceFiberFinalize((SceFiber*)cothread);
}

void co_switch(cothread_t cothread)
{

   uint32_t argOnReturn = 0;
   if(cothread == (cothread_t)1){
     co_active_ = cothread;
     sceFiberReturnToThread(0, NULL);
   }else{
		 SceFiber* theFiber = (SceFiber*)cothread;
		 if(co_active_ == (cothread_t)1){
			 co_active_ = cothread;
			 sceFiberRun(theFiber, 0, &argOnReturn);
		 }else{
			 co_active_ = cothread;
			 sceFiberSwitch(theFiber, 0, &argOnReturn);
		 }
   }
}

#ifdef __cplusplus
}
#endif
