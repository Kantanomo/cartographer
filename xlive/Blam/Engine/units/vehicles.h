#pragma once
#include "units.h"

/* structures */

struct _vehicle_datum
{
	int8 gap[224];
};

struct vehicle_datum
{
	int32 definition_index;
	_object_datum object;
	_unit_datum unit;
	_vehicle_datum vehicle;
};
ASSERT_STRUCT_SIZE(vehicle_datum, 1088)

/* macros */

#define vehicle_get(index) ((vehicle_datum*)(object_get_and_verify_type((index), _object_mask_vehicle)))
#define vehicle_try_and_get(index) ((vehicle_datum*)(object_try_and_get_and_verify_type((index), _object_mask_vehicle)))
