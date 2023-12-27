#include "stdafx.h"
#include "effects.h"

#include "effects_definitions.h"
#include "Blam/Engine/interface/first_person_weapons.h"
#include "Blam/Engine/main/interpolator.h"
#include "Blam/Engine/objects/objects.h"
#include "Blam/Engine/render/render_sky.h"
#include "Util/Hooks/Hook.h"



datum __cdecl effect_new_from_object(
    datum effect_tag_index,
    s_damage_owner* damage_owner,
    datum object_index,
    real32 a4,
    real32 a5,
    real_rgb_color* color,
    const void* effect_vector_field)
{
    return INVOKE(0xAADCE, 0x9CE4E, effect_new_from_object, effect_tag_index, damage_owner, object_index, a4, a5, color, effect_vector_field);
}

s_data_array* get_effects_table()
{
    return *Memory::GetAddress<s_data_array**>(0x4CE884, 0x4F5070);
}

s_data_array* get_effects_location_table()
{
    return *Memory::GetAddress<s_data_array**>(0x4CE880, 0x4F506C);
}

effect_location_datum* __cdecl effect_location_get_next_valid_index(effect_datum* effect_datum, int32* out_index, int16 a3)
{
    return INVOKE(0xA68DD, 0x9895D, effect_location_get_next_valid_index, effect_datum, out_index, a3);
}

real_matrix4x3* effect_datum_get_node_matrix_realtive_or_origin(e_effect_location_flags flags,effect_datum* effect, bool sky_relative, real_matrix4x3* out_matrix)
{
    if(!sky_relative || flags == -1 || !(TEST_BIT(flags, _effect_location_datum_flags_bit_15)) || effect->origin_local_user_index == -1)
    {
        int16 node_index = -1;
        if(TEST_BIT(flags, _effect_location_datum_flags_bit_15))
        {
            first_person_weapon_get_worldspace_node_matrix(
                effect->origin_local_user_index,
                effect->multi_purpose_origin_index,
                (flags & ~0x8000),
                out_matrix);
            return out_matrix;
        }

        if (flags != -1)
            node_index = (flags & ~0x8000);

        halo_interpolator_interpolate_object_node_matrix(effect->multi_purpose_origin_index, node_index, out_matrix);
        return out_matrix;
    }
    else
    {
        memcpy(out_matrix, 
            first_person_weapon_get_relative_node_matrix(
				effect->origin_local_user_index,
				effect->multi_purpose_origin_index,
				flags & ~0x8000),
            sizeof(real_matrix4x3));

        return out_matrix;
    }
}

real_matrix4x3* effect_location_datum::calculate_origin_matrix(effect_datum* effect, bool sky_relative,
    real_matrix4x3* out_matrix)
{
    if (this->flags == -1 || effect->multi_purpose_origin_index == -1)
        return out_matrix;

    real_matrix4x3 t_matrix{};
    effect_datum_get_node_matrix_realtive_or_origin(this->flags, effect, sky_relative, &t_matrix);
    matrix4x3_multiply(&t_matrix, &this->matrix, out_matrix);

	if (!structure_bsp_test_current_sky_owner_cluster_index(effect->sky_owner_cluster))
        return out_matrix;

	if (effect->multi_purpose_origin_index == -1)
        return out_matrix;


    real_vector3d pos{};
    halo_interpolator_interpolate_object_position(effect->multi_purpose_origin_index, &pos);

    render_sky_modify_node_matrices(&pos, out_matrix, out_matrix, 20);
    return out_matrix;
}

void effects_apply_patches()
{
    //PatchCall(Memory::GetAddress(0xA6990), object_try_get_node_matrix_interpolated);
    //PatchCall(Memory::GetAddress(0xA84DC), object_try_get_node_matrix_interpolated);
}
