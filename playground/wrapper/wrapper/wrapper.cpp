#include  "../../test/test/test.h"
#include  "../../test/test/test_global.h"
#include "wrapper.h"


#ifdef __cplusplus
extern "C" {
#endif

typedef struct Test Test;

Test* ctrTest(int argc, char *argv[]){
    return new Test(argc,argv);
}

int CreateMainWindow(Test* p)
{
    return p->CreateMainWindow();
}

#ifdef __cplusplus
}
#endif
