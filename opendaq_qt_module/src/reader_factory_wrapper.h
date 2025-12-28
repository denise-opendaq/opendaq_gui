#pragma once

// Temporarily undefine Qt macros to avoid conflicts with OpenDAQ
#ifdef signals
#define SIGNALS_WAS_DEFINED
#pragma push_macro("signals")
#undef signals
#endif

#ifdef slots
#define SLOTS_WAS_DEFINED
#pragma push_macro("slots")
#undef slots
#endif

#include <opendaq/reader_factory.h>

// Restore Qt macros
#ifdef SIGNALS_WAS_DEFINED
#pragma pop_macro("signals")
#undef SIGNALS_WAS_DEFINED
#endif

#ifdef SLOTS_WAS_DEFINED
#pragma pop_macro("slots")
#undef SLOTS_WAS_DEFINED
#endif
