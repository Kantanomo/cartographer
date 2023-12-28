#pragma once
#include "particle_system_definition.h"
#include "Blam/Engine/math/matrix_math.h"
#include "Blam/Engine/memory/data.h"
#include "Blam/Engine/objects/object_placement.h"

#define k_max_particle_systems 128

enum e_particle_system_flags : int16
{
	_particle_system_flags_updating_bit = 1,
	_particle_system_flags_bit_2 = 2,
	_particle_system_flags_bit_3 = 3,
	_particle_system_flags_bit_4 = 4,
	_particle_system_flags_bit_5 = 5,
	_particle_system_flags_bit_6 = 6,
	_particle_system_flags_bit_7 = 7,
	_particle_system_flags_bit_8 = 8,
	_particle_system_flags_bit_9 = 9,
	_particle_system_scale_with_sky_render_model_bit = 10,
	_particle_system_flags_bit_11 = 11,
	_particle_system_flags_bit_12 = 12,
	_particle_system_flags_bit_13 = 13,
	_particle_system_flags_bit_14 = 14,
	_particle_system_flags_bit_15 = 15,
	k_particle_system_flags_count
};

struct s_particle_system_update_timings
{
	real32 some_delta_calc;
	real32 current_delta;
	datum particle_system_location_index;
};
CHECK_STRUCT_SIZE(s_particle_system_update_timings, 0xC);

class c_particle_system
{
public:
	int32 datum_salt;
	real32 accumulated_time;
	real32 duration;
	c_flags<e_particle_system_flags, int16, k_particle_system_flags_count> flags;
	int16 event_particle_system_index;
	datum tag_index;
	int16 effect_event_index;
	int8 gap_16[2];
	datum parent_effect_index;
	s_location location;
	datum definition_location_index;
	int8 gap_28[8];
	datum particle_system_location_index;
	int8 gap_34[4];
	int32 field_38;
	int8 gap_3C[4];
	datum next_particle_system;
	datum datum_44;
	c_particle_system* parent_system;
	int32 first_particle_index;
	pixel32 color;
	c_particle_system_definition* get_particle_system_definition() const;
	void destroy_children();
	bool update_rewritten(real32 delta_time);
	void update_colors(bool v_mirrored_or_one_shot, bool one_shot, pixel32 color, pixel32 color_2);
	void update_colors_and_get_location(real32 flt_1, real32 flt_2, pixel32 color, pixel32 color_2, s_particle_system_update_timings* timings);
	void adjust_particle_system_indexes(datum* datum_1, datum* datum_2);
	void update_locations(s_particle_system_update_timings* timings, real_matrix4x3* matrix, bool has_bit_15);
	int get_active_particle_locations_count();
	bool flags_bit_10_is_set() const;
	//void update_location_time(s_particle_system_update_timings* timings, real_matrix4x3* matrix, int unused);

	bool static __stdcall update(c_particle_system* thisx, real32 delta_time);
	static void __stdcall update_location_time(c_particle_system* thisx, s_particle_system_update_timings* timings, real_matrix4x3* matrix, int unused);
	static void __cdecl destroy(datum particle_system_index);
	
	static void __stdcall update_effect_time(c_particle_system* thisx, real32 flt);
};
CHECK_STRUCT_SIZE(c_particle_system, 0x54);

s_data_array* get_particle_system_table();
void __cdecl particle_syste_remove_from_effects_cache(datum effect_index, datum particle_system_index);

void apply_particle_system_patches();