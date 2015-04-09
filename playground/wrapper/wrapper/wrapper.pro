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


win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../test/build/release/ -ltest
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../test/build/debug/ -ltest

INCLUDEPATH += $$PWD/../../test/build/debug
DEPENDPATH += $$PWD/../../test/build/debug

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../../test/build/release/libtest.dll.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../../test/build/debug/libtest.dll.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../../test/build/release/test.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../../test/build/debug/test.lib
