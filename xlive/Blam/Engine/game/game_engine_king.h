#pragma once
#include "game_engine_default.h"

class c_king_engine : c_game_engine
{
public:
	virtual e_game_engine_type get_type() override;
	virtual bool setup() override;
	virtual bool function_4() override;
	virtual void send_game_start_event(datum player_index) override;
	virtual void function_13() override;
	virtual void function_14(datum player_index) override;
	virtual void render_game_engine_hud_elements(uint32 user_index) override;
	virtual void function_16(datum player_index) override;
	virtual void function_19() override;
	virtual void get_multiplayer_score_string(wchar_t* out_string) override;
	virtual void function_31(datum player_index, datum player_index_2, bool a3, int32 a4) override;
	virtual void function_33(datum player_index, void* unk) override;
	virtual bool function_35(int32 unk_index) override;
	virtual bool test_variant_engine_flag(datum player_index, e_game_engine_variant_flag_test_type type) override;
	virtual uint32 get_game_engine_entity_type() override;
	virtual void function_41() override;
	virtual void set_simulation_baseline_data(int32 unused, void* unk) override;
	virtual void build_simulation_update(void* unk, int32 unused, void* unk_2) override;
	virtual bool apply_simulation_update(int16 flags, int32 unused, void* unk) override;
};
