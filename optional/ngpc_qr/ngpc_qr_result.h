/* MIT. Result text for ngpc_qr, in the format OVER REV ships (OV1).
 *
 *   <URL><TAG>:<NAME>:<BASE32(data + CRC16)>
 *
 * - URL : optional prefix, e.g. "HTTPS://EXAMPLE.COM/MYGAME/" (QR alphanumeric
 *         alphabet only: 0-9 A-Z space $ % * + - . / :). "" = bare text.
 *         NOT covered by the CRC: the server checks it separately.
 * - TAG : format + version chosen by the game, [A-Z0-9]{1,8}, e.g. "OV1".
 * - NAME: player name, [A-Z0-9]{1,NGPC_QR_RESULT_NAME_MAX}.
 * - data: 1..NGPC_QR_RESULT_DATA_MAX bytes serialised BY THE GAME, field by
 *         field (never a raw C struct: padding and endianness are cc900's).
 * - CRC : CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF, no reflection, no
 *         final xor) of the ASCII "<TAG>:<NAME>:" then data, appended
 *         big-endian (high byte first).
 * - Base32: RFC 4648 alphabet A-Z 2-7, no '=' padding (not in the QR
 *         alphanumeric set); a last partial group is padded with zero bits.
 *
 * (len + 2) bytes give ceil((len + 2) * 8 / 5) characters: 13 data bytes =>
 * 24 characters, exactly OVER REV's OV1 packet.
 *
 * The CRC detects scan/typing errors. It does NOT authenticate the player or
 * the performance: anyone can build a valid code. Treat a result as a claim.
 * Reference encoder/decoder for the server side: tools/result_codec.py.
 */
#ifndef NGPC_QR_RESULT_H
#define NGPC_QR_RESULT_H
#include "ngpc_types.h"
#include "ngpc_qr_config.h"

#define NGPC_QR_RESULT_TAG_MAX   8u
#define NGPC_QR_RESULT_NAME_MAX  16u
#define NGPC_QR_RESULT_DATA_MAX  32u

/* Writes the NUL-terminated text into out and returns its length (> 0).
 * Returns 0, out[0] = '\0' when out_size allows, for: NULL argument, empty or
 * invalid TAG/NAME, URL character outside the QR alphanumeric set, len 0 or
 * above NGPC_QR_RESULT_DATA_MAX, or a text longer than out_size - 1 or than
 * NGPC_QR_TEXT_MAX (what one QR of the compiled profile can hold).
 * No global state; out must not overlap the inputs. */
u8 ngpc_qr_result_build(char *out, u8 out_size, const char *url,
                        const char *tag, const char *name,
                        const u8 *data, u8 len);

#endif
