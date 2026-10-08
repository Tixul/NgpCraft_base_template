/* MIT. Text/URL QR encoder independent of any game, score format or website. */
#ifndef NGPC_QR_H
#define NGPC_QR_H
#include "ngpc_types.h"
#include "ngpc_qr_config.h"
typedef struct {
    u8 matrix[NGPC_QR_BYTES];
    u8 codewords[NGPC_QR_CODEWORDS];
    u16 bit_count;
    u8 valid;
} NgpcQr;
/* Use a static instance on NGPC. No heap, no hidden mutable global state.
 * NUL-terminated text: 0-9 A-Z space $ % * + - . / : only; no lowercasing or
 * silent truncation. Returns 0 for NULL/empty/invalid/too-long input and
 * invalidates the old matrix. Never modifies text. Not safe to overlap text
 * with qr, or to encode concurrently into the SAME instance.
 * Fixed mask 0 is decodable, but does not select the minimum-penalty mask. */
u8 ngpc_qr_encode(NgpcQr *qr, const char *text);
/* Returns 0 outside the symbol or if qr is invalid/NULL. */
u8 ngpc_qr_get_module(const NgpcQr *qr, u8 x, u8 y);
#endif
