#include "stdafx.h"
#include "character_physics.h"

#include "game/game_time.h"
#include "objects/objects.h"

void c_character_physics_component::initialize(datum object_index)
{
	this->m_object_index = object_index;
	this->m_mode = _character_physics_mode_none;
	this->m_early_mover_object_index = NONE;
	this->m_accepted_early_mover_object_index = NONE;
}

void c_character_physics_component::set_mode(e_character_physics_mode mode)
{
	ASSERT(IN_RANGE(mode, k_character_physics_mode_first, k_character_physics_mode_last));

	
	//dispose old mode
	switch(this->m_mode)
	{
		case _character_physics_mode_ground:
			((c_character_physics_mode_ground_datum*)this->m_mode_datum_buffer)->dispose();
			break;
		case _character_physics_mode_flying:
			((c_character_physics_mode_flying_datum*)this->m_mode_datum_buffer)->dispose();
			break;
		case _character_physics_mode_dead:
			((c_character_physics_mode_dead_datum*)this->m_mode_datum_buffer)->dispose();
			break;
		case _character_physics_mode_sentinel:
		case _character_physics_mode_sentinel_climbing:
			((c_character_physics_mode_sentinel_datum*)this->m_mode_datum_buffer)->dispose(this->m_object_index);
			break;
		case _character_physics_mode_melee:
			((c_character_physics_mode_melee_datum*)this->m_mode_datum_buffer)->dispose();
			break;
	}

	//initialize new mode
	switch(mode)
	{
		case _character_physics_mode_ground:
			((c_character_physics_mode_ground_datum*)this->m_mode_datum_buffer)->initialize();
			break;
		case _character_physics_mode_flying:
			((c_character_physics_mode_flying_datum*)this->m_mode_datum_buffer)->initialize();
			break;
		case _character_physics_mode_dead:
			((c_character_physics_mode_dead_datum*)this->m_mode_datum_buffer)->initialize();
			break;
		case _character_physics_mode_sentinel:
		case _character_physics_mode_sentinel_climbing:
		{
			real_point3d object_origin;

			object_get_origin_interpolated(this->m_object_index, &object_origin);

			((c_character_physics_mode_sentinel_datum*)this->m_mode_datum_buffer)->initialize(mode == _character_physics_mode_sentinel_climbing, &object_origin);
			break;
		}
		case _character_physics_mode_melee:
			((c_character_physics_mode_melee_datum*)this->m_mode_datum_buffer)->initialize();
			break;
	}
	

	this->m_mode = mode;
}

e_character_physics_mode c_character_physics_component::get_mode() const
{
	return this->m_mode;
}

void c_character_physics_component::ping_collision_damage_immunity_counter()
{
	this->m_collision_damage_immunity_counter = (uint8)game_seconds_to_ticks_round(k_character_physics_collision_immunity_duration);
}

bool c_character_physics_component::is_immune_to_collision_damage() const
{
	return this->m_collision_damage_immunity_counter != 0;
}

bool c_character_physics_component::is_sentinel_mode() const
{
	return this->m_mode == _character_physics_mode_sentinel || this->m_mode == _character_physics_mode_sentinel_climbing;
}

c_character_physics_mode_ground_datum* c_character_physics_component::get_mode_ground() const
{
	return (c_character_physics_mode_ground_datum*)this->m_mode_datum_buffer;
}

c_character_physics_mode_flying_datum* c_character_physics_component::get_mode_flying() const
{
	return (c_character_physics_mode_flying_datum*)this->m_mode_datum_buffer;
}

c_character_physics_mode_dead_datum* c_character_physics_component::get_mode_dead() const
{
	return (c_character_physics_mode_dead_datum*)this->m_mode_datum_buffer;
}

c_character_physics_mode_sentinel_datum* c_character_physics_component::get_mode_sentinel() const
{
	return (c_character_physics_mode_sentinel_datum*)this->m_mode_datum_buffer;
}

c_character_physics_mode_melee_datum* c_character_physics_component::get_mode_melee() const
{
	return (c_character_physics_mode_melee_datum*)this->m_mode_datum_buffer;
}
