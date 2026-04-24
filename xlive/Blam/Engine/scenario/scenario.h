#pragma once

/* macros */

#define scenario_get_hs_script(scnr, index) \
(TAG_BLOCK_GET_ELEMENT(&((scnr)->hs_scripts), (index), hs_script))

/* prototypes */

void scenario_apply_patches(void);

struct scenario* global_scenario_get(void);

void set_global_scenario(struct scenario* _scenario);

struct collision_bsp* global_collision_bsp_get(void);

int32 global_scenario_index_get(void);

void global_scenario_index_set(datum scenario_index);

int32 scenario_netgame_equipment_size(void);

void location_invalidate(struct s_location* object_location);

int32 scenario_leaf_index_from_point(real_point3d const* point);

void __cdecl scenario_location_from_point(struct s_location* location, real_point3d* point);

bool __cdecl scenario_location_underwater(struct s_location* location, real_point3d* point, int16* global_material_index);

void __cdecl scenario_location_from_leaf(struct s_location* location, int32 leaf_index);

bool __cdecl scenario_switch_bsp(int16 bsp_index);

void scenario_language_pack_unload(void);
