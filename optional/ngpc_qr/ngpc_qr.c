/* Optional QR encoder, V2-M or V3-L, one RS block, fixed mask 0.
 * Templates/RS constants generated from Nayuki's MIT reference (see vendor).
 * A fixed mask is decodable but does not implement minimum-penalty selection.
 * Compile-time profile bounds RAM and runtime. Caller owns all mutable state.
 */
#include "ngpc_qr.h"
#include "ngpc_qr_tables.h"


static u8 alpha(char c)
{
    if (c >= '0' && c <= '9') return (u8)(c - '0');
    if (c >= 'A' && c <= 'Z') return (u8)(c - 'A' + 10);
    switch (c) {
    case ' ': return 36u;
    case '$': return 37u;
    case '%': return 38u;
    case '*': return 39u;
    case '+': return 40u;
    case '-': return 41u;
    case '.': return 42u;
    case '/': return 43u;
    case ':': return 44u;
    }
    return 255u;
}

static void bits(NgpcQr *qr, u16 value, u8 count)
{
    while (count != 0u) {
        count--;
        if ((value >> count) & 1u) {
            qr->codewords[qr->bit_count >> 3] |= (u8)(1u << (7u - (qr->bit_count & 7u)));
        }
        qr->bit_count++;
    }
}

static u8 multiply(u8 a, u8 b)
{
    u16 power;
    if (a == 0u || b == 0u) return 0u;
    power = (u16)((u16)qr_log[a] + (u16)qr_log[b]);
    if (power >= 255u) power -= 255u;
    return qr_exp[power];
}

u8 ngpc_qr_get_module(const NgpcQr *qr, u8 x, u8 y)
{
    u16 bit;
    if (!qr || !qr->valid || x >= NGPC_QR_SIZE || y >= NGPC_QR_SIZE) return 0u;
    bit = (u16)((u16)y * NGPC_QR_SIZE + x);
    return (u8)((qr->matrix[bit >> 3] >> (bit & 7u)) & 1u);
}

u8 ngpc_qr_encode(NgpcQr *qr, const char *text)
{
    u8 len, i, j, pad, factor, y, x, vertical;
    s16 right;
    u16 bit, index;
    if (!qr) return 0u;
    qr->valid = 0u;
    if (!text) return 0u;
    len = 0u;
    while (text[len] != '\0') {
        if (len >= NGPC_QR_TEXT_MAX || alpha(text[len]) == 255u) return 0u;
        len++;
    }
    if (len == 0u) return 0u;
    for (i = 0u; i < NGPC_QR_CODEWORDS; i++) qr->codewords[i] = 0u;
    qr->bit_count = 0u;
    bits(qr, 2u, 4u);
    bits(qr, (u16)len, 9u);
    for (i = 0u; i + 1u < len; i = (u8)(i + 2u)) {
        bits(qr, (u16)((u16)alpha(text[i]) * 45u + alpha(text[i + 1u])), 11u);
    }
    if (i < len) bits(qr, (u16)alpha(text[i]), 6u);
    bits(qr, 0u, (u8)((NGPC_QR_DATA_BITS - qr->bit_count < 4u) ? NGPC_QR_DATA_BITS - qr->bit_count : 4u));
    while (qr->bit_count & 7u) bits(qr, 0u, 1u);
    pad = 0xECu;
    while (qr->bit_count < NGPC_QR_DATA_BITS) {
        bits(qr, (u16)pad, 8u);
        pad ^= 0xFDu;
    }
    /* Remainder is accumulated directly in the final ECC codewords. */
    for (i = 0u; i < NGPC_QR_DATA_WORDS; i++) {
        factor = (u8)(qr->codewords[i] ^ qr->codewords[NGPC_QR_DATA_WORDS]);
        for (j = 0u; j < NGPC_QR_ECC_WORDS - 1u; j++) qr->codewords[NGPC_QR_DATA_WORDS + j] = qr->codewords[NGPC_QR_DATA_WORDS + 1u + j];
        qr->codewords[NGPC_QR_CODEWORDS - 1u] = 0u;
        for (j = 0u; j < NGPC_QR_ECC_WORDS; j++) {
            qr->codewords[NGPC_QR_DATA_WORDS + j] ^= multiply(qr_divisor[j], factor);
        }
    }
    for (i = 0u; i < NGPC_QR_BYTES; i++) qr->matrix[i] = qr_template[i];
    index = 0u;
    for (right = NGPC_QR_SIZE - 1u; right >= 1; right -= 2) {
        if (right == 6) right = 5;
        for (vertical = 0u; vertical < NGPC_QR_SIZE; vertical++) {
            y = (u8)((((right + 1) & 2) == 0) ? NGPC_QR_SIZE - 1u - vertical : vertical);
            for (j = 0u; j < 2u; j++) {
                x = (u8)(right - j);
                bit = (u16)((u16)y * NGPC_QR_SIZE + x);
                if ((qr_reserved[bit >> 3] & (u8)(1u << (bit & 7u))) == 0u) {
                    if (index < NGPC_QR_CODEWORDS * 8u &&
                        ((qr->codewords[index >> 3] >> (7u - (index & 7u))) & 1u)) {
                        qr->matrix[bit >> 3] ^= (u8)(1u << (bit & 7u));
                    }
                    index++;
                }
            }
        }
    }
    qr->valid = 1u;
    return 1u;
}
