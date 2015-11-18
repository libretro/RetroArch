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


win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../wimp/build/release/ -lwimp.dll
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../wimp/build/debug/ -lwimp.dll
else:unix: LIBS += -L$$PWD/../wimp/build/ -lwimp.dll

INCLUDEPATH += $$PWD/../wimp/build/release
DEPENDPATH += $$PWD/../wimp/build/release

INCLUDEPATH += $$PWD/../../../../
INCLUDEPATH += $$PWD/../../../../libretro-common/include/
