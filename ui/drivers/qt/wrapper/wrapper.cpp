#include  "../wimp/wimp.h"
#include  "../wimp/wimp_global.h"
#include "wrapper.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct Wimp Wimp;

Wimp* ctrWimp(int argc, char *argv[]){
    return new Wimp(argc,argv);
}

int CreateMainWindow(Wimp* p)
{
    return p->CreateMainWindow();
}

#ifdef __cplusplus
}
#endif
