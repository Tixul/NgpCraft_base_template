#include "ngpc_entity.h"

NgpcEntity ngpc_entities[ENTITY_COUNT];

void ngpc_entity_init_all(void)
{
    u8 i, j;
    for (i = 0; i < ENTITY_COUNT; i++) {
        ngpc_entities[i].active = 0;
        ngpc_entities[i].type   = 0;
        ngpc_entities[i].x      = 0;
        ngpc_entities[i].y      = 0;
        ngpc_entities[i].timer  = 0;
        ngpc_entities[i].flags  = 0;
        for (j = 0; j < ENTITY_DATA_SIZE; j++)
            ngpc_entities[i].data[j] = 0;
    }
}

NgpcEntity *ngpc_entity_spawn(u8 type, s16 x, s16 y)
{
    u8 i, j;
    for (i = 0; i < ENTITY_COUNT; i++) {
        if (!ngpc_entities[i].active) {
            NgpcEntity *e = &ngpc_entities[i];
            e->type   = type;
            e->x      = x;
            e->y      = y;
            e->active = 1;
            e->timer  = 0;
            e->flags  = 0;
            for (j = 0; j < ENTITY_DATA_SIZE; j++) e->data[j] = 0;
            return e;
        }
    }
    return 0; /* pool plein */
}

void ngpc_entity_update_all(void)
{
    u8 i;
    for (i = 0; i < ENTITY_COUNT; i++) {
        if (ngpc_entities[i].active)
            entity_update(&ngpc_entities[i]);
    }
}

void ngpc_entity_draw_all(void)
{
    u8 i;
    for (i = 0; i < ENTITY_COUNT; i++) {
        if (ngpc_entities[i].active)
            entity_draw(&ngpc_entities[i]);
    }
}

#if ENTITY_KILL_HOOK
/*
 * Discipline B : cache d'abord le sprite (via le hook fourni par le jeu),
 * puis désactive. Le garde `active` évite un double-hide si kill est appelé
 * deux fois sur la même entité.
 */
void ngpc_entity_kill(NgpcEntity *e)
{
    if (e->active) {
        entity_on_kill(e);   /* fourni par le jeu : ngpc_sprite_hide(...) */
        e->active = 0;
    }
}
#endif

NgpcEntity *ngpc_entity_find(u8 type)
{
    u8 i;
    for (i = 0; i < ENTITY_COUNT; i++) {
        if (ngpc_entities[i].active && ngpc_entities[i].type == type)
            return &ngpc_entities[i];
    }
    return 0;
}

u8 ngpc_entity_count_active(void)
{
    u8 i, n = 0;
    for (i = 0; i < ENTITY_COUNT; i++) {
        if (ngpc_entities[i].active) n++;
    }
    return n;
}

void ngpc_entity_kill_all(u8 type)
{
    u8 i;
    for (i = 0; i < ENTITY_COUNT; i++) {
        if (ngpc_entities[i].active && ngpc_entities[i].type == type)
            ngpc_entity_kill(&ngpc_entities[i]);   /* route par la discipline choisie */
    }
}
