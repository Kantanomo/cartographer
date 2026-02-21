#pragma once
#include "objects/objects.h"
#include "physics/character_physics.h"

/* structures */

struct s_creature_control_data
{
	int32 animation_mode;
	int32 animation_state;
	uint16 control_flags;
	uint8 turning_speed;
	real_vector3d throttle;
	real_vector3d forward;
	real_vector3d up;
};

struct _creature_datum
{
	uint16 flags;
	int16 team_index;
	uint8 ground_ticks;
	uint8 air_ticks;
	int32 ai_state_type;
	int32 ai_state_storage;
	s_creature_control_data control_data;
	real_vector3d turning_velocity;
	real32 bank;
	c_character_physics_component physics;
	int8 attached_damage_timer;
	int8 attached_cooldown_timer;
	int16 destroy_after_death_timer;
	int8 block_storage[352]; // cs scripting data storage
};

struct creature_datum
{
	datum definition_index;
	_object_datum object;
	_creature_datum creature;
};
ASSERT_STRUCT_SIZE(creature_datum, 872);