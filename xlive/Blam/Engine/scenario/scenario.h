#pragma once

/* macros */

#define scenario_get_hs_script(scnr, index) \
((hs_script*)tag_block_get_element_with_size((s_tag_block*)(&((scnr)->hs_scripts)), (index), sizeof(hs_script)))


/* public code */

struct scenario* global_scenario_get(void);

void set_global_scenario(struct scenario* _scenario);

struct collision_bsp* global_collision_bsp_get(void);

int32 global_scenario_index_get(void);

void global_scenario_index_set(datum scenario_index);

void scenario_apply_patches(void);

uint32 scenario_netgame_equipment_size(void);

void location_invalidate(struct s_location* object_location);

void __cdecl scenario_location_from_point(struct s_location* location, real_point3d* point);

bool __cdecl scenario_location_underwater(struct s_location* location, real_point3d* point, int16* global_material_index);

void __cdecl scenario_location_from_leaf(struct s_location* location, int32 leaf_index);
