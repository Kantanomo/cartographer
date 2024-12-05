#pragma once
#include "game_engine_slayer.h"



class c_test_engine : public c_slayer_engine
{
public:
	virtual void function_14(datum player_index) override;
};

static c_test_engine g_test_engine;