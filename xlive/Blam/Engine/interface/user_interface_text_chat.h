#pragma once

/* enums */

enum e_text_chat_type : int32
{
	_text_chat_type_0 = 0x0,
	_text_chat_type_all = 0x1,
	_text_chat_type_team = 0x2,
	_text_chat_type_target_player = 0x3,
	_text_chat_type_propose = 0x4,
	_text_chat_type_vote = 0x5,
	_text_chat_type_6 = 0x6,
	_text_chat_type_voting_error = 0x7,
};


/* structures */

struct s_text_chat_message
{
	int32 field_0;
	int32 field_4;
	int32 source;
	int32 destinations;
	e_text_chat_type m_chat_type;
	wchar_t message[121];
	int8 gap_106[2];
};
ASSERT_STRUCT_SIZE(s_text_chat_message, 0x108);