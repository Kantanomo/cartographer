#pragma once

// TODO move this into object_types.h but currently getting issues
// Has to do with header recursion and errors that pop up with tag definitions?

enum e_object_type : int32
{
	_object_type_none = NONE,
	_object_type_biped = 0,
	_object_type_vehicle = 1,
	_object_type_weapon = 2,
	_object_type_equipment = 3,
	_object_type_garbage = 4,
	_object_type_projectile = 5,
	_object_type_scenery = 6,
	_object_type_machine = 7,
	_object_type_control = 8,
	_object_type_light_fixture = 9,
	_object_type_sound_scenery = 10,
	_object_type_crate = 11,
	_object_type_creature = 12,

	k_object_types_count,
	NUMBER_OF_OBJECT_TYPES = k_object_types_count,	// Duplicate constant? Both names appear in halo 2

	_object_mask_all = NONE,
	_object_mask_unit = FLAG(_object_type_biped) | FLAG(_object_type_vehicle),
	_object_mask_biped = FLAG(_object_type_biped),
	_object_mask_vehicle = FLAG(_object_type_vehicle),

	_object_mask_item = FLAG(_object_type_weapon) | FLAG(_object_type_equipment) | FLAG(_object_type_garbage),

	_object_mask_weapon = FLAG(_object_type_weapon),
	_object_mask_equipment = FLAG(_object_type_equipment),
	_object_mask_garbage = FLAG(_object_type_garbage),
	_object_mask_projectile = FLAG(_object_type_projectile),
	_object_mask_scenery = FLAG(_object_type_scenery),
	_object_mask_sound_scenery = FLAG(_object_type_sound_scenery),

	_object_mask_device = FLAG(_object_type_machine) | FLAG(_object_type_control) | FLAG(_object_type_light_fixture),
	_object_mask_machine = FLAG(_object_type_machine),
	_object_mask_control = FLAG(_object_type_control),
	_object_mask_light_fixture = FLAG(_object_type_light_fixture),

	_object_mask_crate = FLAG(_object_type_crate),

	_object_mask_creature = FLAG(_object_type_creature),

	_object_mask_sightblocking = FLAG(_object_type_vehicle) | FLAG(_object_type_scenery) | FLAG(_object_type_machine),
	_object_mask_cannot_interpolate = FLAG(_object_type_sound_scenery) | FLAG(_object_type_light_fixture) | FLAG(_object_type_control) | FLAG(_object_type_machine) | FLAG(_object_type_scenery) | FLAG(_object_type_projectile),
	_object_mask_havok = FLAG(_object_type_creature) | FLAG(_object_type_crate) | FLAG(_object_type_machine) | FLAG(_object_type_vehicle) | FLAG(_object_type_biped)

	
};

inline e_object_type operator++(e_object_type& type)
{
	return (e_object_type)++((int32&)type);
};
