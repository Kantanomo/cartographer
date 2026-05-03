#pragma once
#include "game/player_constants.h"
#include "networking/network_game_definitions.h"

/* typedefs */

typedef bool (__cdecl *life_cycle_update_t)(void* life_cycle_handler);
typedef void (__cdecl *life_cycle_initialize_t)(void* life_cycle_handler_requested, void* life_cycle_handler_current, int32 a3, void* a4);
typedef void (__cdecl *life_cycle_dispose_t)(void* life_cycle_handler_current, void* life_cycle_handler_requested);

/* classes */

class c_game_life_cycle_handler_functions
{
public:
	life_cycle_update_t update;
	life_cycle_initialize_t initialize;
	life_cycle_dispose_t dispose;
};

class c_game_life_cycle_handler
{
public:
	c_game_life_cycle_handler_functions* functions;
	e_life_cycle_state m_life_cycle;
	class c_game_life_cycle_manager* m_life_cycle_manager;
	bool field_C;
	void initialize(class c_game_life_cycle_manager* life_cycle_manager, e_life_cycle_state life_cycle, bool a3);
};

class c_game_life_cycle_handler_none : c_game_life_cycle_handler
{
	int8 gap[15];
};
ASSERT_STRUCT_SIZE(c_game_life_cycle_handler_none, 0x20);

class c_game_life_cycle_handler_pre_game : c_game_life_cycle_handler
{
	int32 count_down_network_time;
	int32 count_down_network_session;
	int32 field_18;
	int8 gap_1C[15];
};
ASSERT_STRUCT_SIZE(c_game_life_cycle_handler_pre_game, 0x2C);

class c_game_life_cycle_handler_start_game : c_game_life_cycle_handler
{
	int8 field_10;
	int8 field_11;
	int8 gap_12[25];
};
ASSERT_STRUCT_SIZE(c_game_life_cycle_handler_start_game, 0x2C);

struct c_game_life_cycle_handler_in_game : c_game_life_cycle_handler
{
	int64 rand_bytes;
	int8 gap_18[15];
};
ASSERT_STRUCT_SIZE(c_game_life_cycle_handler_in_game, 0x28);

struct c_game_life_cycle_handler_post_game : c_game_life_cycle_handler
{
	int8 gap[15];
};
ASSERT_STRUCT_SIZE(c_game_life_cycle_handler_post_game, 0x20);

class c_game_life_cycle_handler_joining : c_game_life_cycle_handler
{
public:
	bool field_10;
	int8 field_11;
	bool field_12;
	int32 field_14;
	XNKID joining_xnkid;
	XNKEY joining_xnkey;
	XNADDR joining_xnaddr;
	int32 field_54;
	int32 joining_user_count;
	s_player_identifier player_identifiers[k_number_of_users];
	wchar_t player_names[k_number_of_users][XUSER_NAME_SIZE];
	int8 gapFC[128];
	int32 field_17C;
	bool field_180;
	int8 field_181;
	int8 gap_182[2];
	int32 field_184;
	int32 field_188;
	int32 field_18C;
	bool field_190;
	int8 field_191;
	bool field_192;
	int8 gap_193[5];
	int32 field_198;
	int8 gap_19C[3804];
	int32 field_1078;
	int8 gap_107C[4];
	int32 join_attempt_result_code;
	static void __cdecl check_joining_capability();
};
ASSERT_STRUCT_SIZE(c_game_life_cycle_handler_joining, 0x1084);

struct c_game_life_cycle_handler_matchmaking : c_game_life_cycle_handler
{
	void(__thiscall* m_execute)(void*);
	int32 field_14;

	void update() 
	{
		// ugly but IDC, this structure holds an actual subclass
		if (m_execute)
			m_execute((void*)&m_execute);
	}
};
ASSERT_STRUCT_SIZE(c_game_life_cycle_handler_matchmaking, 0x18);

class c_game_life_cycle_manager
{
public:
	// TODO: make all these public member variables private
	e_life_cycle_state m_current_state;
	c_game_life_cycle_handler* m_life_cycle_handlers[k_life_cycle_state_count];
	void* m_network_session_manager;
	class c_network_session* m_active_squad_session;
	class c_network_session* m_secondary_squad_session;
	void* m_network_message_gateway;
	class c_network_observer* m_network_observer;
	bool m_state_change_active;
	bool m_update_active;
	bool m_update_requested;

	e_life_cycle_state m_requested_life_cycle;
	int32 m_entry_data_size;
	uint8 m_entry_data[4];

	c_game_life_cycle_manager(
		void* network_message_gateway, 
		class c_network_observer* network_observer, 
		void* network_session_manager, 
		class c_network_session* squad_session_one,
		class c_network_session* squad_session_two)
	{
		csmemset(m_life_cycle_handlers, 0, sizeof(m_life_cycle_handlers));
		
		m_current_state = _life_cycle_state_none;
		m_network_message_gateway = network_message_gateway;
		m_network_observer = network_observer;
		m_network_session_manager = network_session_manager;
		m_active_squad_session = squad_session_one;
		m_secondary_squad_session = squad_session_two;
		m_update_active = false;
		m_state_change_active = false;
		m_update_requested = false;
		m_requested_life_cycle = _life_cycle_state_none;
	}

	static c_game_life_cycle_manager* get();

	void update()
	{
		INVOKE_TYPE(0x1D7738, 0x1BCDA8, void(__thiscall*)(c_game_life_cycle_manager*), this);
	}

	e_life_cycle_state get_life_cycle() const;
	bool get_active_session(class c_network_session** out_session) const;
	
	bool state_is_joining() const;
	bool state_is_in_game() const;
	void request_state_change(e_life_cycle_state requested_state, int a3, void* a4);

	e_life_cycle_state get_current_state(void) const
	{
		ASSERT(m_current_state>=0&&m_current_state<k_life_cycle_state_count);

		return m_current_state;
	}
};
ASSERT_STRUCT_SIZE(c_game_life_cycle_manager, 68);

bool game_life_cycle_initialized();
bool network_life_cycle_in_squad_session(class c_network_session** out_network_session);

void game_life_cycle_apply_patches();

void life_cycle_update(void);
