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


win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../release -lwimp

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../release/libwimp.dll.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../release/wimp.lib
