#pragma once
#include "memory/rockall_heap_manager.h"

#include "networking/delivery/network_link.h"
#include "networking/messages/network_message_gateway.h"
#include "networking/messages/network_message_handler.h"
#include "networking/messages/network_message_type_collection.h"
#include "networking/Session/network_session_manager.h"
#include "networking/Session/network_text_chat_manager.h"


/* structures */

struct s_network_heap_stats
{
	int32 allocations;
	int32 allocations_in_bytes;
};

/* classes */

class c_network_heap
{
public:
	c_fixed_memory_rockall_frontend* rockall_frontend;
	int32 get_block_size(const uint8* block) const;

	void dispose();
};

/* prototypes */

void network_memory_apply_patches(void);

c_network_heap* network_get_heap(void);

s_network_heap_stats* network_heap_get_description(void);

bool __cdecl network_memory_base_initialize(
	c_network_link** link,
	c_network_message_type_collection** message_types,
	c_network_message_gateway** message_gateway,
	c_network_message_handler** message_handler,
	c_network_observer** observer,
	c_network_session** sessions,
	c_network_session_manager** session_manager,
	c_network_text_chat_manager** text_chat_manager);

uint8* __cdecl network_heap_allocate_block(uint32 size);

void __cdecl network_heap_free_block(uint8* block);
