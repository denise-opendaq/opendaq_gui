#pragma once

// Temporarily undefine QT_NO_KEYWORDS to allow QCustomPlot to compile
#ifdef QT_NO_KEYWORDS
#define QT_NO_KEYWORDS_TEMP_UNDEF
#undef QT_NO_KEYWORDS
#endif

#include <qcustomplot.h>

// Re-enable QT_NO_KEYWORDS
#ifdef QT_NO_KEYWORDS_TEMP_UNDEF
#define QT_NO_KEYWORDS
#undef QT_NO_KEYWORDS_TEMP_UNDEF
#endif
