#pragma once
#include "game_engine_default.h"


#define k_ctf_flag_count 9












class c_ctf_engine : public c_game_engine_default
{
public:
	virtual e_game_engine_type get_type() override;
	virtual bool setup() override;
	virtual bool verify_netpoints(uint32 netpoint_index) override;
};