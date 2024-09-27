#include "vmlinux.h"
#include <bpf/bpf_helpers.h>
#include <bpf/bpf_tracing.h>
#include <bpf/bpf_core_read.h>
#include <bpf/bpf_endian.h>

#define _EBPF_ 1

#include "redirect.h"
#include "default_setting.h"
#include "conn_pair.h"
#include "log.h"


char LICENSE[] SEC("license") = "Dual BSD/GPL";


int listen_port = 8000;
int tapindex = 0;
int print_level = PRINT_DEBUG;
#define NAT_MAX_ENTRIES 2*DEFAULT_MAX_FORWARD_CONNECTIONS

/* 
* This map is used for tracking the paired connection. 
* The original connection and connection modified by dnat/snat are paired as a couple, 
* which is often called as connection track tuple.
* The value stores the destin information for doing dnat/snat matching.
*/
struct {
	__uint(type, BPF_MAP_TYPE_HASH);
	__uint(max_entries, NAT_MAX_ENTRIES);
	__type(key, struct conn_key);
	__type(value, struct conn);
} map_nat_dest SEC(".maps");

struct {
	__uint(type, BPF_MAP_TYPE_RINGBUF);
	__uint(max_entries, 256 * 1024);
} log_ringbuff SEC(".maps");

static inline void modify_header(struct __sk_buff *skb, struct conn *dest)
{
	
}

static inline void process_header_tcp( struct conn *conn, struct tcphdr* tcph)
{
	
	conn->remote_port = tcph->source;
	conn->local_port = tcph->dest;
	conn->remote_seq = tcph->seq;
	conn->local_seq = tcph->ack_seq;  // ?
}

static inline void process_header_udp( struct conn *conn, struct udphdr* udph)
{
	conn->remote_port = udph->source;
	conn->local_port = udph->dest;
}

static inline void process_header_ipv6( struct conn *conn, struct ipv6hdr* iph)
{
	conn->proto = iph->nexthdr;
	//todo
}

static inline void process_header_ipv4( struct conn *conn, struct iphdr* iph)
{
	conn->proto = iph->protocol;
	conn->remote_ip = iph->saddr;
	conn->local_ip = iph->daddr;
}

SEC("tc")
int tc_ingress(struct __sk_buff *ctx)
{
	void *data_end = (void *)(__u64)ctx->data_end;
	void *data = (void *)(__u64)ctx->data;
	struct ethhdr *l2;
	struct iphdr *iph;
	struct ipv6hdr *ip6h;
	struct conn conn, *dest;
	struct tcphdr *tcph;
	struct event *ev;

	if (ctx->protocol != bpf_htons(ETH_P_IP))
		return TC_ACT_OK;
	l2 = data;
	if ((void *)(l2 + 1) > data_end)
		return TC_ACT_OK;

	iph = (struct iphdr *)(l2 + 1);
	if ((void *)(iph+1) > data_end)
		return TC_ACT_OK;


	if( iph->version == 4 ){
		process_header_ipv4( &conn, iph );
		print_debug("IPv4 packet: %lx -> %lx", conn.remote_ip, conn.local_ip);
		if( conn.proto == IPPROTO_TCP ) {
			tcph = (void *)iph + iph->ihl*4;
			if ( (void *)(tcph  + 1) > data_end )
				return TC_ACT_OK;

			process_header_tcp(&conn, tcph);
			ev = bpf_ringbuf_reserve(&log_ringbuff, sizeof(*ev), 0);
			if( ev ){
				ev->conn = conn;
				bpf_ringbuf_submit(ev, 0);
			}
		}
		else if ( conn.proto == IPPROTO_UDP ){
			//todo 
			return TC_ACT_OK;
			process_header_udp( &conn, (void *)iph+ iph->ihl*5);
		}else {
			return TC_ACT_OK;
		}
	}
	else if( iph->version == 6 ){
		//todo
		return TC_ACT_OK;
		process_header_ipv6( &conn, ip6h );
		if( conn.proto == IPPROTO_TCP ) {
			process_header_tcp(&conn, (void *)iph+ 40);
		}
		else if ( conn.proto == IPPROTO_UDP ){
			//todo
			return TC_ACT_OK;
			process_header_udp( &conn, (void *)iph+ 40);
		}else {
			//todo.
			return TC_ACT_OK;
		}
	}
	

	dest = bpf_map_lookup_elem( &map_nat_dest, &conn.key);
	if( dest ) {
		print_debug("to dest %lx %d", dest->remote_ip, dest->remote_port);
		modify_header(ctx, dest);
		return TC_ACT_OK;
	}

	if( conn.remote_port == listen_port ) {
		// send packet to local tap device, where the funcion is implemented by user space
		return bpf_redirect(tapindex, BPF_F_INGRESS);
	}

	// destination port is not listening port, pass it to local kernel protocol
	return TC_ACT_OK;

}



