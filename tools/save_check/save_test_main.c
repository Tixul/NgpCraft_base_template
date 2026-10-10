/* Test ROM for the flash journal: the main loop executes commands that
 * save_check.py writes in RAM, so every flash call runs exactly like a game
 * would make it (main loop, interrupts at level 0, VBlank live). */
#include "ngpc_hw.h"
#include "carthdr.h"
#include "ngpc_sys.h"
#include "ngpc_timing.h"
#include "ngpc_flash.h"

volatile u8  g_cmd;          /* 1 save, 2 init+load, 3 erase, 4 bad magic, 5 legacy */
volatile u8  g_done;         /* incremented after each command */
volatile u16 g_res;
volatile u32 g_arg;          /* payload seed for a save */
volatile u8  g_info[8];      /* ready, cart, exists, slots_used */
u8 g_buf[SAVE_SIZE];
u8 g_load[SAVE_SIZE];

static void report(void)
{
    u16 i;
    g_info[0] = ngpc_flash_ready();
    g_info[1] = ngpc_flash_cart_size();
    g_info[2] = ngpc_flash_exists();
    g_info[3] = ngpc_flash_slots_used();
    for (i = 0u; i < (u16)SAVE_SIZE; i++) g_load[i] = 0u;
    if (g_info[2]) ngpc_flash_load(g_load);
}

void main(void)
{
    u16 i;
    u8 c;
    ngpc_init();
    ngpc_flash_init();
    report();
    g_done = 1u;
    for (;;) {
        ngpc_vsync();
        c = g_cmd;
        if (!c) continue;
        if (c == 1u) {
            g_buf[0] = 0xCAu; g_buf[1] = 0xFEu; g_buf[2] = 0x20u; g_buf[3] = 0x26u;
            for (i = 4u; i < (u16)NGPC_FLASH_PAYLOAD; i++)
                g_buf[i] = (u8)((u8)g_arg + (u8)i);
            g_buf[4] = (u8)g_arg; g_buf[5] = (u8)(g_arg >> 8);
            g_res = ngpc_flash_save(g_buf);
        } else if (c == 2u) {
            ngpc_flash_init();
            report();
        } else if (c == 3u) {
            g_res = ngpc_flash_erase();
        } else if (c == 5u) {
            for (i = 0u; i < (u16)SAVE_SIZE; i++) g_load[i] = 0u;
            g_res = ngpc_flash_load_legacy(g_load);
        } else if (c == 4u) {
            /* bad magic must be refused */
            g_buf[0] = 0x00u;
            g_res = ngpc_flash_save(g_buf);
        }
        g_cmd = 0u;
        g_done++;
    }
}
