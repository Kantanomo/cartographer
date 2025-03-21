#pragma once

/* constants */

enum
{
	k_runtime_state_size = 204800
};

/* structures */

struct runtime_state_globals
{
	bool locked;
	void* base_address; // pointer to the allocation of k_runtime_state_size
	int32 allocated_size;
	int32 unk_C;
	int32 unk_10;
};
ASSERT_STRUCT_SIZE(runtime_state_globals, 20);

/* prototypes */

void __cdecl runtime_state_initialize(void);
