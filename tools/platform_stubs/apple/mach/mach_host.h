#ifndef STUB_MACH_HOST_H
#define STUB_MACH_HOST_H
#include <mach/mach.h>
typedef int *host_info_t;
#define HOST_VM_INFO 2
#define HOST_VM_INFO_COUNT 15
typedef unsigned int natural_t;
typedef struct { natural_t free_count; natural_t inactive_count; }
   vm_statistics_data_t;
mach_port_t mach_host_self(void);
kern_return_t host_page_size(mach_port_t h, vm_size_t *sz);
kern_return_t host_statistics(mach_port_t h, int flavor, host_info_t info,
      mach_msg_type_number_t *count);
#endif
