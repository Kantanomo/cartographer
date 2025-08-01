#pragma once
#include "user_interface_text_chat.h"

/* classes */

class c_user_interface_text_chat_sender
{
private:
	e_text_chat_type m_chat_type;
	int m_destination_players;

public:
	void set_chat_data(e_text_chat_type chat_type, int32 players_mask);
	void process_raw_message(const wchar_t* raw_msg);
};
ASSERT_STRUCT_SIZE(c_user_interface_text_chat_sender, 0x8);