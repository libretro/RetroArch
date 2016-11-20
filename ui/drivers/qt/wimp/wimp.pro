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

Release:DESTDIR = ../build/release
Release:OBJECTS_DIR = ../build/release/obj
Release:MOC_DIR = ../build/release/moc
Release:RCC_DIR = ../build/release/rcc
Release:UI_DIR = ../build/release/ui

Debug:DESTDIR = ../build/debug
Debug:OBJECTS_DIR = ../build/debug/obj
Debug:MOC_DIR = ../build/debug/moc
Debug:RCC_DIR = ../build/debug/rcc
Debug:UI_DIR = ../build/debug/ui

INCLUDEPATH += $$PWD/../../../../
INCLUDEPATH += $$PWD/../../../../libretro-common/include/
