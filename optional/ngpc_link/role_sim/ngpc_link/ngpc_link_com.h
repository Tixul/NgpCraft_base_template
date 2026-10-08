/* Host stand-in for the BIOS COM layer, so ngpc_link.c can be exercised off the
 * console. Same names, same shapes; the rings are plain arrays instead of the
 * BIOS's, and the "wire" is driven by the harness (sim.c).
 *
 * This is NOT part of the module. It exists so the role election can be swept
 * across announcement lags, which two real consoles cannot be made to do on
 * demand. */
#ifndef NGPC_LINK_COM_H
#define NGPC_LINK_COM_H

typedef unsigned char  u8;
typedef unsigned short u16;

#define NGPC_COM_RING_SIZE 64u

/* Per-instance state. sim.c owns one of these per console; the two functions
 * below are provided by the harness, which is also the cable. */
extern u8  sim_joypad(void);
extern u8  sim_tx_count(void);
extern u8  sim_rx_count(void);
extern void sim_send_block(const u8 *p, u8 n);
extern void sim_get_data(u8 *out);

#define NGPC_COM_JOYPAD        (sim_joypad())
#define NGPC_COM_TX_COUNT      (sim_tx_count())
#define NGPC_COM_RX_COUNT      (sim_rx_count())

#define ngpc_com_rx_pending()  ((u8)NGPC_COM_RX_COUNT)
#define ngpc_com_tx_free()     ((u8)(NGPC_COM_RING_SIZE - NGPC_COM_TX_COUNT))

#define ngpc_com_init()        ((void)0)
#define ngpc_com_recv_start()  ((void)0)
#define ngpc_com_send_start()  ((void)0)
#define ngpc_com_send_block(p, n)  sim_send_block((p), (n))
#define ngpc_com_get_data(out)     sim_get_data((out))

#endif
