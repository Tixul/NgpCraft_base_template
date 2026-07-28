#ifndef NGPC_LINK_COM_H
#define NGPC_LINK_COM_H

/*
 * BIOS COM calls -- the low layer of the NGPC link cable.
 *
 * The cable is the TLCS-900 SC0 serial channel, driven through BIOS system
 * calls (call [0xFFFE00 + vect*4], register bank 3). Line: UART 8N1, CTS/RTS
 * handshake, 19200 bps. Vector numbers 0x10..0x1A were verified against a real
 * BIOS dump; most public headers stop at 0x0E.
 *
 * Three things shape this interface:
 *
 * 1. No wrapper returns a value. Built at -O3, a cc900 function whose body is
 *    inline asm without a C return yields garbage, so every result is written
 *    through a pointer and buffer levels are read from the BIOS counters at
 *    0x6D00 / 0x6D01 (documented ABI, the official SDK reads them too).
 *
 * 2. COMOFFRTS starts with `ei 6`, which masks VBlank: the SDK idiom
 *    `com_rts_off(); WaitVsync();` hangs, and on real hardware the watchdog
 *    powers the console off after ~1 s. Every wrapper therefore ends with
 *    `ei 0` (NGPC_LINK_FORCE_EI0) -- so never call them from an ISR.
 *
 * 3. The wire is asynchronous. 19200 bps is ~32 bytes per 60 Hz frame and the
 *    BIOS rings hold 64: never block waiting for a byte.
 */

/* u8/u16 come from the host project. Override with
 * -DNGPC_LINK_TYPES_HEADER="ngpc.h"; nothing else is required. */
#ifndef NGPC_LINK_TYPES_HEADER
#define NGPC_LINK_TYPES_HEADER "ngpc_types.h"
#endif
#include NGPC_LINK_TYPES_HEADER

/* Restore `ei 0` after each BIOS COM call (see 2 above). */
#ifndef NGPC_LINK_FORCE_EI0
#define NGPC_LINK_FORCE_EI0 1
#endif

/* ---- BIOS vector numbers ---- */
#define BIOS_COMINIT           0x10
#define BIOS_COMSENDSTART      0x11
#define BIOS_COMRECIVESTART    0x12
#define BIOS_COMCREATEDATA     0x13
#define BIOS_COMGETDATA        0x14
#define BIOS_COMONRTS          0x15
#define BIOS_COMOFFRTS         0x16
#define BIOS_COMSENDSTATUS     0x17
#define BIOS_COMRECIVESTATUS   0x18
#define BIOS_COMCREATEBUFDATA  0x19
#define BIOS_COMGETBUFDATA     0x1A

/* ---- BIOS COM state in work RAM ---- */
#define NGPC_COM_TX_RING   0x006C80u  /* 64 bytes */
#define NGPC_COM_RX_RING   0x006CC0u  /* 64 bytes */
#define NGPC_COM_RING_SIZE 64u

/* Bytes pending in each ring, maintained by the BIOS serial ISRs. */
#define NGPC_COM_TX_COUNT  (*(volatile u8 *)0x006D00u)
#define NGPC_COM_RX_COUNT  (*(volatile u8 *)0x006D01u)

/* Controller state kept by the BIOS. Used as entropy when picking roles:
 * two players do not hold the same buttons at the same time. */
#define NGPC_COM_JOYPAD    (*(volatile u8 *)0x006F82u)

/* Link connector ports. */
#define NGPC_COM_PORT_B1   (*(volatile u8 *)0x0000B1u)  /* bit 2: cable detect, 0 = peer present */
#define NGPC_COM_PORT_B2   (*(volatile u8 *)0x0000B2u)  /* bit 0: RTS, 0 = ready to receive */
#define NGPC_COM_DETECT_BIT 0x04u

/* SC0 registers, read-only, for diagnostics. */
#define NGPC_COM_SC0BUF    (*(volatile u8 *)0x000050u)
#define NGPC_COM_SC0CR     (*(volatile u8 *)0x000051u)
#define NGPC_COM_SC0MOD    (*(volatile u8 *)0x000052u)
#define NGPC_COM_BR0CR     (*(volatile u8 *)0x000053u)

/* ---- Status word bits ---- */
#define COM_COUNT_MASK    0x00FFu  /* bytes in the buffer */
#define COM_BUFOVERERROR  0x0100u
#define COM_FLAMEERROR    0x0200u  /* framing error (RX) */
#define COM_PARITYERROR   0x0400u
#define COM_OVERRUNERROR  0x0800u
#define COM_RX_ERR_MASK   (COM_FLAMEERROR | COM_PARITYERROR | COM_OVERRUNERROR)

/* ---- API ---- */

/* Set up ports, baud rate and serial interrupts. Call once. */
void ngpc_com_init(void);

/* Start transmitting whatever has been queued. */
void ngpc_com_send_start(void);

/* Allow reception (RTS low). Both consoles must call it. */
void ngpc_com_recv_start(void);

void ngpc_com_rts_on(void);

/* RTS high. Contains `ei 6` -- see 2 above; ngpc_link.c never uses it. */
void ngpc_com_rts_off(void);

/* Queue one byte. Check ngpc_com_tx_free() first. */
void ngpc_com_create_data(u8 b);

/* Queue n bytes in a single BIOS call. n <= 64. */
void ngpc_com_send_block(const u8 *p, u8 n);

/* Pop one received byte into *out. Loop on ngpc_com_rx_pending(). */
void ngpc_com_get_data(u8 *out);

void ngpc_com_get_block(u8 *p, u8 n);

/* Status word (count + error bits) into *out. */
void ngpc_com_send_status(u16 *out);
void ngpc_com_recv_status(u16 *out);

#define ngpc_com_tx_pending()  ((u8)NGPC_COM_TX_COUNT)
#define ngpc_com_rx_pending()  ((u8)NGPC_COM_RX_COUNT)
#define ngpc_com_tx_free()     ((u8)(NGPC_COM_RING_SIZE - NGPC_COM_TX_COUNT))

/* 1 when a peer is plugged in. Advisory only: commercial games gate on this
 * bit, but it has never been checked against silicon, so ngpc_link.c uses it
 * for display and never to block the exchange. */
u8 ngpc_com_cable_present(void);

#endif /* NGPC_LINK_COM_H */
