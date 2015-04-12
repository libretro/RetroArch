#ifndef WIMP_H
#define WIMP_H

#include "wimp_global.h"
#include <QtWidgets/qwidget.h>
#include <QtWidgets/qapplication.h>
#include <QtQml/qqmlapplicationengine.h>


class WIMPSHARED_EXPORT Wimp : public QGuiApplication
{
    Q_OBJECT
    public:
        Wimp(int argc, char *argv[]): QGuiApplication(argc, argv) {}
        int CreateMainWindow();

};

#endif // WIMP_H
