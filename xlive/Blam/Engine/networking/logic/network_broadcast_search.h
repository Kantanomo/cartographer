#pragma once

/* forward declarations */

class c_network_message_gateway;
class c_network_link;

/* prototypes */

bool __cdecl network_broadcast_search_initialize(c_network_link* link, c_network_message_gateway* message_gateway);
