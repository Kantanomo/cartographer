#pragma once
#include "network_message_handler.h"

#pragma pack(push, 1)
struct s_network_message_countdown_timer
{
	s_network_message_session_data session_data;
	bool countdown_running;
	int8 _pad_9[3];
	uint32 countdown_timer;
	uint32 delayed_reason;
	uint64 responsible_identifier;
};
#pragma pack(pop)
ASSERT_STRUCT_SIZE(s_network_message_countdown_timer, 0x1C);
