/* MIT. Result text builder, see ngpc_qr_result.h. C89, no global state. */
#include "ngpc_qr_result.h"

static const char b32[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ234567";

static u8 is_an(char c)
{
    return (u8)((c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z'));
}

static u8 is_qr_alpha(char c)
{
    switch (c) {
    case ' ': case '$': case '%': case '*': case '+':
    case '-': case '.': case '/': case ':':
        return 1u;
    }
    return is_an(c);
}

static u16 crc_byte(u16 crc, u8 value)
{
    u8 bit;
    crc ^= (u16)((u16)value << 8);
    for (bit = 0u; bit < 8u; bit++)
        crc = (u16)((crc & 0x8000u) ? ((u16)(crc << 1) ^ 0x1021u) : (u16)(crc << 1));
    return crc;
}

u8 ngpc_qr_result_build(char *out, u8 out_size, const char *url,
                        const char *tag, const char *name,
                        const u8 *data, u8 len)
{
    u8 p, i, n, limit, bits, byte;
    u16 crc, acc;

    if (!out || out_size == 0u) return 0u;
    out[0] = '\0';
    if (!url || !tag || !name || !data) return 0u;
    if (len == 0u || len > NGPC_QR_RESULT_DATA_MAX) return 0u;
    limit = (u8)(out_size - 1u);
    if (limit > (u8)NGPC_QR_TEXT_MAX) limit = (u8)NGPC_QR_TEXT_MAX;

    p = 0u;
    for (i = 0u; url[i] != '\0'; i++) {
        if (!is_qr_alpha(url[i]) || p >= limit) { out[0] = '\0'; return 0u; }
        out[p++] = url[i];
    }

    /* "<TAG>:<NAME>:" - written AND hashed. */
    crc = 0xFFFFu;
    for (n = 0u; tag[n] != '\0'; n++) {
        if (n >= NGPC_QR_RESULT_TAG_MAX || !is_an(tag[n]) || p >= limit) {
            out[0] = '\0'; return 0u;
        }
        out[p++] = tag[n];
        crc = crc_byte(crc, (u8)tag[n]);
    }
    if (n == 0u || p >= limit) { out[0] = '\0'; return 0u; }
    out[p++] = ':';
    crc = crc_byte(crc, (u8)':');
    for (n = 0u; name[n] != '\0'; n++) {
        if (n >= NGPC_QR_RESULT_NAME_MAX || !is_an(name[n]) || p >= limit) {
            out[0] = '\0'; return 0u;
        }
        out[p++] = name[n];
        crc = crc_byte(crc, (u8)name[n]);
    }
    if (n == 0u || p >= limit) { out[0] = '\0'; return 0u; }
    out[p++] = ':';
    crc = crc_byte(crc, (u8)':');
    for (i = 0u; i < len; i++) crc = crc_byte(crc, data[i]);

    /* Base32 of data then CRC high, CRC low. acc keeps < 13 bits. */
    acc = 0u;
    bits = 0u;
    for (i = 0u; i < (u8)(len + 2u); i++) {
        if (i < len)       byte = data[i];
        else if (i == len) byte = (u8)(crc >> 8);
        else               byte = (u8)crc;
        acc = (u16)((u16)(acc << 8) | byte);
        bits = (u8)(bits + 8u);
        while (bits >= 5u) {
            bits = (u8)(bits - 5u);
            if (p >= limit) { out[0] = '\0'; return 0u; }
            out[p++] = b32[(u8)((acc >> bits) & 31u)];
        }
        acc = (u16)(acc & (u16)((1u << bits) - 1u));
    }
    if (bits != 0u) {
        if (p >= limit) { out[0] = '\0'; return 0u; }
        out[p++] = b32[(u8)((u16)(acc << (5u - bits)) & 31u)];
    }
    out[p] = '\0';
    return p;
}
