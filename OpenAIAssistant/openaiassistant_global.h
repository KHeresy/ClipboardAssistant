#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(OPENAIASSISTANT_LIB)
#  define OPENAIASSISTANT_EXPORT Q_DECL_EXPORT
# else
#  define OPENAIASSISTANT_EXPORT Q_DECL_IMPORT
# endif
#else
# define OPENAIASSISTANT_EXPORT
#endif
