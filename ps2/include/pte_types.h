/* pte_types.h  */

#ifndef PTE_TYPES_H
#define PTE_TYPES_H

#include <errno.h>
#include <sys/types.h>
#include <sys/timeb.h>
#include <tcpip.h>

/** UIDs are used to describe many different kernel objects. */
typedef int SceUID;

/* Misc. kernel types. */
typedef unsigned int SceSize;
typedef int SceSSize;

typedef unsigned char SceUChar;
typedef unsigned int SceUInt;

/* File I/O types. */
typedef int SceMode;
typedef long SceOff;
typedef long SceIores;

#endif /* PTE_TYPES_H */
