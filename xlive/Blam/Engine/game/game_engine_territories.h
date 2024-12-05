#pragma once
#include "game_engine_default.h"

#define k_maximum_territories_flags 8


class c_territories_engine : c_game_engine
{
public:
	virtual e_game_engine_type get_type() override;
	virtual bool setup() override;
	virtual bool function_4() override;
	virtual void send_game_start_event(datum player_index) override;
	virtual void swap_player_indices(uint32 old_index, uint32 new_index) override;
	virtual void player_team_change(datum player_index) override;
	virtual void function_13() override;
	virtual void function_14(datum player_index) override;
	virtual void render_game_engine_elements(uint32 user_index) override;
	virtual void function_16(datum player_index) override;
	virtual bool player_can_interact_with_weapon(datum player_index, datum weapon_index) override;
	virtual void function_19() override;
	virtual void update_object_color_change(datum object_index) override;
	virtual void function_31(datum player_index, datum player_index_2, bool a3, int32 a4) override;
	virtual void function_33(datum player_index, void* unk) override;
	virtual bool function_35(int32 unk_index) override;
	virtual uint32 get_game_engine_entity_type() override;
	virtual void function_41() override;
	virtual void set_simulation_baseline_data(int32 unused, void* unk) override;
	virtual void build_simulation_update(void* unk, int32 unused, void* unk_2) override;
	virtual bool apply_simulation_update(int16 flags, int32 unused, void* unk) override;
	virtual uint32 get_territory_name(wchar_t* a1, int32 a2, int32 a3, wchar_t* a4, wchar_t* a5) override;
};