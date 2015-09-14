#include <QDebug>
#include "../wrapper/wrapper.h"
#include  "../wimp/wimp.h"

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>

struct Wimp* t;


int i=0;

void *initGui(void *arg)
{
    char **arguments = (char**)arg;
    t = ctrWimp(i,arguments);
    CreateMainWindow(t); //-->uncomment this to open the QT gui
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

    for(int j=0;j<100;j++)
    {
        Sleep(1000);
        printf("test = %d\n",i);
        i++;
        if(j < 2)
            t->SetTitle("test");
    }

    pthread_join(gui,NULL);
    return 0;
}

