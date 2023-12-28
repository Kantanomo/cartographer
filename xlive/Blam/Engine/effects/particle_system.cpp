#include "stdafx.h"
#include "particle_system.h"

#include "effects_definitions.h"
#include "particle.h"
#include "particle_emitter.h"
#include "particle_location.h"
#include "particle_model_definitions.h"
#include "particle_system_definition.h"
#include "Blam/Engine/game/game_time.h"
#include "Blam/Engine/physics/breakable_surface_definitions.h"
#include "Blam/Engine/render/render_visibility_collection.h"
#include "Blam/Engine/tag_files/global_string_ids.h"
#include "H2MOD/Tags/TagInterface.h"
#include "Util/Hooks/Hook.h"

int particle_system_game_time[k_max_particle_systems];
real32 particle_system_accumulated_time[k_max_particle_systems];

s_data_array* get_particle_system_table()
{
	return *Memory::GetAddress<s_data_array**>(0x4D83D4, 0x4FFEA8);
}

void particle_system_update_update_game_time(datum datum_index)
{
	particle_system_game_time[DATUM_INDEX_TO_ABSOLUTE_INDEX(datum_index)] = time_globals::get_game_time();
}

c_particle_system_definition* c_particle_system::get_particle_system_definition() const
{
	blam_tag tag_type = tags::datum_to_instance(this->tag_index)->type;

	if (tag_type.tag_type == blam_tag::tag_group_type::effect)
	{
		effect_definition* effect = (effect_definition*)tags::get_tag_fast(this->tag_index);
		return effect->events[this->effect_event_index]->particle_systems[this->event_particle_system_index];
	}
	if (tag_type.tag_type == blam_tag::tag_group_type::particlemodel)
	{
		c_particle_system_definition* parent_system_definition = this->parent_system->get_particle_system_definition();
		c_particle_model_definition_interface* system_interface = dynamic_cast<c_particle_model_definition_interface*>(parent_system_definition->get_particle_system_interface());
		return system_interface->get_attached_particle_system(this->event_particle_system_index);
	}
	if(tag_type.tag_type == blam_tag::tag_group_type::particle)
	{
		c_particle_system_definition* parent_system_definition = this->parent_system->get_particle_system_definition();
		c_particle_sprite_definition_interface* system_interface = dynamic_cast<c_particle_sprite_definition_interface*>(parent_system_definition->get_particle_system_interface());
		return system_interface->get_attached_particle_system(this->event_particle_system_index);
	}
	if (tag_type.tag_type == blam_tag::tag_group_type::breakablesurface)
	{
		s_breakable_surface_definition* breakable_surface = (s_breakable_surface_definition*)tags::get_tag_fast(this->tag_index);
		return breakable_surface->particle_effects[this->event_particle_system_index];
	}
	return nullptr;
}
typedef bool(__stdcall* c_particle_system_update_t)(c_particle_system* thisx, real32 dt);
c_particle_system_update_t p_c_particle_system_update;

const real32 tolerance = 1e-5f;
bool __stdcall c_particle_system::update(c_particle_system* thisx, real32 delta_time)
{
	bool result = thisx->update_rewritten(delta_time);//p_c_particle_system_update(thisx, delta_time);

	datum particle_system_datum_index = ((char*)thisx - get_particle_system_table()->data) / sizeof(c_particle_system);
	particle_system_datum_index |= (thisx->datum_salt << 16);
	if(!result)
	{
		real32* accum = &particle_system_accumulated_time[DATUM_INDEX_TO_ABSOLUTE_INDEX(particle_system_datum_index)];
		real32 max_accum = ((thisx->duration != 0.0f) ? 1.0f / thisx->duration : 0.0f) + time_globals::get_seconds_per_tick();
		real32 difference = max_accum - *accum;
		int32 ticks = time_globals::get_game_time() - particle_system_game_time[DATUM_INDEX_TO_ABSOLUTE_INDEX(particle_system_datum_index)];
		
		if (difference <= tolerance && ticks >= 1)
		{
			particle_system_game_time[DATUM_INDEX_TO_ABSOLUTE_INDEX(particle_system_datum_index)] = 0;
			*accum = 0.f;
			return false;
		}
		else
		{
			particle_system_accumulated_time[DATUM_INDEX_TO_ABSOLUTE_INDEX(particle_system_datum_index)] += delta_time;
			return true;
		}
	}
	particle_system_accumulated_time[DATUM_INDEX_TO_ABSOLUTE_INDEX(particle_system_datum_index)] += delta_time;
	return  result;
}

typedef void(__stdcall* t_c_particle_system__update_location_time)(c_particle_system*, s_particle_system_update_timings*, real_matrix4x3*, int );

t_c_particle_system__update_location_time p_c_particle_system__update_location_time;

void c_particle_system::update_location_time(c_particle_system* thisx, s_particle_system_update_timings* timings, real_matrix4x3* matrix, int unused)
{
	datum particle_system_datum_index = ((char*)thisx - get_particle_system_table()->data) / sizeof(c_particle_system);
	particle_system_datum_index |= (thisx->datum_salt << 16);
	particle_system_update_update_game_time(particle_system_datum_index);
	p_c_particle_system__update_location_time(thisx, timings, matrix, unused);
}


void __cdecl c_particle_system::destroy(datum particle_system_index)
{
	INVOKE(0xC3C49, 0xB0B34, c_particle_system::destroy, particle_system_index);
}

void c_particle_system::destroy_children()
{
	typedef void(__thiscall* destroy_children_t)(c_particle_system*);
	auto function = Memory::GetAddress<destroy_children_t>(0xc37de, 0);
	function(this);
}

bool c_particle_system::update_rewritten(real32 delta_time)
{
	datum particle_system_datum_index = ((char*)this - get_particle_system_table()->data) / sizeof(c_particle_system);
	particle_system_datum_index |= (this->datum_salt << 16);


 	if (this->flags.test(_particle_system_flags_bit_9) && this->flags.test(_particle_system_flags_bit_5))
		this->flags.set(_particle_system_flags_bit_11, true);
	else
		this->flags.set(_particle_system_flags_bit_11, false);

	if (this->flags.test(_particle_system_flags_bit_8))
		this->flags.set(_particle_system_flags_bit_9, true);
	else
		this->flags.set(_particle_system_flags_bit_9, false);

	this->flags.set(_particle_system_flags_bit_8, false);

	if(!this->flags.test(_particle_system_flags_bit_5) && !render_visibility_check_location_cluster_active(&this->location))
	{
		this->destroy_children();
	}

	c_particle_system_definition* particle_system_definition = this->get_particle_system_definition();

	datum current_particle_location_index = this->particle_system_location_index;
	if (current_particle_location_index != NONE) {
		do
		{
			c_particle_location* particle_location = (c_particle_location*)datum_get(get_particle_location_table(), current_particle_location_index);

			particle_location->update(this, particle_system_definition, delta_time);
			//particle_location->update_rewritten(this, particle_system_definition, delta_time);

			current_particle_location_index = particle_location->next_particle_location;
		} while (current_particle_location_index != NONE);
	}
	effect_location_definition* location_definitions = nullptr;
	size_t location_definitions_size = 0;

	blam_tag::tag_group_type definition_type = tags::datum_to_instance(this->tag_index)->type.tag_type;

	if(definition_type == blam_tag::tag_group_type::particlemodel)
	{
		c_particle_model_definition_interface* system_interface = dynamic_cast<c_particle_model_definition_interface*>(particle_system_definition->get_particle_system_interface());
		location_definitions = system_interface->get_particle_definition_locations();
		location_definitions_size = system_interface->get_particle_definition_locations_size();

	}
	if(definition_type == blam_tag::tag_group_type::particle)
	{
		c_particle_sprite_definition_interface* system_interface = dynamic_cast<c_particle_sprite_definition_interface*>(particle_system_definition->get_particle_system_interface());
		location_definitions = system_interface->get_particle_definition_locations();
		location_definitions_size = system_interface->get_particle_definition_locations_size();
	}

	datum current_particle_system_index = this->next_particle_system;
	if (current_particle_system_index != NONE)
	{
		do
		{
			c_particle_system* current_particle_system = (c_particle_system*)datum_get(get_particle_system_table(), current_particle_system_index);

			c_particle* particle = (c_particle*)datum_get(get_particle_table(), current_particle_system->first_particle_index);
			if (particle)
			{
				real_matrix4x3 marker_matrix;
				real_vector3d interp_velocity;
				real_vector3d interp_position;
				halo_particle_interpolator_get_interpolated_velocity(current_particle_system->first_particle_index, &interp_velocity);
				halo_particle_interpolator_get_interpolated_position(current_particle_system->first_particle_index, &interp_position);
				if (current_particle_system->definition_location_index == NONE || current_particle_system->definition_location_index >= location_definitions_size)
				{
					
					scale_vector3d(&interp_velocity, -1.f, &marker_matrix.vectors.forward);
					normalize3d(&marker_matrix.vectors.forward);
					perpendicular3d(&marker_matrix.vectors.forward, &marker_matrix.vectors.up);
					cross_product3d(&marker_matrix.vectors.up, &marker_matrix.vectors.forward, &marker_matrix.vectors.left);
				}
				else
				{
					string_id marker_name = location_definitions[current_particle_location_index].marker_name;

					switch (marker_name.get_id())
					{
						case HS_UP:
							marker_matrix.vectors.forward = global_up3d;
							marker_matrix.vectors.up = global_forward3d;
							marker_matrix.vectors.left = global_left3d;
							break;
						case HS_GRAVITY:
							marker_matrix.vectors.forward = global_down3d;
							marker_matrix.vectors.up = global_back3d;
							marker_matrix.vectors.left = global_left3d;
							break;
						default:
							scale_vector3d(&interp_velocity, -1.f, &marker_matrix.vectors.forward);
							normalize3d(&marker_matrix.vectors.forward);
							perpendicular3d(&marker_matrix.vectors.forward, &marker_matrix.vectors.up);
							cross_product3d(&marker_matrix.vectors.up, &marker_matrix.vectors.forward, &marker_matrix.vectors.left);
							break;
					}
				}

				marker_matrix.position = interp_position;
				marker_matrix.scale = 1.0f;

				s_particle_system_update_timings location_update_timing;
				location_update_timing.some_delta_calc = (particle->time_accumulator / particle->effect_duration) - delta_time;
				location_update_timing.current_delta = delta_time;
				location_update_timing.particle_system_location_index = current_particle_system->particle_system_location_index;

				bool definition_one_shot = false;
				bool definition_one_shot_or_v_mirrored = false;

				if (definition_type == blam_tag::tag_group_type::particlemodel)
				{
					c_particle_model_definition_interface* system_interface = dynamic_cast<c_particle_model_definition_interface*>(current_particle_system->get_particle_system_definition()->get_particle_system_interface());
					definition_one_shot = system_interface->particle_is_one_shot();
					definition_one_shot_or_v_mirrored = system_interface->particle_is_v_mirrored_or_one_shot();
				}
				if (definition_type == blam_tag::tag_group_type::particle)
				{
					c_particle_sprite_definition_interface* system_interface = dynamic_cast<c_particle_sprite_definition_interface*>(current_particle_system->get_particle_system_definition()->get_particle_system_interface());
					definition_one_shot = system_interface->particle_is_one_shot();
					definition_one_shot_or_v_mirrored = system_interface->particle_is_v_mirrored_or_one_shot();
				}

				current_particle_system->update_colors(definition_one_shot_or_v_mirrored, definition_one_shot, this->color, global_black_pixel32);

				c_particle_system::update_location_time(current_particle_system, &location_update_timing, &marker_matrix, 0);
				//current_particle_system->update_location_time(&location_update_timing, &marker_matrix, 0);

			}
			if (!current_particle_system->update_rewritten(delta_time))
			{
				current_particle_system->adjust_particle_system_indexes(&this->next_particle_system, &this->datum_44);
				c_particle_system::destroy(current_particle_system_index);
			}

			current_particle_system_index = current_particle_system->field_38;
		} while (current_particle_system_index != NONE);
	}
	this->flags.set(_particle_system_flags_updating_bit, false);
	auto active_count = this->get_active_particle_locations_count();

	bool flag_check = (this->flags.test(_particle_system_flags_bit_4) ||
		this->flags.test(_particle_system_flags_updating_bit) ||
		this->next_particle_system != -1 ||
		(((int16)this->flags & 2560) != 0));
	//LOG_INFO_GAME("{:X} {} {}", particle_system_datum_index, active_count, flag_check);
	bool result = active_count > 0 && flag_check;



	if (!result)
	{
		real32* accum = &particle_system_accumulated_time[DATUM_INDEX_TO_ABSOLUTE_INDEX(particle_system_datum_index)];
		real32 max_accum = ((this->duration != 0.0f) ? 1.0f / this->duration : 0.0f) + time_globals::get_seconds_per_tick();
		real32 difference = max_accum - *accum;
		int32 ticks = time_globals::get_game_time() - particle_system_game_time[DATUM_INDEX_TO_ABSOLUTE_INDEX(particle_system_datum_index)];

		if (difference <= tolerance && ticks >= 5)
		{
			particle_system_game_time[DATUM_INDEX_TO_ABSOLUTE_INDEX(particle_system_datum_index)] = 0;
			*accum = 0.f;
			return false;
		}
		else
		{
			particle_system_accumulated_time[DATUM_INDEX_TO_ABSOLUTE_INDEX(particle_system_datum_index)] += delta_time;
			return true;
		}
	}
	particle_system_accumulated_time[DATUM_INDEX_TO_ABSOLUTE_INDEX(particle_system_datum_index)] += delta_time;
	return  result;
}

void c_particle_system::update_colors(bool v_mirrored_or_one_shot, bool one_shot, pixel32 color, pixel32 color_2)
{
	typedef void(__thiscall* update_colors_t)(c_particle_system*, bool, bool, pixel32, pixel32);
	auto function = Memory::GetAddress<update_colors_t>(0xC381F, 0);
	function(this, v_mirrored_or_one_shot, one_shot, color, color_2);
}

void c_particle_system::update_colors_and_get_location(real32 flt_1, real32 flt_2, pixel32 color, pixel32 color_2,
	s_particle_system_update_timings* timings)
{
	typedef void(__thiscall* update_colors_and_get_location_t)(c_particle_system*, real32, real32, pixel32, pixel32, s_particle_system_update_timings*);
	auto function = Memory::GetAddress<update_colors_and_get_location_t>(0xC3DD5);
	function(this, flt_1, flt_2, color, color_2, timings);
}

void c_particle_system::adjust_particle_system_indexes(datum* datum_1, datum* datum_2)
{
	typedef void(__thiscall* adjust_particle_system_indexes_t)(c_particle_system*, datum*, datum*);
	auto function = Memory::GetAddress<adjust_particle_system_indexes_t>(0xC35F7, 0);
	function(this, datum_1, datum_2);
}

void c_particle_system::update_locations(s_particle_system_update_timings* timings, real_matrix4x3* matrix,
	bool has_bit_15)
{
	typedef void(__thiscall* update_locations_t)(c_particle_system*, s_particle_system_update_timings*, real_matrix4x3*, bool);
	auto function = Memory::GetAddress<update_locations_t>(0xC376A, 0);
	function(this, timings, matrix, has_bit_15);
}

int c_particle_system::get_active_particle_locations_count()
{
	/*typedef int(__thiscall* adjust_particle_system_indexes_t)(c_particle_system*);
	auto function = Memory::GetAddress<adjust_particle_system_indexes_t>(0xC37A8, 0);
	return function(this);*/
	datum current_location_index = this->particle_system_location_index;
	int32 location_count = 0;
	if (current_location_index != NONE)
	{
		do
		{
			c_particle_location* particle_location = (c_particle_location*)datum_get(get_particle_location_table(), current_location_index);
			datum current_emitter_index = particle_location->particle_emitter_index;
			int32 emitter_count = 0;
			if (current_emitter_index != NONE)
			{
				do
				{
					c_particle_emitter* particle_emitter = (c_particle_emitter*)datum_get(get_particle_emitter_table(), current_emitter_index);

					if (particle_emitter->active)
						++emitter_count;

					current_emitter_index = particle_emitter->next_emitter_index;
				} while (current_emitter_index != NONE);
			}
			if (emitter_count > 0)
				++location_count;
			current_location_index = particle_location->next_particle_location;
		} while (current_location_index != NONE);
	}
	return location_count;
}

bool c_particle_system::flags_bit_10_is_set() const
{
	return ((int16)this->flags >> 10) & 1;
}

//void c_particle_system::update_location_time(s_particle_system_update_timings* timings, real_matrix4x3* matrix,
//	int unused)
//{
//	typedef void(__thiscall* update_location_time_t)(c_particle_system*, s_particle_system_update_timings*, real_matrix4x3*, int);
//	auto function = Memory::GetAddress<update_location_time_t>(0xC3E32, 0);
//	function(this, timings, matrix, unused);
//}


typedef void(__stdcall* c_particle_system_update_effect_time_t)(c_particle_system* thisx, real32 flt);
c_particle_system_update_effect_time_t pc_particle_system_update_effect_time;

void __stdcall c_particle_system::update_effect_time(c_particle_system* thisx, real32 flt)
{
	thisx->flags.set(_particle_system_flags_updating_bit, true);
	thisx->accumulated_time = 0;
	real32 time = 1.0;
	if (flt > 0.0f)
		time = 1.0f / (flt);

	thisx->duration = time;
}

void __cdecl particle_syste_remove_from_effects_cache(datum effect_index, datum particle_system_index)
{
	INVOKE(0xA69EB, 0x98A6B, particle_syste_remove_from_effects_cache, effect_index, particle_system_index);
}

void particle_location_update(c_particle_location* location, c_particle_system* system, c_particle_system_definition* definition, real32 delta)
{
	typedef void(__thiscall* particle_location_update_t)(c_particle_location*, c_particle_system*, c_particle_system_definition*, real32);
	auto function = Memory::GetAddress<particle_location_update_t>(0xc37de, 0);
	function(location, system, definition, delta);
}

void __cdecl particle_initialize()
{
	memset(&particle_system_game_time, 0, sizeof(particle_system_game_time));
	memset(&particle_system_accumulated_time, 0, sizeof(particle_system_accumulated_time));
	INVOKE(0x1045D8, 0, particle_initialize);
}

void apply_particle_system_patches()
{
	//pc_particle_system_update_effect_time = (c_particle_system_update_effect_time_t)DetourClassFunc((uint8*)Memory::GetAddress(0xC373B), (uint8*)c_particle_system::update_effect_time, 10);
	//p_c_particle_system_update = (c_particle_system_update_t)DetourClassFunc((uint8*)Memory::GetAddressRelative(0x4C43F9), (uint8*)c_particle_system::update, 10);
	p_c_particle_system__update_location_time = (t_c_particle_system__update_location_time)DetourClassFunc((uint8*)Memory::GetAddress(0xC3E32), (uint8*)c_particle_system::update_location_time, 11);
	//WriteValue(Memory::GetAddress(0x10496D), 256 * 4);  // Emitters
	//WriteValue(Memory::GetAddress(0x1045E3), 1024 * 4); // Particles
	//WriteValue(Memory::GetAddress(0x105DEE), 256 * 4);  // Particles Locations
	//WriteValue(Memory::GetAddress(0xc34ee), 128 * 4);  // Particles Systems
}

