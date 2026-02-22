#pragma once
#include "units.h"

/* structures */

struct _vehicle_datum
{
	uint16 flags;
	uint8 field_2;
	uint8 upside_down_on_ground_ticks;
	uint8 airborne_ticks;
	uint8 upending_type;
	uint8 upending_ticks;
	uint8 on_ground_ticks;
	uint8 trick_type;
	uint8 trick_tick;
	uint8 pad_A[2];
	real32 speed;
	real32 slide;
	real32 turn;
	real32 wheel;
	real32 rear_wheel;
	real32 left_tread;
	real32 right_tread;
	real32 function_variable_3;
	real32 function_variable_4;
	real32 engine_angular_velocity;
	real32 torque_from_wheels;
	real32 cruising_to_loaded_transition;
	uint8 field_3C;
	uint8 shift_counter;
	uint8 ticks_in_gear;
	uint8 ticks_above_shift_threshold;
	uint8 ticks_below_shift_threshold;
	uint8 pad_41[2];
	uint8 gear;
	uint8 desired_gear;
	uint8 field_45;
	uint16 slipping_wheels_bit_vector;
	uint8 suspension_last_measurement[8];
	uint8 suspension[8];
	int32 squad_index;
	datum next_vehicle_index;
	string_id starting_location_name;
	uint32 bipeds_killed;
	uint32 reserved_seats;
	uint32 disallowed_seats;
	uint32 last_pathfinding_attempt_time;
	datum pathfinding_instanced_geometry_index;
	uint32 pathfinding_sector;
	int32 field_7C;
	int32 pathfinding_surface_index;
	int32 pathfinding_bsp_reference;
	real_point3d pathfinding_point;
	real32 speed_function_position;
	real32 speed_function_velocity;
	real_point3d previous_root_position;
	real_vector3d root_position_delta;
	real32 mean_antigrav_fraction;
	datum last_known_animation_graph_index;
	int32 steering_animation_id;
	int32 roll_animation_id;
	int32 throttle_animation_id;
	int32 velocity_animation_id;
	int32 ground_speed_animation_id;
	int32 ground_speed_rear_animation_id;
	int32 acceleration_animation_id;
	int32 animation_id_unknown_1;
	int32 animation_id_unknown_2;
};
ASSERT_STRUCT_SIZE(_vehicle_datum, 0xE0);

struct vehicle_datum
{
	int32 definition_index;
	_object_datum object;
	_unit_datum unit;
	_vehicle_datum vehicle;
};
ASSERT_STRUCT_SIZE(vehicle_datum, 0x440);

/* macros */

#define vehicle_get(index) ((vehicle_datum*)(object_get_and_verify_type((index), _object_mask_vehicle)))
#define vehicle_try_and_get(index) ((vehicle_datum*)(object_try_and_get_and_verify_type((index), _object_mask_vehicle)))
