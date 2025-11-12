#include "stdafx.h"
#include "real_quantization.h"

#include "networking/network_event.h"

/* constants */

enum
{
	_face_pos_i = 0,
	_face_pos_j = 1,
	_face_pos_k = 2,
	_face_neg_i = 3,
	_face_neg_j = 4,
	_face_neg_k = 5,
};

/* public code */

int32 quantize_real(real32 value, real32 min_value, real32 max_value, int32 size_in_bits, bool exact_midpoint)
{
	ASSERT(size_in_bits > 0);
	ASSERT(max_value > min_value);
	ASSERT(value >= min_value);
	ASSERT(value <= max_value);

	int32 step_count = (1 << size_in_bits) - 1;
	if (exact_midpoint)
	{
		step_count -= step_count % 2;
	}

	ASSERT(step_count > 0);

	const real32 step = (max_value - min_value) / (real32)step_count;
	ASSERT(step > 0.0f);

	const int32 quantized_value = (int32)(((value - min_value) / step));
	ASSERT(IN_RANGE(quantized_value, 0, step_count));
	return quantized_value;
}

real32 dequantize_real(int32 quantized_value, real32 min_value, real32 max_value, int32 size_in_bits, bool exact_midpoint)
{
	ASSERT(size_in_bits > 0);
	ASSERT(max_value > min_value);
	ASSERT(!exact_midpoint || size_in_bits > 1);

	int32 step_count = (1 << size_in_bits) - 1;
	if (exact_midpoint)
	{
		step_count -= step_count % 2;
	}
	ASSERT(step_count > 0);

	real32 value;
	if (quantized_value)
	{
		if (quantized_value < step_count)
		{
			value = (real32)((((real32)(step_count - quantized_value) * min_value) + ((real32)quantized_value * max_value)) / (real32)step_count);
		}
		else
		{
			value = max_value;
		}
	}
	else
	{
		value = min_value;
	}

	if (exact_midpoint && 2 * quantized_value == step_count)
	{
		ASSERT(value == (max_value + min_value) / 2);
	}
	return value;
}

int32 quantize_unit_vector3d(const real_vector3d* unit_vector)
{
	real32 u, v;
	
	const real_vector3d abs_unit_vector = { ::abs(unit_vector->i), ::abs(unit_vector->j), ::abs(unit_vector->k) };

	int32 type;
	if (abs_unit_vector.i > abs_unit_vector.j
		&& abs_unit_vector.i > abs_unit_vector.k)
	{
		type = unit_vector->i > 0.f ? _face_pos_i : _face_neg_i;
		u = unit_vector->j / abs_unit_vector.i;
		v = unit_vector->k / abs_unit_vector.i;
	}
	else if (abs_unit_vector.j > abs_unit_vector.k)
	{
		type = unit_vector->j > 0.f ? _face_pos_j : _face_neg_j;
		u = unit_vector->i / abs_unit_vector.j;
		v = unit_vector->k / abs_unit_vector.j;
	}
	else
	{
		type = unit_vector->k > 0.f ? _face_pos_k : _face_neg_k;
		u = unit_vector->i / abs_unit_vector.k;
		v = unit_vector->j / abs_unit_vector.k;
	}

	ASSERT(IN_RANGE(u, -1.0f, 1.0f));
	ASSERT(IN_RANGE(v, -1.0f, 1.0f));

	const int32 quantized_f1 = quantize_real(u, -1.f, 1.f, SIZEOF_BITS(uint8), true);
	return type | ((quantized_f1 | (quantize_real(v, -1.f, 1.f, SIZEOF_BITS(uint8), true) << 8)) << 3);
}

void dequantize_unit_vector3d(int32 unit_vector_quantization, real_vector3d* unit_vector)
{
	// value & BYTE_MAX for both quantized values since we want to make sure we only take those 8 bits into account when casting down
	const uint8 quantized_u = (uint8)((unit_vector_quantization >> 3) & BYTE_MAX);
	const uint8 quantized_v = (uint8)((unit_vector_quantization >> 11) & BYTE_MAX);

	real32 dequantized_u = dequantize_real(quantized_u, -1.f, 1.f, SIZEOF_BITS(uint8), true);
	real32 dequantized_v = dequantize_real(quantized_v, -1.f, 1.f, SIZEOF_BITS(uint8), true);

	const int32 face = unit_vector_quantization & ((1 << 3) - 1);
	switch (face)
	{
	case _face_pos_i:
		*unit_vector = { 1.f, dequantized_u, dequantized_v };
		break;
	case _face_pos_j:
		*unit_vector = { dequantized_u, 1.f, dequantized_v };
		break;
	case _face_pos_k:
		*unit_vector = { dequantized_u, dequantized_v, 1.f };
		break;
	case _face_neg_i:
		*unit_vector = { -1.f, dequantized_u, dequantized_v };
		break;
	case _face_neg_j:
		*unit_vector = { dequantized_u, -1.f, dequantized_v };
		break;
	case _face_neg_k:
		*unit_vector = { dequantized_u, dequantized_v, -1.f };
		break;
	default:
		*unit_vector = *global_up3d;
		event(_event_error, "%s: bad face value '%d' when reading unit vector", __FUNCTION__, face);
		break;
	}

	normalize3d(unit_vector);
	return;
}