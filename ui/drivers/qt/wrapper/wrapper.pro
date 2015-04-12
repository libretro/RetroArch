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


win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../wimp/build/release/ -lwimp
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../wimp/build/debug/ -lwimp

INCLUDEPATH += $$PWD/../../wimp/build/debug
DEPENDPATH += $$PWD/../../wimp/build/debug

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../../wimp/build/release/libwimp.dll.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../../wimp/build/debug/libwimp.dll.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../../wimp/build/release/wimp.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../../wimp/build/debug/wimp.lib
