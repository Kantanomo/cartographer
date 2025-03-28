#include "stdafx.h"
#include "cseries_errors.h"

/* constants */

const s_error_category k_category_constants[k_error_category_count] =
{
	{ _error_category_generic, "generic", *global_real_rgb_white, ""},
	{ _error_category_internal_full, "generic", *global_real_rgb_white, "" },
	{ _error_category_internal_subfolder, "generic", *global_real_rgb_white, "debug.txt" },
	{ _error_category_animation, "animation", *global_real_rgb_yellow, "animation_debug.txt" },
	{ _error_category_ai, "ai", *global_real_rgb_salmon, "ai_debug.txt" },
	{ _error_category_shaders, "shaders", *global_real_rgb_blue, "shaders_debug.txt" },
	{ _error_category_geometry, "geometry", { 0.f, 0.f, 0.8f }, "geometry_debug.txt" },				// Blue
	{ _error_category_environment, "environment", { 0.f, 0.f, 0.6f }, "environment_debug.txt" },	// Dark blue
	{ _error_category_objects, "objects", { 0.4f, 0.1f, 0.4f }, "objects_debug.txt" },				// Light green
	{ _error_category_networking, "networking", { 1.f, 0.4f, 0.7f }, "networking_debug.txt" },		// Pink
	{ _error_category_tags, "tags", *global_real_rgb_magenta, "tag_debug.txt" },
	{ _error_category_ui, "ui", *global_real_rgb_orange, "ui_debug.txt" },
	{ _error_category_sound, "sound", { 0.7f, 0.7f, 0.7f }, "sound_debug.txt" },					// Grey
	{ _error_category_multiplayer, "multiplayer", { 0.8f, 0.1f, 0.6f }, "multiplayer_debug.txt" },	// Violet
	{ _error_category_effects, "effects", { 0.f, 0.f, 0.8f }, "effects_debug.txt" },				// Blue
	{ _error_category_animation_audio_content, "animation_audio_content", { 0.7f, 0.7f, 0.5f }, "animation_audio_content_debug.txt" },	// Yellowish grey
	{ _error_category_environment_materials, "environment_materials", { 0.f, 0.f, 0.6f }, "environment_materials_debug.txt" },			// Dark blue
	{ _error_category_object_materials, "object_materials", { 0.4f, 0.1f, 0.4f }, "object_materials_debug.txt" },						// Dark purple
	{ _error_category_design, "design", *global_real_rgb_salmon, "design_debug.txt" },
	{ _error_category_localization, "localization", *global_real_rgb_black, "localization_debug.txt" }
};

/* prototypes */

static void error_internal(e_error_category category, int32 priority, const char* format /*, va_list va_args*/);

/* public code */

void error(int32 priority, const char* format, ...)
{
#ifdef ERRORS_ENABLED
	va_list va_args;
	va_start(va_args, format);
	char string[1024];
	vsnprintf(string, NUMBEROF(string), NUMBEROF(string), format, va_args);
	error_internal(_error_category_generic, priority, string/*, va_args*/);
	va_end(va_args);
#endif
	return;
}

void error(e_error_category category, int32 priority, const char* format, ...)
{
#ifdef ERRORS_ENABLED
	va_list va_args;
	va_start(va_args, format);
	char string[1024];
	vsnprintf(string, NUMBEROF(string), NUMBEROF(string), format, va_args);
	error_internal(category, priority, string/*, va_args*/);
	va_end(va_args);
#endif
	return;
}

/* private code */

static void error_internal(e_error_category category, int32 priority, const char* format /*, va_list va_args*/)
{
#ifdef ERRORS_ENABLED
	LOG_INFO_GAME(format/*, va_args*/);
#endif
	return;
}
