#pragma once

/* constants */

enum
{
	k_sha256_hash_size_bytes = 32,
};

/* structures */

struct s_sha256_hash
{
	uint8 data[k_sha256_hash_size_bytes];
};

/* prototypes */

bool crypto_windows_sha256_hash_data(void const *data, uint32 size, s_sha256_hash *hash);
