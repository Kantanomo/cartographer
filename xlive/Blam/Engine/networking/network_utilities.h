#pragma once

struct s_network_locator_result
{
	uint8 gap_0[116];
	int16 executable_type;
	int32 executable_version;
	int32 compatible_version;
};
ASSERT_STRUCT_OFFSET(s_network_locator_result, executable_type, 116);

void network_utilities_apply_patches();
void __cdecl network_utilities_get_game_version(int32* executable_type, int32* executable_version, int32* compatible_version);
