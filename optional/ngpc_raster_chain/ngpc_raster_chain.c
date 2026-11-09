/*
 * ngpc_raster_chain.c - CPU raster splits without MicroDMA
 *
 * Part of NgpCraft_base_template (MIT License)
 *
 * Sonic §2.3 chain pattern (each ISR handler):
 *   1. Write 1-2 video registers
 *   2. Reprogram TREG0 = (next_split.line - this_split.line)
 *   3. Stop + restart Timer0 so the count resets from the new TREG0
 *   4. RETI
 *
 * This implementation uses a single ISR function with a state index,
 * equivalent to Sonic's RAM function-pointer chain but simpler in C.
 *
 * Timer0 HBlank clock:
 *   T01MOD bits 1-0 = 0b00 (TI0 clock source, internal)
 *   TREG0 = N  → ISR fires N HBlanks after (re)start
 *   HBlank rate = 152 per frame (one per visible scanline)
 */

#include "ngpc_raster_chain.h"
#include "../../src/core/ngpc_hw.h"

/* ---- State ---- */

static const RChainSplit *s_splits;
static u8 s_count;
static volatile u8 s_idx; /* (compat) index du prochain evenement */

/* corrigé campagne 2026-11-08 : evenements precalcules par arm() pour que l'ISR soit
 * courte (voir la note ci-dessous). Un evenement = un groupe de decoupes de meme ligne. */
static u8 s_nev;
static u8 s_ev_split[RCHAIN_MAX_SPLITS];  /* decoupe a appliquer (la derniere du groupe) */
static u8 s_ev_treg[RCHAIN_MAX_SPLITS];   /* TREG0 vers l'evenement suivant              */

/* ---- HBlank ISR ---- */

/* corrigé campagne 2026-11-08 -- cause des decoupes mal placees (mesuree en emulation,
 * trace des entrees d'ISR) :
 *  1. L'impulsion TI0 (H-int) arrive UNE ligne en avance : la 1re de l'image au debut de
 *     la ligne 198 (elle prepare la ligne 0), puis au debut de la ligne N pour la ligne
 *     N+1. Depuis le VBlank, la valeur de la ligne L s'ecrit a la (L+1)-ieme impulsion :
 *     1er TREG0 = L+1 (l'ancien code mettait L).
 *  2. L'ancienne ISR arretait puis relancait Timer0 : une impulsion tombant pendant
 *     l'arret est perdue (une ISR cc900 dure plusieurs centaines de cycles, une ligne
 *     515) -> decoupes de lignes consecutives espacees de 2 lignes, et une decoupe en
 *     ligne 0 (TREG0 = 1) re-declenchait aussitot l'IRQ suivante (decoupe 64 en ligne 1).
 *     Le compteur repart de 0 a chaque concordance : il suffit d'ecrire le nouveau TREG0,
 *     en PREMIER, sans arreter le timer.
 *  3. Les decoupes de la ligne 0 sont ecrites par arm() directement (en VBlank).
 *  4. Plusieurs decoupes sur la meme ligne : la derniere gagne (avant : TREG0 = 0 = 256). */
/* ISR minimale (corrigé campagne 2026-11-08) : en emulation l'ISR C d'origine durait
 * plus d'une ligne ; deux decoupes sur des lignes consecutives tombaient 1 ligne trop tard.
 * Tout est precalcule par arm() : 4 octets de scroll par evenement + TREG0 suivant. */
static u8 s_ev_regs[RCHAIN_MAX_SPLITS * 4u];
static u8 * volatile s_ev_ptr;           /* registres du prochain evenement     */
static const u8 * volatile s_treg_ptr;   /* TREG0 a ecrire a l'entree suivante  */
static volatile u8 s_left;               /* evenements restants                 */

static void __interrupt isr_rchain(void)
{
    u8 *p;

    if (s_left == 0u) {                  /* IRQ en trop : rien a appliquer */
        HW_TRUN &= (u8)~0x01u;
        return;
    }
    HW_TREG0 = *s_treg_ptr;              /* tout de suite : le compteur repart de 0 */
    p = s_ev_ptr;
    HW_SCR1_OFS_X = p[0];
    HW_SCR1_OFS_Y = p[1];
    HW_SCR2_OFS_X = p[2];
    HW_SCR2_OFS_Y = p[3];
    s_ev_ptr   = p + 4;
    s_treg_ptr = s_treg_ptr + 1;
    if (--s_left == 0u) HW_TRUN &= (u8)~0x01u;      /* derniere : arret jusqu'au VBlank */
    s_idx++;
}

/* ---- Public API ---- */

void ngpc_rchain_init(void)
{
    s_splits = 0;
    s_count  = 0;
    s_idx    = 0;
    s_nev    = 0;
    s_left   = 0;

    /* Enable the Timer0 IRQ once, via the BIOS. Writing the interrupt-level
     * registers directly does NOT arm the IRQ on NGPC hardware -- the BIOS owns
     * the interrupt-level hardware. Convention per the official Toshiba SDK
     * (8Bit.txt "H-int Setting"): rw3 = VECT_INTLVSET (4), rb3 = priority level,
     * rc3 = interrupt number (2 = 8-bit Timer 0). The per-frame arm() below only
     * (re)programs T01MOD/TREG0/TRUN; the interrupt level is set here just once. */
    __asm("ldb rb3, 4");   /* priority level 4 (VBlank-level, fires under EI 0) */
    __asm("ldb rc3, 2");   /* interrupt number 2 = Timer0                       */
    __asm("ldb rw3, 4");   /* rw3 = BIOS_INTLVSET (= 4)                         */
    __asm("swi 1");        /* BIOS installs the level; NOW the IRQ can fire     */
}

void ngpc_rchain_arm(const RChainSplit *splits, u8 count)
{
    u8 i, n, first;

    if (!splits || count == 0)
        return;

    HW_TRUN &= (u8)~0x01u;    /* stop Timer0 only (corrigé campagne 2026-11-08 : ~0xC3
                               * effacait aussi PRRUN et T1RUN -> plus de decoupe, son coupe) */
    s_splits = splits;
    s_count  = (count > RCHAIN_MAX_SPLITS) ? RCHAIN_MAX_SPLITS : count;

    /* Groupes de meme ligne -> evenements. */
    n = 0;
    for (i = 0; i < s_count; i++) {
        if (n > 0 && splits[s_ev_split[n - 1u]].line == splits[i].line) {
            s_ev_split[n - 1u] = i;                       /* la derniere gagne */
        } else {
            s_ev_split[n] = i;
            n++;
        }
    }
    for (i = 0; (u8)(i + 1u) < n; i++)
        s_ev_treg[i] = (u8)(splits[s_ev_split[i + 1u]].line - splits[s_ev_split[i]].line);
    s_nev = n;

    /* Ligne 0 : ecrite tout de suite (on est en VBlank). */
    first = 0;
    if (splits[s_ev_split[0]].line == 0u) {
        i = s_ev_split[0];
        HW_SCR1_OFS_X = splits[i].scr1x;
        HW_SCR1_OFS_Y = splits[i].scr1y;
        HW_SCR2_OFS_X = splits[i].scr2x;
        HW_SCR2_OFS_Y = splits[i].scr2y;
        first = 1;
    }
    s_idx = first;
    if (first >= n) { s_left = 0u; return; }        /* rien d'autre : timer arrete */
    for (i = first; i < n; i++) {
        u8 k = s_ev_split[i];
        s_ev_regs[(u8)(i * 4u)]      = splits[k].scr1x;
        s_ev_regs[(u8)(i * 4u + 1u)] = splits[k].scr1y;
        s_ev_regs[(u8)(i * 4u + 2u)] = splits[k].scr2x;
        s_ev_regs[(u8)(i * 4u + 3u)] = splits[k].scr2y;
    }
    s_ev_treg[(u8)(n - 1u)] = 1u;                   /* inutilise (timer arrete apres) */
    s_ev_ptr   = &s_ev_regs[(u8)(first * 4u)];
    s_treg_ptr = &s_ev_treg[first];
    s_left     = (u8)(n - first);

    /* Install ISR on Timer0 vector. */
    HW_INT_TIM0 = (IntHandler *)isr_rchain;

    /* Timer0 sur TI0 (H-int), mode 8 bits. Valeur de la ligne L ecrite a la (L+1)-ieme
     * impulsion depuis le VBlank (voir la note au-dessus de l'ISR). */
    HW_T01MOD &= (u8)~0xC3u;
    HW_TREG0   = (u8)(splits[s_ev_split[first]].line + 1u);
    HW_TRUN   |= 0x01u; /* start */
}

void ngpc_rchain_disarm(void)
{
    HW_TRUN &= (u8)~0x01u; /* stop Timer0 */
    /* Do not NULL HW_INT_TIM0: a late-firing interrupt is harmless
     * with the timer stopped, but a NULL pointer crash is not. */
    s_count = 0;
    s_idx   = 0;
    s_nev   = 0;
    s_left  = 0;
}
