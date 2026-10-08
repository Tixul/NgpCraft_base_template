#ifndef NGPC_QR_CONFIG_H
#define NGPC_QR_CONFIG_H
/* Compile ALL consumers with the same profile. Rebuild after changing it.
 * 2 = V2-M (38 alphanumeric characters), 3 = V3-L (77 characters).
 * Override globally with -DNGPC_QR_PROFILE=2 or edit the default here. */
#ifndef NGPC_QR_PROFILE
#define NGPC_QR_PROFILE 3
#endif
#if NGPC_QR_PROFILE == 2
#define NGPC_QR_SIZE 25u
#define NGPC_QR_BYTES 79u
#define NGPC_QR_TEXT_MAX 38u
#define NGPC_QR_DATA_WORDS 28u
#define NGPC_QR_ECC_WORDS 16u
#elif NGPC_QR_PROFILE == 3
#define NGPC_QR_SIZE 29u
#define NGPC_QR_BYTES 106u
#define NGPC_QR_TEXT_MAX 77u
#define NGPC_QR_DATA_WORDS 55u
#define NGPC_QR_ECC_WORDS 15u
#else
#error NGPC_QR_PROFILE_must_be_2_or_3
#endif
#define NGPC_QR_CODEWORDS (NGPC_QR_DATA_WORDS + NGPC_QR_ECC_WORDS)
#define NGPC_QR_DATA_BITS (NGPC_QR_DATA_WORDS * 8u)
#define NGPC_QR_MAP_SIZE ((NGPC_QR_SIZE + 9u) / 2u)
#define NGPC_QR_PIXELS (NGPC_QR_MAP_SIZE * 8u)
#endif
