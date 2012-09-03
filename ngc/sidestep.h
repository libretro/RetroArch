/****************************************************************************
* SideStep DOL Loading
*
* This module runs a DOL file from Auxilliary RAM. This removes any memory
* issues which might occur - and also means you can easily overwrite yourself!
*
* softdev March 2007
***************************************************************************/
#ifndef HW_RVL
#ifndef __SIDESTEP__
#define __SIDESTEP__

/*** A standard DOL header ***/
#define DOLHDRLENGTH 256	/*** All DOLS must have a 256 byte header ***/
#define MAXTEXTSECTION 7
#define MAXDATASECTION 11

/*** A handy DOL structure ***/
typedef struct {
    unsigned int textOffset[MAXTEXTSECTION];
    unsigned int dataOffset[MAXDATASECTION];

    unsigned int textAddress[MAXTEXTSECTION];
    unsigned int dataAddress[MAXDATASECTION];

    unsigned int textLength[MAXTEXTSECTION];
    unsigned int dataLength[MAXDATASECTION];

    unsigned int bssAddress;
    unsigned int bssLength;

    unsigned int entryPoint;
    unsigned int unused[MAXTEXTSECTION];
} DOLHEADER;

int DOLtoARAM(const char *dol_name);

#endif
#endif
