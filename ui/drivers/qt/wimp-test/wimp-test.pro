TARGET = wimp-test
CONFIG += console


SOURCES += main.cpp

win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../wimp/build/release/ -lwimp.dll
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../wimp/build/debug/ -lwimp.dll
else:unix: LIBS += -L$$PWD/../wimp/build/ -lwimp.dll

INCLUDEPATH += $$PWD/../wimp/build/release
DEPENDPATH += $$PWD/../wimp/build/release


win32:CONFIG(release, debug|release): LIBS += -L$$PWD/../wrapper/build/release/ -lwrapper.dll
else:win32:CONFIG(debug, debug|release): LIBS += -L$$PWD/../wrapper/build/debug/ -lwrapper.dll
else:unix: LIBS += -L$$PWD/../wrapper/build/ -lwrapper.dll

INCLUDEPATH += $$PWD/../wrapper/build/release
DEPENDPATH += $$PWD/../wrapper/build/release
