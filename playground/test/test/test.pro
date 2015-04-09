#-------------------------------------------------
#
# Project created by QtCreator 2014-10-21T21:15:44
#
#-------------------------------------------------

QT += qml quick widgets

TARGET = test
TEMPLATE = lib

DEFINES += TEST_LIBRARY

SOURCES += test.cpp

HEADERS += test.h\
        test_global.h

unix {
    target.path = /usr/lib
    INSTALLS += target
}

RESOURCES += \
    qml.qrc
