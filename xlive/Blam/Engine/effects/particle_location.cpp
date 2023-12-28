#include "stdafx.h"
#include "particle_location.h"

#include "particle_emitter.h"
#include "particle_model_definitions.h"
#include "Blam/Engine/render/render.h"

void c_particle_location::update(c_particle_system* particle_system, c_particle_system_definition* particle_system_definition, real32 delta)
{
	typedef void(__thiscall* particle_location_update_t)(c_particle_location*, c_particle_system*, c_particle_system_definition*, real32);
	auto function = Memory::GetAddress<particle_location_update_t>(0x106055);
	function(this, particle_system, particle_system_definition, delta);
}

void c_particle_location::update_rewritten(c_particle_system* particle_system, c_particle_system_definition* particle_system_definition, real32 delta)
{
	real32 largest_particle_size = 0.f;
	real32 previous_largest_particle_size = 0.f;
	real_vector3d particle_location_position = this->position;
	c_particle_location* thisx = this;
	LOG_DEBUG_GAME("{:X}", (int32)this);
	if (this->particle_emitter_index != NONE)
	{
		datum current_particle_emitter_index = this->particle_emitter_index;
		size_t current_system_emitter_index = 0;
		do
		{
			c_particle_emitter* current_emitter = (c_particle_emitter*)datum_get(get_particle_emitter_table(), current_particle_emitter_index);

			if (current_system_emitter_index < particle_system_definition->emitters.size)
			{
				real32 particle_size = particle_system_definition->emitters[current_system_emitter_index]->particle_size_evaluate();
				if (particle_size > largest_particle_size)
				{
					previous_largest_particle_size = largest_particle_size;
					largest_particle_size = particle_size;
				}

				current_emitter->update_particles(
					particle_system,
					thisx,
					particle_system_definition,
					particle_system_definition->emitters[current_system_emitter_index],
					delta,
					&particle_location_position);

			}
			current_particle_emitter_index = current_emitter->next_emitter_index;
			++current_system_emitter_index;
		} while (current_particle_emitter_index != NONE);
	}
	auto a = get_render_state_count();
	LOG_DEBUG_GAME("{:X}", (int32)this);
	if(this->render_state_index + 1 <  a && !(TEST_BIT(particle_system_definition->flags, 1)))
	{
		datum current_particle_emitter_index = this->particle_emitter_index;
		if(current_particle_emitter_index != NONE)
		{
			do
			{
				c_particle_emitter* current_emitter = (c_particle_emitter*)datum_get(get_particle_emitter_table(), current_particle_emitter_index);

				current_emitter->destroy_particles();

				current_particle_emitter_index = current_emitter->next_emitter_index;
			} while (current_particle_emitter_index != NONE);
		}
	}
	real_vector3d calc_position;
	add_vectors3d(&this->position, &particle_location_position, &calc_position);
	scale_vector3d(&calc_position, 0.5f, &calc_position);

	this->new_position_maybe = calc_position;

	blam_tag::tag_group_type definition_type = tags::datum_to_instance(particle_system->tag_index)->type.tag_type;
	if (definition_type == blam_tag::tag_group_type::particlemodel)
	{
		c_particle_model_definition_interface* system_interface = dynamic_cast<c_particle_model_definition_interface*>(particle_system_definition->get_particle_system_interface());
		this->field_2C = system_interface->particle_scale_evaluate() * largest_particle_size + distance3d(&this->position, &this->new_position_maybe);
	}
	if (definition_type == blam_tag::tag_group_type::particle)
	{
		c_particle_sprite_definition_interface* system_interface = dynamic_cast<c_particle_sprite_definition_interface*>(particle_system_definition->get_particle_system_interface());
		this->field_2C = system_interface->particle_scale_evaluate() * largest_particle_size + distance3d(&this->position, &this->new_position_maybe);
	}
}

s_data_array* get_particle_location_table()
{
	return *Memory::GetAddress<s_data_array**>(0x4DD094, 0);
}
