/* Test ROM for ngpc_qr_result (built by check_result.py, not by the template).
 * Builds fixed vectors and error cases into RAM, then shows vector 0 as a QR
 * through examples/qr_example.c. */
#include "ngpc_hw.h"
#include "carthdr.h"
#include "ngpc_sys.h"
#include "ngpc_qr_result.h"

u8 qr_example_show(const char *text);

char g_out[12][80];
u8 g_len[12];
volatile u8 g_ready;

static const u8 ov1[13] = { 0, 1, 0, 0, 0, 0x11, 0, 0x11, 0x94, 0, 0, 0, 1 };
static const u8 one[1] = { 0xA5 };
static u8 big[32];

void main(void)
{
    u8 i;
    ngpc_init();
    for (i = 0u; i < 32u; i++) big[i] = (u8)(i * 7u + 3u);
    g_len[0] = ngpc_qr_result_build(g_out[0], 80u, "HTTPS://WWW.NGPC-DEV.COM/OVERREV/",
                                    "OV1", "PLAYER01", ov1, 13u);
    g_len[1] = ngpc_qr_result_build(g_out[1], 80u, "", "T1", "N", one, 1u);
    g_len[2] = ngpc_qr_result_build(g_out[2], 80u, "", "MG1", "ABCDEFGHIJKLMNOP", big, 32u);
    /* errors: all must return 0 */
    g_len[3] = ngpc_qr_result_build(g_out[3], 80u, "HTTPS://EXAMPLE.COM/", "X", "Y", big, 32u); /* > 77 */
    g_len[4] = ngpc_qr_result_build(g_out[4], 80u, "", "OV1", "player", ov1, 13u);   /* lowercase */
    g_len[5] = ngpc_qr_result_build(g_out[5], 80u, "HTTPS://X/?A", "OV1", "A", ov1, 13u); /* '?' */
    g_len[6] = ngpc_qr_result_build(g_out[6], 80u, "", "OV1", "A", ov1, 0u);        /* len 0 */
    g_len[7] = ngpc_qr_result_build(g_out[7], 80u, "", "ABCDEFGHI", "A", ov1, 13u);  /* tag 9 */
    g_len[8] = ngpc_qr_result_build(g_out[8], 10u, "", "OV1", "A", ov1, 13u);        /* out small */
    g_len[9] = ngpc_qr_result_build(g_out[9], 80u, "", "", "A", ov1, 13u);           /* no tag */
    g_len[10] = ngpc_qr_result_build(g_out[10], 80u, "", "OV1", "ABCDEFGHIJKLMNOPQ", ov1, 13u); /* name 17 */
    g_len[11] = ngpc_qr_result_build(g_out[11], 80u, "", "OV1", "A", ov1, 33u);      /* len 33 */
    g_ready = 1u;
    qr_example_show(g_out[0]);
    for (;;) { }
}
