/*
 * ngpc_timing.c - VSync synchronization and CPU speed
 *
 * Part of NgpCraft_base_template (MIT License)
 * Written from hardware specification (ngpcspec.txt).
 */

#include "ngpc_hw.h"
#include "ngpc_sys.h"
#include "ngpc_timing.h"
#if NGP_ENABLE_DEBUG && !NGP_PROFILE_RELEASE
#include "ngpc_vramq.h"
#include "ngpc_assert.h"
#endif

/* ---- Public API ---- */

void ngpc_vsync(void)
{
    static u8 s_power_hold = 0;

    /* Wait until g_vb_counter changes (incremented by VBI at 60 Hz).
     * This busy-waits but lets the CPU execute other code in the
     * interrupt handler between checks. */
    u8 prev = g_vb_counter;
    while (g_vb_counter == prev)
        ;

#if NGP_ENABLE_DEBUG && !NGP_PROFILE_RELEASE
    /* DBG-1: catch VRAMQ overflow — too many commands queued in one frame.
     * The assert fires once and halts (with file/line on screen) so you can
     * identify the offending frame. Raise VRAMQ_MAX_CMDS or reduce writes. */
    NGPC_ASSERT(ngpc_vramq_dropped() == 0);
    ngpc_vramq_clear_dropped();
#endif

    /* LE SYSTEME DEMANDE L'EXTINCTION, ET C'EST LE SEUL ENDROIT QUI LA DECIDE.
     *
     * User_Shutdown (0x6F85) est un champ de BITS et les trois du haut sont
     * definis (SysWork.txt) :
     *
     *   bit 7  interrupteur d'alimentation ;
     *   bit 6  inactivite prolongee -- dix minutes sans appui ;
     *   bit 5  TENSION DE LA PILE PRINCIPALE TROP BASSE.
     *
     * "Please initiate shutdown if any of these bits are 1" : on teste donc
     * l'octet entier, les bits du bas pouvant servir plus tard.
     *
     * ET LE BIT 5 EST CELUI QUI COMPTE ICI. Une console qui s'eteint n'est pas
     * une console qui plante : c'est le systeme qui a demande, et le jeu qui a
     * obei. Une ecriture flash est la plus grosse pointe de courant que la
     * cartouche produise, donc le moment ou une pile fatiguee tombe sous le
     * seuil -- ce qui expliquerait un extinction "a la fin de la course" qui
     * arrive sur un circuit et pas sur l'autre, tard dans une partie, et
     * qu'aucun emulateur ne peut reproduire faute de modeliser une pile. */
    if (HW_USR_SHUTDOWN) {
        ngpc_shutdown_reason(HW_USR_SHUTDOWN);
    }

    /* LE REPLI SUR UN BIT DE MANETTE EST PARTI, ET IL ETAIT FAUX.
     *
     * Il eteignait la console quand le bit 7 de Sys_Lever (0x6F82) restait a 1
     * pendant trente trames. Or ce bit N'EST PAS l'interrupteur : SysWork.txt
     * donne le plan complet de ce champ, bit 6 = bouton OPTION (bouton C d'une
     * manette NEOGEO branchee) et bit 7 = BOUTON D DE CETTE MEME MANETTE. Il
     * n'y a aucun bit d'alimentation dans Sys_Lever ; l'interrupteur ne se lit
     * que dans User_Shutdown, ci-dessus.
     *
     * Le jeu s'eteignait donc lui-meme sur un bit dont il ne savait rien --
     * exactement ce que la liste de controle Toshiba interdit pour les
     * drapeaux non definis : "please input the value 0 ... undetermined values
     * may change the operation timing". */
    (void)s_power_hold;
}

u8 ngpc_in_vblank(void)
{
    /* ngpcspec.txt: 0x8010 bit 6 = BLNK (0=displaying, 1=vblank). */
    return (HW_STATUS & STATUS_VBLANK) ? 1 : 0;
}

void ngpc_sleep(u8 frames)
{
    u8 i;

    /* Reduce CPU speed during sleep to save battery. */
    ngpc_cpu_speed(4);

    for (i = 0; i < frames; i++)
        ngpc_vsync();

    /* Restore full speed. */
    ngpc_cpu_speed(0);
}

void ngpc_cpu_speed(u8 divider)
{
    /* BIOS system call: VECT_CLOCKGEARSET.
     * ngpcspec.txt: CPU clock divider 0=6MHz, 1=3MHz, 2=1.5MHz, 3=768KHz, 4=384KHz.
     * Pass divider in RB3 and joypad-speedup flag in RC3. */
    (void)divider;
    __asm("ld rw3, " NGPC_STR(BIOS_CLOCKGEARSET));
    __asm("ld xde, (xsp+4)");    /* get divider argument */
    __asm("ld b, e");            /* RB3 = divider */
    __asm("ld c, 0");            /* RC3 = no auto speed-up on joypad */
    __asm("ldf 3");
    __asm("add w, w");
    __asm("add w, w");
    __asm("ld xix, 0xfffe00");
    __asm("ld xix, (xix+w)");
    __asm("call xix");
}
