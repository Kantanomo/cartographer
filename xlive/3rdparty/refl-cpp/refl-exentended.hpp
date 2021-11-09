#pragma once
#include "refl-cpp.hpp"
#include "magicenum/magic_enum.hpp"


namespace tag_refl
{
	struct refl_tag_base : refl::attr::usage::member {};
	struct refl_tag_ref : refl::attr::usage::member {};
	struct refl_tag_block : refl::attr::usage::member {};
	struct refl_real_bounds : refl::attr::usage::member{};
	struct refl_real_point3d : refl::attr::usage::member{};
	struct refl_real_vector3d : refl::attr::usage::member {};
	struct refl_data_block : refl::attr::usage::member{};
	struct refl_real_color_rgb : refl::attr::usage::member{};
	struct refl_real_color_argb : refl::attr::usage::member {};
	struct refl_blam_tag : refl::attr::usage::member{};
	struct refl_angle : refl::attr::usage::member{};

	struct property : refl::attr::usage::field{};
	struct refl_string_id : refl::attr::usage::field {};
}
#define TAG_REFL_BASE_STRUCT(property_name) REFL_FIELD(property_name, tag_refl::refl_tag_base())
#define TAG_REFL_PROPERTY(property_name) REFL_FIELD(property_name, tag_refl::property())
#define TAG_REFL_TAG_REFERENCE(property_name) REFL_FIELD(property_name, tag_refl::refl_tag_ref())
#define TAG_REFL_STRING_ID(property_name) REFL_FIELD(property_name, tag_refl::refl_string_id())
#define TAG_REFL_TAG_BLOCK(property_name) REFL_FIELD(property_name, tag_refl::refl_tag_block())
#define TAG_REFL_REAL_BOUNDS(property_name) REFL_FIELD(property_name, tag_refl::refl_real_bounds())
#define TAG_REFL_REAL_POINT3D(property_name) REFL_FIELD(property_name, tag_refl::refl_real_point3d())
#define TAG_REFL_REAL_VECTOR3D(property_name) REFL_FIELD(property_name, tag_refl::refl_real_vector3d())
#define TAG_REFL_DATA_BLOCK(property_name) REFL_FIELD(property_name, tag_refl::refl_data_block())
#define TAG_REFL_REAL_COLOR_RGB(property_name) REFL_FIELD(property_name, tag_refl::refl_real_color_rgb())
#define TAG_REFL_REAL_COLOR_ARGB(property_name) REFL_FIELD(property_name, tag_refl::refl_real_color_argb())
#define TAG_REFL_BLAM_TAG(property_name) REFL_FIELD(property_name, tag_refl::refl_blam_tag())
#define TAG_REFL_ANGLE(property_name) REFL_FIELD(property_name, tag_refl::refl_angle())

#define TAG_REFL(struct_name) REFL_TYPE(struct_name, refl_impl::metadata::debug(debug_generic_discard<struct_name>), bases<>)
#define TAG_REFL_TAG_BLOCK_DEF(struct_name) \
	REFL_TYPE(tag_block<struct_name>) \
		REFL_FIELD(size, tag_refl::property()) \
		REFL_FIELD(data, tag_refl::property()) \
		REFL_FUNC(data_size, refl::attr::property{}) \
		REFL_FUNC(type_size, refl::attr::property{}) \
		REFL_FUNC(begin, refl::attr::property{}) \
		REFL_FUNC(end, refl::attr::property{}) \
		REFL_FUNC(operator[], refl::attr::property{}) \
	REFL_END \
	TAG_REFL(struct_name)


template<typename T>
void debug_generic_discard(std::ostream& os, const T& pt) {}

REFL_TYPE(tag_reference, refl_impl::metadata::debug(debug_generic_discard<tag_reference>), bases<>)
	TAG_REFL_BLAM_TAG(TagGroup)
	REFL_FIELD(TagIndex, tag_refl::property())
REFL_END

REFL_TYPE(real_bounds, refl_impl::metadata::debug(debug_generic_discard<real_bounds>), bases<>)
	REFL_FIELD(lower, tag_refl::property())
	REFL_FIELD(upper, tag_refl::property())
REFL_END

TAG_REFL(real_vector3d)
	REFL_FIELD(x, tag_refl::property())
	REFL_FIELD(y, tag_refl::property())
	REFL_FIELD(z, tag_refl::property())
REFL_END

TAG_REFL(real_color_rgb)
	REFL_FIELD(red, tag_refl::property())
	REFL_FIELD(green, tag_refl::property())
	REFL_FIELD(blue, tag_refl::property())
REFL_END

TAG_REFL(real_color_argb)
	REFL_FIELD(alpha, tag_refl::property())
	REFL_FIELD(red, tag_refl::property())
	REFL_FIELD(green, tag_refl::property())
	REFL_FIELD(blue, tag_refl::property())
REFL_END

TAG_REFL(blam_tag)
	REFL_FIELD(tag_type, tag_refl::property())
REFL_END

TAG_REFL(data_block)
	REFL_FIELD(block_size, tag_refl::property())
	REFL_FIELD(block_offset, tag_refl::property())
REFL_END

TAG_REFL(angle)
	REFL_FIELD(rad, tag_refl::property())
REFL_END
