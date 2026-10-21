#include "eup_aiot_auth.h"

#include <errno.h>
#include <tinycrypt/aes.h>
#include <tinycrypt/constants.h>

/* SuperEupAes_1 */
static const uint8_t SuperEupAes_1[16] = {
	0x38, 0x30, 0x36, 0x37, 0x35, 0x35, 0x34, 0x38,
	0x45, 0x75, 0x70, 0x46, 0x69, 0x6e, 0x42, 0x54,
};

int eup_aiot_auth_compute_token(const uint8_t nonce[16], uint8_t token[16])
{
	struct tc_aes_key_sched_struct schedule;

	if (nonce == NULL || token == NULL) {
		return -EINVAL;
	}
	if (tc_aes128_set_encrypt_key(&schedule, SuperEupAes_1) != TC_CRYPTO_SUCCESS ||
	    tc_aes_encrypt(token, nonce, &schedule) != TC_CRYPTO_SUCCESS) {
		return -EIO;
	}
	return 0;
}
