#ifndef TEST_GLOBAL_H
#define TEST_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(TEST_LIBRARY)
#  define TESTSHARED_EXPORT Q_DECL_EXPORT
#else
#  define TESTSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // TEST_GLOBAL_H
