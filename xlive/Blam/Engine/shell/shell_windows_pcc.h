#pragma once

/* constants */

/* enums */

/* structures */

struct s_shell_pcc_settings
{
	uint32 supports_hardwaretnl;
	int32 supported_pixelshader_version;
	uint32 video_memory;
	int32 system_memory;
	uint32 cpu_maxspeed;
	uint32 cinematic_shadow;
	uint32 allow_vsync;
	real64 cpu_score;
	real64 d3d_score;
	real64 disc_score;
	real64 graphics_score;
	real64 memory_score;
	real64 system_score;
};
ASSERT_STRUCT_SIZE(s_shell_pcc_settings, 0x50);

/* prototypes */

bool __cdecl shell_windows_pcc_initialize(void);

s_shell_pcc_settings* shell_windows_pcc_get_properties(void);

bool shell_windows_pcc_allows_vsync(void);
