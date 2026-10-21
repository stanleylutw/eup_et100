#ifndef EUP_AIOT_AUTH_H_
#define EUP_AIOT_AUTH_H_

#include <stdint.h>

int eup_aiot_auth_compute_token(const uint8_t nonce[16], uint8_t token[16]);

#endif
