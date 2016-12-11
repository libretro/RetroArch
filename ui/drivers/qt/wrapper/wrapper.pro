TEMPLATE = lib
CONFIG += console
CONFIG -= app_bundle
#CONFIG -= qt
QT += qml quick widgets

SOURCES += \
    wrapper.cpp

include(deployment.pri)
qtcAddDeployment()

HEADERS += \
    wrapper.h


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

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../build/release/ -lwimp.dll
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../build/debug/ -lwimp.dll
else:unix:CONFIG(debug, debug|release): LIBS += -L$$PWD/../build/debug/ -lwimp.so
else:unix:CONFIG(release, debug|release): LIBS += -L$$PWD/../build/debug/ -lwimp.so

win32:CONFIG(release, debug|release): INCLUDEPATH += $$PWD/../build/release
else:win32:CONFIG(debug, debug|release): INCLUDEPATH += $$PWD/../build/debug
else:unix:CONFIG(debug, debug|release): INCLUDEPATH += $$PWD/../build/release
else:unix:CONFIG(release, debug|release): INCLUDEPATH += $$PWD/../build/release

INCLUDEPATH += $$PWD/../../../../
INCLUDEPATH += $$PWD/../../../../libretro-common/include/
