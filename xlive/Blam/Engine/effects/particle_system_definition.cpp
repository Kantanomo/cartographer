#include "stdafx.h"
#include "particle_system_definition.h"
#include "Blam/Cache/DataTypes/BlamDataTypes.h"
#include "H2MOD/Tags/TagInterface.h"


void c_particle_emitter_definition::get_emitter_particle_color(s_particle_state* particle_state,
	real_argb_color* out_color)
{
	typedef void(__thiscall* get_emitter_particle_color_t)(c_particle_emitter_definition*, s_particle_state*, real_argb_color*);
	auto function = Memory::GetAddress<get_emitter_particle_color_t>(0xFF455, 0xB2EB4);
	function(this, particle_state, out_color);
}

void c_particle_emitter_definition::get_emitter_particle_inverse_color(s_particle_state* particle_state,
	real_argb_color* out_color)
{
	typedef void(__thiscall* get_emitter_particle_inverse_color_t)(c_particle_emitter_definition*, s_particle_state*, real_argb_color*);
	auto function = Memory::GetAddress<get_emitter_particle_inverse_color_t>(0xFF492, 0xB2EF1);
	function(this, particle_state, out_color);
}

real32 c_particle_emitter_definition::get_particle_emissions_per_tick(s_particle_state* particle_state)
{
	typedef real32(__thiscall* get_particle_emissions_per_tick_t)(c_particle_emitter_definition*, s_particle_state*);
	auto function = Memory::GetAddress<get_particle_emissions_per_tick_t>(0x104941);
	return function(this, particle_state);
	//return this->particle_emmision_rate.get_result(particle_state) * 120.f;
}

real32 c_particle_emitter_definition::particle_size_evaluate()
{
	typedef real32(__thiscall* get_particle_size_t)(c_particle_emitter_definition*);
	auto function = Memory::GetAddress<get_particle_size_t>(0xFF4CF);
	return function(this);
}

c_particle_definition_interface* c_particle_system_definition::get_particle_system_interface() const
{
	return get_particle_system_interface_from_tag_index(this->particle.TagIndex);
}

bool c_particle_system_definition::system_is_cinematic()
{
	return this->flags >> _particle_system_definition_flags_cinematics & 1;
}

c_particle_system_definition* c_particle_sprite_definition_interface::get_attached_particle_system(int32 particle_system_index)
{
	return this->particle_definition->attached_particle_systems[particle_system_index];
}

effect_location_definition* c_particle_sprite_definition_interface::get_particle_definition_locations()
{
	return this->particle_definition->locations.begin();
}

size_t c_particle_sprite_definition_interface::get_particle_definition_locations_size()
{
	return this->particle_definition->locations.size;
}

bool c_particle_sprite_definition_interface::particle_is_v_mirrored_or_one_shot()
{
	return (this->particle_definition->flags >> 12) & 1;
}

bool c_particle_sprite_definition_interface::particle_is_one_shot()
{
	return (this->particle_definition->flags >> 8) & 1;
}

real32 c_particle_sprite_definition_interface::particle_scale_evaluate()
{
	typedef real32(__thiscall* particle_scale_evaluate_t)(c_particle_sprite_definition_interface*);
	auto function = Memory::GetAddress<particle_scale_evaluate_t>(0x103B14, 0);
	return function(this);
}
