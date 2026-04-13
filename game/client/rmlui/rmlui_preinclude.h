// ==================================================================
// RmlUI Pre-include Header
// Fixes NULL macro conflicts with Source SDK
// ==================================================================

#ifndef RMLUI_PREINCLUDE_H
#define RMLUI_PREINCLUDE_H

// Include RmlUI config first to ensure consistent Vector definitions
#include "RmlUi/Config/Config.h"

// Fix NULL macro conflict from Source SDK - use standard nullptr
// This must be done AFTER basetypes.h is included, so we'll do it in the implementation files

#endif // RMLUI_PREINCLUDE_H