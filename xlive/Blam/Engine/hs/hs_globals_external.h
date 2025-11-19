#pragma once
#include "hs.h"

/* structures */

struct hs_global_runtime
{
	int16 identifier;
	uint16 pad;
	union
	{
		int8 byte_value;
		int16 short_value;
		int32 long_value;
		real32 real_value;

		const char* string_value;
	};
};

struct hs_global_external
{
#ifdef HS_COMPILER_ENABLED
	const char* name;
#endif
	e_hs_type type;
	void* pointer;
};

/* constants */

extern const struct hs_global_external* hs_external_globals[];
extern const int16 k_hs_external_global_count;
