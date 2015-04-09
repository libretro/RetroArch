TEMPLATE = app
CONFIG += console
#CONFIG -= app_bundle
QMAKE_CC= gcc -std=c99

include(deployment.pri)
qtcAddDeployment()

SOURCES += \
    main.c

HEADERS += \
    ../../wrapper/wrapper/wrapper.h


win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../test/build/release/ -ltest
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../test/build/debug/ -ltest

INCLUDEPATH += $$PWD/../../test/build/debug
DEPENDPATH += $$PWD/../../test/build/debug

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../../test/build/release/libtest.dll.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../../test/build/debug/libtest.dll.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../../test/build/release/test.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../../test/build/debug/test.lib

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../../wrapper/build/release/ -lwrapper
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../../wrapper/build/debug/ -lwrapper

INCLUDEPATH += $$PWD/../../wrapper/build/debug
DEPENDPATH += $$PWD/../../wrapper/build/debug

win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../../wrapper/build/release/libwrapper.dll.a
else:win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../../wrapper/build/debug/libwrapper.dll.a
else:win32:!win32-g++:CONFIG(release, debug|release): PRE_TARGETDEPS += $$PWD/../../wrapper/build/release/wrapper.lib
else:win32:!win32-g++:CONFIG(debug, debug|release): PRE_TARGETDEPS += $$PWD/../../wrapper/build/debug/wrapper.lib
