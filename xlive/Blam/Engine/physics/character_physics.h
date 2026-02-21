#pragma once
#include "character_physics_mode_dead.h"
#include "character_physics_mode_flying.h"
#include "character_physics_mode_ground.h"
#include "character_physics_mode_melee.h"
#include "character_physics_mode_sentinel.h"

/* constants */

constexpr real32 k_character_physics_collision_immunity_duration = 0.3f;

enum
{
	// todo: when all physics mode datum types are made do a sizeof(largest)?
	k_character_physics_mode_datum_buffer_size = 120,
};

/* enums */

enum e_character_physics_mode : uint8
{
	_character_physics_mode_none = 0,
	_character_physics_mode_ground = 1,
	_character_physics_mode_flying = 2,
	_character_physics_mode_dead = 3,
	_character_physics_mode_sentinel = 4,
	_character_physics_mode_sentinel_climbing = 5,
	_character_physics_mode_melee = 6,

	k_character_physics_mode_count,
	k_character_physics_mode_first = _character_physics_mode_ground,
	k_character_physics_mode_last = _character_physics_mode_melee
};

/* structures */

class c_character_physics_component
{
private:
	e_character_physics_mode m_mode;
	uint8 m_collision_damage_immunity_counter;
	datum m_object_index;
	datum m_early_mover_object_index;
	datum m_accepted_early_mover_object_index;

	// doesn't seem to be a union just a buffer sized to the largest physics mode class
	int8 m_mode_datum_buffer[k_character_physics_mode_datum_buffer_size];

public:
	void initialize(datum object_index);

	void set_mode(e_character_physics_mode mode);
	e_character_physics_mode get_mode() const;

	void ping_collision_damage_immunity_counter();
	bool is_immune_to_collision_damage() const;

	bool is_sentinel_mode() const;

	c_character_physics_mode_ground_datum* get_mode_ground() const;
	c_character_physics_mode_flying_datum* get_mode_flying() const;
	c_character_physics_mode_dead_datum* get_mode_dead() const;
	c_character_physics_mode_sentinel_datum* get_mode_sentinel() const;
	c_character_physics_mode_melee_datum* get_mode_melee() const;
};
ASSERT_STRUCT_SIZE(c_character_physics_component, 0x88);
