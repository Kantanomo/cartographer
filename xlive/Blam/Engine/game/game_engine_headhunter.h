#pragma once
#include "game_engine_default.h"
#include "game_engine_slayer.h"


class c_headhunter_engine : public c_slayer_engine
{
public:
	virtual e_game_engine_type get_type() override;
	virtual bool function_34(datum player_index, void* unk) override;
	//virtual bool setup() override;
	//virtual void player_join(datum player_index) override;
	//virtual void send_game_start_event(datum player_index) override;
	//virtual void swap_player_indices(uint32 old_index, uint32 new_index) override;
	//virtual void function_13() override;
	//virtual void render_game_engine_hud_elements(uint32 user_index) override;
	//virtual void function_16(datum player_index) override;
	//virtual void function_31(datum player_index, datum player_index_2, bool a3, int32 a4) override;
	//virtual bool function_35(int32 unk_index) override;
	virtual uint32 get_game_engine_entity_type() override;
	//virtual void function_41() override;
	virtual void set_simulation_baseline_data(int32 unused, void* state_data) override;
	virtual void build_simulation_update(uint32* update_mask, int32 unused, void* state_data)  override;
	virtual bool apply_simulation_update(uint32 update_mask, int32 unused, void* state_data) override;
};

static c_headhunter_engine g_headhunter_engine;