#pragma once

#define k_real_epsilon 0.0001f
#define _pi M_PI

#define DEGREES_TO_RADIANS(angle) ((real32)((angle) * M_PI / 180.0))
#define RADIANS_TO_DEGREES(angle) ((real32)((angle) * 180.0 / M_PI))

typedef real32 real_angle;

union real_euler_angles2d
{
	real32 n[2];
	struct { real_angle yaw, pitch; };
};
ASSERT_STRUCT_SIZE(real_euler_angles2d, sizeof(real_angle) * 2);

union real_euler_angles3d
{
	real32 n[3];
	struct { real_angle yaw, pitch, roll; };
};
ASSERT_STRUCT_SIZE(real_euler_angles3d, sizeof(real_angle) * 3);

union real_vector2d
{
	real32 n[2];
	struct { real32 i, j; };
};
ASSERT_STRUCT_SIZE(real_vector2d, sizeof(real32) * 2);

union real_point2d
{
	real32 n[2];
	struct { real32 x, y; };
	struct { real32 u, v; };
};
ASSERT_STRUCT_SIZE(real_point2d, sizeof(real32) * 2);

union real_vector3d
{
	real32 n[3];
	struct { real32 i, j, k; };
};
ASSERT_STRUCT_SIZE(real_vector3d, sizeof(real32) * 3);

union real_point3d
{
	real32 n[3];
	struct { real32 x, y, z; };
	struct { real32 u, v, w; };
};
ASSERT_STRUCT_SIZE(real_point3d, sizeof(real32) * 3);

union real_vector4d
{
	real32 n[4];
	struct { real32 i, j, k, l; };
};
ASSERT_STRUCT_SIZE(real_vector4d, sizeof(real32) * 4);

struct real_plane2d
{
	real_vector2d n;
	real32 d;
};
ASSERT_STRUCT_SIZE(real_plane2d, sizeof(real_vector2d) + 4);

struct real_plane3d
{
	real_vector3d n;
	real32 d;
};
ASSERT_STRUCT_SIZE(real_plane3d, sizeof(real_vector3d) + sizeof(real32));

struct real_quaternion
{
	real_vector3d v;
	real32 w;
};
ASSERT_STRUCT_SIZE(real_quaternion, sizeof(real32) * 4);

union real_bounds
{
	real32 v[2];
	struct { real32 lower, upper; };
};
ASSERT_STRUCT_SIZE(real_bounds, sizeof(real32) * 2);

union real_rectangle2d
{
	real32 n[4];
	struct { real32 x0, x1, y0, y1; };
};
ASSERT_STRUCT_SIZE(real_rectangle2d, sizeof(real32) * 4);

union real_rectangle3d
{
	real32 n[6];
	struct { real32 x0, x1, y0, y1, z0, z1; };
};
ASSERT_STRUCT_SIZE(real_rectangle3d, sizeof(real32) * 6);

class c_quantized_orientation
{
public:
	int16 rotation_x;
	int16 rotation_y;
	int16 rotation_z;
	int16 rotation_w;
	real_point3d default_translation;
	real32 default_scale;
};
ASSERT_STRUCT_SIZE(c_quantized_orientation, 24);

struct real_orientation
{
	real_quaternion rotation;
	real_point3d translation;
	real32 scale;
};
ASSERT_STRUCT_SIZE(real_orientation, 32);

union real_matrix3x3
{
	struct { real_vector3d forward, left, up; };
	struct { real32 n[9]; } v;						// Array of values
	real32 matrix[3][3];							// Matrix of values
};
ASSERT_STRUCT_SIZE(real_matrix3x3, 36);

struct real_matrix4x3
{
	real32 scale;
	real_matrix3x3 vectors;
	real_point3d position;
};
ASSERT_STRUCT_SIZE(real_matrix4x3, 52);

/* constants */

extern const real_point2d* const global_origin2d;

extern const real_point2d* const global_x_axis2d;
extern const real_point2d* const global_y_axis2d;

extern const real_point2d* const global_negative_x_axis2d;
extern const real_point2d* const global_negative_y_axis2d;

extern const real_vector2d* const global_zero_vector2d;
extern const real_vector2d* const global_forward2d;
extern const real_vector2d* const global_left2d;

extern const real_vector2d* const global_backward2d;
extern const real_vector2d* const global_right2d;

extern const real_point3d* const global_origin3d;

extern const real_point3d* const global_x_axis3d;
extern const real_point3d* const global_y_axis3d;
extern const real_point3d* const global_z_axis3d;
extern const real_point3d* const global_negative_x_axis3d;
extern const real_point3d* const global_negative_y_axis3d;
extern const real_point3d* const global_negative_z_axis3d;

extern const real_vector3d* const global_zero_vector3d;

extern const real_vector3d* const global_forward3d;
extern const real_vector3d* const global_left3d;
extern const real_vector3d* const global_up3d;

extern const real_vector3d* const global_backward3d;
extern const real_vector3d* const global_right3d;
extern const real_vector3d* const global_down3d;

extern const real_vector4d* const global_zero_vector4d;
extern const real_euler_angles2d* const global_zero_angles2d;
extern const real_euler_angles3d* const global_zero_angles3d;
extern const real_quaternion* const global_identity_quaternion;

extern const real_matrix4x3* const global_identity4x3;
extern const real_matrix4x3* const global_negative_identity4x3;

extern const real_orientation* const global_identity_orientation;

extern const real_rectangle2d* const global_null_rectangle2d;
extern const real_rectangle3d* const global_null_rectangle3d;

/* public code */

inline real32 square_root(real32 f)
{
	return sqrt(f);
}

inline real32 reciprocal_square_root(real32 f)
{
	return 1.0f / square_root(f);
}

inline real32 dot_product2d(const real_vector2d* a, const real_vector2d* b)
{
	return (a->i * b->i) + (a->j * b->j);
}

inline real32 dot_product3d(const real_vector3d* a, const real_vector3d* b)
{
	return a->i * b->i + a->j * b->j + a->k * b->k;
}

inline real32 dot_product4d_quaternion(const real_quaternion* a, const real_quaternion* b)
{
	return a->v.i * b->v.i + a->v.j * b->v.j + a->v.k * b->v.k + a->w * b->w;
}

inline real32 magnitude_squared2d(const real_vector2d* vector)
{
	return vector->i * vector->i + vector->j * vector->j;
}

inline real32 magnitude2d(const real_vector2d* vector)
{
	real32 magnitude_squared = magnitude_squared2d(vector);
	return square_root(magnitude_squared);
}

inline real32 magnitude_squared3d(const real_vector3d* vector)
{
	return vector->i * vector->i + vector->j * vector->j + vector->k * vector->k;
}

inline real32 magnitude3d(const real_vector3d* v)
{
	real32 magnitude_squared = magnitude_squared3d(v);
	return square_root(magnitude_squared);
}

inline bool valid_real(real32 value)
{
	int32 value_as_int = *(int32*)&value;
	return (value_as_int & 0x7F800000) != 0x7F800000;
}

inline bool valid_realcmp(real32 a, real32 b)
{
	real32 result = a - b;
	return valid_real(result) && fabs(result) < 0.01f;
}

inline bool valid_real_normal3d(const real_vector3d* normal)
{
	real32 magnitude = magnitude_squared3d(normal);
	return valid_realcmp(magnitude, 1.0f);
}

inline bool valid_real_point3d(real_point3d const* p)
{
	return valid_real(p->x) && valid_real(p->y) && valid_real(p->z);
}

inline bool valid_real_vector3d_axes3(const real_vector3d* f, const real_vector3d* l, const real_vector3d* u)
{
	return 
		valid_real_normal3d(f) &&
		valid_real_normal3d(l) &&
		valid_real_normal3d(u) &&
		valid_realcmp(dot_product3d(f, l), 0.f) &&
		valid_realcmp(dot_product3d(l, u), 0.f) &&
		valid_realcmp(dot_product3d(u, f), 0.f);
}

inline bool valid_real_matrix4x3(real_matrix4x3 const *matrix)
{
	return 
		valid_real(matrix->scale) &&
		valid_real_vector3d_axes3(&matrix->vectors.forward, &matrix->vectors.left, &matrix->vectors.up) &&
		valid_real_point3d(&matrix->position);
}

inline real_vector2d* scale_vector2d(const real_vector2d* in, real32 scale, real_vector2d* out)
{
	out->i = in->i * scale;
	out->j = in->j * scale;
	return out;
}

inline real_vector3d* scale_vector3d(const real_vector3d* in, real32 scale, real_vector3d* out)
{
	out->i = in->i * scale;
	out->j = in->j * scale;
	out->k = in->k * scale;
	return out;
}

inline real_vector3d* negate_vector3d(const real_vector3d* in, real_vector3d* out)
{
	out->i = -in->i;
	out->j = -in->j;
	out->k = -in->k;
	return out;
}

inline void scale_vector4d(const real_vector4d* in, real32 scale, real_vector4d* out)
{
	out->i = in->i * scale;
	out->j = in->j * scale;
	out->k = in->k * scale;
	out->l = in->l * scale;
	return;
}

inline real_vector3d* add_vectors3d(const real_vector3d* a, const real_vector3d* b, real_vector3d* out)
{
	out->i = a->i + b->i;
	out->j = a->j + b->j;
	out->k = a->k + b->k;
	return out;
}

inline real_vector3d* multiply_vectors3d(const real_vector3d* a, const real_vector3d* b, real_vector3d* out)
{
	out->i = a->i * b->i;
	out->j = a->j * b->j;
	out->k = a->k * b->k;
	return out;
}

inline real_vector3d* subtract_vectors3d(const real_vector3d* a, const real_vector3d* b, real_vector3d* result)
{
	result->i = a->i - b->i;
	result->j = a->j - b->j;
	result->k = a->k - b->k;
	return result;
}

inline real_vector3d* vector_from_points3d(const real_point3d* a, const real_point3d* b, real_vector3d* result)
{
	result->i = b->x - a->x;
	result->j = b->y - a->y;
	result->k = b->z - a->z;
	return result;
}

inline real_point3d* point_from_line3d(const real_point3d* p, const real_vector3d* v, real32 t, real_point3d* result)
{
	real_vector3d direction_scaled;
	scale_vector3d(v, t, &direction_scaled);
	result->x = direction_scaled.i + p->x;
	result->y = direction_scaled.j + p->y;
	result->z = direction_scaled.k + p->z;
	return result;
}

inline real32 normalize3d(real_vector3d* v1)
{
	real32 length = magnitude3d(v1);
	if (abs(length) >= k_real_epsilon)
	{
		scale_vector3d(v1, 1.0f / length, v1);
	}
	else
	{
		length = 0.0f;	// vector already normalized
	}
	return length;
}

inline real_vector3d* cross_product3d(const real_vector3d* a, const real_vector3d* b, real_vector3d* result)
{
	result->i = a->j * b->k - a->k * b->j;
	result->j = a->k * b->i - a->i * b->k;
	result->k = a->i * b->j - a->j * b->i;
	return result;
}

inline void set_real_point3d(real_point3d* point, real32 x, real32 y, real32 z)
{
	point->x = x;
	point->y = y;
	point->z = z;
	return;
}

inline real32 cosine(real32 x)
{
	return cos(x);
}

inline real32 sine(real32 x)
{
	return sin(x);
}

inline real32 tangent(real32 x)
{
	return tan(x);
}

inline real32 arccosine(real32 x)
{
	ASSERT(x >= -1.f - k_real_epsilon && x <= +1.f + k_real_epsilon);

	// Pin parameter between -1 and 1
	return acos(PIN(x, -1.f, 1.f));
}

inline real32 arcsine(real32 x)
{
	ASSERT(x >= -1.f - k_real_epsilon && x <= +1.f + k_real_epsilon);

	// Pin parameter between -1 and 1
	return asin(PIN(x, -1.f, 1.0f));
}

inline real32 arctangent(real32 a, real32 b)
{
	return atan2(a, b);
}


void __cdecl real_math_initialize(void);

void __cdecl real_math_reset_precision(void);

real32 distance_squared2d(const real_point2d* a, const real_point2d* b);

real32 distance2d(const real_point2d* a, const real_point2d* b);

real32 normalize2d(real_vector2d* v);

real_vector2d* perpendicular2d(const real_vector2d* a, real_vector2d* result);

real_quaternion* quaternion_normalize(real_quaternion* q);

real_quaternion* fast_quaternion_interpolate_and_normalize(const real_quaternion* previous, const real_quaternion* current, real32 fractional_ticks, real_quaternion* quaternion);

real32 normalize3d_with_default(real_vector3d* a, const real_vector3d* b);

bool valid_real_vector3d_axes2(const real_vector3d* forward, const real_vector3d* up);

real32 distance_squared3d(const real_point3d* p1, const real_point3d* p2);

real32 distance3d(const real_point3d* p1, const real_point3d* p2);

bool limit3d(real_vector3d* v, real32 limit);

real_point3d* points_interpolate(const real_point3d* a, const real_point3d* b, real32 t, real_point3d* result);

real32 scale_interpolate(real32 previous_scale, real32 current_scale, real32 fractional_tick, real32* out_scale);

real_vector3d* __cdecl perpendicular3d(const real_vector3d* in, real_vector3d* out);

real_vector3d* __cdecl generate_up_vector3d(const real_vector3d* forward, real_vector3d* up);

void vectors3d_from_euler_angles3d(real_vector3d* forward, real_vector3d* up, const real_euler_angles3d* angles);

void vector3d_from_euler_angles2d(real_vector3d* forward, const real_euler_angles2d* angles);

/* macros */

#ifdef VALIDATE_REAL_ENABLED
#define assert_valid_real(value)	\
if (!valid_real(value))				\
{									\
	const char* string = csnprintf(g_temporary, NUMBEROF(g_temporary), NUMBEROF(g_temporary), "%s: assert_valid_real(0x%08X %f)", #value, value, value);	\
	DISPLAY_ASSERT(string);			\
}									\
(void)0

#define assert_valid_real_normal3d(normal)		\
if (!valid_real_normal3d(normal))				\
{												\
	const char* string = csprintf(g_temporary, NUMBEROF(g_temporary), "%s: assert_valid_real_normal3d(%f, %f, %f)", #normal, (*normal).i, (*normal).j, (*normal).k);	\
	DISPLAY_ASSERT(string);						\
}												\
(void)0

#define assert_valid_real_vector3d_axes2(forward, up)	\
if (!valid_real_vector3d_axes2(forward, up))			\
{														\
	const char* string = csprintf(g_temporary, NUMBEROF(g_temporary), "%s, %s: assert_valid_real_vector3d_axes2(%f, %f, %f / %f, %f, %f)", #forward, #up, (*forward).i, (*forward).j, (*forward).k, (*up).i, (*up).j, (*up).k);	\
	DISPLAY_ASSERT(string);								\
}														\
(void)0

#define assert_valid_real_matrix4x3(matrix)		\
if (!valid_real_matrix4x3(matrix))				\
{												\
	if (!valid_real(*matrix.scale))				\
	{											\
		const char* string = csprintf(g_temporary, NUMBEROF(g_temporary), "%s had a bad scale %f", #matrix, *matrix.scale);	\
		DISPLAY_ASSERT(string);					\
	}											\
	if (!valid_real_normal3d(matrix.vectors.forward))	\
	{											\
		const char* string = csprintf(g_temporary, NUMBEROF(g_temporary), "%s had a bad forward (%f,%f,%f)", #matrix, *matrix.vectors.forward.i, *matrix.vectors.forward.j, *matrix.vectors.forward.k);	\
		DISPLAY_ASSERT(string);					\
	}											\
	if (!valid_real_normal3d(matrix.vectors.left))		\
	{											\
		const char* string = csprintf(g_temporary, NUMBEROF(g_temporary), "%s had a bad left (%f,%f,%f)", #matrix, *matrix.vectors.left.i, *matrix.vectors.left.j, *matrix.vectors.left.k);	\
		DISPLAY_ASSERT(string);					\
	}											\
	if (!valid_real_normal3d(matrix.vectors.forward))	\
	{											\
		const char* string = csprintf(g_temporary, NUMBEROF(g_temporary), "%s had a bad up (%f,%f,%f)", #matrix, *matrix.vectors.up.i, *matrix.vectors.up.j, *matrix.vectors.up.k);	\
		DISPLAY_ASSERT(string);					\
	}											\
	if (!valid_real_point3d(matrix.position))	\
	{											\
		const char* string = csprintf(g_temporary, NUMBEROF(g_temporary), "%s had a bad up (%f,%f,%f)", #matrix, *matrix.position.x, *matrix.position.y, *matrix.position.z);	\
		DISPLAY_ASSERT(string);					\
	}											\
	if (!valid_realcmp(dot_product3d(matrix.vectors.forward, matrix.vectors.left), 0.0))	\
	{											\
		const char* string = csprintf(g_temporary, NUMBEROF(g_temporary), "%s had a forward (%f,%f,%f) not perpendicular to left (%f,%f,%f)", \
										#matrix,\
										*matrix.vectors.forward.i, *matrix.vectors.forward.j, *matrix.vectors.forward.k,	\
										*matrix.vectors.left.i, *matrix.vectors.left.j, *matrix.vectors.left.k);			\
		DISPLAY_ASSERT(string);					\
	}											\
	if (!valid_realcmp(dot_product3d(matrix.vectors.up, matrix.vectors.left), 0.0))	\
	{											\
		const char* string = csprintf(g_temporary, NUMBEROF(g_temporary), "%s had a up (%f,%f,%f) not perpendicular to left (%f,%f,%f)", \
										#matrix,\
										*matrix.vectors.up.i, *matrix.vectors.up.j, *matrix.vectors.up.k,			\
										*matrix.vectors.left.i, *matrix.vectors.left.j, *matrix.vectors.left.k);	\
		DISPLAY_ASSERT(string);					\
	}											\
	if (!valid_realcmp(dot_product3d(matrix.vectors.forward, matrix.vectors.up), 0.0))	\
	{											\
		const char* error_string = csprintf(g_temporary, NUMBEROF(g_temporary), "%s had a forward (%f,%f,%f) not perpendicular to up (%f,%f,%f)", \
										#matrix,\
										*matrix.vectors.forward.i, *matrix.vectors.forward.j, *matrix.vectors.forward.k,	\
										*matrix.vectors.up.i, *matrix.vectors.up.j, *matrix.vectors.up.k);					\
		DISPLAY_ASSERT(error_string);			\
	}											\
	if (!valid_real_matrix4x3(matrix))			\
	{											\
		const char* string = csprintf(g_temporary, NUMBEROF(g_temporary), "%s: assert_valid_real_matrix4x3", #matrix);	\
		DISPLAY_ASSERT(string);					\
	}											\
}												\
(void)0
#else
#define assert_valid_real(value)						(void)0
#define assert_valid_real_normal3d(normal)				(void)0
#define assert_valid_real_vector3d_axes2(forward, up)	(void)0
#define assert_valid_real_matrix4x3(matrix)				(void)0
#endif