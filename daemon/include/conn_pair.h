#ifndef _H_CONN_PAIR_H_
#define _H_CONN_PAIR_H_

#include "types.h"
#include "global_conn_pair.h"

struct conn_pair_session
{
  u8 state;
  u8 used;
  u8 nat_type;
  struct tcp_pcb *pcb_from;
  struct tcp_pcb *pcb_to;
  /* staging for incoming route message*/
  struct pbuf *pkt;
  conn_pair pair;
  void *extend[0];
};

extern void run_tcp_daemon_ipv4(u16 port);
extern void tap_netif_poll(struct netif * netif);
extern struct netif * init_tap_netif(const ip4_addr_t *ipaddr, const ip4_addr_t *netmask, const ip4_addr_t *gw);

#endif