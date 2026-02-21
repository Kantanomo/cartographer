#pragma once
#include "items.h"

/* constants */

enum
{
	MAXIMUM_NUMBER_OF_MAGAZINES_PER_WEAPON = 2,
	MAXIMUM_NUMBER_OF_MAGAZINE_OBJECTS_PER_MAGAZINE = 8,
	k_weapon_trigger_count = 2,
	k_weapon_barrel_count = 2,
	k_weapon_barrel_effect_count = 3
};

/* enums */

enum e_weapon_datum_barrel_flags : uint16
{
	_weapon_datum_barrel_fire_barrel_next_update,

	k_weapon_datum_barrel_flags_count
};

/* structures */

struct s_weapon_datum_barrel
{
	int8 data_0[4];
	bool field_4;
	bool field_5;
	int8 data_6[2];
	c_flags_no_init<e_weapon_datum_barrel_flags, uint16, k_weapon_datum_barrel_flags_count> flags;
	int16 field_A;
	int8 data3[4];
	int32 recovery_timer;
	real32 reload_ticks_maybe;
	int8 data2[28];
};

struct s_weapon_magazine
{
	int16 weapon_state;
	int16 field_2;
	int16 field_4;
	int16 rounds_inventory_maximum;
	int16 rounds_loaded_maximum;
	int16 field_A;
	int16 field_C;
	int16 field_E;
};
ASSERT_STRUCT_SIZE(s_weapon_magazine, 16);

struct _weapon_datum
{
	int16 weapon_flags;
	uint16 control_flags;
	int8 field_16C[12];
	int16 first_person_animation_duration_ticks;
	int8 field_17E[6];
	real32 field_184;
	real32 field_188;
	int8 field_18C[18];
	int16 turn_on_time_ticks;
	s_weapon_datum_barrel barrels[k_weapon_barrel_count];
	int8 field_202[26];
	int16 field_222;
	s_weapon_magazine magazines[MAXIMUM_NUMBER_OF_MAGAZINES_PER_WEAPON];
	int8 field_22E[24];
};

struct weapon_datum
{
	datum definition_index;
	_object_datum object;
	_item_datum item;
	_weapon_datum weapon;
};
ASSERT_STRUCT_SIZE(weapon_datum, 604);

/* prototypes */

int32 __cdecl weapon_get_rounds_total(datum object_index, int32 magazine_index, bool a3);

void __cdecl weapons_fire_barrels(void);

void weapons_apply_patches();

/* macros */

#define weapon_get(index) ((weapon_datum*)(object_get_and_verify_type((index), _object_mask_weapon)))
#define weapon_try_and_get(index) ((weapon_datum*)(object_try_and_get_and_verify_type((index), _object_mask_weapon)))
