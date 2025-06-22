#pragma once
#include "memory/data.h"

/* constants */

enum
{
	k_minimum_physics_model_shape_reference_count = 128,
	k_environement_body_user_data = 3401362352
};

/* structures */

struct s_havok_globals
{
	void* profiler;
	void* environment_body;
	int32 rigid_bodies_count;
	int32 rigid_bodies_connected_to_world_count;
	int32 time;
	bool rigid_bodies_active;
	bool environment_active;
	bool disable_rebuilding_environment_mopp;
	bool garbage_collection_locked;
};
ASSERT_STRUCT_SIZE(s_havok_globals, 24);

struct s_havok_game_state
{
	int32 havok_components_allocated;
};
ASSERT_STRUCT_SIZE(s_havok_game_state, 4);

/* prototypes */

data_array* havok_components_get(void);

s_havok_game_state* havok_game_state_get(void);

bool havok_memory_allocator_locked(void);

bool is_havok_update_memory_initialized(void);

bool havok_can_modify_state(void);

bool havok_can_allocate_space_for_instance_of_object_definition(datum tag_index);

void __cdecl havok_update(void);
