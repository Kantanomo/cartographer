#pragma once

/* classes */

class c_network_text_chat_manager
{
public:
	bool initialize_text_chat_manager(void);
private:
	bool m_initialized;
	int8 gap0[8487];
};
ASSERT_STRUCT_SIZE(c_network_text_chat_manager, 8488);
