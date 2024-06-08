#include <linux/skbuff.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/fdtable.h>
#include <linux/timer.h>
#include <linux/netfilter_ipv4/ip_tables.h>
#include <net/netfilter/nf_conntrack.h>
#include <net/ip.h>
#include <net/tcp.h>
#include <net/net_namespace.h>
#include <net/checksum.h>
#include "log.h"
#include "common.h"
#include "forward.h"


void print_skb( struct sk_buff *skb )
{
    struct iphdr *iph;
    struct tcphdr *tcph;

    iph = ip_hdr(skb);
    tcph = tcp_hdr(skb);
    logs("skb_buff %x:%d - %x:%d seq %u ack %u",  
        ntohl(iph->saddr), ntohs(tcph->source), ntohl(iph->daddr), ntohs(tcph->dest),
        ntohl(tcph->seq), ntohl(tcph->ack_seq));
}

static void modify_tcp_skb(struct sk_buff *skb, conn *link, conn *pairlink)
{
    struct iphdr *ip_header;
    struct tcphdr *tcp_header;
    u32 seq;

    ip_header = ip_hdr(skb);
    tcp_header = tcp_hdr(skb);

    print_skb(skb);
    inet_proto_csum_replace4(&tcp_header->check, skb, ip_header->daddr, pairlink->remote_ip, true);
    csum_replace4(&ip_header->check, ip_header->daddr , pairlink->remote_ip);
    ip_header->daddr = pairlink->remote_ip;

    inet_proto_csum_replace4(&tcp_header->check, skb, ip_header->saddr, pairlink->local_ip, true);
    csum_replace4(&ip_header->check, ip_header->saddr , pairlink->local_ip);
    ip_header->saddr = pairlink->local_ip;

    inet_proto_csum_replace2(&tcp_header->check, skb, tcp_header->dest, pairlink->remote_port, false);
    tcp_header->dest = pairlink->remote_port;
    
    inet_proto_csum_replace2(&tcp_header->check, skb, tcp_header->source, pairlink->local_port, false);
    tcp_header->source = pairlink->local_port;
    
    seq = ntohl(tcp_header->seq) + link->seq_offset;
    seq = htonl(seq);
    inet_proto_csum_replace4(&tcp_header->check, skb, tcp_header->seq, seq, false);
    tcp_header->seq = seq;

    seq = ntohl(tcp_header->ack_seq) + link->ack_offset;
    seq = htonl(seq);
    inet_proto_csum_replace4(&tcp_header->check, skb, tcp_header->ack_seq, seq, false);
    tcp_header->ack_seq = seq;
    print_skb(skb);
}

int check_tcp_state(struct sk_buff *skb, conn *link, conn *pairlink)
{
    struct iphdr *iph;
    struct tcphdr *tcph;
    conn_pair *cp = get_conn_pair(link);

    iph = ip_hdr(skb);
    tcph = tcp_hdr(skb);
    if( unlikely( tcph->fin ) ) {
        if( cp->state == TCP_ESTABLISHED) {
            link->state = TCP_FIN_WAIT1;
            cp->state = TCP_FIN_WAIT1;
        }else {
            if( pairlink->state == TCP_FIN_WAIT1 ){
                link->state = TCP_LAST_ACK;
                cp->state = TCP_LAST_ACK;
            }
        }        
    }
    return cp->state;
}

static void modify_udp_skb(struct sk_buff *skb, conn *link, conn *pairlink)
{
    struct iphdr *ip_header;
    struct udphdr *udp_header;

    ip_header = ip_hdr(skb);
    udp_header = udp_hdr(skb);

    inet_proto_csum_replace4(&udp_header->check, skb, ip_header->daddr, pairlink->remote_ip, true);
    csum_replace4(&ip_header->check, ip_header->daddr , pairlink->remote_ip);
    ip_header->daddr = pairlink->remote_ip;

    inet_proto_csum_replace4(&udp_header->check, skb, ip_header->saddr, pairlink->local_ip, true);
    csum_replace4(&ip_header->check, ip_header->saddr , pairlink->local_ip);
    ip_header->saddr = pairlink->local_ip;

    inet_proto_csum_replace2(&udp_header->check, skb, udp_header->dest, pairlink->remote_port, false);
    udp_header->dest = pairlink->remote_port;
    
    inet_proto_csum_replace2(&udp_header->check, skb, udp_header->source, pairlink->local_port, false);
    udp_header->source = pairlink->local_port;

}


static unsigned int
nf_forwarder(void *priv, struct sk_buff *skb, const struct nf_hook_state *state)
{
    struct iphdr *ip_header;
    struct tcphdr *tcp_header;
    struct udphdr *udp_header;
    conn link;
    conn *hashlink;
    conn *pairlink;
    conn_pair *cp;
    int ret;

    ip_header = ip_hdr(skb);
    link.local_ip = ip_header->daddr;
    link.remote_ip = ip_header->saddr;
    link.proto = ip_header->protocol;
    if (ip_header->protocol == IPPROTO_TCP) {
        tcp_header = tcp_hdr(skb);
        link.local_port = tcp_header->dest;
        link.remote_port = tcp_header->source;
        link.hash = hash_conn( &link );
        hashlink = find_hash_conn(&link);
        logs("Try to find hash %d %px %x:%d - %x:%d seq %u ack %u", 
            link.hash, hashlink, ntohl(link.remote_ip), ntohs(link.remote_port), 
            ntohl(link.local_ip), ntohs(link.local_port), ntohl(tcp_header->seq), ntohl(tcp_header->ack_seq));

        if( ! hashlink ) {
            logs("No hash link, submmit to kernel protocal ....");
            return NF_ACCEPT;
        }
        cp = get_conn_pair(hashlink);
        pairlink = opposite_conn(hashlink);
        ret = check_tcp_state(skb, hashlink, pairlink);
        modify_tcp_skb(skb, hashlink, pairlink);
        if( unlikely( ret == TCP_LAST_ACK) ) {
            cp->timer.expires = jiffies + (HZ*5);
            add_timer(&cp->timer);
        }
    }
    else if (ip_header->protocol == IPPROTO_UDP) {
        udp_header = udp_hdr(skb);
        link.local_port = udp_header->dest;
        link.remote_port = udp_header->source;
        link.hash = hash_conn( &link );
        hashlink = find_hash_conn(&link);
        if( ! hashlink ) {
            return NF_ACCEPT;
        }
        pairlink = opposite_conn(hashlink);
        modify_udp_skb(skb, hashlink, pairlink);

    }else{
        return NF_ACCEPT;
    }

    skb->_nfct = IP_CT_UNTRACKED;
    return NF_ACCEPT;
}

static const struct nf_hook_ops forwarder_ops = {
	.hook = nf_forwarder,
	.pf = NFPROTO_IPV4,
	.hooknum = NF_INET_PRE_ROUTING,
	.priority = NF_IP_PRI_CONNTRACK - 1,
};

void nf_init( void  )
{
    nf_register_net_hook(&init_net, &forwarder_ops);
}

void nf_exit ( void )
{
    nf_unregister_net_hook(&init_net, &forwarder_ops);
}
