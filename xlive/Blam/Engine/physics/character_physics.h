#pragma once
#include "character_physics_mode_dead.h"
#include "character_physics_mode_flying.h"
#include "character_physics_mode_ground.h"
#include "character_physics_mode_melee.h"
#include "character_physics_mode_sentinel.h"

/* enums */

enum e_character_physics_mode : uint8
{
	_character_physics_mode_none = 0,
	_character_physics_mode_ground,
	_character_physics_mode_flying,
	_character_physics_mode_dead,
	_character_physics_mode_sentinel,
	_character_physics_mode_sentinel_climbing,
	_character_physics_mode_melee,

	k_character_physics_mode_count,
	k_character_physics_mode_first = _character_physics_mode_ground,
	k_character_physics_mode_last = _character_physics_mode_melee
};

/* structures */

class c_character_physics_component
{
private:
	struct datum_buffer
	{
		union
		{
			c_character_physics_mode_ground_datum ground;
			c_character_physics_mode_flying_datum flying;
			c_character_physics_mode_dead_datum dead;
			c_character_physics_mode_sentinel_datum sentinel;
			c_character_physics_mode_melee_datum melee;
		};
		int32 pad;
	};

	e_character_physics_mode m_mode;
	uint8 m_collision_damage_immunity_counter;
	datum m_object_index;
	datum m_early_mover_object_index;
	datum m_accepted_early_mover_object_index;
	int8 m_mode_datum_buffer[sizeof(datum_buffer)];

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
