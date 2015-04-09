#include <stdio.h>
#include<pthread.h>
#include "../../wrapper/wrapper/wrapper.h"

struct Test* t;


int i=0;

void *initGui(void *arg)
{
    char **arguments = (char**)arg;
    t = ctrTest(i,arguments);
    CreateMainWindow(t);
    return 0;
}

int main(int argc, char *argv[])
{
    i = argc;

    pthread_t gui;
    int rc;
    rc=pthread_create(&gui, NULL, initGui, (void *)argv);
    if(rc!=0)
    {
        printf("failed");
        exit(1);
    }

    /*t = ctrTest(argc,argv);
    CreateMainWindow(t);/*/

    for(int j=0;j<100;j++)
    {
        sleep(1);
        printf("test = %d\n",i);
        i++;
    }

    pthread_join(gui,NULL);
    return 0;
}



