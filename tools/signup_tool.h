#pragma once

#include <openssl/opensslv.h>
#include <openssl/evp.h>
#include <openssl/hmac.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <signal/signal_protocol.h>
#include <stdint.h>
#include <iostream>

#ifdef __cplusplus
extern "C" {
#endif

std::string generate_account();
uint32_t generate_client_nonce(char* uid, uint32_t difficulty, uint32_t serverNonce);

#ifdef __cplusplus
}
#endif
