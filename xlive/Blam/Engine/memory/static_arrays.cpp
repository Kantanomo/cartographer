#include "stdafx.h"
#include "static_arrays.h"

/* globals */

#ifdef ASSERTS_ENABLED
// This is only used in a couple of places in debug versions of the game
// g_temporary character array is used in most other places
c_static_string<256> g_static_string_assert_text;
#endif
