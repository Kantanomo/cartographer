#pragma once

/* macros */

#define XUSER_XUID_SAME_MASK (ULONGLONG)(~(((0x3ULL << 62) | (0xC0ULL << 48))))

constexpr bool ONLINE_USER_VALID(XUID account)
{
	return account != 0ULL;
}

/* public code */

bool online_xuid_is_guest_account(XUID xuid);

bool online_xuid_same_account(XUID xuid1, XUID xuid2);

uint8 online_xuid_get_guest_account_number(XUID xuid);

bool __cdecl online_connected_to_xbox_live();

void online_account_transition_to_offline();