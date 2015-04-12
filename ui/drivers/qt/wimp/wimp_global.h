#ifndef WIMP_GLOBAL_H
#define WIMP_GLOBAL_H

#include <QtCore/qglobal.h>

#if defined(WIMP_LIBRARY)
#  define WIMPSHARED_EXPORT Q_DECL_EXPORT
#else
#  define WIMPSHARED_EXPORT Q_DECL_IMPORT
#endif

#endif // WIMP_GLOBAL_H
