#include "stdafx.h"
#include "ai_script.h"

#include "ai.h"
#include "ai_orders.h"
#include "cl_engine.h"
#include "squads.h"

#include "memory/data.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"

/* public code */

void __cdecl ai_script_initialize_for_new_map(void)
{
	INVOKE(0x30F219, 0x2BA6B7, ai_script_initialize_for_new_map);
	return;
}

bool ai_handle_script_verification(bool suppress_errors)
{
	bool result = true;

	ai_globals_type* ai_globals = ai_globals_get();
	if (ai_globals->ai_initialized_for_map)
	{
		const struct scenario* scenario = global_scenario_get();
		for (int32 squad_num = 0; squad_num < scenario->squads.count; ++squad_num)
		{
			result &= squad_verify_script_references(squad_num, suppress_errors);
		}

		for (int32 order_num = 0; order_num < scenario->orders.count; ++order_num)
		{
			orders_definition* order = TAG_BLOCK_GET_ELEMENT(&scenario->orders, order_num, orders_definition);
			result &= orders_verify_script_references(order, suppress_errors);
		}

		for (int32 trigger_num = 0; trigger_num < scenario->triggers.count; ++trigger_num)
		{
			trigger_definition* trigger = TAG_BLOCK_GET_ELEMENT(&scenario->triggers, trigger_num, trigger_definition);
			result &= trigger_verify_script_references(trigger, suppress_errors);
		}

		if (ai_globals->ai_initialized_for_map)
		{
			data_iterator iterator;
			iterator_new(&iterator, command_script_data_get());
			while (command_script_datum* cs = (command_script_datum*)iterator_next(&iterator))
			{
				cs->field_38 = NONE;
			}
		}
	}

	return result;
}
