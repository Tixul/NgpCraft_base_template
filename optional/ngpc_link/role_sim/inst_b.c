/* One instance of the module. Two translation units => two independent copies
   of its file-scope statics, which is what makes two "consoles" in one process. */
#define ngpc_link_state      B_link_state
#define ngpc_link_host       B_link_host
#define ngpc_link_fresh      B_link_fresh
#define ngpc_link_peer_seq   B_link_peer_seq
#define ngpc_link_out        B_link_out
#define ngpc_link_in         B_link_in
#define ngpc_link_stats      B_link_stats
#define ngpc_link_init       B_link_init
#define ngpc_link_update     B_link_update
#define ngpc_link_close      B_link_close
#define ngpc_link_resync     B_link_resync
#define ngpc_link_set_role   B_link_set_role
#define ngpc_link_recv       B_link_recv
#define ngpc_link_rx_waiting B_link_rx_waiting
#include "ngpc_link.c"
