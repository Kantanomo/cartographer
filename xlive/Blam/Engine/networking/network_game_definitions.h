#pragma once
#include "memory/bitstream.h"
#include "saved_games/game_variant.h"

void network_game_definitions_apply_patches();

void __cdecl network_game_definitions_encode_game_variant(c_bitstream* packet, s_game_variant* variant);
bool __cdecl network_game_definitions_decode_game_variant(c_bitstream* packet, s_game_variant* variant);
