VPATH = ../..:../helper

TARGET_LIB = libpthread-psp.a

MUTEX_OBJS = pthread_mutex.o

MUTEXATTR_OBJS = pthread_mutexattr.o

SUPPORT_OBJS = pte.o pthread.o

THREAD_OBJS = sched.o

TLS_OBJS = pthread_key.o

MISC_OBJS = pthread_get.o

SEM_OBJS = sem.o

BARRIER_OBJS = pthread_barrier.o pthread_barrierattr.o
  
SPIN_OBJS = pthread_spin.o

CONDVAR_OBJS = pthread_cond.o pthread_condattr.o

RWLOCK_OBJS = pthread_rwlock.o pthread_rwlockattr.o

CANCEL_OBJS = pthread.o pthread_set.o

OS_OBJS = psp_osal.o tls-helper.o

OBJS = $(MUTEX_OBJS) $(MUTEXATTR_OBJS) $(THREAD_OBJS) $(SUPPORT_OBJS) $(TLS_OBJS) $(MISC_OBJS) $(SEM_OBJS) $(BARRIER_OBJS) $(SPIN_OBJS) $(CONDVAR_OBJS) $(RWLOCK_OBJS) $(CANCEL_OBJS) $(OS_OBJS)

INCDIR = 
CFLAGS = $(GLOBAL_CFLAGS) -G0 -O2 -Wall -g -fno-strict-aliasing -I../.. -I../helper 
CXXFLAGS = $(CFLAGS) -fexceptions -fno-rtti -Werror
ASFLAGS = $(CFLAGS)

LDFLAGS = 
LIBS = 

PSPSDK=$(shell psp-config --pspsdk-path)
include $(PSPSDK)/lib/build.mak

test :
	make -C ../../tests test

install: $(TARGET_LIB)
	@cp -v $(TARGET_LIB) `psp-config --psp-prefix`/lib
	@cp -v *.h `psp-config --psp-prefix`/include
	@echo "Done."

