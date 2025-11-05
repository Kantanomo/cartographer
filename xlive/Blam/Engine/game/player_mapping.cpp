#include "stdafx.h"
#include "player_mapping.h"

#include "players.h"

/* public code */

int32 player_mapping_first_active_output_user(void)
{
    s_players_globals* players_globals = get_players_globals();

    int32 result = 0;
    for (size_t i = 0; players_globals->player_user_mapping[i] == NONE; ++i)
    {
        if (i + 1 >= NUMBEROF(players_globals->player_user_mapping))
        {
            result = NONE;
            break;
        }
    }
    return result;
}
