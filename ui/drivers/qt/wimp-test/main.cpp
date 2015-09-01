#include <QDebug>
#include "../wrapper/wrapper.h"
#include  "../wimp/wimp.h"

int main(int argc, char *argv[])
{
    struct Wimp* wimp;
    char* args[] = {""};

    qDebug() << "C++ Style Debug Message";

    wimp = ctrWimp(0, argv);
    if(wimp)
       CreateMainWindow(wimp);

    return 1;
}

