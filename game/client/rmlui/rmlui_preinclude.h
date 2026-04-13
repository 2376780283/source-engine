// ==================================================================
// RmlUI Pre-include Header
// Fixes NULL macro conflicts with Source SDK
// ==================================================================

#ifndef RMLUI_PREINCLUDE_H
#define RMLUI_PREINCLUDE_H

// CRITICAL: Define NULL as nullptr BEFORE anything that includes <algorithm>
// This prevents type mismatches in std::vector templates used by RmlUI
#ifdef NULL
#undef NULL
#endif
#define NULL nullptr

// Include RmlUI config 
#include "RmlUi/Config/Config.h"

#endif // RMLUI_PREINCLUDE_H