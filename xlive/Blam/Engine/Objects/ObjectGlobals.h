#include "Blam/Math/real_vector3d.h"

struct s_object_globals
{
	bool initialized;
	bool do_garbage_collection;
	bool do_garbage_collection_unsafe;
	bool objects_updating;
	int active_objects_maybe;
	int field_8;
	int field_C;
	int field_10;
	int unk_game_time_store;
	int field_18;
	real_color_rgb cinematic_ambient_color;
	real_vector3d cimenatic_primary_light_vector_1;
	float cimenatic_primary_light_unk1;
	float cimenatic_primary_light_unk2;
	real_color_rgb cimenatic_primary_light_color;
	real_vector3d cimenatic_primary_light_vector;
	real_color_rgb cinematic_secondary_light_color;
	real_vector3d cinematic_secondary_light_vector;
	int field_6C;
	float global_function_values[4];
	char object_custom_animations_hold_on_last_frame;
	bool object_custom_animations_prevent_lipsync_head_movement;
	bool enable_warthog_turret_light;
	bool field_83;

	static s_object_globals* get();
};
CHECK_STRUCT_SIZE(s_object_globals, 0x84);