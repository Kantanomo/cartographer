#pragma once
#include "network_session.h"

class c_network_session_manager
{
	c_network_session* m_sessions[k_network_maximum_sessions];

public:
	bool initialize_session_manager(void);
	c_network_session* get_session(uint32 session_index) const;
	c_network_session* get_session(const XNKID* session_id) const;
};