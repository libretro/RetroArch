#ifndef STUB_MACH_H
#define STUB_MACH_H
/* Stands in for the SDK's umbrella header: on a real Apple SDK
 * mach/mach.h reaches mach/mach_port.h, mach/task.h and the rest through
 * mach/mach_interface.h, so anything that chain provides is declared
 * here.  A mem_stats Apple arm that starts naming a new mach symbol gets
 * its declaration added to this file - not an extra #include added to
 * mem_stats.c, which would then need a stub of its own and would still
 * be describing this harness rather than any SDK. */
#include <stdint.h>
typedef int kern_return_t;
#define KERN_SUCCESS 0
typedef unsigned int mach_port_t;
typedef uintptr_t vm_size_t;
typedef unsigned int mach_msg_type_number_t;
typedef int *task_info_t;
#define TASK_VM_INFO 22
#define TASK_VM_INFO_COUNT 87
typedef struct { uint64_t phys_footprint; uint64_t limit_bytes_remaining; }
   task_vm_info_data_t;
mach_port_t mach_task_self(void);
kern_return_t task_info(mach_port_t t, int flavor, task_info_t info,
      mach_msg_type_number_t *count);
kern_return_t mach_port_deallocate(mach_port_t task, mach_port_t name);
#endif
