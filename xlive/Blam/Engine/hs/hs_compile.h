#pragma once

#ifdef HS_COMPILER_ENABLED

enum
{
	k_script_name_size = 32,
	k_hs_buffer_size = 1024,
	HS_MAXIMUM_DYNAMIC_SOURCE_DATA_BYTES = 1024,
};

/* globals */


/* prototypes */

void hs_compile_initialize(bool resize);

void hs_compile_dispose(void);

int32 hs_compile_expression(int32 source_size, const char* source_data, const char** error_message_pointer, const char** error_source_pointer);

void hs_compile_source(bool fail_on_error);

void resize_scenario_syntax_data(int32 size);

bool hs_parse(int32 expression_index, int16 expected_type);

bool hs_parse_begin(int16 function_index, int32 expression_index);

bool hs_parse_if(int16 function_index, int32 expression_index);

bool hs_parse_cond(int16 function_index, int32 expression_index);

bool hs_parse_set(int16 function_index, int32 expression_index);

bool hs_parse_logical(int16 function_index, int32 expression_index);

bool hs_parse_arithmetic(int16 function_index, int32 expression_index);

bool hs_parse_equality(int16 function_index, int32 expression_index);

bool hs_parse_inequality(int16 function_index, int32 expression_index);

bool hs_parse_sleep(int16 function_index, int32 expression_index);

bool hs_parse_sleep_forever(int16 function_index, int32 expression_index);

bool hs_parse_sleep_until(int16 function_index, int32 expression_index);

bool hs_parse_wake(int16 function_index, int32 expression_index);

bool hs_parse_inspect(int16 function_index, int32 expression_index);

bool hs_parse_object_cast_up(int16 function_index, int32 expression_index);

bool hs_macro_function_parse(int16 function_index, int32 expression_index);

#endif
