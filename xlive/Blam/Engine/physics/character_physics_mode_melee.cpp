#include "stdafx.h"
#include "character_physics_mode_melee.h"

#include "game/game.h"
#include "game/game_time.h"

extern bool g_xbox_tickrate_enabled;

bool melee_lunge_hook_enabled = true;

#define MELEE_DEBUG 0

#if MELEE_DEBUG
#define LOG_TRACE_MELEE(msg, ...) LOG_TRACE_GAME(msg, __VA_ARGS__)
#else
#define LOG_TRACE_MELEE(msg, ...) ((void)0)
#endif

real32 melee_get_acceleration(real32 max_speed_per_tick)
{
	return max_speed_per_tick / 3.0f;
}

// not entirely sure what this calculates
real32 melee_lunge_compute_something_1(real32 max_speed_per_tick, real32 acceleration)
{
	return ((max_speed_per_tick - acceleration) * 3.0f) / 2.0f;
}

real32 __cdecl melee_lunge_get_max_speed_per_tick(real32 tick_length, real32 target_distance, bool weapon_is_sword)
{
	real32 max_velocity = 8.0f;
	if (weapon_is_sword)
		max_velocity = 12.0f;

	real32 t1 = target_distance * 0.375f;
	if (t1 >= k_valid_real_epsilon)
		return MIN(tick_length * max_velocity, t1);
	else
		return k_valid_real_epsilon;
}

// check if this is what this actually does
real32 melee_lunge_get_tick_count(real32 distance, real32 max_speed_per_tick)
{
	real32 f1 = (distance - ((max_speed_per_tick * 4.0f) / 2.0f))
		- melee_lunge_compute_something_1(max_speed_per_tick, melee_get_acceleration(max_speed_per_tick));

	f1 = MAX(f1, 0.0f);

	if (max_speed_per_tick > k_valid_real_epsilon)
		return 6.0f + (f1 / max_speed_per_tick);
	else
		return 6.0f;
}

bool c_character_physics_mode_melee_datum::pin_localized_velocity(real_vector3d* output, real_vector3d* localized_velocity)
{
	real32 output_velocity_magnitude = magnitude3d(output);
	real32 localized_velocity_magnitude = magnitude3d(localized_velocity) + 0.80000001f;

	bool unk_bool = false;

	if (output_velocity_magnitude > localized_velocity_magnitude)
	{
		unk_bool = true;
		scale_vector3d(output, localized_velocity_magnitude / output_velocity_magnitude, output);
	}

	real_vector3d new_vec;
	subtract_vectors3d(localized_velocity, output, &new_vec);

	real32 magnitude = normalize3d(&new_vec);
	if (magnitude > 1.2f)
	{
		real32 dot_product = dot_product3d(output, localized_velocity);
		real32 factor = MIN(magnitude - 1.2f, -dot_product);
		if (factor > 0.0f)
		{
			unk_bool = true;
			scale_vector3d(&new_vec, factor, &new_vec);
			add_vectors3d(&new_vec, output, output);
		}
	}

	if (output->k - localized_velocity->k <= 0.30000001f)
		return unk_bool;

	output->k = localized_velocity->k + 0.30000001f;

	return true;
}

// TODO: 
void c_character_physics_mode_melee_datum::build_initial_melee_parameters(bool valid)
{
}

void c_character_physics_mode_melee_datum::update_melee_parameters()
{
}

void c_character_physics_mode_melee_datum::update_melee_deceleration()
{
}

CLASS_HOOK_DECLARE_LABEL(c_character_physics_mode_melee_datum__update_internal, c_character_physics_mode_melee_datum::update_internal);
void __thiscall c_character_physics_mode_melee_datum::update_internal
(
	s_character_physics_update_output_datum* physics_output,
	bool has_target,
	real32 target_distance,
	const real_vector3d* target_velocity,
	const real_point3d* target_position,
	const real_vector3d* aiming,
	const real_vector3d* velocity,
	const real_point3d* position
)
{
	LOG_TRACE_MELEE("{} : start melee log @ tick {}", __FUNCTION__, m_counter);

	auto p_character_physics_mode_melee_datum_update_internal = Memory::GetAddressRelative<void(__thiscall*)(c_character_physics_mode_melee_datum*, s_character_physics_update_output_datum*, bool, real32, const real_vector3d*, const real_point3d*, const real_vector3d*, const real_vector3d*, const real_point3d*)>(0x50B0D1, 0x4FD701);
	if (!melee_lunge_hook_enabled)
	{
		LOG_TRACE_MELEE("{} : using default halo 2 function!", __FUNCTION__);
		p_character_physics_mode_melee_datum_update_internal(this, physics_output, has_target, target_distance, target_velocity, target_position, aiming, velocity, position);
	}
	else
	{
		// to note this will not force exit if the actual action/animation didn't finish
		bool force_leave_melee_lunge_physics = false;

		if (has_target)
		{
			if (!m_has_target && target_distance > k_valid_real_epsilon)
			{
				real32 distance_unadjusted;
				if (m_maximum_distance == 0.0f)
					distance_unadjusted = target_distance;
				else
					distance_unadjusted = MIN(target_distance, m_maximum_distance);

				real32 max_speed_per_tick_unadjusted = melee_lunge_get_max_speed_per_tick(game_tick_length(), distance_unadjusted, m_weapon_is_sword);

				// this might not actually be melee_lunge_get_tick_count
				real32 lunge_tick_count_unadjusted = melee_lunge_get_tick_count(distance_unadjusted, max_speed_per_tick_unadjusted);

				m_initial_target_velocity_dot_vector = dot_product3d(aiming, target_velocity);

				real_vector3d vector_to_target;
				vector_from_points3d(position, target_position, &vector_to_target);

				real32 unk1 = MIN(MAX(m_initial_target_velocity_dot_vector * 1.5f, 0.0f), 2.5f);

				real32 distance_to_target = magnitude3d(&vector_to_target);
				if (distance_to_target > k_valid_real_epsilon)
				{
					scale_vector3d(&vector_to_target, (target_distance / distance_to_target), &vector_to_target);
				}

				scale_vector3d(target_velocity, game_tick_length() * lunge_tick_count_unadjusted * (unk1 / 2.5f), &m_target_expected_velocity_vector);

				real_vector3d target_velocity_vector;
				add_vectors3d(&vector_to_target, &m_target_expected_velocity_vector, &target_velocity_vector);

				real32 distance_adjusted = 0.0f;
				real32 target_velocity_vector_length = magnitude3d(&target_velocity_vector);

				if (m_maximum_distance == 0.0f)
					distance_adjusted = target_velocity_vector_length;
				else
					distance_adjusted = MIN(target_velocity_vector_length, m_maximum_distance);

				real32 max_speed_per_tick_adjuted = melee_lunge_get_max_speed_per_tick(game_tick_length(), distance_adjusted, m_weapon_is_sword);
				real32 lunge_tick_count_adjusted = melee_lunge_get_tick_count(distance_adjusted, max_speed_per_tick_adjuted);

				field_E = m_maximum_distance != 0.0f && target_velocity_vector_length > m_maximum_distance;
				m_target_distance = field_E ? m_maximum_distance : target_velocity_vector_length;

				m_initial_position = *position;
				m_has_target = true;
				m_maximum_counter = real_to_long_round((m_weapon_is_sword ? 7.f : 1.f) + lunge_tick_count_adjusted);

				if (target_velocity_vector_length > k_valid_real_epsilon)
				{
					point_from_line3d(position, &target_velocity_vector, m_target_distance / target_velocity_vector_length, &m_target_position);
				}
				else
				{
					m_target_position = *position;
				}

				vector_from_points3d(position, &m_target_position, &m_initial_aiming_vector);
				if (normalize3d(&m_initial_aiming_vector) == 0.0f)
				{
					m_initial_aiming_vector = *aiming;
				}
			}

			if (m_has_target)
			{
				// update aiming vectors??
				if (!field_E)
				{
					point_from_line3d(position, &m_initial_aiming_vector, target_distance, &m_target_position);
					point_from_line3d(&m_target_position, &m_initial_aiming_vector, MIN(MAX(0.0f, m_initial_target_velocity_dot_vector), 2.5f) / 5.0f, &m_target_position);
				}

				vector_from_points3d(position, &m_target_position, &m_initial_aiming_vector);
				if (normalize3d(&m_initial_aiming_vector) == 0.0f)
					m_initial_aiming_vector = *aiming;
			}

			if (dot_product3d(&m_initial_aiming_vector, aiming) < 0.86602539f)
				m_initial_aiming_vector = *aiming;
		}

		physics_output->new_velocity = *velocity;

		if (m_has_target)
		{
			// initialize some data
			real32 max_speed_per_tick = melee_lunge_get_max_speed_per_tick(game_tick_length(), m_target_distance, m_weapon_is_sword);
			real32 acceleration = melee_get_acceleration(max_speed_per_tick);

			real32 translational_velocity_magnitude_per_tick = dot_product3d(&m_initial_aiming_vector, velocity) * game_tick_length();

			real32 unk1 = melee_lunge_compute_something_1(max_speed_per_tick, acceleration);
			real32 unk2
				= MAX(0.0f, melee_lunge_compute_something_1(translational_velocity_magnitude_per_tick, acceleration));

			// temp improvement
			unk2 *= game_tick_rate_legacy_ratio_real();

			real_vector3d target_vector;
			vector_from_points3d(position, &m_target_position, &target_vector);
			real32 target_distance_from_vector = magnitude3d(&target_vector);

			real_vector3d current_velocity_per_tick_vector = m_counter == 0 ? *global_zero_vector3d : *velocity;
			scale_vector3d(&current_velocity_per_tick_vector, game_tick_length(), &current_velocity_per_tick_vector);

			// check if we should start decelerating
			if (max_speed_per_tick > k_valid_real_epsilon)
			{
				LOG_TRACE_MELEE("{} m_counter: {}, m_maximum_counter: {}, unk2: {:.3f} target_vector_length: {:.3f}  ",
					__FUNCTION__, m_counter, m_maximum_counter, unk2, target_distance_from_vector);

				if (m_started_decelerating
					|| unk2 > target_distance_from_vector
					|| (real32)(m_maximum_counter - m_counter) <= k_deceleration_ticks_real)
				{
					// deceleration code
					// field_28 is always the same after the first melee tick
					real32 unk3 = m_initial_target_velocity_dot_vector * 1.5f;

					// not entirely sure if this is actually min_velocity_after_deceleration_per_tick
					// but it looks like it
					real32 temp = MAX(MIN(unk3, 3.5f), 0.75f);

					real32 min_velocity_after_deceleration_per_tick = game_tick_length() * temp;

					// magnitude3d might change the floating point unit settings
					// apparently the issue is present in default code as well
					real32 current_velocity_per_tick = magnitude3d(&current_velocity_per_tick_vector);

					real_vector3d current_direction_vector = current_velocity_per_tick_vector;
					real32 normalized_direction_vector_magnitude = normalize3d(&current_direction_vector);

					if (!m_started_decelerating)
					{
						// if we weren't previously decelerating
						// build the deceleration parameters
						m_started_decelerating = true;
						m_deceleration_counter = 0;
						m_velocity_to_decelerate = MAX(min_velocity_after_deceleration_per_tick, current_velocity_per_tick - min_velocity_after_deceleration_per_tick);
						m_distance_to_target_point_before_deceleration = MAX(target_distance_from_vector, k_valid_real_epsilon);
					}

					bool decelerate_allow;
					real32 deceleration_tick_count = k_deceleration_ticks_real;

					if (m_weapon_is_sword)
					{
						// when lunging with the sword, allow it to fully decelerate,
						// hence "k_deceleration_ticks_real + 1.0f"
						deceleration_tick_count += 1.0f;
					}

					decelerate_allow = (real32)m_deceleration_counter < deceleration_tick_count
						|| (real32)(m_maximum_counter - m_counter) > k_deceleration_ticks_real;

					if (dot_product3d(aiming, &current_direction_vector) <= current_velocity_per_tick * 0.087155744f
						|| normalized_direction_vector_magnitude == 0.0f)
					{
						real32 deceleration = MIN((m_velocity_to_decelerate + min_velocity_after_deceleration_per_tick) / 3.0f, current_velocity_per_tick);

						// the best we can do is to reproduce the 30 tick behaviour
						// on the physics velocity vector
						scale_vector3d(
							&current_direction_vector,
							-deceleration * game_tick_rate_legacy_ratio_real(),
							//-deceleration,
							&physics_output->new_velocity);
						add_vectors3d(&physics_output->new_velocity, &current_velocity_per_tick_vector, &physics_output->new_velocity);

						// for some reason in the actual game the following line of code is missing from the actual game
						// no idea why
						//scale_vector3d(&physics_output->out_translational_velocity, time_globals::seconds_to_ticks_precise(1.0f), &physics_output->out_translational_velocity);

						// to reproduce the same behaviour 30 tick has, scale the vector with the difference between the tickrates
						// scale_vector3d(&physics_output->translational_velocity, time_globals::get_ticks_difference_real(), &physics_output->translational_velocity);

						if (min_velocity_after_deceleration_per_tick > ((current_velocity_per_tick - ((m_velocity_to_decelerate + min_velocity_after_deceleration_per_tick) / 3.0f)) + k_valid_real_epsilon))
						{
							force_leave_melee_lunge_physics = true;
							set_time_to_target(0.0f);
						}
					}
					else if (decelerate_allow)
					{
						if (current_velocity_per_tick > k_valid_real_epsilon)
						{
							real32 deceleration = MIN(m_velocity_to_decelerate / k_deceleration_ticks_real, current_velocity_per_tick);

							scale_vector3d(
								&current_direction_vector,
								-deceleration,
								&physics_output->new_velocity);
							add_vectors3d(&physics_output->new_velocity, &current_velocity_per_tick_vector, &physics_output->new_velocity);
							scale_vector3d(&physics_output->new_velocity, game_seconds_to_ticks_real(1.0f), &physics_output->new_velocity);

							set_time_to_target((target_distance_from_vector / unk1) - 0.5f);

							// increase the counter only if not pre-decelerating
							if ((real32)(m_maximum_counter - m_counter) <= k_deceleration_ticks_real)
							{
								m_deceleration_counter++;
							}

							// this does "fix" (more like improve, it still doesn't match 30 tick behaviour) the sword going too hard 
							// but breaks the sword glitches like butterflying and sword flying
							// real32 remaining_time_to_target_real = floor(((current_velocity_per_tick / deceleration) - 0.5f));
							// remaining_time_to_target_real += (remaining_time_to_target_real > 0.0f ? 0.5f : -0.5f);
							// set_time_to_target(remaining_time_to_target_real);
						}
						else
						{
							physics_output->new_velocity = current_velocity_per_tick_vector;
							scale_vector3d(&physics_output->new_velocity, game_seconds_to_ticks_real(1.0f), &physics_output->new_velocity);
							set_time_to_target(0.0f);
							force_leave_melee_lunge_physics = true;
						}
					}
					else
					{
						// maintain velocity						
						physics_output->new_velocity = current_velocity_per_tick_vector;
						scale_vector3d(&physics_output->new_velocity, game_seconds_to_ticks_real(1.0f), &physics_output->new_velocity);
						set_time_to_target(0.0f);
					}

				}
				else if ((real32)m_counter >= 5.0f)
				{
					// maintain current velocity
					physics_output->new_velocity = current_velocity_per_tick_vector;
					scale_vector3d(&physics_output->new_velocity, game_seconds_to_ticks_real(1.0f), &physics_output->new_velocity);

					// FIXME we should use 30hz values here when computing the time to target
					// and adjust it to 60hz or to whatever tickrate where this is used
					// i.e (target_distance_from_vector - unk1) / max_speed_per_tick) is computed using 60hz values + 3.0f ticks, which are 30hz ticks
					set_time_to_target((((target_distance_from_vector - unk1) / max_speed_per_tick) + 3.0f) - 0.5f);
				}
				else
				{
					// acceleration
					real32 final_accel = MIN(acceleration, MAX(0.0f, max_speed_per_tick - MAX(0.0f, translational_velocity_magnitude_per_tick)));

					scale_vector3d(&m_initial_aiming_vector, final_accel, &physics_output->new_velocity);
					add_vectors3d(&physics_output->new_velocity, &current_velocity_per_tick_vector, &physics_output->new_velocity);
					scale_vector3d(&physics_output->new_velocity, game_seconds_to_ticks_real(1.0f), &physics_output->new_velocity);

					real32 time_to_target = ((((
						(target_distance_from_vector - (((((3.0f - (real32)m_counter) - 0.5f) * max_speed_per_tick) * 0.5f) + unk1))
						/ max_speed_per_tick)
						+ (3.0f - (real32)m_counter))
						+ 3.0f)
						+ 0.5f);

					set_time_to_target(time_to_target);

					LOG_TRACE_MELEE("{} - max_speed_per_tick: {}, acceleration: {}, output magnitude: {}",
						__FUNCTION__, max_speed_per_tick, final_accel, magnitude3d(&physics_output->new_velocity));
				}
			}
			else
			{
				m_started_decelerating = true;
				force_leave_melee_lunge_physics = true;
				set_time_to_target(0.0f);
			}
		}

		// build_initial_melee_parameters(a3);
		// update_melee_parameters();

		m_counter++;
		if (((!game_is_campaign() && !g_xbox_tickrate_enabled) && target_distance > 5.0f)
			|| force_leave_melee_lunge_physics
			|| m_time_to_target <= 0
			|| m_counter >= (m_maximum_counter + 6)
			)
		{
			//pin_localized_velocity(&a2->out_translational_velocity, &localized_velocity);
			physics_output->flags |= FLAG(2); // set the flag to let the game know it should get out of the melee lunge physics
		}
		else
		{
			physics_output->flags &= ~FLAG(2); // maintain lunge physics
		}
	}

#if MELEE_DEBUG
	if (m_counter == 1)
	{
		int added_ticks = m_maximum_counter - 6 - (m_weapon_is_sword ? 7 : 1);

		LOG_TRACE_MELEE("{} - update melee at tickrate: {}", __FUNCTION__, game_tick_rate());
		LOG_TRACE_MELEE("{} - added tick count to maximum counter: {}", __FUNCTION__, added_ticks);
		LOG_TRACE_MELEE("{} - target_distance: {}, maximum counter: {}", __FUNCTION__, target_distance, m_maximum_counter);
	}
#endif

#if MELEE_DEBUG
	{
		real_vector3d vector_to_target;
		vector_from_points3d(position, &m_target_position, &vector_to_target);
		real32 remaining_distance = magnitude3d(&vector_to_target);

		real32 unk_real32_distance = dot_product3d(&m_initial_aiming_vector, velocity);
		unk_real32_distance *= game_tick_length();
		real32 acceleration = melee_lunge_get_max_speed_per_tick(game_tick_length(), m_target_distance, m_weapon_is_sword);
		acceleration = melee_get_acceleration(acceleration);
		real32 unk_velocity = MAX(0.0f, melee_lunge_compute_something_1(unk_real32_distance, acceleration));
		real32 log_magnitude = magnitude3d(&physics_output->new_velocity);

		LOG_TRACE_MELEE("{} : output velocity:       i: {}, j: {}, k: {}, magnitude: {}, decelerating?: {}, remaining distance to target: {}",
			__FUNCTION__,
			physics_output->new_velocity.i,
			physics_output->new_velocity.j,
			physics_output->new_velocity.k,
			log_magnitude,
			m_started_decelerating,
			remaining_distance);
	}
#endif

	LOG_TRACE_MELEE("{} : remaining distance in ticks: {} ", __FUNCTION__, m_time_to_target);

	LOG_TRACE_MELEE("{} : target point: x: {} y: {}, z: {}",
		__FUNCTION__,
		m_target_position.x,
		m_target_position.y,
		m_target_position.z);

	LOG_TRACE_MELEE("{} : aiming vector adjusted length (should be 1.0 or a little lower): {}",
		__FUNCTION__,
		magnitude3d(&m_initial_aiming_vector));

	LOG_TRACE_MELEE("{} : previous velocity: {}, previous velocity dot product with aiming vector adjusted: {}",
		__FUNCTION__,
		magnitude3d(velocity),
		dot_product3d(&m_initial_aiming_vector, velocity));

	if ((real32)(m_maximum_counter - m_counter) <= k_deceleration_ticks_real && !m_started_decelerating)
		LOG_TRACE_MELEE("{} : we are about to start decelerating @ next tick : {}", __FUNCTION__, m_counter);

	LOG_TRACE_MELEE("{} : end log for melee @ tick {} \n", __FUNCTION__, m_counter - 1);
}

__declspec(naked) void call_character_melee_physics_input_update_internal()
{
	CLASS_HOOK_JMP(c_character_physics_mode_melee_datum__update_internal, c_character_physics_mode_melee_datum::update_internal);
}

void c_character_physics_mode_melee_datum::apply_hooks()
{
	PatchCall(Memory::GetAddressRelative(0x50BD96, 0x4FE3C6), call_character_melee_physics_input_update_internal);
}

// LOGS
/*

 // 60 Hz / tick

 : start melee log @ tick 0
 m_counter: 0, m_maximum_counter: 24, unk2: 0.000 target_vector_length: 2.734
 - max_speed_per_tick: 0.20000002, acceleration: 0.06666667, output magnitude: 4
 - update melee at tickrate: 60
 - added tick count to maximum counter: 11
 - target_distance: 2.7339647, maximum counter: 24
 : output velocity:       i: -3.999102, j: 0.08475585, k: -0.00020371364, magnitude: 4, decelerating?: false, remaining distance to target: 2.7339644
 : remaining distance in ticks: 17
 : target point: x: -1.0892599 y: -1.5074415, z: -8.489982
 : aiming vector adjusted length (should be 1.0 or a little lower): 1
 : previous velocity: 0.058500007, previous velocity dot product with aiming vector adjusted: -0.0002044205
 : end log for melee @ tick 0

 : start melee log @ tick 1
 m_counter: 1, m_maximum_counter: 24, unk2: 0.000 target_vector_length: 2.601
 - max_speed_per_tick: 0.20000002, acceleration: 0.06666667, output magnitude: 8
 : output velocity:       i: -7.998204, j: 0.16951178, k: -0.00040606022, magnitude: 8, decelerating?: false, remaining distance to target: 2.600744
 : remaining distance in ticks: 16
 : target point: x: -1.0227209 y: -1.5088516, z: -8.487478
 : aiming vector adjusted length (should be 1.0 or a little lower): 1
 : previous velocity: 4, previous velocity dot product with aiming vector adjusted: 4
 : end log for melee @ tick 1

 : start melee log @ tick 2
 m_counter: 2, m_maximum_counter: 24, unk2: 0.100 target_vector_length: 2.467
 - max_speed_per_tick: 0.20000002, acceleration: 0.06666666, output magnitude: 12.000001
 : output velocity:       i: -11.997307, j: 0.25426778, k: -0.00061013555, magnitude: 12.000001, decelerating?: false, remaining distance to target: 2.4674222
 : remaining distance in ticks: 15
 : target point: x: -1.0227326 y: -1.5088514, z: -8.487478
 : aiming vector adjusted length (should be 1.0 or a little lower): 1.0000001
 : previous velocity: 8, previous velocity dot product with aiming vector adjusted: 8.000001
 : end log for melee @ tick 2

 : start melee log @ tick 3
 m_counter: 3, m_maximum_counter: 24, unk2: 0.200 target_vector_length: 2.267
 - max_speed_per_tick: 0.20000002, acceleration: 0, output magnitude: 12.000001
 : output velocity:       i: -11.997307, j: 0.2542678, k: -0.0006101356, magnitude: 12.000001, decelerating?: false, remaining distance to target: 2.2674417
 : remaining distance in ticks: 14
 : target point: x: -1.022752 y: -1.508851, z: -8.487478
 : aiming vector adjusted length (should be 1.0 or a little lower): 1
 : previous velocity: 12.000001, previous velocity dot product with aiming vector adjusted: 12.000001
 : end log for melee @ tick 3

 : start melee log @ tick 4
 m_counter: 4, m_maximum_counter: 24, unk2: 0.200 target_vector_length: 2.067
 - max_speed_per_tick: 0.20000002, acceleration: 0, output magnitude: 12.000001
 : output velocity:       i: -11.997307, j: 0.2542678, k: -0.00061013567, magnitude: 12.000001, decelerating?: false, remaining distance to target: 2.0674663
 : remaining distance in ticks: 12
 : target point: x: -1.0227765 y: -1.5088506, z: -8.487478
 : aiming vector adjusted length (should be 1.0 or a little lower): 1
 : previous velocity: 12.000001, previous velocity dot product with aiming vector adjusted: 12
 : end log for melee @ tick 4

 : start melee log @ tick 5
 m_counter: 5, m_maximum_counter: 24, unk2: 0.200 target_vector_length: 1.867
 : output velocity:       i: -11.997307, j: 0.2542678, k: -0.0006101357, magnitude: 12.000001, decelerating?: false, remaining distance to target: 1.8674982
 : remaining distance in ticks: 10
 : target point: x: -1.0228084 y: -1.50885, z: -8.487478
 : aiming vector adjusted length (should be 1.0 or a little lower): 1
 : previous velocity: 12.000001, previous velocity dot product with aiming vector adjusted: 12.000001
 : end log for melee @ tick 5

 : start melee log @ tick 6
 m_counter: 6, m_maximum_counter: 24, unk2: 0.200 target_vector_length: 1.668
 : output velocity:       i: -11.997307, j: 0.2542678, k: -0.0006101357, magnitude: 12.000001, decelerating?: false, remaining distance to target: 1.6675401
 : remaining distance in ticks: 9
 : target point: x: -1.0228503 y: -1.5088491, z: -8.487478
 : aiming vector adjusted length (should be 1.0 or a little lower): 0.99999994
 : previous velocity: 12.000001, previous velocity dot product with aiming vector adjusted: 12
 : end log for melee @ tick 6

 : start melee log @ tick 7
 m_counter: 7, m_maximum_counter: 24, unk2: 0.200 target_vector_length: 1.468
 : output velocity:       i: -11.997307, j: 0.2542678, k: -0.0006101357, magnitude: 12.000001, decelerating?: false, remaining distance to target: 1.4675959
 : remaining distance in ticks: 8
 : target point: x: -1.0229062 y: -1.508848, z: -8.487478
 : aiming vector adjusted length (should be 1.0 or a little lower): 1
 : previous velocity: 12.000001, previous velocity dot product with aiming vector adjusted: 12.000001
 : end log for melee @ tick 7

 : start melee log @ tick 8
 m_counter: 8, m_maximum_counter: 24, unk2: 0.200 target_vector_length: 1.268
 : output velocity:       i: -11.997307, j: 0.2542678, k: -0.0006101357, magnitude: 12.000001, decelerating?: false, remaining distance to target: 1.2676741
 : remaining distance in ticks: 7
 : target point: x: -1.0229844 y: -1.5088463, z: -8.487479
 : aiming vector adjusted length (should be 1.0 or a little lower): 1.0000001
 : previous velocity: 12.000001, previous velocity dot product with aiming vector adjusted: 12.000002
 : end log for melee @ tick 8

 : start melee log @ tick 9
 m_counter: 9, m_maximum_counter: 24, unk2: 0.200 target_vector_length: 1.068
 : output velocity:       i: -11.997307, j: 0.2542678, k: -0.0006101357, magnitude: 12.000001, decelerating?: false, remaining distance to target: 1.0677873
 : remaining distance in ticks: 6
 : target point: x: -1.0230976 y: -1.5088439, z: -8.487479
 : aiming vector adjusted length (should be 1.0 or a little lower): 1
 : previous velocity: 12.000001, previous velocity dot product with aiming vector adjusted: 12
 : end log for melee @ tick 9

 : start melee log @ tick 10
 m_counter: 10, m_maximum_counter: 24, unk2: 0.200 target_vector_length: 0.868
 : output velocity:       i: -11.997307, j: 0.2542678, k: -0.0006101357, magnitude: 12.000001, decelerating?: false, remaining distance to target: 0.8679607
 : remaining distance in ticks: 5
 : target point: x: -1.023271 y: -1.5088402, z: -8.48748
 : aiming vector adjusted length (should be 1.0 or a little lower): 1
 : previous velocity: 12.000001, previous velocity dot product with aiming vector adjusted: 12.000001
 : end log for melee @ tick 10

 : start melee log @ tick 11
 m_counter: 11, m_maximum_counter: 24, unk2: 0.200 target_vector_length: 0.668
 : output velocity:       i: -11.997307, j: 0.2542678, k: -0.0006101357, magnitude: 12.000001, decelerating?: false, remaining distance to target: 0.668246
 : remaining distance in ticks: 4
 : target point: x: -1.0235562 y: -1.5088342, z: -8.48748
 : aiming vector adjusted length (should be 1.0 or a little lower): 1
 : previous velocity: 12.000001, previous velocity dot product with aiming vector adjusted: 12
 : end log for melee @ tick 11

 : start melee log @ tick 12
 m_counter: 12, m_maximum_counter: 24, unk2: 0.200 target_vector_length: 0.469
 : output velocity:       i: -11.997307, j: 0.2542678, k: -0.0006101357, magnitude: 12.000001, decelerating?: false, remaining distance to target: 0.46876332
 : remaining distance in ticks: 3
 : target point: x: -1.0240735 y: -1.5088233, z: -8.487481
 : aiming vector adjusted length (should be 1.0 or a little lower): 1
 : previous velocity: 12.000001, previous velocity dot product with aiming vector adjusted: 12
 : end log for melee @ tick 12

 : start melee log @ tick 13
 m_counter: 13, m_maximum_counter: 24, unk2: 0.200 target_vector_length: 0.270
 : output velocity:       i: -11.997307, j: 0.2542678, k: -0.0006101357, magnitude: 12.000001, decelerating?: false, remaining distance to target: 0.26985434
 : remaining distance in ticks: 2
 : target point: x: -1.0251642 y: -1.5088001, z: -8.487481
 : aiming vector adjusted length (should be 1.0 or a little lower): 1
 : previous velocity: 12.000001, previous velocity dot product with aiming vector adjusted: 12.000001
 : end log for melee @ tick 13

 : start melee log @ tick 14
 m_counter: 14, m_maximum_counter: 24, unk2: 0.200 target_vector_length: 0.181
 : output velocity:       i: -9.185438, j: 0.1946738, k: -0.0004671352, magnitude: 9.187501, decelerating?: true, remaining distance to target: 0.1812679
 : remaining distance in ticks: 0
 : target point: x: -1.1365528 y: -1.5064394, z: -8.487487
 : aiming vector adjusted length (should be 1.0 or a little lower): 1
 : previous velocity: 12.000001, previous velocity dot product with aiming vector adjusted: 12
 : end log for melee @ tick 14

 30 Hz / tick

 : start melee log @ tick 0
 m_counter: 0, m_maximum_counter: 17, unk2: 0.000 target_vector_length: 2.750
 - max_speed_per_tick: 0.40000007, acceleration: 0.13333336, output magnitude: 4.0000005
 - update melee at tickrate: 30
 - added tick count to maximum counter: 4
 - target_distance: 2.7495363, maximum counter: 17
 : output velocity:       i: -3.9998107, j: 0.03899227, k: -0.00024834403, magnitude: 4.0000005, decelerating?: false, remaining distance to target: 2.7495363
 : remaining distance in ticks: 11
 : target point: x: -1.0938808 y: -1.1434509, z: -8.489973
 : aiming vector adjusted length (should be 1.0 or a little lower): 1
 : previous velocity: 0.11700003, previous velocity dot product with aiming vector adjusted: -0.0004076074
 : end log for melee @ tick 0

 : start melee log @ tick 1
 m_counter: 1, m_maximum_counter: 17, unk2: 0.000 target_vector_length: 2.483
 - max_speed_per_tick: 0.40000007, acceleration: 0.13333336, output magnitude: 8.000002
 : output velocity:       i: -7.9996223, j: 0.07798455, k: -0.0004973178, magnitude: 8.000002, decelerating?: false, remaining distance to target: 2.4829924
 : remaining distance in ticks: 9
 : target point: x: -0.96067655 y: -1.1447494, z: -8.484965
 : aiming vector adjusted length (should be 1.0 or a little lower): 1
 : previous velocity: 4.000001, previous velocity dot product with aiming vector adjusted: 4.000001
 : end log for melee @ tick 1

 : start melee log @ tick 2
 m_counter: 2, m_maximum_counter: 17, unk2: 0.200 target_vector_length: 2.216
 - max_speed_per_tick: 0.40000007, acceleration: 0.13333327, output magnitude: 12.000003
 : output velocity:       i: -11.999433, j: 0.11697678, k: -0.00074688595, magnitude: 12.000003, decelerating?: false, remaining distance to target: 2.2163525
 : remaining distance in ticks: 8
 : target point: x: -0.9607034 y: -1.1447492, z: -8.484965
 : aiming vector adjusted length (should be 1.0 or a little lower): 1
 : previous velocity: 8.000002, previous velocity dot product with aiming vector adjusted: 8.000003
 : end log for melee @ tick 2

 : start melee log @ tick 3
 m_counter: 3, m_maximum_counter: 17, unk2: 0.400 target_vector_length: 1.816
 - max_speed_per_tick: 0.40000007, acceleration: 0, output magnitude: 12.000005
 : output velocity:       i: -11.999434, j: 0.1169768, k: -0.0007468861, magnitude: 12.000005, decelerating?: false, remaining distance to target: 1.8164126
 : remaining distance in ticks: 7
 : target point: x: -0.9607637 y: -1.1447486, z: -8.484965
 : aiming vector adjusted length (should be 1.0 or a little lower): 1
 : previous velocity: 12.000003, previous velocity dot product with aiming vector adjusted: 12.000003
 : end log for melee @ tick 3

 : start melee log @ tick 4
 m_counter: 4, m_maximum_counter: 17, unk2: 0.400 target_vector_length: 1.417
 - max_speed_per_tick: 0.40000007, acceleration: 0, output magnitude: 12.000007
 : output velocity:       i: -11.999436, j: 0.11697681, k: -0.0007468862, magnitude: 12.000007, decelerating?: false, remaining distance to target: 1.4165186
 : remaining distance in ticks: 5
 : target point: x: -0.96086985 y: -1.1447475, z: -8.484965
 : aiming vector adjusted length (should be 1.0 or a little lower): 0.99999994
 : previous velocity: 12.000005, previous velocity dot product with aiming vector adjusted: 12.000005
 : end log for melee @ tick 4

 : start melee log @ tick 5
 m_counter: 5, m_maximum_counter: 17, unk2: 0.400 target_vector_length: 1.017
 : output velocity:       i: -11.999438, j: 0.116976835, k: -0.0007468863, magnitude: 12.000009, decelerating?: false, remaining distance to target: 1.0167291
 : remaining distance in ticks: 4
 : target point: x: -0.96108073 y: -1.1447455, z: -8.484965
 : aiming vector adjusted length (should be 1.0 or a little lower): 1
 : previous velocity: 12.000007, previous velocity dot product with aiming vector adjusted: 12.000007
 : end log for melee @ tick 5

 : start melee log @ tick 6
 m_counter: 6, m_maximum_counter: 17, unk2: 0.400 target_vector_length: 0.617
 : output velocity:       i: -11.99944, j: 0.11697686, k: -0.0007468865, magnitude: 12.0000105, decelerating?: false, remaining distance to target: 0.61725295
 : remaining distance in ticks: 3
 : target point: x: -0.96160483 y: -1.1447403, z: -8.484965
 : aiming vector adjusted length (should be 1.0 or a little lower): 1
 : previous velocity: 12.000009, previous velocity dot product with aiming vector adjusted: 12.000009
 : end log for melee @ tick 6

 : start melee log @ tick 7
 m_counter: 7, m_maximum_counter: 17, unk2: 0.400 target_vector_length: 0.219
 : output velocity:       i: -9.187073, j: 0.08956042, k: -0.00057183503, magnitude: 9.18751, decelerating?: true, remaining distance to target: 0.21923475
 : remaining distance in ticks: 0
 : target point: x: -0.963587 y: -1.144721, z: -8.484965
 : aiming vector adjusted length (should be 1.0 or a little lower): 1
 : previous velocity: 12.0000105, previous velocity dot product with aiming vector adjusted: 12.0000105
 : end log for melee @ tick 7

 : start melee log @ tick 8
 m_counter: 8, m_maximum_counter: 17, unk2: 0.259 target_vector_length: 0.263
 : output velocity:       i: -6.3747053, j: 0.06214398, k: -0.00039678358, magnitude: 6.375008, decelerating?: true, remaining distance to target: 0.26296625
 : remaining distance in ticks: 0
 : target point: x: -1.3135523 y: -1.1413094, z: -8.484987
 : aiming vector adjusted length (should be 1.0 or a little lower): 1
 : previous velocity: 9.18751, previous velocity dot product with aiming vector adjusted: 9.187509
 : end log for melee @ tick 8

 : start melee log @ tick 9
 m_counter: 9, m_maximum_counter: 17, unk2: 0.000 target_vector_length: 0.021
 : output velocity:       i: -0.3916148, j: 0.003817632, k: -2.714671e-05, magnitude: 0.3916334, decelerating?: true, remaining distance to target: 0.021081496
 : remaining distance in ticks: 0
 : target point: x: -0.9870811 y: -1.144492, z: -8.484966
 : aiming vector adjusted length (should be 1.0 or a little lower): 0.99999994
 : previous velocity: 3.204136, previous velocity dot product with aiming vector adjusted: 3.2041357
 : end log for melee @ tick 9

 : start melee log @ tick 10
 m_counter: 10, m_maximum_counter: 17, unk2: 0.000 target_vector_length: 0.176
 : output velocity:       i: -2.7939677e-08, j: 0, k: -1.7053026e-12, magnitude: 2.7939677e-08, decelerating?: true, remaining distance to target: 0.17585947
 : remaining distance in ticks: 0
 : target point: x: -1.1549056 y: -1.142856, z: -8.484974
 : aiming vector adjusted length (should be 1.0 or a little lower): 0.99999994
 : previous velocity: 0.3916334, previous velocity dot product with aiming vector adjusted: 0.3916334
 : end log for melee @ tick 10

***** 60 tickrate SWORD max target distance log end
*/