#include "stdafx.h"
#include "ai_orders.h"

#include "hs/hs.h"

/* prototypes */

static void orders_verify_script_indices(const char* script_name, int16 script_index);

/* public code */

void __cdecl orders_initialize_for_new_map(void)
{
	INVOKE(0x36788D, 0x3125FD, orders_initialize_for_new_map);
	return;
}

bool orders_verify_script_references(orders_definition* order, bool suppress_errors)
{
	orders_verify_script_indices(order->entry_script, order->entry_script_index);
	return true;
}

bool trigger_verify_script_references(trigger_definition* trigger, bool suppress_errors)
{
	for (int32 i = 0; i < trigger->conditions.count; ++i)
	{
		const order_completion_condition_definition* condition = TAG_BLOCK_GET_ELEMENT(&trigger->conditions, i, order_completion_condition_definition);
		const e_order_rule_type rule_type = condition->rule_type;
		if (rule_type == _order_rule_type_script_ref_true || rule_type == _order_rule_type_script_ref_false)
		{
			orders_verify_script_indices(condition->exit_condition_script, condition->exit_condition_script_index);
		}
	}
	return true;
}

/* private code */

static void orders_verify_script_indices(const char* script_name, int16 script_index)
{
	/*
	vassert(
		script_index == NONE || script_index == hs_find_script_by_name(script_name),
		"script '%s'(%d) doesn't match actual script index '%d'",
		script_name,
		script_index,
		hs_find_script_by_name(script_name)
	);*/
	return;
}
