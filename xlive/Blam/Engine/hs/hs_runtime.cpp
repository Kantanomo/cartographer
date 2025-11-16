#include "stdafx.h"
#include "hs_runtime.h"

#include "hs_library_internal_runtime.h"

#include "hs_compile.h"
#include "hs_globals_external.h"
#include "object_lists.h"

#include "ai/ai.h"
#include "ai/ai_script.h"
#include "cutscene/cinematics.h"
#include "game/game.h"
#include "game/game_time.h"
#include "main/main_time.h"
#include "objects/objects.h"
#include "render/render_debug.h"
#include "scenario/scenario.h"
#include "scenario/scenario_definitions.h"
#include "text/draw_string.h"

/* structures */

struct s_hs_runtime_globals
{
	bool initialized;
	bool field_1;
	int32 executing_thread_index;
};

/* prototypes */

#pragma warning(push)
#pragma warning(disable : 4211)
static s_hs_runtime_globals* hs_runtime_globals_get(void);

static data_array* hs_global_data_get(void);

static void hs_typecasting_table_initialize(void);

static int32 hs_data_to_void(int32 n);

static int32 hs_long_to_boolean(int32 n);

static int32 hs_short_to_boolean(int32 s);

static int32 hs_string_to_boolean(int32 n);

static int32 hs_short_to_real(int32 s);

static int32 hs_long_to_real(int32 n);

static int32 hs_enum_to_real(int32 e);

static int32 hs_real_to_short(int32 r);

static int32 hs_long_to_short(int32 n);

static int32 hs_real_to_long(int32 r);

static int32 hs_short_to_long(int32 s);

static int32 hs_object_to_object_list(int32 object_index);

static int32 hs_object_name_to_object_list(int32 object_name_index);

static void* hs_stack_allocate(int32 thread_index, int32 size);

static void hs_script_evaluate(int16 script_index, int32 thread_index, bool initialize);

static bool hs_syntax_node_exists(int32 index);

static void hs_thread_stack_adjust(int32 thread_index);

static const char* hs_thread_format(int32 thread_index);

static int32 hs_global_designator_get_index(int16 global_designator);

static int32 hs_global_evaluate(int16 global_designator);

static void hs_global_reconcile_read(int16 global_designator);

static void hs_global_reconcile_write(int16 global_designator);

static bool hs_type_mask_can_cast(int16 actual_type, int16 desired_type);

static const char* expression_get_function_name(int32 thread_index, int32 expression_index);

static bool script_error(int32 thread_index, const char* message, const char* condition);

static int32* hs_arguments_evaluate(int32 thread_index, int16 formal_parameter_count, const int16* formal_parameters, bool initialize);

#pragma warning(pop)

/* globals */

static int32(*g_typecasting_procedures[NUMBER_OF_HS_NODE_TYPES][NUMBER_OF_HS_NODE_TYPES])(int32);

#ifdef HS_COMPILER_ENABLED

static bool g_require_gc = false;

static bool g_run_game_scripts = true;

#endif

bool debug_scripting = false;

/* macros */

#define valid_thread(thread)											\
(thread) >= hs_thread_data->data										\
&& (int8*)(thread) < (int8*)hs_thread_data->data + hs_thread_data->first_free_absolute_index * hs_thread_data->size	\
&& (thread)->stack >= (hs_stack_frame*)(thread)->stack_data				\
&& (thread)->stack < (hs_stack_frame*)&thread[1]						\
&& &(thread)->stack->data[(thread)->stack->size] <= (uint8*)&thread[1]

#define hs_thread_assert(expression, reason, thread_index)			\
vassert(															\
	expression,														\
	"a problem occurred while executing the script %s: %s (%s)",	\
	hs_thread_format((thread_index)),								\
	reason ? reason : "no reason given",							\
	#expression														\
)

// If the expression fails and script_error doesn't return true then don't execute the statement
#define hs_script_error(expression, reason, thread_index)					\
(expression || script_error((thread_index), (reason), #expression))

/* public code */

void hs_runtime_apply_patches(void)
{
	PatchCall(Memory::GetAddress(0xA3997, 0x95BF7), hs_runtime_initialize);
	return;
}

data_array* hs_thread_data_get(void)
{
	return *Memory::GetAddress<data_array**>(0x4CDF94, 0x4FFDEC);
}

bool hs_runtime_initialized(void)
{
	const s_hs_runtime_globals* hs_runtime_globals = hs_runtime_globals_get();
	return hs_runtime_globals->initialized;
}

void __cdecl hs_runtime_initialize(void)
{
	// TODO: rewrite
	INVOKE(0x94EC4, 0xAA0C4, hs_runtime_initialize);
	
	hs_typecasting_table_initialize();
	return;
}

void hs_runtime_update(void)
{
	const s_hs_runtime_globals* hs_runtime_globals = hs_runtime_globals_get();

	if ((!cinematic_in_progress_not_main_menu() || main_time_get_tickrate() <= 30 || (game_time_get() & 1) == 0) && hs_runtime_globals->initialized)
	{
		const uint32 current_time = game_time_get();
#ifdef HS_COMPILER_ENABLED
		bool dont_garbage_collect = false;
#endif

		data_array* hs_thread_data = hs_thread_data_get();
		for (
			int32 thread_index = data_next_index(hs_thread_data, NONE);
			hs_runtime_globals->initialized && thread_index != NONE;
			thread_index = data_next_index(hs_thread_data, thread_index)
		)
		{
			hs_thread* thread = hs_thread_get(thread_index);
			if (thread->type == _hs_thread_type_performance_script)
			{
				continue;
			}

#ifdef HS_COMPILER_ENABLED
			else if (thread->type == _hs_thread_type_runtime_evaluate)
			{
				dont_garbage_collect = true;
			}
			else if (!g_run_game_scripts)
			{
				continue;
			}
#endif

			if (IN_RANGE(thread->sleep_until, 0, (int32)current_time))
			{
				hs_thread_main_original(thread_index);
			}
		}

		object_list_gc();
#ifdef HS_COMPILER_ENABLED
		if (g_require_gc && !dont_garbage_collect && !(game_time_get() % 16))
		{
			hs_node_gc();
			g_require_gc = false;
		}
#endif
	}
	return;
}

#ifdef HS_COMPILER_ENABLED

void hs_runtime_dirty(void)
{
	hs_runtime_dispose_from_old_map();
	hs_runtime_initialize_for_new_map();
	hs_compile_initialize(true);
	hs_compile_dispose();
	ai_reset();
	ai_handle_script_verification(true);
	return;
}

void hs_runtime_require_gc(void)
{
	g_require_gc = true;
	return;
}

bool hs_runtime_safe_to_gc(void)
{
	const s_hs_runtime_globals* hs_runtime_globals = hs_runtime_globals_get();

	bool result = true;
	if (hs_runtime_globals->initialized)
	{
		data_array* hs_thread_data = hs_thread_data_get();
		for (
			int32 thread_index = data_next_index(hs_thread_data, NONE);
			hs_runtime_globals->initialized && thread_index != NONE;
			thread_index = data_next_index(hs_thread_data, thread_index)
		)
		{
			if (hs_thread_get(thread_index)->type == _hs_thread_type_runtime_evaluate)
			{
				result = false;
				break;
			}
		}
	}
	return result;
}

#endif

void __cdecl hs_runtime_initialize_for_new_map(void)
{
	INVOKE(0x96BFD, 0xABDFD, hs_runtime_initialize_for_new_map);
	return;
}

void __cdecl hs_runtime_dispose_from_old_map(void)
{
	INVOKE(0x94F4D, 0xAA14D, hs_runtime_dispose_from_old_map);
	return;
}

hs_syntax_node* hs_syntax_get(datum index)
{
	data_array* g_hs_syntax_data = *hs_syntax_data_get();
	return (hs_syntax_node*)datum_get(g_hs_syntax_data, index);
}

hs_thread* hs_thread_get(datum index)
{
	return (hs_thread*)datum_get(hs_thread_data_get(), index);
}

int32 hs_thread_new(e_hs_thread_type type, int32 script_index)
{
	const int32 result = datum_new(hs_thread_data_get());

	ASSERT(VALID_INDEX(type, NUMBER_OF_HS_THREAD_TYPES));
	ASSERT(type != _hs_thread_type_script || script_index != NONE);

	if (result != NONE)
	{
		hs_thread* thread = hs_thread_get(result);
		thread->stack = (hs_stack_frame*)thread->stack_data;
		thread->stack->parent = NULL;
		thread->stack->size = 0;
		thread->stack->expression_index = NONE;
		thread->type = (uint8)type;
		thread->script_index = script_index;
		thread->flags = 0;

		if (script_index != NONE && scenario_get_hs_script(global_scenario_get(), script_index)->script_type == _hs_special_form)
		{
			thread->sleep_until = -2;
		}
		else
		{
			thread->sleep_until = 0;
		}
	}
	return result;
}

// TODO: remove this once we've fully re-implemented the hs function table
void hs_thread_main_original(int32 thread_index)
{
	void* func = Memory::GetAddress<void*>(0x96A71, 0xABC71);
	__asm
	{
		push edi
		mov edi, thread_index
		call func
		pop edi
	}
	return;
}

void hs_thread_main(int32 thread_index)
{
	s_hs_runtime_globals* hs_runtime_globals = hs_runtime_globals_get();

	hs_thread* thread = hs_thread_get(thread_index);

	bool terminate_thread = false;
	
	hs_script* script = NULL;
	hs_runtime_globals->executing_thread_index = thread_index;
	if (thread->type == _hs_thread_type_script || thread->type == _hs_thread_type_performance_script)
	{
		script = scenario_get_hs_script(global_scenario_get(), thread->script_index);
		ASSERT(script);

		hs_thread_assert(script->script_type != _hs_script_static && script->script_type != _hs_script_stub, "found a static script at toplevel.", thread_index);
	}

	const data_array* hs_thread_data = hs_thread_data_get();
	hs_thread_assert(valid_thread(thread), "corrupted stack.", thread_index);

	thread->sleep_until = 0;
	if ((uint8*)thread->stack == thread->stack_data)
	{
		ASSERT(script);
		thread->stack->size = 0;

		int32* stack = (int32*)hs_stack_allocate(thread_index, sizeof(int32));
		hs_evaluate(thread_index, script->root_expression_index, stack);
	}

	while (
		(uint8*)thread->stack != thread->stack_data &&
		thread->sleep_until >= 0 && 
		(!game_in_progress() || (uint32)thread->sleep_until <= game_time_get()) 
		&& hs_runtime_globals->initialized)
	{
		if (thread->type == _hs_syntax_node_variable_bit && !hs_syntax_node_exists(thread->stack->expression_index))
		{
			error(_error_silent, "terminating console script unexpectedly");
			terminate_thread = true;
			break;
		}

		const hs_syntax_node* expression = hs_syntax_get(thread->stack->expression_index);
		const bool initialize = TEST_BIT(thread->flags, _hs_thread_in_function_call_bit);

		thread->stack->size = 0;
		SET_BIT(thread->flags, _hs_thread_in_function_call_bit, false);

		if (TEST_BIT(expression->flags, _hs_syntax_node_script_bit))
		{
			hs_script_evaluate(expression->script_index, thread_index, initialize);
		}
		else
		{
			const hs_function_definition* function = hs_function_get(expression->function_index);
			ASSERT(function->evaluate);
			
			function->evaluate(expression->function_index, thread_index, initialize);
		}
	}

	if ((uint8*)thread->stack == thread->stack_data || terminate_thread)
	{
		ASSERT(!terminate_thread || thread->type == _hs_thread_type_runtime_evaluate);

		if (thread->type)
		{
			if (thread->type == _hs_thread_type_performance_script)
			{
				thread->sleep_until = NONE;
			}
			else if (thread->type == _hs_thread_type_runtime_evaluate)
			{
				hs_thread_delete(thread_index);
			}
		}
		else if (script->script_type == _hs_script_startup || script->script_type == _hs_script_dormant)
		{
			thread->sleep_until = NONE;
		}
	}

	hs_runtime_globals->executing_thread_index = NONE;
	return;
}

void hs_thread_delete(int32 thread_index)
{
	ASSERT(hs_thread_get(thread_index)->type != _hs_thread_type_script);
	datum_delete(hs_thread_data_get(), thread_index);
	return;
}

void hs_evaluate(int32 thread_index, int32 expression_index, int32* destination)
{
	hs_thread* thread = hs_thread_get(thread_index);
	hs_syntax_node* node = hs_syntax_get(expression_index);

	const data_array* hs_thread_data = hs_thread_data_get();
	hs_thread_assert(valid_thread(thread), "corrupted stack.", thread_index);
	ASSERT(destination);

	if (TEST_BIT(hs_syntax_get(expression_index)->flags, _hs_syntax_node_primitive_bit))
	{
		int32 result;
		if (TEST_BIT(node->flags, _hs_syntax_node_variable_bit))
		{
			const int32 value = hs_global_evaluate(node->short_value);
			const int16 actual_type = hs_global_get_type(node->short_value);
			result = hs_cast(thread_index, actual_type, node->type, value);
		}
		else
		{
			result = hs_cast(thread_index, node->script_index, node->type, node->long_value);
		}
		*destination = result;
	}
	else
	{
		thread->stack->child_result = destination;
		hs_thread_stack_adjust(thread_index);
		SET_BIT(thread->flags, _hs_thread_in_function_call_bit, true);
		thread->stack->expression_index = expression_index;
	}
	return;
}

void __cdecl hs_return(int32 thread_index, int32 value)
{
	INVOKE(0x9505D, 0xAA25D, hs_return, thread_index, value);
	return;
}

int32* hs_macro_function_evaluate(int16 function_index, int32 thread_index, bool initialize)
{
	const hs_function_definition* function = hs_function_get(function_index);
	return hs_arguments_evaluate(thread_index, function->formal_parameter_count, (const int16*)function->formal_parameters, initialize);
}

int32 hs_runtime_evaluate(datum expression_index, bool display_expression_result)
{
	datum result = NONE;
	if (expression_index != NONE)
	{
		const s_hs_runtime_globals* hs_runtime_globals = hs_runtime_globals_get();

		bool dispose = false;
		if (!hs_runtime_globals->initialized)
		{
			hs_runtime_initialize_for_new_map();
			dispose = true;
		}

		if (hs_runtime_globals->initialized)
		{
			const datum thread_index = hs_thread_new(_hs_thread_type_runtime_evaluate, NONE);
			if (thread_index == NONE)
			{
				error(_error_log, "### ERROR %s: could not allocate thread to execute a command!", __FUNCTION__);
			}
			else
			{
				hs_thread* thread = (hs_thread*)hs_thread_get(thread_index);
				SET_BIT(thread->flags, _hs_thread_display_expression_bit, display_expression_result);

				hs_evaluate(thread_index, expression_index, &thread->result);
				
				if (TEST_BIT(thread->flags, _hs_thread_in_function_call_bit))
				{
					hs_thread_main(thread_index);
				}
				else
				{
					result = thread->result;
				}
			}
		}
		else
		{
			error(_error_silent, "unable to initialize scripting system to execute that command.");
		}

		if (dispose)
		{
			hs_runtime_dispose_from_old_map();
		}
	}
	return result;
}

bool hs_can_cast(int16 actual_type, int16 desired_type)
{
	ASSERT(actual_type == _hs_passthrough || hs_type_valid(actual_type));
	ASSERT(hs_type_valid(desired_type));

	bool result;

	// Trying to cast to itself...
	if (actual_type == _hs_passthrough || actual_type == desired_type)
	{
		result = true;
	}
	// Not casting objects
	else if (!IN_RANGE(actual_type, FIRST_HS_OBJECT_TYPE, LAST_HS_OBJECT_TYPE))
	{
		if (!IN_RANGE(actual_type, FIRST_HS_OBJECT_NAME_TYPE, LAST_HS_OBJECT_NAME_TYPE))
		{
			result = g_typecasting_procedures[desired_type][actual_type] != NULL;
		}
		else
		{
			result = 
				IN_RANGE(actual_type, FIRST_HS_OBJECT_NAME_TYPE, LAST_HS_OBJECT_NAME_TYPE) &&
				hs_type_mask_can_cast(actual_type - FIRST_HS_OBJECT_NAME_TYPE, desired_type - FIRST_HS_OBJECT_NAME_TYPE);
		}
	}
	// Casting objects
	else
	{
		if (!IN_RANGE(actual_type, FIRST_HS_OBJECT_TYPE, LAST_HS_OBJECT_TYPE))
		{
			result =
				IN_RANGE(actual_type, FIRST_HS_OBJECT_NAME_TYPE, LAST_HS_OBJECT_NAME_TYPE) &&
				hs_type_mask_can_cast(actual_type - FIRST_HS_OBJECT_NAME_TYPE, desired_type - FIRST_HS_OBJECT_TYPE);

		}
		else
		{
			result = hs_type_mask_can_cast(actual_type - FIRST_HS_OBJECT_TYPE, desired_type - FIRST_HS_OBJECT_TYPE);
		}
	}
	return result;
}

int32 hs_cast(int32 thread_index, int16 actual_type, int16 desired_type, int32 value)
{
	hs_thread_assert(hs_can_cast(actual_type, desired_type), "bad typecast.", thread_index);
	
	if (actual_type == desired_type || actual_type == _hs_passthrough)
		return value;
	if (desired_type >= FIRST_HS_OBJECT_NAME_TYPE && desired_type <= LAST_HS_OBJECT_NAME_TYPE)
		return value;
	
	int32 result;
	if (desired_type < FIRST_HS_OBJECT_TYPE || desired_type > LAST_HS_OBJECT_TYPE)
	{
		ASSERT(VALID_INDEX(desired_type, NUMBER_OF_HS_NODE_TYPES));
		ASSERT(VALID_INDEX(actual_type, NUMBER_OF_HS_NODE_TYPES));
		ASSERT(g_typecasting_procedures[desired_type][actual_type] != NULL);
		result = g_typecasting_procedures[desired_type][actual_type](value);
	}
	else if (IN_RANGE(actual_type, FIRST_HS_OBJECT_NAME_TYPE, LAST_HS_OBJECT_NAME_TYPE))
	{
		result = object_index_from_name_index((int16)value);
	}
	else
	{
		result = value;
	}

	return result;
}

void render_debug_scripting(void)
{
	if (debug_scripting)
	{
		const int16 tab_stops[2] = {250, 350};

		char string[10240];
		csprintf(string, NUMBEROF(string), "|n|n|nscript name|tsleep time|tfunction");

		const s_hs_runtime_globals* hs_runtime_globals = hs_runtime_globals_get();
		data_array* hs_thread_data = hs_thread_data_get();

		for (
			int32 index = data_next_index(hs_thread_data, NONE);
			hs_runtime_globals->initialized && index != NONE;
			index = data_next_index(hs_thread_data, index))
		{
			hs_thread* thread = hs_thread_get(index);
			if (thread->sleep_until >= 0)
			{
				const char* thread_string = hs_thread_format(index);
				csnappendf(string, NUMBEROF(string), "|n%s|t", thread_string);

				const int32 sleep_time = thread->sleep_until ? thread->sleep_until - game_time_get() : 0;
				csnappendf(string, NUMBEROF(string), "%d", sleep_time);
				csstrncat(string, "|t", NUMBEROF(string));

				if ((uint8*)thread->stack != thread->stack_data && thread->sleep_until != -2)
				{
					const char* function_name = expression_get_function_name(index, thread->stack->expression_index);
					csstrncat(string, function_name, NUMBEROF(string));
				}
			}
		}
		string[1024] = '\0';

		draw_string_set_tab_stops(tab_stops, 2);
		render_debug_string(string);
		draw_string_set_tab_stops(tab_stops, 0);
	}
	return;
}

/* private code */

static s_hs_runtime_globals* hs_runtime_globals_get(void)
{
	return Memory::GetAddress<s_hs_runtime_globals*>(0x4CDF98, 0x4FFDF0);
}

static data_array* hs_global_data_get(void)
{
	return *Memory::GetAddress<data_array**>(0x4CDF90, 0x4FFDE8);
}

static void hs_typecasting_table_initialize(void)
{
	for (int32 type = _hs_type_boolean; type < NUMBER_OF_HS_NODE_TYPES; ++type)
	{
		g_typecasting_procedures[_hs_type_void][type] = hs_data_to_void;
	}
	
	g_typecasting_procedures[_hs_type_boolean][_hs_type_real] = hs_long_to_boolean;
	g_typecasting_procedures[_hs_type_boolean][_hs_type_short_integer] = hs_short_to_boolean;
	g_typecasting_procedures[_hs_type_boolean][_hs_type_long_integer] = hs_long_to_boolean;
	g_typecasting_procedures[_hs_type_boolean][_hs_type_string] = hs_string_to_boolean;
	g_typecasting_procedures[_hs_type_real][_hs_type_short_integer] = hs_short_to_real;
	g_typecasting_procedures[_hs_type_real][_hs_type_long_integer] = hs_long_to_real;

	for (int32 type = FIRST_HS_ENUM_TYPE; type <= NOT_LAST_HS_ENUM_TYPE; ++type)
	{
		g_typecasting_procedures[_hs_type_real][type] = hs_enum_to_real;
	}

	g_typecasting_procedures[_hs_type_short_integer][_hs_type_real] = hs_real_to_short;
	g_typecasting_procedures[_hs_type_short_integer][_hs_type_long_integer] = hs_long_to_short;
	g_typecasting_procedures[_hs_type_long_integer][_hs_type_real] = hs_real_to_long;
	g_typecasting_procedures[_hs_type_long_integer][_hs_type_short_integer] = hs_short_to_long;

	for (int32 type = FIRST_HS_OBJECT_TYPE; type <= LAST_HS_OBJECT_TYPE; ++type)
	{
		g_typecasting_procedures[_hs_type_object_list][type] = hs_object_to_object_list;
	}

	for (int32 type = FIRST_HS_OBJECT_NAME_TYPE; type <= LAST_HS_OBJECT_NAME_TYPE; ++type)
	{
		g_typecasting_procedures[_hs_type_object_list][type] = hs_object_name_to_object_list;
	}

	return;
}

static int32 hs_data_to_void(int32 n)
{
	return NULL;
}

static int32 hs_long_to_boolean(int32 n)
{
	return n == 0;
}

static int32 hs_short_to_boolean(int32 s)
{
	return ((int16)s) == 0;
}

static int32 hs_string_to_boolean(int32 n)
{
	const size_t length = cstrlen((const char*)n);
	return hs_long_to_boolean(length);
}

static int32 hs_short_to_real(int32 s)
{
	const real32 v = (real32)(int16)s;
	return (int32)v;
}

static int32 hs_long_to_real(int32 n)
{
	const real32 v = (real32)n;
	return (int32)v;
}

static int32 hs_enum_to_real(int32 e)
{
	const real32 v = (real32)e;
	return (int32)v;
}

static int32 hs_real_to_short(int32 r)
{
	return (int16)(int32)*(real32*)&r;
}

static int32 hs_long_to_short(int32 n)
{
	return (int16)n;
}

static int32 hs_real_to_long(int32 r)
{
	return (int32)*(real32*)&r;
}

static int32 hs_short_to_long(int32 s)
{
	return (int16)s;
}

static int32 hs_object_to_object_list(int32 object_index)
{
	int32 result = NONE;
	if (object_index != NONE)
	{
		result = object_list_new();
		object_list_add(result, object_index);
	}
	return result;
}

static int32 hs_object_name_to_object_list(int32 object_name_index)
{
	int32 result = NONE;
	const int32 object_index = object_index_from_name_index((int16)object_name_index);
	if (object_index != NONE)
	{
		result = object_list_new();
		object_list_add(result, object_index);
	}
	return result;
}

static bool hs_syntax_node_exists(int32 index)
{
	data_array* g_hs_syntax_data = *hs_syntax_data_get();
	return datum_try_and_get(g_hs_syntax_data, index) != NULL;
}

static void* hs_stack_allocate(int32 thread_index, int32 size)
{
	const hs_thread* thread = hs_thread_get(thread_index);
	hs_stack_frame* frame = thread->stack;

	const data_array* hs_thread_data = hs_thread_data_get();
	hs_thread_assert(valid_thread(thread), "corrupted stack.", thread_index);
	hs_thread_assert(size, "attempt to allocate zero space from the stack.", thread_index);
	hs_thread_assert(frame->data + frame->size + size <= thread->stack_data + HS_THREAD_STACK_SIZE, "stack overflow.", thread_index);

	void* result = &frame->data[frame->size];
	frame->size += (int16)size;
	return result;
}

static void hs_script_evaluate(int16 script_index, int32 thread_index, bool initialize)
{
	hs_thread_get(thread_index);
	int32* stack = (int32*)hs_stack_allocate(thread_index, sizeof(int32));
	if (initialize)
	{
		const hs_script* script = scenario_get_hs_script(global_scenario_get(), script_index);
		hs_evaluate(thread_index, script->root_expression_index, stack);
	}
	else
	{
		hs_return(thread_index, *stack);
	}
	return;
}

static void hs_thread_stack_adjust(int32 thread_index)
{
	hs_thread* thread = hs_thread_get(thread_index);

	hs_stack_frame* new_frame = (hs_stack_frame*)&thread->stack->data[thread->stack->size + 2];

	hs_thread_assert((byte*)(new_frame+1) < thread->stack_data + HS_THREAD_STACK_SIZE, "stack overflow", thread_index);

	new_frame->parent = thread->stack;
	thread->stack = new_frame;
	new_frame->size = 0;
	return;
}

static const char* hs_thread_format(int32 thread_index)
{
	const char* result;
	
	int32 script_index;
	switch (hs_thread_get(thread_index)->type)
	{
	case _hs_thread_type_script:
	case _hs_thread_type_command_script:
	case _hs_thread_type_performance_script:
		script_index = hs_thread_get(thread_index)->script_index;
		if (script_index == NONE)
		{
			result = "[unknown thread type]";
		}
		else
		{
			result = scenario_get_hs_script(global_scenario_get(), script_index)->name;
		}
		break;
	case _hs_thread_type_global_initialize:
		result = "[global initialize]";
		break;
	case _hs_thread_type_runtime_evaluate:
		result = "[console command]";
		break;
	default:
		result = "[unknown thread type]";
		break;
	}
	return result;
}

static int32 hs_global_designator_get_index(int16 global_designator)
{
	return global_designator < 0 ? HS_GLOBAL_INDEX(global_designator) : k_hs_external_global_count + HS_GLOBAL_INDEX(global_designator);
}

static int32 hs_global_evaluate(int16 global_designator)
{
	hs_global_reconcile_read(global_designator);
	int32 index = hs_global_designator_get_index(global_designator);
	return ((hs_global_runtime*)datum_get_absolute(hs_global_data_get(), index))->long_value;
}

static void hs_global_reconcile_read(int16 global_designator)
{
	if (global_designator < 0)
	{
		int32 index = hs_global_designator_get_index(global_designator);
		hs_global_runtime* absolute = (hs_global_runtime*)datum_get_absolute(hs_global_data_get(), index);
		const hs_global_external* global = hs_global_external_get(HS_GLOBAL_INDEX(global_designator));
		switch (hs_global_get_type(global_designator))
		{
		case _hs_type_boolean:
			absolute->byte_value = global->pointer ? *(bool*)global->pointer : hs_type_boolean_default;
			break;
		case _hs_type_real:
			absolute->real_value = global->pointer? *(real32*)global->pointer : hs_type_real_default;
			break;
		case _hs_type_short_integer:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_short_integer_default;
			break;
		case _hs_type_long_integer:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_long_integer_default;
			break;
		case _hs_type_string:
			absolute->string_value = global->pointer ? *(char**)global->pointer : hs_type_string_default;
			break;
		case _hs_type_script:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_script_default;
			break;
		case _hs_type_string_id:
			absolute->long_value = global->pointer ? *(string_id*)global->pointer : hs_type_string_id_default;
			break;
		case _hs_type_unit_seat_mapping:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_unit_seat_mapping_default;
			break;
		case _hs_type_trigger_volume:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_trigger_volume_default;
			break;
		case _hs_type_cutscene_flag:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_cutscene_flag_default;
			break;
		case _hs_type_cutscene_camera_point:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_cutscene_camera_point_default;
			break;
		case _hs_type_cutscene_title:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_cutscene_title_default;
			break;
		case _hs_type_cutscene_recording:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_cutscene_recording_default;
			break;
		case _hs_type_device_group:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_device_group_default;
			break;
		case _hs_type_ai:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_ai_default;
			break;
		case _hs_type_ai_command_list:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_ai_command_list_default;
			break;
		case _hs_type_ai_command_script:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_ai_command_script_default;
			break;
		case _hs_type_ai_behavior:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_ai_behavior_default;
			break;
		case _hs_type_ai_orders:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_ai_orders_default;
			break;
		case _hs_type_starting_profile:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_starting_profile_default;
			break;
		case _hs_type_conversation:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_conversation_default;
			break;
		case _hs_type_structure_bsp:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_structure_bsp_default;
			break;
		case _hs_type_navpoint:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_navpoint_default;
			break;
		case _hs_type_style:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_style_default;
			break;
		case _hs_type_hud_message:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_hud_message_default;
			break;
		case _hs_type_object_list:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_object_list_default;
			break;
		case _hs_type_sound:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_sound_default;
			break;
		case _hs_type_effect:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_effect_default;
			break;
		case _hs_type_damage:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_damage_default;
			break;
		case _hs_type_looping_sound:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_looping_sound_default;
			break;
		case _hs_type_animation_graph:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_animation_graph_default;
			break;
		case _hs_type_damage_effect:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_damage_effect_default;
			break;
		case _hs_type_object_definition:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_object_definition_default;
			break;
		case _hs_type_bitmap:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_bitmap_default;
			break;
		case _hs_type_shader:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_shader_default;
			break;
		case _hs_type_render_model:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_render_model_definition_default;
			break;
		case _hs_type_structure_definition:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_structure_bsp_definition_default;
			break;
		case _hs_type_lightmap_definition:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_structure_lightmap_definition_default;
			break;
		case _hs_type_enum_game_difficulty:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_enum_game_difficulty_default;
			break;
		case _hs_type_enum_team:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_enum_team_default;
			break;
		case _hs_type_enum_actor_type:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_enum_actor_type_default;
			break;
		case _hs_type_enum_hud_corner:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_enum_hud_corner_default;
			break;
		case _hs_type_enum_model_state:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_enum_model_state_default;
			break;
		case _hs_type_enum_network_event:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_enum_network_event_default;
			break;
		case _hs_type_object:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_object_default;
			break;
		case _hs_type_unit:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_unit_default;
			break;
		case _hs_type_vehicle:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_vehicle_default;
			break;
		case _hs_type_weapon:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_weapon_default;
			break;
		case _hs_type_device:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_device_default;
			break;
		case _hs_type_scenery:
			absolute->long_value = global->pointer ? *(int32*)global->pointer : hs_type_scenery_default;
			break;
		case _hs_type_object_name:
			absolute->short_value = global->pointer ? *(int16*)global->pointer : hs_type_object_name_default;
			break;
		default:
			halt();
		}
	}
	return;
}

static void hs_global_reconcile_write(int16 global_designator)
{
	const e_hs_type type = (e_hs_type)hs_global_get_type(global_designator);
	const int32 index = hs_global_designator_get_index(global_designator);
	const hs_global_runtime* absolute = (hs_global_runtime*)datum_get_absolute(hs_global_data_get(), index);

	if (global_designator < 0)
	{
		const hs_global_external* global = hs_global_external_get(HS_GLOBAL_INDEX(global_designator));
		if (global->pointer)
		{
			switch (type)
			{
			case _hs_type_boolean:
				*(bool*)global->pointer = absolute->byte_value;
				break;
			case _hs_type_real:
				*(real32*)global->pointer = absolute->real_value;
				break;
			case _hs_type_short_integer:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case _hs_type_long_integer:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_string:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_script:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case _hs_type_string_id:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_unit_seat_mapping:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_trigger_volume:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case _hs_type_cutscene_flag:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case _hs_type_cutscene_camera_point:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case _hs_type_cutscene_title:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case _hs_type_cutscene_recording:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case _hs_type_device_group:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_ai:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_ai_command_list:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case _hs_type_ai_command_script:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case _hs_type_ai_behavior:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case _hs_type_ai_orders:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case _hs_type_starting_profile:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case _hs_type_conversation:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case _hs_type_structure_bsp:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case _hs_type_navpoint:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case _hs_type_point_reference:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_style:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_hud_message:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case _hs_type_object_list:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case FIRST_HS_TAG_REFERENCE_TYPE:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_effect:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_damage:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_looping_sound:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_animation_graph:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_damage_effect:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_object_definition:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_bitmap:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_shader:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_render_model:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_structure_definition:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_lightmap_definition:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case FIRST_HS_ENUM_TYPE:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case _hs_type_enum_team:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case _hs_type_enum_actor_type:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case _hs_type_enum_hud_corner:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case _hs_type_enum_model_state:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case _hs_type_enum_network_event:
				*(int16*)global->pointer = absolute->short_value;
				break;
			case FIRST_HS_OBJECT_TYPE:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_unit:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_vehicle:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_weapon:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_device:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_scenery:
				*(int32*)global->pointer = absolute->long_value;
				break;
			case _hs_type_object_name:
				*(int16*)global->pointer = absolute->short_value;
				break;
			default:
				halt();
			}
		}
	}

	if (type < FIRST_HS_OBJECT_TYPE || type > LAST_HS_OBJECT_TYPE)
	{
		if (type == _hs_type_object_list)
		{
			int32 reference;
			for (int32 i = object_list_get_first(absolute->long_value, &reference); i != NONE; i = object_list_get_next(absolute->long_value, &reference))
			{
				object_datum* object = object_get(i);
				object->object.flags.set(_object_ever_referenced_by_hs_bit, true);
			}
		}
	}
	else if (absolute->long_value != NONE)
	{
		object_datum* object = object_get(absolute->long_value);
		object->object.flags.set(_object_ever_referenced_by_hs_bit, true);
	}
	return;
}

static bool hs_type_mask_can_cast(int16 actual_type, int16 desired_type)
{
	ASSERT(VALID_INDEX(actual_type, NUMBER_OF_HS_OBJECT_TYPES));
	ASSERT(VALID_INDEX(desired_type, NUMBER_OF_HS_OBJECT_TYPES));
	return (hs_object_type_masks[desired_type] & hs_object_type_masks[actual_type]) == hs_object_type_masks[actual_type];
}

static const char* expression_get_function_name(int32 thread_index, int32 expression_index)
{
	const hs_syntax_node* expression = hs_syntax_get(expression_index);
	const hs_thread* thread = hs_thread_get(thread_index);

	const char* result;
	if (TEST_BIT(expression->flags, _hs_syntax_node_script_bit))
	{
		result = scenario_get_hs_script(global_scenario_get(), expression->script_index)->name;
	}
	else if (expression->function_index || expression_index != thread->stack->expression_index)
	{
#ifdef HS_COMPILER_ENABLED
		result = hs_function_get(expression->function_index)->name;
#endif
		result = "";
	}
	else if (thread->stack->size <= 0)
	{
		result = "(invalid expression reference)";
	}
	else
	{
		result = *(int32*)thread->stack->data != NONE ? expression_get_function_name(thread_index, *(int32*)thread->stack->data) : "(end of script)";
	}

	return result;
}

static bool script_error(int32 thread_index, const char* message, const char* condition)
{
	bool result = false;	// Always fail on error

	error(
		_error_silent,
		"script %s needs to be recompiled. (%s: %s)",
		hs_thread_format(thread_index),
		message ? message : "no reason given.",
		condition
	);
	return result;
}

static int32* hs_arguments_evaluate(int32 thread_index, int16 formal_parameter_count, const int16* formal_parameters, bool initialize)
{
	hs_thread* thread = hs_thread_get(thread_index);
	int32* arguments = (int32*)hs_stack_allocate(thread_index, sizeof(int32) * formal_parameter_count);
	int16* argument_index = (int16*)hs_stack_allocate(thread_index, sizeof(int16));
	int32* expression_index = (int32*)hs_stack_allocate(thread_index, sizeof(int32));

	if (initialize)
	{
		*argument_index = 0;
		*expression_index = hs_syntax_get(hs_syntax_get(thread->stack->expression_index)->long_value)->next_node_index;
	}

	if (*argument_index >= formal_parameter_count)
	{
		hs_thread_assert(*expression_index == NONE, "corrupted syntax tree.", thread_index);
	}
	else
	{
		hs_thread_assert(*expression_index != NONE, "corrupted syntax tree.", thread_index);

		if (hs_script_error(hs_syntax_get(*expression_index)->type == formal_parameters[*argument_index], "unexpected actual parameters.", thread_index))
		{
			hs_evaluate(thread_index, *expression_index, &arguments[*argument_index]);
			*expression_index = hs_syntax_get(*expression_index)->next_node_index;
			++*argument_index;
			arguments = NULL;
		}
	}

	return arguments;
}
