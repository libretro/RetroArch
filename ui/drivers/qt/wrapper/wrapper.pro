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
