#-------------------------------------------------
#
# Project created by QtCreator 2014-10-21T21:15:44
#
#-------------------------------------------------

QT += qml quick widgets

TARGET = wimp
TEMPLATE = lib

DEFINES += WIMP_LIBRARY

SOURCES += \
    wimp.cpp

HEADERS +=\
    wimp.h \
    wimp_global.h

RESOURCES += \
    qml.qrc


INCLUDEPATH += $$PWD/../../../../
INCLUDEPATH += $$PWD/../../../../libretro-common/include/
