#include "stdafx.h"
#include "particle_emitter.h"

#include "effects.h"
#include "Blam/Engine/camera/camera.h"
#include "Blam/Engine/main/interpolator.h"
#include "Blam/Engine/objects/objects.h"
#include "Util/Hooks/Hook.h"


s_data_array* get_particle_emitter_table()
{
	return *Memory::GetAddress<s_data_array**>(0x4DD090, 0x5053B8);
}

void c_particle_emitter::adjust_matrix_and_vector_to_effect_camera(bool use_effect_camera, real_matrix3x3* out_matrix, real_point3d* out_point) const
{
	if(use_effect_camera)
	{
		s_camera* effect_camera = get_effect_camera();
		real_matrix3x3 effect_camera_matrix;
		real_matrix3x3 temp_matrix;

		memcpy(&temp_matrix, &this->matrix, sizeof(real_matrix3x3));
		real_point3d temp_point = this->position;
		matrix3x3_from_forward_and_up(&effect_camera_matrix, &effect_camera->forward, &effect_camera->up);
		matrix3x3_multiply(&effect_camera_matrix, &temp_matrix, &temp_matrix);
		matrix3x3_transform_vector(&effect_camera_matrix, &temp_point, &temp_point);

		temp_point.x += effect_camera->point.x;
		temp_point.y += effect_camera->point.y;
		temp_point.z += effect_camera->point.z;

		*out_matrix = temp_matrix;
		*out_point = temp_point;
	}
	else
	{
		*out_matrix = this->matrix;
		*out_point = this->position;
	}
}

void c_particle_emitter::update_children(s_particle_state* particle_state,
	c_particle_system* particle_system, c_particle_emitter_definition* emitter_definition, real32 a5, real32 a6,
	real32 delta, real32 a7)
{
	typedef void(__thiscall* update_children_t)(c_particle_emitter*, s_particle_state*, c_particle_system*, c_particle_emitter_definition*, real32, real32, real32, real32);
	auto function = Memory::GetAddress<update_children_t>(0x104FFE);
	function(this, particle_state, particle_system, emitter_definition, a5, a6, delta, a7);
}

typedef void(__stdcall* c_particle_emitter_update_matrix_and_child_particles_t)(c_particle_emitter* thisx, real32 delta,
	c_particle_system* particle_system, c_particle_emitter_definition* emitter_definition,
	s_particle_state* particle_state, real_matrix4x3* in_martix, real32 alpha);
c_particle_emitter_update_matrix_and_child_particles_t p_c_particle_emitter_update_matrix_and_child_particles;

void c_particle_emitter::update_matrix_and_child_particles(c_particle_emitter* thisx, real32 delta,
                                                           c_particle_system* particle_system, c_particle_emitter_definition* emitter_definition,
                                                           s_particle_state* particle_state, real_matrix4x3* in_martix, real32 alpha)
{
	c_particle_system_definition* particle_system_definition = particle_system->get_particle_system_definition();
	real32 scale = 1.0f;
	effect_datum* effect = (effect_datum*)datum_get(get_effects_table(), particle_system->parent_effect_index);
	object_datum* object = (object_datum*)object_get_fast_unsafe(effect->multi_purpose_origin_index);

	thisx->previous_position = thisx->position;
	if(in_martix)
	{
		if (particle_system->flags_bit_10_is_set())
			scale = in_martix->scale;

		thisx->matrix = in_martix->vectors;
		thisx->position = in_martix->position;
		scale_vector3d(&thisx->position, scale, &thisx->position);

		real_vector3d translated_vector;
		matrix3x3_transform_vector(&thisx->matrix, &emitter_definition->translational_offset, &translated_vector);

		if(fabs(emitter_definition->relative_direction.yaw) >= k_real_math_epsilon ||
			fabs(emitter_definition->relative_direction.pitch) >= k_real_math_epsilon)
		{
			real_matrix3x3 rotations_matrix;
			matrix3x3_create_from_rotations(&rotations_matrix, emitter_definition->relative_direction.yaw, emitter_definition->relative_direction.pitch, 0.0f);
			matrix3x3_multiply(&thisx->matrix, &rotations_matrix, &thisx->matrix);
		}

		add_vectors3d(&thisx->position, &translated_vector, &thisx->position);
	}
	if(!particle_system_definition->system_is_cinematic() || thisx->particle_index == -1)
	{
		thisx->emission_time = emitter_definition->get_particle_emissions_per_tick(particle_state) * delta + thisx->emission_time;
	}
	if(thisx->emission_time + k_real_math_epsilon >= 1.0f)
	{
		bool particle_system_flag_check = (particle_system->flags & 2560) != 0;
		bool particle_system_definition_flag_check = (particle_system_definition->flags >> 7) & 1;

		real32 emitter_time_a = 0.f;
		real32 emitter_time_b = 0.f;


		if(particle_system_flag_check || particle_system_definition_flag_check)
		{
			emitter_time_a = 1.0f / thisx->emission_time;
		}
		else
		{
			emitter_time_b = 1.0f;
			delta = 0.f;
		}

		while(thisx->emission_time + k_real_math_epsilon >= 1.0f)
		{
			thisx->emission_time -= 1.0f;

			//thisx->update_children(particle_state, particle_system, emitter_definition, alpha, emitter_time_b, delta, scale);
			thisx->update_children(particle_state, particle_system, emitter_definition, alpha , 0, delta, scale);

			emitter_time_b += emitter_time_a;
		}
	}
}

__declspec(naked) void c_particle_emitter_update_matrix_and_child_particles_to_stdcall()
{
	__asm
	{
		pop eax
		push ecx
		push eax
		jmp c_particle_emitter::update_matrix_and_child_particles
	}
}

void particle_emitter_apply_patches()
{
	PatchCall(Memory::GetAddress(0x106423), c_particle_emitter_update_matrix_and_child_particles_to_stdcall);
	//p_c_particle_emitter_update_matrix_and_child_particles = (c_particle_emitter_update_matrix_and_child_particles_t)DetourClassFunc((uint8*)Memory::GetAddress(0x105A90), (uint8*)c_particle_emitter::update_matrix_and_child_particles, 10);
}
