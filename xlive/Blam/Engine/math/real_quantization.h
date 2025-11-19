#pragma once

/* prototypes */

int32 quantize_real(real32 value, real32 min_value, real32 max_value, int32 size_in_bits, bool exact_midpoint);

real32 dequantize_real(int32 quantized_value, real32 min_value, real32 max_value, int32 size_in_bits, bool exact_midpoint);

int32 quantize_unit_vector3d(const real_vector3d* unit_vector);

void dequantize_unit_vector3d(int32 unit_vector_quantization, real_vector3d* unit_vector);
