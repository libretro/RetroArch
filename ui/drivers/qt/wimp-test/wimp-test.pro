TARGET = wimp-test
CONFIG += console


SOURCES += main.cpp

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../wimp/build/release/ -lwimp.dll
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../wimp/build/debug/ -lwimp.dll
else:unix: LIBS += -L$$PWD/../wimp/build/ -lwimp.dll

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../build/release/ -lwrapper.dll
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../build/debug/ -lwrapper.dll
else:unix:CONFIG(debug, debug|release): LIBS += -L$$PWD/../build/debug/ -lwrapper.so
else:unix:CONFIG(release, debug|release): LIBS += -L$$PWD/../build/debug/ -lwrapper.so

INCLUDEPATH += $$PWD/../../../../
INCLUDEPATH += $$PWD/../../../../libretro-common/include/

win32:CONFIG(release, debug|release): INCLUDEPATH += $$PWD/../build/release
else:win32:CONFIG(debug, debug|release): INCLUDEPATH += $$PWD/../build/debug
else:unix:CONFIG(debug, debug|release): INCLUDEPATH += $$PWD/../build/release
else:unix:CONFIG(release, debug|release): INCLUDEPATH += $$PWD/../build/release
