; id_road_asm.asm - la marche par tranches de id_road_write(), en assembleur
;
; POURQUOI EN ASSEMBLEUR. Profil de la ROM livree (tools/profil_course.py,
; temporisation de la cartouche comprise) : cette boucle etait le plus gros
; poste du tour de course. cc900 n'y gardait pas ses valeurs en registres --
; tout allait sur la pile -- et relisait chaque adresse de table par une
; instruction de cinq octets, cinquante-six fois par trame. Sur cette machine
; chaque octet d'instruction se lit dans la cartouche.
;
; LE CALCUL EST CELUI DU C, A L'OCTET PRES : la version C est gardee dans
; id_road.c (ID_ROAD_WALK_C=1) et tools/equivalence.py compare, tour de boucle
; par tour de boucle, la RAM de cette ROM a celle de la ROM d'origine sur les
; dix circuits.
;
; !! SEULEMENT DES FORMES QUE cc900 EMET DEJA. Les pieges materiel connus de ce
; processeur (docs du projet, fiches) venaient d'encodages que Toshiba n'emet
; pas. `python tools/formes_asm.py` verifie chaque instruction de ce fichier
; contre le code que cc900 genere pour ce projet (build/prof_asm/*.asm).
;
; void id_road_walk_asm(RoadWalk *w);
;
;   (xsp+0) adresse de retour, 4 octets ; (xsp+4) w.
;   cc900 : seul XIZ doit etre preserve (ses fonctions le poussent quand elles
;   s'en servent) ; XWA XBC XDE XHL XIX XIY sont a l'appele.
;
; SECONDE ECRITURE (2026-09-19). La premiere relisait dans le bloc, a chaque
; tranche, sept pointeurs de table et l'accumulateur : 44 500 cycles par tour.
; Celle-ci tient toute la marche en registres :
;
;   XIX = &id_line_of[k]   (descend de 1)   la pente a +SL_GRADE, la bande a +SL_BANDI
;   XIY = &id_shear[k]     (descend de 2)
;   XHL = &xy[ligne]       (descend de 2 par ligne ecrite)
;   XDE = &bande[ligne]    (descend de 2 par ligne ecrite)
;   B   = l'accumulateur   C = la ligne courante (>= top : tient sur un octet)
;   XIZ = &hill_rate[20]   (la cadence, indice signe)
;
; Les valeurs fixes du bloc sont RECOPIEES sur la pile de la routine (F_*) et
; lues par (xsp+d) : c'est ce qui libere XIZ pour la table des cadences.
;
; LES DEUX RETENUES SANS SEIZE BITS. c = (acc + 2 rate) >> 8 et le nouvel
; accumulateur est l'octet bas : c'est exactement le nombre de retenues de deux
; additions de `rate` sur huit bits, et l'octet restant.
;
; LA LIGNE Y. (u8)((row >> 8) - (u8)(scr - plane0)) = (row >> 8) + plane0 - scr
; modulo 256 : les deux formes rendent le meme octet.
;
; LA COULEUR DE BANDE EN DEUX TEMPS. band_col[lut[id_bandi[k]]] demande deux
; pointeurs de table et la marche n'a plus de registre. Elle pose donc l'INDICE
; id_bandi[k] dans l'octet bas de l'entree du tampon de couleur, et une passe
; courte, apres elle, le convertit sur les seules lignes ecrites. Aucun octet de
; RAM de plus : la pile n'en a pas a donner (voir RoadSlices dans id_road.c).
;
$MAXIMUM

; Le bloc RoadWalk (id_road.c ; sa TAILLE y est VERIFIEE a la compilation) :
V_XY      equ 0     ; u16 * &xy[road_bottom]
V_BAND    equ 4     ; u16 * &bandcol[road_bottom]
V_SL      equ 8     ; u8  * &id_line_of[lines]   (juste APRES la fin)
V_SH      equ 12    ; s16 * &id_shear[lines]     (juste APRES la fin)
V_RTAB    equ 16    ; u8  * &hill_rate[20]
V_LUT     equ 20    ; u8  * ligne de band_lut de cette trame
V_BCOL    equ 24    ; u16 * band_col
V_ROW     equ 28    ; u16  row_fx     (8.8)
V_STEP    equ 30    ; u16  step_fx
V_HALF    equ 32    ; u16  (u16)step_fx >> 1
V_TOP     equ 34    ; u8   top_limit        } recopies d'un mot
V_PLANE0  equ 35    ; u8   road_plane0      }
V_BASEX   equ 36    ; u8   road_base_x      } recopies d'un mot
V_N       equ 37    ; u8   entree : lines ; sortie : k + 1
V_LINE    equ 38    ; u8   entree : road_bottom ; sortie : la ligne a l'arret
; Les tableaux voisins de id_line_of (RoadSlices dans id_road.c) :
SL_GRADE  equ 64    ; id_grade[k] = id_line_of[k] + 64
SL_BANDI  equ 128   ; id_bandi[k] = id_line_of[k] + 128
; Le cadre de la routine, sous la sauvegarde de XIZ :
F_ROW     equ 0     ; u16  row_fx (modifie)
F_STEP    equ 2
F_HALF    equ 4
F_TOP     equ 6
F_PLANE0  equ 7
F_BASEX   equ 8
F_N       equ 9     ; le compteur de tranches
F_BOT     equ 10    ; road_bottom
F_SIZE    equ 12
F_W       equ 20    ; w : F_SIZE + XIZ (4) + adresse de retour (4)

        module  id_road_asm

        public  _id_road_walk_asm

ROADW   section code large

_id_road_walk_asm:
        push    xiz
        ld      xwa,(xsp+0x8)           ; w
        lda     xsp,xsp-F_SIZE
        ld      bc,(xwa+V_ROW)
        ld      (xsp+F_ROW),bc
        ld      bc,(xwa+V_STEP)
        ld      (xsp+F_STEP),bc
        ld      bc,(xwa+V_HALF)
        ld      (xsp+F_HALF),bc
        ld      bc,(xwa+V_TOP)          ; top et plane0
        ld      (xsp+F_TOP),bc
        ld      bc,(xwa+V_BASEX)        ; base_x et le compteur
        ld      (xsp+F_BASEX),bc
        ld      c,(xwa+V_LINE)          ; la ligne : road_bottom, >= top (le C le verifie)
        ld      (xsp+F_BOT),c
        ld      xhl,(xwa+V_XY)
        ld      xde,(xwa+V_BAND)
        ld      xix,(xwa+V_SL)
        ld      xiy,(xwa+V_SH)
        ld      xiz,(xwa+V_RTAB)
        ld      b,0x0                   ; l'accumulateur part de zero, CHAQUE trame

vw_slice:
        dec     0x1,xix
        dec     0x2,xiy
        ; ou va la tranche : id_line_of[k] = max(ligne, top) = la ligne, puisque
        ; la marche s'arrete des qu'elle passe sous top
        ld      (xix),c
        ; ---- la cadence : hill_rate[20 + id_grade[k]]
        ld      a,(xix+SL_GRADE)
        exts    wa
        ld      a,(xiz+wa)
        ld      w,a
        ; ---- les deux retenues : A = acc + rate + rate, sur huit bits
        ld      a,b
        add     a,w
        j       ult,vw_c1               ; premiere retenue
        add     a,w
        j       ult,vw_one              ; une seule retenue : une ligne
        ld      b,a                     ; aucune : la tranche ne prend pas de ligne
        j       vw_next
vw_c1:
        add     a,w
        j       ult,vw_two              ; deux retenues : deux lignes
vw_one:
        ld      b,a
        ; ---- une ligne, a la rangee de la tranche
        ld      a,(xsp+F_ROW+1)         ; (u8)(row_fx >> 8)
        add     a,(xsp+F_PLANE0)
        sub     a,c
        ld      w,a
        ld      a,(xiy)                 ; (u8)dx
        add     a,(xsp+F_BASEX)
        ld      (xhl),wa
        ld      a,(xix+SL_BANDI)        ; l'indice de bande, converti plus bas
        ld      (xde),a
        dec     0x2,xhl
        dec     0x2,xde
        dec     0x1,c
        j       vw_next

vw_two:
        ld      b,a
        ; ---- premiere ligne, a la rangee de la tranche
        ld      a,(xsp+F_ROW+1)
        add     a,(xsp+F_PLANE0)
        sub     a,c
        ld      w,a
        ld      a,(xiy)
        add     a,(xsp+F_BASEX)
        ld      (xhl),wa
        ld      a,(xix+SL_BANDI)
        ld      (xde),a
        dec     0x2,xhl
        dec     0x2,xde
        dec     0x1,c
        ; ---- seconde ligne, seulement si elle reste sur l'ecran, a mi-marche
        cp      c,(xsp+F_TOP)
        j       ult,vw_next
        ld      wa,(xsp+F_ROW)
        cp      wa,(xsp+F_HALF)
        j       ule,vw_mid              ; row_fx <= half : pas sous la rangee 0
        sub     wa,(xsp+F_HALF)
vw_mid:
        ld      a,w                     ; (u8)(mid >> 8)
        add     a,(xsp+F_PLANE0)
        sub     a,c
        ld      w,a
        ld      a,(xiy)
        add     a,(xsp+F_BASEX)
        ld      (xhl),wa
        ld      a,(xix+SL_BANDI)
        ld      (xde),a
        dec     0x2,xhl
        dec     0x2,xde
        dec     0x1,c

vw_next:
        ; TOUJOURS, meme pour une tranche qui n'a pris aucune ligne
        ld      wa,(xsp+F_STEP)
        sub     (xsp+F_ROW),wa
        cp      c,(xsp+F_TOP)
        j       ult,vw_done             ; plus d'ecran : le compteur reste k + 1
        decb    0x1,(xsp+F_N)
        j       ne,vw_slice             ; au bout : 0, soit k = -1

vw_done:
        ; ---- les indices de bande deviennent des couleurs, lignes C+1..bas.
        ; XDE pointe l'entree de la ligne C (la prochaine qu'on aurait ecrite).
        ld      a,(xsp+F_BOT)
        sub     a,c
        j       eq,vw_conv_done         ; aucune ligne ecrite
        ld      b,a
        ld      xwa,(xsp+F_W)
        ld      xix,(xwa+V_LUT)
        ld      xiy,(xwa+V_BCOL)
vw_conv:
        inc     0x2,xde
        ld      a,(xde)
        extz    wa
        ld      a,(xix+wa)
        extz    wa
        add     wa,wa
        ld      wa,(xiy+wa)
        ld      (xde),wa
        dec     0x1,b                   ; huit bits : DEC pose les drapeaux
        j       ne,vw_conv
vw_conv_done:
        ld      xwa,(xsp+F_W)
        ld      (xwa+V_LINE),c
        ; !! PAS PAR A : A est l'octet bas de XWA, qui tient le bloc. Une
        ; premiere version faisait `ld a,(xsp+F_N)` ici et ecrivait le
        ; compteur a 0x..00+37, dans la pile.
        ld      b,(xsp+F_N)
        ld      (xwa+V_N),b
        lda     xsp,xsp+F_SIZE
        pop     xiz
        ret

; ================================================================
; void id_road_ahead_asm(RoadAhead *a);
;
; build_road_ahead() de main.c : pour chaque tranche, du pare-chocs vers
; l'horizon, avancer sur le profil jusqu'a la distance que la tranche regarde,
; puis rendre le virage et la pente a cette distance, joints arrondis. La
; version C reste dans main.c (ID_ROAD_AHEAD_C=1) ; tools/equivalence.py prouve
; que les deux rendent la meme RAM, tour par tour, sur les dix circuits.
;
; Registres pendant la boucle :
;   XIX = &ahead[k]            (descend de 2)
;   XHL = &road_bend[k]        (descend de 2)
;   XBC = &id_grade[k]         (descend de 1 ; id_road_grade_buf)
;   XIY = sc, le segment courant
;   DE  = into, la position dans ce segment
;   XIZ = le bloc (ci-dessous)
;
; SECONDE ECRITURE (2026-09-19) : DEUX CACHES PAR SEGMENT, valables un appel.
; Profil par bloc : le fondu aux jonctions coutait 10 000 cycles par tour et la
; borne de pente + grade_tab 8 000, refaits a CHAQUE tranche alors qu'ils ne
; dependent que du SEGMENT tant qu'on y reste :
;   - la pente a plat d'un segment, bornee et convertie, est la meme pour
;     toutes ses tranches (cle A_FKEY) ;
;   - un fondu melange le segment courant et un voisin avec un poids w : les
;     deux ecarts (voisin - courant, virage et pente) ne dependent que du
;     segment et du cote (cles A_NKEY, A_PKEY). Il ne reste par tranche que les
;     deux multiplications, et la seconde saute quand les deux pentes sont
;     egales -- le cas de toute route plate.
; Les cles sont le numero du segment ; 0xFF = vide (track_len est un u8, un
; numero vaut au plus 254). Le C les vide avant chaque appel.
;
; Le bloc RoadAhead (main.c ; taille VERIFIEE a la compilation) :
A_AHEAD  equ 0      ; const u16 * &ahead[lines]            (juste APRES la fin)
A_BEND   equ 4      ; s16 *       &road_bend[lines]
A_GL     equ 8      ; s8 *        &id_grade[lines]
A_SEG    equ 12     ; const Segment * track_seg
A_GTAB   equ 16     ; const s8 *  &grade_tab[HILL_SLOPE_MAX]  (indice signe)
A_SC     equ 20     ; const Segment * &track_seg[seg_index]
A_INTO   equ 24     ; u16 seg_pos
A_W      equ 26     ; u16 want (temporaire, traversee de segments)
A_K      equ 28     ; u8  lignes (> 0)
A_CUR    equ 29     ; u8  seg_index
A_LEN    equ 30     ; u8  track_len
A_FKEY   equ 31     ; u8  segment de la pente a plat en cache (0xFF = vide)
A_FG     equ 32     ; s8  cette pente, bornee et convertie
A_NKEY   equ 33     ; u8  segment du fondu vers le SUIVANT en cache
A_PKEY   equ 34     ; u8  ... vers le PRECEDENT
A_NDB    equ 36     ; s16 suivant->bend - bend
A_NDP    equ 38     ; s16 suivant->slope - slope
A_PDB    equ 40     ; s16 precedent->bend - bend
A_PDP    equ 42     ; s16 precedent->slope - slope
; Segment : span u16 +0, bend s16 +2, slope s8 +4, six octets.

        public  _id_road_ahead_asm

_id_road_ahead_asm:
        push    xiz
        ld      xiz,(xsp+0x8)
        ld      xix,(xiz+A_AHEAD)
        ld      xhl,(xiz+A_BEND)
        ld      xbc,(xiz+A_GL)
        ld      xiy,(xiz+A_SC)
        ld      de,(xiz+A_INTO)
        ; la PREMIERE tranche : walked vaut 0
        dec     0x2,xix
        dec     0x2,xhl
        dec     0x1,xbc
        ld      wa,(xix)
        j       ah_want

        public  _id_road_ahead_asm__L1
_id_road_ahead_asm__L1:
ah_slice:
        dec     0x2,xix
        dec     0x2,xhl
        dec     0x1,xbc
        ; want = ahead[k] - walked, et walked est ahead[k+1] : la tranche
        ; d'avant, deux octets plus haut dans le meme tableau
        ld      wa,(xix)
        sub     wa,(xix+0x2)
        public  _id_road_ahead_asm__L2
_id_road_ahead_asm__L2:
ah_want:
        ; LE CAS COURANT SANS MEMOIRE : into += want, et si ca reste dans le
        ; segment, c'est fini. (u16)(into + want) >= span se lit sur DE lui-meme.
        add     de,wa
        cp      de,(xiy)
        j       ult,ah_seg_ok
        sub     de,wa                   ; on defait, et on marche segment par segment
        ld      (xiz+A_W),wa
        public  _id_road_ahead_asm__L3
_id_road_ahead_asm__L3:
ah_seg:
        ld      wa,de
        add     wa,(xiz+A_W)
        cp      wa,(xiy)
        j       ult,ah_seg_last
        ld      wa,(xiy)                ; want -= span - into
        sub     wa,de                   ; (formes emises par cc900 ; l'ancien
        sub     (xiz+A_W),wa            ;  `sub wa,(xiy)` n'en etait pas une)
        ld      a,(xiz+A_CUR)           ; le voisin SANS modulo
        inc     0x1,a
        cp      a,(xiz+A_LEN)
        j       ult,ah_cur_ok
        ld      a,0x0
ah_cur_ok:
        ld      (xiz+A_CUR),a
        extz    wa
        muls    xwa,0x6
        ld      xiy,(xiz+A_SEG)
        lda     xiy,xiy+wa
        ld      de,0x0
        j       ah_seg
ah_seg_last:
        add     de,(xiz+A_W)            ; into += want
        public  _id_road_ahead_asm__L4
_id_road_ahead_asm__L4:
ah_seg_ok:
        ; rem = span - into
        ld      wa,(xiy)
        sub     wa,de
        cp      wa,de
        j       ugt,ah_prev             ; pas (rem <= into)
        cp      wa,0x80
        j       uge,ah_prev             ; pas (rem < BEND_HALF)
        neg     wa                      ; w = BEND_HALF - rem
        add     wa,0x80
        push    xde
        ld      de,wa                   ; DE = w pendant le fondu
        ld      a,(xiz+A_CUR)
        cp      a,(xiz+A_NKEY)
        j       eq,ah_nhit
        ; ---- les ecarts vers le SUIVANT, une fois par segment
        ld      (xiz+A_NKEY),a
        inc     0x1,a                   ; other = cursor + 1, sans modulo
        cp      a,(xiz+A_LEN)
        j       ult,ah_nseg
        ld      a,0x0
ah_nseg:
        extz    wa
        muls    xwa,0x6
        push    xiy
        ld      xiy,(xiz+A_SEG)
        lda     xiy,xiy+wa              ; XIY = sn
        ld      wa,(xiy+0x2)
        ld      (xiz+A_NDB),wa
        ld      a,(xiy+0x4)
        exts    wa
        ld      (xiz+A_NDP),wa
        pop     xiy                     ; XIY = sc
        ld      wa,(xiy+0x2)
        sub     (xiz+A_NDB),wa          ; (s16)(sn->bend - b)
        ld      a,(xiy+0x4)
        exts    wa
        sub     (xiz+A_NDP),wa          ; (s16)sn->slope - p
        public  _id_road_ahead_asm__L5
_id_road_ahead_asm__L5:
ah_nhit:
        ; road_bend[k] = b + (s16)(((s16)(sn->bend - b) * (s16)w) >> 8)
        ld      wa,(xiz+A_NDB)
        muls    xwa,de
        sra     0x8,wa
        add     wa,(xiy+0x2)
        ld      (xhl),wa
        ld      wa,(xiz+A_NDP)
        j       ah_blend_slope

        public  _id_road_ahead_asm__L6
_id_road_ahead_asm__L6:
ah_prev:
        cp      de,0x80
        j       uge,ah_flat             ; loin des deux joints : le cas courant
        ld      wa,de                   ; w = BEND_HALF - into
        neg     wa
        add     wa,0x80
        push    xde
        ld      de,wa                   ; DE = w pendant le fondu
        ld      a,(xiz+A_CUR)
        cp      a,(xiz+A_PKEY)
        j       eq,ah_phit
        ; ---- les ecarts vers le PRECEDENT, une fois par segment
        ld      (xiz+A_PKEY),a
        cp      a,0x0                   ; other = cursor ? cursor - 1 : len - 1
        j       ne,ah_prev_dec
        ld      a,(xiz+A_LEN)
ah_prev_dec:
        dec     0x1,a
        extz    wa
        muls    xwa,0x6
        push    xiy
        ld      xiy,(xiz+A_SEG)
        lda     xiy,xiy+wa              ; XIY = sn
        ld      wa,(xiy+0x2)
        ld      (xiz+A_PDB),wa
        ld      a,(xiy+0x4)
        exts    wa
        ld      (xiz+A_PDP),wa
        pop     xiy                     ; XIY = sc
        ld      wa,(xiy+0x2)
        sub     (xiz+A_PDB),wa
        ld      a,(xiy+0x4)
        exts    wa
        sub     (xiz+A_PDP),wa
        public  _id_road_ahead_asm__L7
_id_road_ahead_asm__L7:
ah_phit:
        ld      wa,(xiz+A_PDB)
        muls    xwa,de
        sra     0x8,wa
        add     wa,(xiy+0x2)
        ld      (xhl),wa
        ld      wa,(xiz+A_PDP)
ah_blend_slope:
        ; sl = (s8)(p + (s16)(((s16)(sn->slope - p) * (s16)w) >> 8))
        cp      wa,0x0
        j       eq,ah_blend_flat        ; meme pente des deux cotes : sl = p
        muls    xwa,de
        sra     0x8,wa
        add     a,(xiy+0x4)             ; (s8) : l'octet bas de la somme suffit
        pop     xde
        j       ah_clamp
ah_blend_flat:
        pop     xde
        j       ah_grade_flat

        public  _id_road_ahead_asm__L8
_id_road_ahead_asm__L8:
ah_flat:
        ld      wa,(xiy+0x2)            ; road_bend[k] = b
        ld      (xhl),wa
ah_grade_flat:
        ; la pente du segment, bornee et convertie UNE fois par segment
        ld      a,(xiz+A_CUR)
        cp      a,(xiz+A_FKEY)
        j       ne,ah_fmiss
        ld      a,(xiz+A_FG)
        j       ah_store
ah_fmiss:
        ld      (xiz+A_FKEY),a
        ld      a,(xiy+0x4)             ; sl = (s8)p
        cp      a,0x6
        j       le,ah_fclamp_hi
        ld      a,0x6
ah_fclamp_hi:
        cp      a,0xfa
        j       ge,ah_fclamp_lo
        ld      a,0xfa
ah_fclamp_lo:
        push    xiy
        ld      xiy,(xiz+A_GTAB)
        exts    wa
        ld      a,(xiy+wa)
        pop     xiy
        ld      (xiz+A_FG),a
        j       ah_store

        public  _id_road_ahead_asm__L9
_id_road_ahead_asm__L9:
ah_clamp:
        ; la pente mise a l'echelle de la planche : grade_tab[clamp(sl) + 6]
        cp      a,0x6
        j       le,ah_clamp_hi
        ld      a,0x6
ah_clamp_hi:
        cp      a,0xfa
        j       ge,ah_clamp_lo
        ld      a,0xfa
ah_clamp_lo:
        push    xiy
        ld      xiy,(xiz+A_GTAB)
        exts    wa
        ld      a,(xiy+wa)
        pop     xiy
ah_store:
        ld      (xbc),a
        decb    0x1,(xiz+A_K)
        j       ne,ah_slice
        pop     xiz
        ret

; ================================================================
; void id_road_shear_asm(RoadShearWalk *s);
;
; Le cisaillement de chaque tranche, en tete de id_road_write() :
;   dx = (s16)((lateral * w_lat[k]) >> 8) + curve_dx[k], borne a [min, max].
; Le C reste dans id_road.c (ID_ROAD_SHEAR_C=1). cc900 y relisait sur la pile,
; a chaque tranche, trois des cinq pointeurs et le deplacement lateral : 13 200
; cycles par tour, le plus gros bloc C restant (2026-09-19).
;
;   XIX = &id_w_lat[k]      (monte de 1)
;   XIY = &id_curve_dx[k]   (monte de 2)
;   XHL = &id_dx_max[k]     (monte de 2) ; id_dx_min[k] est a -128 (RoadClamp)
;   XBC = &id_shear[k]      (monte de 2)
;   IZ  = lateral           (xsp+0) = tranches restantes
S_W      equ 0      ; const u8  * id_w_lat
S_CV     equ 4      ; const s16 * id_curve_dx
S_MX     equ 8      ; const s16 * id_dx_max  (id_dx_min = id_dx_max - 128)
S_SH     equ 12     ; s16 *       id_shear
S_LAT    equ 16     ; s16  lateral
S_N      equ 18     ; u16  lignes (> 0, < 256)
RC_MIN   equ 0x80   ; id_dx_max - id_dx_min, en octets (RoadClamp)

        public  _id_road_shear_asm

_id_road_shear_asm:
        push    xiz
        ld      xwa,(xsp+0x8)
        ld      xix,(xwa+S_W)
        ld      xiy,(xwa+S_CV)
        ld      xhl,(xwa+S_MX)
        ld      xbc,(xwa+S_SH)
        ld      iz,(xwa+S_LAT)
        ld      de,(xwa+S_N)
        push    de                      ; le compteur, en (xsp)
sh_loop:
        ld      e,(xix+:1)
        extz    de
        muls    xde,iz
        sra     0x8,de                  ; (s16)(lateral * w) >> 8 : l'octet bas du produit, comme cc900
        ld      wa,(xiy+:2)
        add     wa,de
        cp      wa,(xhl)
        j       gt,sh_hi                ; dx > max
        cp      wa,(xhl-RC_MIN)
        j       ge,sh_st
        ld      wa,(xhl-RC_MIN)         ; dx < min
        j       sh_st
sh_hi:
        ld      wa,(xhl)
sh_st:
        ld      (xbc+:2),wa
        inc     0x2,xhl
        decb    0x1,(xsp)
        j       ne,sh_loop
        pop     de
        pop     xiz
        ret

; ================================================================
; void id_road_curve_asm(RoadCurve *c);
;
; La double integration de id_road_frame_ahead() : la courbure de chaque
; tranche, du pare-chocs vers l'horizon, sommee deux fois -- le cap, puis le
; deplacement lateral. Le C reste dans id_road.c (ID_ROAD_CURVE_C=1).
;
;   XIX = &bend[k] (descend de 2)   XIY = &id_curve_dx[k] (descend de 2)
;   DE  = id_curve_cm               BC  = slope      HL = acc      IZ = k
C_BEND   equ 0      ; const s16 * &bend[lines]
C_OUT    equ 4      ; s16 *       &id_curve_dx[lines]
C_CM     equ 8      ; u16 id_curve_cm
C_N      equ 10     ; u16 lignes (> 0)

        public  _id_road_curve_asm

_id_road_curve_asm:
        push    xiz
        ld      xiz,(xsp+0x8)
        ld      xix,(xiz+C_BEND)
        ld      xiy,(xiz+C_OUT)
        ld      de,(xiz+C_CM)
        ld      iz,(xiz+C_N)
        ld      bc,0x0
        ld      hl,0x0
        ; (Un cache "meme virage que la tranche d'avant, meme c" a ete essaye
        ; le 2026-09-19 : 71 a 76 % de tranches repetees, et ZERO cycle gagne
        ; mesure -- la comparaison en memoire et le saut pris coutent ce que la
        ; multiplication coutait. Retire.)
cv_loop:
        dec     0x2,xix
        ld      wa,(xix)                ; b
        cp      wa,0x0
        j       ge,cv_pos
        neg     wa                      ; mag = -b
        mul     xwa,de                  ; (u16)mag * cm, 16 bits gardes
        add     wa,0x80
        srl     0x8,wa                  ; c
        neg     wa                      ; b < 0 : -c
        j       cv_have
cv_pos:
        mul     xwa,de
        add     wa,0x80
        srl     0x8,wa
cv_have:
        add     bc,wa                   ; slope += c
        add     hl,bc                   ; acc += slope
        ld      wa,hl
        sra     0x8,wa
        dec     0x2,xiy
        ld      (xiy),wa                ; id_curve_dx[k] = acc >> 8
        sub     iz,0x1                  ; SUB pose les drapeaux (DEC 16 bits non)
        j       ne,cv_loop
        pop     xiz
        ret

        end
