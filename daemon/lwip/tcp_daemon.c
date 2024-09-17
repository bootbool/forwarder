/**
 * Tcp raw server daemon listenning on a tap device  running on linux
 *
 */

#include "lwip/opt.h"
#include "lwip/debug.h"
#include "lwip/stats.h"
#include "lwip/tcp.h"
#include "lwip/priv/tcp_priv.h"
#include "lwip/etharp.h"
#include "lwip/timeouts.h"
#include "lwip/init.h"

#include "log.h"
#include "conn_pair.h"
#include "setting.h"

#include <string.h>

#if LWIP_TCP && LWIP_CALLBACK_API

static struct tcp_pcb *tcp_default_daemon;

static inline void _conn_pair_session_destroy( struct conn_pair_session *ss, int reset )
{
    struct tcp_pcb *pcb;

    if( ss->pcb_from ) {
        pcb = ss->pcb_from;
        ss->pcb_from = NULL;
        tcp_arg(pcb, NULL);
        tcp_sent(pcb, NULL);
        tcp_recv(pcb, NULL);
        tcp_err(pcb, NULL);
        tcp_poll(pcb, NULL, 0);
        tcp_abandon(pcb, reset);
    }
    if( ss->pcb_to ) {
        pcb = ss->pcb_to;
        ss->pcb_to = NULL;
        tcp_arg(pcb, NULL);
        tcp_sent(pcb, NULL);
        tcp_recv(pcb, NULL);
        tcp_err(pcb, NULL);
        tcp_poll(pcb, NULL, 0);
        tcp_abandon(pcb, reset);
    }
    if( --ss->used < 1 ) 
        mem_free(ss);
}


static void
conn_pair_session_close(struct conn_pair_session *ss)
{
    _conn_pair_session_destroy(ss, 0);
}

static void
conn_pair_session_reset(struct conn_pair_session *ss)
{
     _conn_pair_session_destroy(ss, 1);

}

int deploy_conn_session( struct conn_pair_session *ss )
{
    ss->state = PAIR_PAIRED;
    conn *client, *server;
    client = &ss->pair.stage1;
    server = &ss->pair.stage2;
    print_debug("-----paired-----");
    print_debug("%x:%d->%x:%d(remote_seq %d local_seq %d)  %x:%d->%x:%d(remoteseq %d localseq %d)", 
        client->remote_ip, client->remote_port, client->local_ip, client->local_port, client->remote_seq, client->local_seq, 
        server->remote_ip, server->remote_port, server->local_ip, server->local_port, server->remote_seq, server->local_seq);
    
    conn_pair_session_close(ss);
}

static void conn_pair_err(void *arg, err_t err)
{
    struct conn_pair_session *ss;
    LWIP_UNUSED_ARG(err);
    ss = (struct conn_pair_session *)arg;
    if( ss ) {
        conn_pair_session_reset(ss);
    }
}

static err_t conn_pair_stage_2_finish(void *arg, struct tcp_pcb *tpcb, err_t err)
{
    struct conn_pair_session * session = (struct conn_pair_session *)arg;
    session->pcb_to = tpcb;
    session->used = 2;
    session->pair.stage2.proto = IP_PROTO_TCP;
    session->pair.stage2.remote_seq = tpcb->rcv_nxt;
    session ->pair.stage2.local_seq = tpcb->snd_nxt;
    session->pair.stage2.remote_ip = tpcb->remote_ip.addr;
    session->pair.stage2.remote_port = tpcb->remote_port;
    session->pair.stage2. local_ip = tpcb->local_ip.addr;
    session->pair.stage2.local_port = tpcb->local_port;
    session->state = PAIR_CONNECTED_PEER;
    tpcb->rcv_wnd = 0;
    print_debug("Conn pair finished");
    deploy_conn_session(session);
}

static err_t
conn_pair_stage_2_omit_msg(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err) 
{
    struct conn_pair_session * session = (struct conn_pair_session *)arg;
    print_info("Should not receive message at stage2...");
    tpcb->rcv_nxt = session->pair.stage2.remote_seq;
    tpcb->rcv_wnd = 0;
    pbuf_free(p);
    return ERR_OK;
}


int netif_num = 0;

struct tapif {
  /* Add whatever per-interface state that is needed here. */
  int fd;
};
static err_t low_level_output(struct netif *netif, struct pbuf *p)
{
    struct tapif *tapif = (struct tapif *)netif->state;
    char buf[1518]; /* max packet size including VLAN excluding CRC */
    ssize_t written;

    if (p->tot_len > sizeof(buf)) {
        perror("tapif: packet too large");
        return ERR_IF;
    }

    /* initiate transfer(); */
    pbuf_copy_partial(p, buf, p->tot_len, 0);

    /* signal that packet should be sent(); */
    print_debug("Netif%s write>> len=%d  payload=%p",netif->name, p->tot_len, p);
    written = write(tapif->fd, buf, p->tot_len);
    if (written < p->tot_len) {
        perror("tapif: write");
        return ERR_IF;
    } else {
        return ERR_OK;
    }
}


static err_t _tapif_init(struct netif *netif)
{
    netif->state = netif_default->state;
    sprintf(netif->name, "%d", netif_num);
    #if LWIP_IPV4
    netif->output = etharp_output;
    #endif /* LWIP_IPV4 */
    #if LWIP_IPV6
      netif->output_ip6 = ethip6_output;
    #endif /* LWIP_IPV6 */
    netif->linkoutput = low_level_output;
    netif->mtu = 1500;
    netif->hwaddr[0] = 0x02;
    netif->hwaddr[1] = 0x12;
    netif->hwaddr[2] = 0x34;
    netif->hwaddr[3] = 0x56;
    netif->hwaddr[4] = 0x78;
    netif->hwaddr[5] = 0xab;
    netif->hwaddr_len = 6;
    netif->flags = NETIF_FLAG_BROADCAST | NETIF_FLAG_ETHARP | NETIF_FLAG_IGMP;
    netif_set_link_up(netif);
    netif_set_up(netif);
    return ERR_OK;
}

void tcp_bind_add_netif( ip4_addr_t ipaddr)
{
    ip4_addr_t netmask, gw;
    IP4_ADDR(&netmask, 255, 255, 255, 255);
    gw.addr = TAP_DEFAULT_IP;
    struct netif *netif = mem_malloc(sizeof(struct netif));
    netif_num++;
    netif_add(netif, &ipaddr, &netmask, &gw, NULL, _tapif_init, netif_input);
}

u16 select_port()
{
    //todo, current send any port
    return 0;
}

/* issue a new connection to the target server */
struct tcp_pcb* conn_pair_stage_2(struct conn_pair_session *session)
{
    struct tcp_pcb *tcp_client_pcb;
    ip4_addr_t ip;
    u16 port;
    
    if( session->nat_type == DNAT ){
        ip = session->pcb_from->remote_ip;
        port = session->pcb_from->remote_port;
        tcp_bind_add_netif(ip);
    }
    else if( session->nat_type == SNAT ){
        ip.addr = 0;
        port = select_port();
    }
    else {
        print_info("NAT type? %d ", session->nat_type);
        return NULL;
    }
    tcp_client_pcb = tcp_new();
    if( !tcp_client_pcb ) {
        print_err("Fail to allocate client tcp_pcb");
        return NULL;
    }
    
    tcp_arg(tcp_client_pcb, session);
    tcp_bind(tcp_client_pcb, &ip, port);
    ip.addr = session->pair.stage2.remote_ip;
    tcp_connect(tcp_client_pcb, &ip, 
        session->pair.stage2.remote_port, conn_pair_stage_2_finish);
    tcp_recv(tcp_client_pcb, conn_pair_stage_2_omit_msg);
    tcp_err(tcp_client_pcb, conn_pair_err);

    return tcp_client_pcb;
}

unsigned int ip_string_to_int(const char *ip_str) {
    unsigned int ip = 0;
    unsigned int ip1 = 0;
    unsigned int octet;
    char *next;

    for (int i = 0; i < 4; ++i) {
        octet = strtoul(ip_str, &next, 10);

        if (octet > 255) {
            return 0;
        }
        ip = (ip << 8) | octet;
        if (i < 3 && *next != '.') {
            return 0;
        }
        ip_str = next + 1;
    }

    *(((char *)&ip1)+3) = *((char *)&ip);
    *(((char *)&ip1)+2) = *(((char *)&ip)+1);
    *(((char *)&ip1)+1) = *(((char *)&ip)+2);
    *(((char *)&ip1)) = *(((char *)&ip)+3);
    return ip1;
}


static void dump_tcp_msg (struct tcp_pcb *tpcb, struct pbuf *p) 
{
    print_debug("dump tcp %x:%x -> %x:%x pcb %p buf %p ", 
        tpcb->remote_ip.addr, tpcb->remote_port, tpcb->local_ip.addr, tpcb->local_port, tpcb, p);
    if( p ) {
        char *charp = p->payload;
        for( int i = 0; i<p->len; i++) {
          print_debug_raw("%c", charp[i]);
        }
        print_debug_raw("\n");
    }
}

/*NOTE: the route message format shold be: ip port.
* For example 192.168.1.2 999. 
* The string is composed by two human readable chars with space in the middle.
*/
int parse_route_msg(struct conn_pair_session *ss, char *payload)
{
    ip_addr_t ipaddr;
    u16_t port;
    char *s1, *s2;
    char *nat_type;
    nat_type = strtok(payload, " ");
    s1 = strtok(NULL, " ");
    s2 = strtok(NULL, " ");
    if( !( nat_type  && s1 && s2  ) ) {
        print_info("Wrong route msg:%s %s %s ",nat_type, s1, s2);
        return ERRCODE_FORMAT;
    }
    ss->nat_type = atoi(nat_type);
    ss->pair.stage2.remote_ip = ip_string_to_int(s1);
    ss->pair.stage2.remote_port = atoi(s2);
    if( ss->nat_type 
        && ss->pair.stage2.remote_ip
        && ss->pair.stage2.remote_port ) 
    {
        return ERRCODE_OK;
    }
    else {
        return ERRCODE_VALUE;
    }
}

static err_t conn_pair_stage_1(void *arg, struct tcp_pcb *tpcb, struct pbuf *p, err_t err)
{
    struct conn_pair_session *session;
    err_t ret_err;
    print_debug("-----Stage 1-----");
    dump_tcp_msg(tpcb, p);
    LWIP_ASSERT("arg != NULL",arg != NULL);
    session = (struct conn_pair_session *)arg;
        
    /* remote host closed connection */
    if (p == NULL) {
        session->state = PAIR_NONE;
        print_debug("Connection disrupted");
        conn_pair_session_close(session);
        ret_err = ERR_OK;
        return ret_err;
    } 
    /* cleanup, for unknown reason */
    if(err != ERR_OK) {
        print_debug("Receive err %d", err);
        if (p != NULL) {
          pbuf_free(p);
        }
        conn_pair_session_close(session);
        ret_err = err;
        return ret_err;
    }
    /* first packet should contain route info */
    if(session->state == PAIR_ACCEPTED) {
        /* first data chunk in p->payload */
        session->state = PAIR_RECEIVED_ROUTE;

        if( p->tot_len < ROUTE_MSG_LENGTH ) {
            print_debug("Wrong route len");
            pbuf_free(p);
            conn_pair_session_reset(session);
            return ERR_OK;
        }
        session->pair.stage1.remote_seq = tpcb->rcv_nxt;
        session ->pair.stage1.local_seq = tpcb->snd_nxt;
        tpcb->rcv_wnd = 0;

        ret_err = parse_route_msg(session, p->payload);
        if( ret_err != ERRCODE_OK ) {
            print_info("Wrong route msg: %s ", (char *)p->payload);
            pbuf_free(p);
            conn_pair_session_reset(session);
            return ERR_OK;
        }
        struct tcp_pcb* c_pcb = conn_pair_stage_2( session);
        print_debug("Try to connect %s", (char *)(p->payload));
        if( !c_pcb ) {
            print_info("Connect fail, Wrong route msg format: %s ", (char *)(p->payload));
            pbuf_free(p);
            conn_pair_session_reset(session);
            return ERR_OK;
        }
        return ERR_OK;
    } else if (session->state >= PAIR_RECEIVED_ROUTE) {
        print_err("Should not receive packets after parsing route message");
        tpcb->rcv_nxt = session->pair.stage1.remote_seq;
        tpcb->rcv_wnd = 0;
        pbuf_free(p);
        return ERR_OK;
    } 
    print_err("confusing path here\n");
    pbuf_free(p);
    return ret_err;
}

static err_t tcp_accept_cb(void *arg, struct tcp_pcb *newpcb, err_t err)
{
    err_t ret_err;
    struct conn_pair_session *session;

    LWIP_UNUSED_ARG(arg);
    if ((err != ERR_OK) || (newpcb == NULL)) {
        return ERR_VAL;
    }
    print_debug("-----Accept new-----");
    /* Unless this pcb should have NORMAL priority, set its priority now.
        When running out of pcbs, low priority pcbs can be aborted to create
        new pcbs of higher priority. */
    tcp_setprio(newpcb, TCP_PRIO_MIN);

    session = (struct conn_pair_session *)mem_malloc(sizeof(struct conn_pair_session));
    if( ! session ) {
        print_err("Fail to malloc\n");
        return ERR_MEM;
    }
    memset(session, 0, sizeof(struct conn_pair_session));
    session->state = PAIR_ACCEPTED;
    session->used = 1;
    session->pcb_from = newpcb;
    session->pair.stage1.proto = IP_PROTO_TCP;
    session->pair.stage1.remote_seq = newpcb->rcv_nxt;
    session ->pair.stage1.local_seq = newpcb->snd_nxt;
    session->pair.stage1.remote_ip = newpcb->remote_ip.addr;
    session->pair.stage1.remote_port = newpcb->remote_port;
    session->pair.stage1. local_ip = newpcb->local_ip.addr;
    session->pair.stage1.local_port = newpcb->local_port;
    newpcb->rcv_wnd = ROUTE_MSG_LENGTH;
    /* pass newly allocated es to our callbacks */
    tcp_arg(newpcb, session);
    tcp_recv(newpcb, conn_pair_stage_1);
    tcp_err(newpcb, conn_pair_err);
    return ERR_OK;
}

struct tcp_pcb* new_tcp_daemon_ipv4(u16_t portnumber)
{
    struct tcp_pcb *new_tcp_connection;
    struct tcp_pcb *_tcp_daemon = NULL;
    err_t err;
    _tcp_daemon = tcp_new_ip_type(IPADDR_TYPE_ANY);
    if( _tcp_daemon == NULL ) {
        print_err("Fail to create tcp daemon\n");
        abort();
        return NULL;
    }

    err = tcp_bind(_tcp_daemon, IP_ANY_TYPE, portnumber);
    if (err == ERR_OK) {
        new_tcp_connection = tcp_listen(_tcp_daemon);
        tcp_accept(new_tcp_connection, tcp_accept_cb);
    } else {
        print_err("Bind error %d\n", err);
        abort();
        return NULL;
    }
    return _tcp_daemon;
}

void run_tcp_daemon_ipv4(u16 port)
{
    ip4_addr_t ipaddr, netmask, gw;
    struct tcp_pcb *daemon = NULL;

    ipaddr.addr = lwip_htonl( TAP_DEFAULT_IP_USER );
    netmask.addr = lwip_htonl( TAP_DEFAULT_MASK );
    gw.addr = lwip_htonl( TAP_DEFAULT_IP );
 
    print_debug("Try to start new tcp service on %x:%x", TAP_DEFAULT_IP_USER, port);

    lwip_init();
    struct netif *_netif = init_tap_netif(&ipaddr, &netmask, &gw);
    daemon = new_tcp_daemon_ipv4(port);
    if( !daemon ){
        abort();
    }
    tcp_default_daemon = daemon;
    print_debug("Running main loop...");

    while (1) {
      sys_check_timeouts();

      tap_netif_poll(_netif);
    }
}



#endif /* LWIP_TCP && LWIP_CALLBACK_API */
