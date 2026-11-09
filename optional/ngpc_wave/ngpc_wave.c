#include "ngpc_wave.h"

/* Bit interne : la 1re update a eu lieu (le timer avance à partir de la 2e). */
#define _WAVE_FLAG_STARTED 0x04

void ngpc_wave_start(NgpcWaveSeq *seq,
                     const NgpcWaveEntry *entries, u8 count)
{
    seq->entries = entries;
    seq->count   = count;
    seq->timer   = 0;
    seq->next    = 0;
    seq->flags   = (count > 0) ? WAVE_FLAG_ACTIVE : WAVE_FLAG_DONE;
}

void ngpc_wave_stop(NgpcWaveSeq *seq)
{
    seq->flags = 0;
    seq->timer = 0;
    seq->next  = 0;
}

/* Retourne un pointeur si un spawn est dû au timer courant. */
static const NgpcWaveEntry *_wave_poll_internal(NgpcWaveSeq *seq)
{
    const NgpcWaveEntry *e;

    if (!(seq->flags & WAVE_FLAG_ACTIVE)) return 0;
    if (seq->next >= seq->count) {
        seq->flags = (u8)((seq->flags & ~WAVE_FLAG_ACTIVE) | WAVE_FLAG_DONE);
        return 0;
    }

    e = &seq->entries[seq->next];

    /* Vérification sentinel */
    if (e->delay == WAVE_END) {
        seq->flags = (u8)((seq->flags & ~WAVE_FLAG_ACTIVE) | WAVE_FLAG_DONE);
        return 0;
    }

    if (seq->timer >= e->delay) {
        seq->next++;
        return e;
    }
    return 0;
}

const NgpcWaveEntry *ngpc_wave_update(NgpcWaveSeq *seq)
{
    const NgpcWaveEntry *e;

    if (!(seq->flags & WAVE_FLAG_ACTIVE)) return 0;

    /* corrigé campagne 2026-11-08 : avant, le timer n'avançait que si next>0 || timer>0,
     * donc une table dont la 1re entrée a un délai > 0 ne démarrait jamais.
     * 1re update = image 0 (pas d'incrément), puis +1 par update. */
    if (seq->flags & _WAVE_FLAG_STARTED) {
        if (seq->timer < 0xFFFFu) seq->timer++;
    } else {
        seq->flags |= _WAVE_FLAG_STARTED;
    }

    e = _wave_poll_internal(seq);
    return e;
}

void ngpc_wave_tick(NgpcWaveSeq *seq)
{
    if (!(seq->flags & WAVE_FLAG_ACTIVE)) return;
    if (seq->timer < 0xFFFFu) seq->timer++;
}

const NgpcWaveEntry *ngpc_wave_poll(NgpcWaveSeq *seq)
{
    return _wave_poll_internal(seq);
}
