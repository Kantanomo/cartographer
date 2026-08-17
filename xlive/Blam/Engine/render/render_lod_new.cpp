#include "stdafx.h"
#include "render_lod_new.h"

#include "render.h"

#include "cache/cache_files.h"
#include "game/player_control.h"
#include "memory/data.h"
#include "models/render_models.h"
#include "models/render_model_definitions.h"
#include "objects/objects.h"
#include "objects/object_globals.h"
#include "rasterizer/rasterizer_memory.h"
#include "render/render_objects.h"

#include "H2MOD/Modules/Shell/Config.h"


static e_scenery_lightmapping_policy render_lod_new_get_scenery_lightmapping_policy(datum object_index);


uint32 render_object_cache_create_index()
{
	uint32 result = (*get_global_window_bound_index() << 30) | (*global_frame_index_get() & 0x3FFFFFFF);
	return result;
}

data_array* get_cached_object_render_states_array()
{
    return *Memory::GetAddress<data_array**>(0x4EC384);
}

int8 render_object_cache_get_level_of_detail(datum object_index)
{
    int8 result = NONE;
    if (*get_global_window_bound_index() < 4)
    {
        result = INVOKE_TYPE(0x196364, 0x0, int8*(__cdecl*)(datum), object_index)[*get_global_window_bound_index()];
    }
    return result;
}

int8* __cdecl render_object_cache_get_unk1(datum object_index)
{
    return INVOKE(0x196322, 0x0, render_object_cache_get_unk1, object_index);
}

s_render_cache_storage* __cdecl render_object_cache_get_render_state(datum object_index)
{
    return INVOKE(0x196397, 0x0, render_object_cache_get_render_state, object_index);
}

bool render_object_cache_storage_is_object_cached(s_render_cache_storage* storage)
{
    bool result = false;

    if (storage != NULL
        && storage->rasterizer_cpu_render_cache_offset != NONE
        && storage->render_frame_allocated == render_object_cache_create_index())
    {
        result = true;
    }

    return result;
}

// Read-only mirror of the engine's object_get_cached_render_lighting (halo2.exe 0x5969FF):
// the same ultimate-parent walk and, critically, the same AUTHORITATIVE validity gate -- the
// lighting-valid byte at cache entry+3, which is what tells you entry+84 actually holds this
// object's computed lighting. (render_object_cache_storage_is_object_cached above answers a
// different question: it gates the rasterizer GEOMETRY cache fields, so an entry can pass it
// while the lighting is still a previous tenant's or uninitialised.)
//
// Deliberately NOT a call to the engine function: that one routes through
// object_get_cached_render_state (0x59604D), which is an allocator -- on a miss it does
// datum_new_in_range and, when the pool is full, LRU-EVICTS another object's entry, re-inits
// it (entry[3] = 0) and calls render_object_update_change_colors. Invoking that from a render
// hook would thrash live entries and hand back freshly invalidated lighting anyway.
render_lighting* render_object_cache_get_lighting(datum object_index)
{
    // entry layout, 256-byte stride (from object_get_cached_render_state, halo2.exe 0x59604D)
    const int32 k_cached_render_state_lighting_valid_offset = 3;
    const int32 k_cached_render_state_owner_offset = 4;
    const int32 k_cached_render_state_storage_offset = 68;

    while (true)
    {
        const object_datum* object = object_get(object_index);
        if (object == NULL)
        {
            return NULL;
        }
        if (object->object.flags.test(_object_uses_cinematic_lighting_bit))
        {
            // CONFIRMED render_lighting-shaped (was: "never confirmed to carry a shadow_direction,
            // so callers get NULL"). Overlaying render_lighting on s_object_globals + 0x1C maps
            // every field onto a real member, with nothing left over:
            //
            //   +0  ambient                 -> cinematic_ambient_color          (0x1C)
            //   +12 shadow_direction        -> cinematic_primary_light_vector_1 (0x28)
            //   +24 lighting_accuracy       -> cinematic_primary_light_unk1     (0x34)
            //   +28 shadow_opacity          -> cinematic_primary_light_unk2     (0x38)
            //   +32 primary_direction_color -> cinematic_primary_light_color    (0x3C)
            //   +44 primary_direction       -> cinematic_primary_light_vector   (0x48)
            //   +56 / +68 secondary         -> cinematic_secondary_light_*      (0x54/0x60)
            //   +80 sh_index                -> field_6C                         (0x6C)
            //
            // 0x1C + sizeof(render_lighting) == 0x70, exactly where global_function_values begins.
            // cinematic_lighting_set_primary_light (halo2.exe 0x530900) confirms it is POPULATED as
            // one: it builds a vector from the script's yaw/pitch, negates it in place into
            // primary_direction (+44), copies that same negated vector into shadow_direction (+12),
            // sets lighting_accuracy and shadow_opacity to 0.8, and stores sh_index = 0xFFFF (NONE).
            // A negated light vector is exactly what shadow_direction means, and 0.8 is a usable
            // opacity -- so this is real data, not a coincidental shape. The engine's own signature
            // returns this address as render_lighting*, so stock consumers already read it this way.
            //
            // Safe when the script never ran: the blob is then all zeros, and the caster loop's
            // zero-length shadow_direction guard falls back to the fixed sun AND the default
            // opacity, so the worst case is a default-lit shadow rather than none.
            //
            // Returning NULL here made every cinematic-lit object count as `nolighting` and cast no
            // shadow. The live log (17/08) shows nolighting rejecting 2 of 3 eligible casters with
            // every other counter at zero, so this may account for a large share of that.
            s_object_globals* globals = object_globals_get();
            if (globals == NULL)
            {
                return NULL;
            }
            return (render_lighting*)&globals->cinematic_ambient_color;
        }
        if (object->object.parent_object_index == NONE)
        {
            break;
        }
        datum parent_index = object_get_ultimate_parent(object_index);
        if (parent_index == NONE || parent_index == object_index)
        {
            break;
        }
        object_index = parent_index;
    }

    const object_datum* object = object_get(object_index);
    if (object == NULL || object->object.cached_render_state_index == NONE)
    {
        return NULL;
    }

    uint8* entry = (uint8*)datum_get(get_cached_object_render_states_array(),
        object->object.cached_render_state_index);
    if (entry == NULL)
    {
        return NULL;
    }
    // the engine's own reuse test: the entry may have been recycled to another object
    if (*(datum*)(entry + k_cached_render_state_owner_offset) != object_index)
    {
        return NULL;
    }
    if (entry[k_cached_render_state_lighting_valid_offset] == 0)
    {
        return NULL;
    }

    return &((s_render_cache_storage*)(entry + k_cached_render_state_storage_offset))->lighting;
}

void __cdecl render_object_update_change_colors(datum object_index, bool a2)
{
    INVOKE(0x195F91, 0x0, render_object_update_change_colors, object_index, a2);
    return;
}

int16 __cdecl sub_59CC02_to_usercall(
    datum object_index,
    real32 a2,
    int32 a3,
    bool* first_person,
    bool a5,
    int8* a6,
    s_render_object_info* render_info)
{
    int16 result;

    void* sub_59CC02 = Memory::GetAddress<void*>(0x19CC02);
    __asm
    {
        mov esi, a6
        push esi

        mov al, a5
        push eax
        
        mov esi, first_person
        push esi
        
        push a3
        push a2
        push object_index
        mov esi, render_info
        call sub_59CC02
        add esp, 4 * 6
        mov result, ax
    }
    return result;
}

bool __cdecl sub_59D024_to_usercall(
    int32* region_count,
    int16* desired_lod,
    uint8* section_indices,
    datum render_model_index,
    datum object_index,
    bool first_person,
    int8 current_lod,
    bool* a8)
{
    bool result;
    void* sub_59D024 = Memory::GetAddress<void*>(0x19D024);
    __asm
    {
        push a8

        mov dl, current_lod
        push edx
        
        mov dl, first_person
        push edx

        push object_index
        mov esi, render_model_index
        mov edi, section_indices
        mov ecx, desired_lod
        mov edx, region_count
        call sub_59D024
        add esp, 4 * 4
        mov result, al
    }
    return result;
}

int32 __cdecl sub_77DCF6_get_unk_count(datum model_index, int32 level_of_detail, uint8* region_section_indices)
{
    return INVOKE(0x37DCF6, 0x0, sub_77DCF6_get_unk_count, model_index, level_of_detail, region_section_indices);
}

int32 render_object_cache_get_pool_size(int32 object_model_count)
{
    int32 result = 0;

    if (object_model_count)
    {
        result = 68 + object_model_count * 48;
    }

    return result;
}

int __cdecl sub_77DEBD(
    datum object_index,
    datum model_definition_index,
    const real_matrix4x3* input_object_node_matrices,
    uint8* section_indices,
    int32 level_of_detail,
    bool first_person,
    int32 a7,
    void* skinning)
{
    return INVOKE(0x37DEBD, 0x0, sub_77DEBD, object_index, model_definition_index, input_object_node_matrices, section_indices, level_of_detail, first_person, a7, skinning);
}

bool __cdecl sub_59EB0C(
    int32 a1,
    prt_info* prt,
    int32 level_of_detail,
    datum object_index,
    void* object_render_cache_pool,
    int32 a6,
    uint8* section_indices,
    datum object_model_index)
{
    return INVOKE(0x19EB0C, 0x0, sub_59EB0C, a1, prt, level_of_detail, object_index, object_render_cache_pool, a6, section_indices, object_model_index);
}

render_lighting* __cdecl sub_597370(datum object_index, real32 a2)
{
    return INVOKE(0x197370, 0x0, sub_597370, object_index, a2);
}

void __cdecl object_build_render_cache_and_info(
    datum object_index,
    int32 a2,
    real32 a3,
    real32 a4,
    s_render_object_info* info,
    int32 a6,
    bool force_no_fp_object,
    bool use_skinning_matrices)
{
    int8 desired_object_lod = NONE;
    int32 skipped_object_count = 0;

    int16 render_model_count = sub_59CC02_to_usercall(
        object_index,
        a3,
        a2,
        &info->first_person,
        force_no_fp_object,
        &desired_object_lod,
        info);

    info->level_of_detail = desired_object_lod;

    ASSERT(VALID_INDEX(render_model_count, MAXIMUM_RENDER_MODELS_PER_OBJECT));

    if (info->first_person 
        && *global_user_render_index_get() != NONE 
        && player_control_get_zoom_level(*global_user_render_index_get()) != NONE)
    {
        render_model_count = 0;
    }

    info->object_count = 0;
    info->scenery_lightmapping_policy = render_lod_new_get_scenery_lightmapping_policy(object_index);
   
    for (int32 i = 0; i < render_model_count; ++i)
    {
        int32 render_model_storage_index = i - skipped_object_count;

        uint32 flags;
        int32 object_node_count;
        const real_matrix4x3* object_node_matrices;

        bool object_is_cached = false;
        bool object_render_valid = true;

        real_matrix4x3 skinning_matrices[MAXIMUM_NODES_PER_MODEL];

        render_object_get_model_node_data(
            object_index,
            info->first_person,
            (int16)render_model_storage_index,
            &flags,
            &info->object_index[render_model_storage_index],
            &info->render_model_tag_defs[render_model_storage_index],
            &object_node_matrices,
            &object_node_count
            );

        int8 cached_lod = render_object_cache_get_level_of_detail(info->object_index[render_model_storage_index]);

        // this just gets called
        render_object_cache_get_unk1(info->object_index[render_model_storage_index]);

        info->render_info[render_model_storage_index] = render_object_cache_get_render_state(info->object_index[render_model_storage_index]);
        info->field_70[render_model_storage_index] = TEST_BIT(flags, 0);

        ASSERT(!info->render_info[render_model_storage_index] || info->render_info[render_model_storage_index]->context == info->object_index[render_model_storage_index]);

        // partial missing code from Halo 2 Vista
        object_is_cached = render_object_cache_storage_is_object_cached(info->render_info[render_model_storage_index]) && cached_lod != NONE;
        if (object_is_cached)
        {
            info->level_of_detail = cached_lod;
        }

        bool using_old_permutation;
        bool result_boolean1 = sub_59D024_to_usercall(
            &info->field_18[render_model_storage_index],
            &info->level_of_detail,
            &info->field_75[16 * render_model_storage_index],
            info->render_model_tag_defs[render_model_storage_index],
            object_index,
            info->first_person,
            cached_lod,
            &using_old_permutation
        );

        bool lod_mismatch = info->level_of_detail != desired_object_lod;
        object_is_cached = object_is_cached && !using_old_permutation;

        if (object_is_cached && lod_mismatch)
        {
            object_is_cached = false;
        }

        if (i > 0 && lod_mismatch || !result_boolean1)
        {
            skipped_object_count++;
            continue;
        }

        int32 unk_count1 = sub_77DCF6_get_unk_count(
            info->render_model_tag_defs[render_model_storage_index],
            info->level_of_detail,
            &info->field_75[16 * render_model_storage_index]
            );

        info->field_2C[render_model_storage_index] = (uint8)unk_count1;
        info->rasterizer_pool_offsets[render_model_storage_index] = NONE;

        if (!object_is_cached || info->first_person)
        {
            // allocate and initialize the cache, because object is not cached

            uint32 allocated_pool_offset = rasterizer_pool_allocate(
                0,
                1,
                render_object_cache_get_pool_size(info->field_2C[render_model_storage_index])
            );

            info->rasterizer_pool_offsets[render_model_storage_index] = allocated_pool_offset;

            if (allocated_pool_offset == NONE)
            {
                object_render_valid = false;
            }
            else
            {
				if (info->render_info[render_model_storage_index]
					&& info->rasterizer_pool_offsets[render_model_storage_index] != NONE
					&& !info->first_person
					&& !using_old_permutation
					&& !lod_mismatch)
				{
					// set the cache up
                    s_render_cache_storage* render_cache_storage = info->render_info[render_model_storage_index];

                    render_cache_storage->render_frame_allocated = render_object_cache_create_index();
                    render_cache_storage->rasterizer_cpu_render_cache_offset = info->rasterizer_pool_offsets[render_model_storage_index];
				}

                if (use_skinning_matrices)
                {
                    object_get_skinning_matrices(object_index, object_node_count, object_node_matrices, skinning_matrices);
                    object_node_matrices = skinning_matrices;
                }

                sub_77DEBD(
                    object_index,
                    info->render_model_tag_defs[render_model_storage_index],
                    object_node_matrices,
                    &info->field_75[16 * render_model_storage_index],
                    info->level_of_detail,
                    info->first_person,
                    unk_count1,
                    rasterizer_pool_get_from_offset(info->rasterizer_pool_offsets[render_model_storage_index])
                );
			}
        }

        if (object_render_valid)
        {
            if (object_is_cached)
            {
                // more of missing code...
                if (!info->first_person && *get_global_window_bound_index() != NONE)
                {
                    s_render_cache_storage* render_cache_storage = info->render_info[render_model_storage_index];

                    ASSERT(*get_global_window_bound_index() == (int32)(info->render_info[render_model_storage_index]->render_frame_allocated >> 30));

                    info->rasterizer_pool_offsets[render_model_storage_index] = render_cache_storage->rasterizer_cpu_render_cache_offset;
                }
            }
            else
            {
                if (TEST_BIT(info->field_170, 6))
                {
                    render_model_definition* render_model = (render_model_definition*)tag_get_fast(info->render_model_tag_defs[render_model_storage_index]);
                    prt_info* prt = render_model->prt_info[0];

                    object_render_valid = sub_59EB0C(
                        a6,
                        prt,
                        info->level_of_detail,
                        info->object_index[render_model_storage_index],
                        rasterizer_pool_get_from_offset(info->rasterizer_pool_offsets[render_model_storage_index]),
                        info->field_18[render_model_storage_index],
                        &info->field_75[16 * render_model_storage_index],
                        info->render_model_tag_defs[render_model_storage_index]
                    );
                }
				
                render_object_update_change_colors(info->object_index[render_model_storage_index], false);

				if (info->render_info[render_model_storage_index] != NULL)
				{
					s_render_cache_storage* render_cache_storage = info->render_info[render_model_storage_index];
					render_cache_storage->lighting = *sub_597370(info->object_index[render_model_storage_index], a4);
				}
            }
        }

        if (object_render_valid)
        {
            ++info->object_count;
            const object_datum* render_object = object_get(info->object_index[render_model_storage_index]);
            if (render_object->object.cached_render_state_index != NONE)
            {
                uint8* cached_object_render_state = (uint8*)datum_get(get_cached_object_render_states_array(), render_object->object.cached_render_state_index);

                if (*(datum*)(cached_object_render_state + 4) == info->object_index[render_model_storage_index])
                {
                    *(uint32*)(cached_object_render_state + 12) = *global_frame_index_get();
                }
            }
        }
        else
        {
            skipped_object_count++;
        }
    }
}

void* object_render_calculate_lod_usercall = NULL;
__declspec(naked) void object_render_calculate_lod_nak_to_usercall()
{
    __asm
    {
        // ### TODO check if this is needed when using a static LOD, might save on some processor time
        // other usercall registers are setup, push the single stack variable
        mov eax, dword ptr[esp + 4]
        push eax
        call object_render_calculate_lod_usercall
        add esp, 4

        cmp H2Config_static_lod_state, 0
        jz END_DETOUR

        mov al, H2Config_static_lod_state
        sub al, 1 // convert setting to in-game model LOD value (0 - 5, L1 - L6)

        END_DETOUR:
        ret
    }
}

static e_scenery_lightmapping_policy render_lod_new_get_scenery_lightmapping_policy(datum object_index)
{
    e_scenery_lightmapping_policy result = _scenery_lightmapping_policy_none;
    if (object_get_type(object_index) == _object_type_scenery)
    {
        const scenery_datum* scenery = (scenery_datum*)object_try_and_get_and_verify_type(object_index, _object_mask_scenery);
        result = (e_scenery_lightmapping_policy)scenery->scenery.lightmapping_policy;
    }

    return result;
}

void render_lod_new_apply_patches()
{
    PatchCall(Memory::GetAddress(0xBCC69), object_build_render_cache_and_info);
    PatchCall(Memory::GetAddress(0xBCFC3), object_build_render_cache_and_info);

    object_render_calculate_lod_usercall = Memory::GetAddress<void*>(0x19CA3E);
    PatchCall(Memory::GetAddress<void*>(0x19CDA3), object_render_calculate_lod_nak_to_usercall);
}