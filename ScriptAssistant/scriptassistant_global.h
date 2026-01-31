#pragma once

#include <QtCore/qglobal.h>

#ifndef BUILD_STATIC
# if defined(SCRIPTASSISTANT_LIB)
#  define SCRIPTASSISTANT_EXPORT Q_DECL_EXPORT
# else
#  define SCRIPTASSISTANT_EXPORT Q_DECL_IMPORT
# endif
#else
# define SCRIPTASSISTANT_EXPORT
#endif
