#pragma once

/* prototypes */

bool network_session_time_get_id_and_time(int32 session_id, struct s_transport_secure_identifier* id, uint32* time);

int32 network_time_get(void);

uint32 __cdecl network_time_get_exact(void);

int32 network_time_since(uint32 timestamp);

void network_session_time_register_session_manager(class c_network_session_manager* session_manager);
