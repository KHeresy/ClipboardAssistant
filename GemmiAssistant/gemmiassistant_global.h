#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(GEMMIASSISTANT_LIB)
#  define GEMMIASSISTANT_EXPORT Q_DECL_EXPORT
# else
#  define GEMMIASSISTANT_EXPORT Q_DECL_IMPORT
# endif
#else
# define GEMMIASSISTANT_EXPORT
#endif
