#pragma once
#include "math/color_math.h"

/* enums */

enum e_error_category : int32
{
	_error_category_generic = 0,
	_error_category_internal_full = 1,
	_error_category_internal_subfolder = 2,
	_error_category_animation = 3,
	_error_category_ai = 4,
	_error_category_shaders = 5,
	_error_category_geometry = 6,
	_error_category_environment = 7,
	_error_category_objects = 8,
	_error_category_networking = 9,
	_error_category_tags = 10,
	_error_category_ui = 11,
	_error_category_sound = 12,
	_error_category_multiplayer = 13,
	_error_category_effects = 14,
	_error_category_animation_audio_content = 15,
	_error_category_environment_materials = 16,
	_error_category_object_materials = 17,
	_error_category_design = 18,
	_error_category_localization = 19,
	k_error_category_count
};

/* structures */

struct s_error_category
{
	e_error_category category;
	const char* name;
	real_rgb_color color;
	const char* path;
};


/* prototypes */

// TODO implement this properly (using spdlog as a temp solution)
void error(int32 priority, const char* format, ...);

// TODO implement this properly (using spdlog as a temp solution)
void error(e_error_category category, int32 priority, const char* format, ...);