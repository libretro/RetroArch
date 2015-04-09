#ifndef TEST_H
#define TEST_H

#include "test_global.h"
#include <QtWidgets/qwidget.h>
#include <QtWidgets/qapplication.h>
#include <QtQml/qqmlapplicationengine.h>


class TESTSHARED_EXPORT Test : public QGuiApplication
{
    Q_OBJECT
    public:
        Test(int argc, char *argv[]): QGuiApplication(argc, argv) {}
        int CreateMainWindow();

};

#endif // TEST_H
