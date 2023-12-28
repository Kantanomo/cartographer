#pragma once
#include "particle_system.h"
#include "Blam/Engine/math/real_math.h"
#include "Blam/Engine/memory/data.h"

class c_particle_location
{
public:
	int8 gap_0[2];
	bool parent_effect_has_bit_15_set;
	int8 gap_3;
	datum particle_emitter_index;
	int8 gap_8[4];
	int32 next_particle_location;
	real_point3d position;
	float field_1C;
	real_point3d new_position_maybe;
	float field_2C;
	int32 render_state_index;

	void update(c_particle_system* particle_system, c_particle_system_definition* particle_system_definition, real32 delta);
	void update_rewritten(c_particle_system* particle_system, c_particle_system_definition* particle_system_definition, real32 delta);
};
CHECK_STRUCT_SIZE(c_particle_location, 0x34);

s_data_array* get_particle_location_table();