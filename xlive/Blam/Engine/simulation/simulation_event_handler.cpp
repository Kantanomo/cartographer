#include "stdafx.h"
#include "simulation_event_handler.h"

#include "simulation_queue_events.h"

#include "math/random_math.h"
#include "simulation/game_interface/simulation_game_events.h"

#include "networking/network_memory.h"

/* globals */

bool g_use_network_queue_storage = true;

typedef void(__thiscall* t_process_incoming_event)(c_simulation_event_handler* thisx, e_simulation_event_type simulation_event_type, int32* entity_reference_indices, int32 block_count, s_replication_allocation_block* block);
t_process_incoming_event p_process_incoming_event;

/* prototypes */

static __declspec(naked) void jmp_c_simulation_event_handler_process_incoming_event()
{
	CLASS_HOOK_JMP(c_simulation_event_handler__process_incoming_event, c_simulation_event_handler::process_incoming_event);
}

/* public code */

void simulation_event_handler_apply_patches(void)
{
	DETOUR_ATTACH(p_process_incoming_event, Memory::GetAddress<t_process_incoming_event>(0x1D3E02, 0x1D9092), jmp_c_simulation_event_handler_process_incoming_event);
	return;
}

CLASS_HOOK_DECLARE_LABEL(c_simulation_event_handler__process_incoming_event, c_simulation_event_handler::process_incoming_event);
void c_simulation_event_handler::process_incoming_event(e_simulation_event_type event_type, int32* entity_reference_indices, int32 block_count, s_replication_allocation_block* payload_block)
{
	ASSERT(VALID_INDEX(event_type, m_type_collection->get_event_definition_count()));
	ASSERT(entity_reference_indices);

	c_simulation_event_definition* event_definition = m_type_collection->get_event_definition(event_type);
	ASSERT(event_definition);

	if (g_use_network_queue_storage)
	{
		int32 entity_reference_indices_count = event_definition->number_of_entity_references();


		void* block_data;
		int32 block_size;
		if (event_definition->payload_size() <= 0)
		{
			ASSERT(block_count==0);
			block_data = NULL;
			block_size = 0;
		}
		else
		{
			ASSERT(block_count==1);
			ASSERT(payload_block->block_size == (int16)event_definition->payload_size());
			ASSERT(payload_block->block_type == _network_memory_block_simulation_event);
			ASSERT(payload_block->block_data != NULL);

			block_data = payload_block->block_data;
			block_size = payload_block->block_size;
			payload_block->block_data = NULL;
		}
		simulation_queue_event_insert(event_type, entity_reference_indices_count, entity_reference_indices, block_size, block_data);

		if (block_data)
		{
			network_heap_free_block(block_data);
		}
	}
	else
	{
		random_seed_allow_use();
		// call the original
		p_process_incoming_event(this, event_type, entity_reference_indices, block_count, payload_block);
		random_seed_disallow_use();
	}
	return;
}
