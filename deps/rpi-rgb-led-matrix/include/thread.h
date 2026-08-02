// -*- mode: c++; c-basic-offset: 2; indent-tabs-mode: nil; -*-
// Copyright (C) 2013 Henner Zeller <h.zeller@acm.org>
//
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation version 2.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see <http://gnu.org/licenses/gpl-2.0.txt>

#ifndef RPI_THREAD_H
#define RPI_THREAD_H

#include <stdint.h>
#include <pthread.h>

namespace rgb_matrix {
// Simple thread abstraction.
class Thread {
public:
  Thread();

  // The destructor waits for Run() to return so make sure it does.
  virtual ~Thread();

  // Wait for the Run() method to return.
  void WaitStopped();

  // Start thread. If realtime_priority is > 0, then this will be a
  // thread with SCHED_FIFO and the given priority.
  // If cpu_affinity is set !=, chooses the given bitmask of CPUs
  // this thread should have an affinity to.
  // On a Raspberry Pi 1, this doesn't matter, as there is only one core,
  // Raspberry Pi 2 can has 4 cores, so any combination of (1<<0) .. (1<<3) is
  // valid.
  virtual void Start(int realtime_priority = 0, uint32_t cpu_affinity_mask = 0);

  // Override this to do the work.
  //
  // This will be called in a thread once Start() has been called. You typically
  // will have an endless loop doing stuff.
  //
  // It is a good idea to provide a way to communicate to the thread that
  // it should stop (see ThreadedCanvasManipulator for an example)
  virtual void Run() = 0;

private:
  static void *PthreadCallRun(void *tobject);
  bool started_;
  pthread_t thread_;
};

// Non-recursive Mutex.
class Mutex {
public:
  Mutex() { pthread_mutex_init(&mutex_, NULL); }
  ~Mutex() { pthread_mutex_destroy(&mutex_); }
  void Lock() { pthread_mutex_lock(&mutex_); }
  void Unlock() { pthread_mutex_unlock(&mutex_); }

  // Wait on condition. If "timeout_ms" is < 0, it waits forever, otherwise
  // until timeout is reached.
  // Returns 'true' if condition is met, 'false', if wait timed out.
  bool WaitOn(pthread_cond_t *cond, long timeout_ms = -1);

private:
  pthread_mutex_t mutex_;
};

// Useful RAII wrapper around mutex.
class MutexLock {
public:
  MutexLock(Mutex *m) : mutex_(m) { mutex_->Lock(); }
  ~MutexLock() { mutex_->Unlock(); }
private:
  Mutex *const mutex_;
};

}  // end namespace rgb_matrix

#endif  // RPI_THREAD_H
