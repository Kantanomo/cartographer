#pragma once

/* structures */

struct transport_address
{
	union
	{
		int32 raw_ipv4;
		uint8 ipv4[4];
		uint16 ipv6[8];
		char string[16];
	} address;
	uint16 port;
	int16 address_type;
};
ASSERT_STRUCT_SIZE(transport_address, 20);

/* prototypes */

const char* transport_address_get_string(const transport_address* address);

char* transport_address_to_string(const transport_address* address, const struct s_transport_secure_address* secure_address, char* string, int16 maximum_string_length, bool include_port, bool secure);
