#pragma once

#include "network_session.h"

class c_network_session_manager
{
	c_network_session* m_sessions[k_network_maximum_sessions];

public:
	c_network_session* get_network_session_by_id(const XNKID* target_session_id) const;
};