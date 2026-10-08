/* One instance of the module. Two translation units => two independent copies
   of its file-scope statics, which is what makes two "consoles" in one process. */
#define ngpc_link_state      A_link_state
#define ngpc_link_host       A_link_host
#define ngpc_link_fresh      A_link_fresh
#define ngpc_link_peer_seq   A_link_peer_seq
#define ngpc_link_out        A_link_out
#define ngpc_link_in         A_link_in
#define ngpc_link_stats      A_link_stats
#define ngpc_link_init       A_link_init
#define ngpc_link_update     A_link_update
#define ngpc_link_close      A_link_close
#define ngpc_link_resync     A_link_resync
#define ngpc_link_set_role   A_link_set_role
#define ngpc_link_recv       A_link_recv
#define ngpc_link_rx_waiting A_link_rx_waiting
#include "ngpc_link.c"
