/*
 * BIOS COM calls for cc900.
 *
 * Each call is  call [0xFFFE00 + vect*4]  in register bank 3. The cc900 C
 * runtime already runs in bank 3 (w==rw3, b==rb3, a==ra3, xhl==xhl3), so the
 * `ldf 3` is belt and braces rather than a real switch.
 *
 * cc900 ABI: arguments on the stack (xsp+4, xsp+8), result in WA. Nothing here
 * returns a value -- see point 1 in the header.
 */

#include "ngpc_link/ngpc_link_com.h"

/* Restore the game's normal interrupt level. COMOFFRTS does `ei 6` (VBlank
 * masked, watchdog reset on real hardware) and COMINIT also touches IFF. */
#if NGPC_LINK_FORCE_EI0
#define COM_RESTORE_IFF()  __asm(" ei 0")
#else
#define COM_RESTORE_IFF()
#endif

/* ---- No-argument calls ---- */

void ngpc_com_init(void)
{
    __asm(" ld rw3, 0x10");
    __asm(" ldf 3");
    __asm(" add w, w");
    __asm(" add w, w");
    __asm(" ld xix, 0xfffe00");
    __asm(" ld xix, (xix+w)");
    __asm(" call xix");
    COM_RESTORE_IFF();
}

void ngpc_com_send_start(void)
{
    __asm(" ld rw3, 0x11");
    __asm(" ldf 3");
    __asm(" add w, w");
    __asm(" add w, w");
    __asm(" ld xix, 0xfffe00");
    __asm(" ld xix, (xix+w)");
    __asm(" call xix");
    COM_RESTORE_IFF();
}

void ngpc_com_recv_start(void)
{
    __asm(" ld rw3, 0x12");
    __asm(" ldf 3");
    __asm(" add w, w");
    __asm(" add w, w");
    __asm(" ld xix, 0xfffe00");
    __asm(" ld xix, (xix+w)");
    __asm(" call xix");
    COM_RESTORE_IFF();
}

void ngpc_com_rts_on(void)
{
    __asm(" ld rw3, 0x15");
    __asm(" ldf 3");
    __asm(" add w, w");
    __asm(" add w, w");
    __asm(" ld xix, 0xfffe00");
    __asm(" ld xix, (xix+w)");
    __asm(" call xix");
    COM_RESTORE_IFF();
}

void ngpc_com_rts_off(void)
{
    /* The BIOS does `ei 6` in here: without the trailing `ei 0`, VBlank stays
     * masked and a real console powers off through the watchdog in ~1 s. */
    __asm(" ld rw3, 0x16");
    __asm(" ldf 3");
    __asm(" add w, w");
    __asm(" add w, w");
    __asm(" ld xix, 0xfffe00");
    __asm(" ld xix, (xix+w)");
    __asm(" call xix");
    COM_RESTORE_IFF();
}

/* ---- One byte: argument in rb3 ---- */

void ngpc_com_create_data(u8 b)
{
    __asm(" ld rw3, 0x13");
    __asm(" ld xde, (xsp+4)");    /* first argument off the stack */
    __asm(" ld b, e");            /* rb3 = byte */
    __asm(" ldf 3");
    __asm(" add w, w");
    __asm(" add w, w");
    __asm(" ld xix, 0xfffe00");
    __asm(" ld xix, (xix+w)");
    __asm(" call xix");
    COM_RESTORE_IFF();
}

/* ---- Received byte: the BIOS returns it in rb3, we store it in *out ---- */

void ngpc_com_get_data(u8 *out)
{
    __asm(" ld rw3, 0x14");
    __asm(" ldf 3");
    __asm(" add w, w");
    __asm(" add w, w");
    __asm(" ld xix, 0xfffe00");
    __asm(" ld xix, (xix+w)");
    __asm(" call xix");
    __asm(" ld xde, (xsp+4)");    /* output pointer (xsp restored by the BIOS) */
    __asm(" ld (xde), b");        /* *out = received byte */
    COM_RESTORE_IFF();
}

/* ---- Blocks: xhl3 = pointer, rb3 = size ---- */

void ngpc_com_send_block(const u8 *p, u8 n)
{
    __asm(" ld rw3, 0x19");
    __asm(" ld xhl, (xsp+4)");    /* pointer -> xhl3 */
    __asm(" ld xde, (xsp+8)");    /* size */
    __asm(" ld b, e");            /* rb3 = size */
    __asm(" ldf 3");
    __asm(" add w, w");
    __asm(" add w, w");
    __asm(" ld xix, 0xfffe00");
    __asm(" ld xix, (xix+w)");
    __asm(" call xix");
    COM_RESTORE_IFF();
}

void ngpc_com_get_block(u8 *p, u8 n)
{
    __asm(" ld rw3, 0x1a");
    __asm(" ld xhl, (xsp+4)");
    __asm(" ld xde, (xsp+8)");
    __asm(" ld b, e");
    __asm(" ldf 3");
    __asm(" add w, w");
    __asm(" add w, w");
    __asm(" ld xix, 0xfffe00");
    __asm(" ld xix, (xix+w)");
    __asm(" call xix");
    COM_RESTORE_IFF();
}

/* ---- Status words: the BIOS returns them in rwa3, stored in *out ---- */

void ngpc_com_send_status(u16 *out)
{
    __asm(" ld rw3, 0x17");
    __asm(" ldf 3");
    __asm(" add w, w");
    __asm(" add w, w");
    __asm(" ld xix, 0xfffe00");
    __asm(" ld xix, (xix+w)");
    __asm(" call xix");
    __asm(" ld xde, (xsp+4)");
    __asm(" ld (xde), wa");
    COM_RESTORE_IFF();
}

void ngpc_com_recv_status(u16 *out)
{
    __asm(" ld rw3, 0x18");
    __asm(" ldf 3");
    __asm(" add w, w");
    __asm(" add w, w");
    __asm(" ld xix, 0xfffe00");
    __asm(" ld xix, (xix+w)");
    __asm(" call xix");
    __asm(" ld xde, (xsp+4)");
    __asm(" ld (xde), wa");
    COM_RESTORE_IFF();
}

/* ---- Cable detect (advisory, see the header) ---- */

u8 ngpc_com_cable_present(void)
{
    return (u8)((NGPC_COM_PORT_B1 & NGPC_COM_DETECT_BIT) ? 0 : 1);
}
