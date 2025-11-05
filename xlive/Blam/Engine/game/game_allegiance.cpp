#include "stdafx.h"
#include "game_allegiance.h"

/* constants */

const char* global_campaign_team_names[16]
{
	"default",
	"player",
	"human",
	"covenant",
	"flood",
	"sentinel",
	"heretic",
	"prophet",
	"unused8",
	"unused9",
	"unused10",
	"unused11",
	"unused12",
	"unused13",
	"unused14",
	"unused15"
};

/* public code */

void __cdecl game_allegiance_update(void)
{
	INVOKE(0xB6D36, 0xA65B6, game_allegiance_update);
	return;
}
