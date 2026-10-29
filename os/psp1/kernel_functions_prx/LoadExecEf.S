.set noreorder

#include "pspimport.s"

    IMPORT_START    "LoadExecForKernel",0x00010000
    IMPORT_FUNC     "LoadExecForKernel",0x032A7938,sceKernelLoadExecVSHEf2
