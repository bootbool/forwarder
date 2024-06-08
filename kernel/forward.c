#include <linux/inet.h>
#include <linux/jhash.h>
#include <linux/list.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/sched.h>
#include <linux/sched/mm.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/file.h>
#include <linux/compiler.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/atomic.h>
#include <linux/fdtable.h>
#include <linux/profile.h>

#include <net/tcp.h>

#include "common.h"
#include "log.h"



static int listen_port = 0;
struct hlist_head *conn_pair_list = NULL;
unsigned int conn_pair_list_order = 16;
static int conn_pair_list_mask = 0;
static struct task_struct *forwarder_daemon = NULL;



void set_port_h(int port)
{
    listen_port = htons(port);
}

int get_port_n( void )
{
    return listen_port;
}

static int destroy_sockfd(int fd)
{
    int ret;
    struct socket *link;
    struct file *file;
    link = sockfd_lookup(fd, &ret);
	if (!link){
        logs("Fail to get socket %d", fd);
		return 1;
    }
    tcp_set_state(link->sk, TCP_CLOSE);
    file = fget(fd);
    fput(file);
    __close_fd(current->files, fd);

    return 0;
}


u32 hash_conn( conn *link) 
{
    u32 res;
    res = jhash2( &link->local_ip, 3, 0);
    return res & conn_pair_list_mask;
}

void destroy_conn_pair( conn_pair *cp );

static void timer_destroy_conn_pair(struct timer_list *t) 
{
    conn_pair *cp;
    cp = container_of(t, conn_pair, timer);
    destroy_conn_pair(cp);
}

int fill_conn_pair(conn_pair *pair, struct sock *client, struct sock *server)
{
    struct tcp_sock *tpc = tcp_sk(client);
    struct tcp_sock *tps = tcp_sk(server);
    pair->task = current;
    if( (client->sk_protocol == IPPROTO_TCP) && (server->sk_protocol==IPPROTO_TCP) ){
        if( (client->sk_state != TCP_ESTABLISHED ) || (server->sk_state != TCP_ESTABLISHED)){
            logs("sock state is not TCP_ESTABLISHED %d %d", client->sk_state, server->sk_state);
            return 1;
        }
        pair->state = TCP_ESTABLISHED;
        pair->client.state = TCP_ESTABLISHED;
        pair->client.local_seq = tpc->snd_nxt;
        pair->client.remote_seq = tpc->rcv_nxt;
        pair->server.state = TCP_ESTABLISHED;
        pair->server.local_seq = tps->snd_nxt;
        pair->server.remote_seq = tps->rcv_nxt;        
        pair->client.seq_offset = pair->server.local_seq - pair->client.remote_seq;
        pair->client.ack_offset = pair->server.remote_seq - pair->client.local_seq;


        pair->server.seq_offset = pair->client.local_seq - pair->server.remote_seq;
        pair->server.ack_offset = pair->client.remote_seq - pair->server.local_seq;

    }else if( (client->sk_protocol == IPPROTO_UDP) && (server->sk_protocol==IPPROTO_UDP) ){
            // pass
    }else{
        logs("Only support tcp/udp, current is %d %d" , client->sk_protocol, server->sk_protocol);
        return 1;
    }
    pair->proto = client->sk_protocol;
    pair->client.proto = client->sk_protocol;
    pair->client.local_ip = client->sk_rcv_saddr;
    pair->client.remote_ip = client->sk_daddr;
    pair->client.local_port = htons(client->sk_num);
    pair->client.remote_port = client->sk_dport;
    pair->client.direction = IS_CLIENT;
    pair->client.hash = hash_conn(&pair->client);


    pair->server.proto = server->sk_protocol;
    pair->server.local_ip = server->sk_rcv_saddr;
    pair->server.remote_ip = server->sk_daddr;
    pair->server.local_port = htons(server->sk_num);
    pair->server.remote_port = server->sk_dport;
    pair->server.direction = IS_SERVER;
    pair->server.hash = hash_conn(&pair->server);
    timer_setup(&pair->timer, timer_destroy_conn_pair, 0);

    return 0;
}

void add_to_hashlist(conn * link) 
{
    logs("hash add to list %d %px %x:%x-%x:%x", 
        link->hash, link, link->local_ip, link->local_port, link->remote_ip, link->remote_port);
    hlist_add_head_rcu(&link->node, &conn_pair_list[link->hash]);
}

conn_pair* build_forward_relation(struct sock *client, struct sock *server)
{

    int ret;
    conn_pair *pair;
    pair = kmalloc(sizeof(conn_pair), GFP_KERNEL);
    if( !pair ){
        logs("Fail to malloc conn_pair");
        return NULL;
    }
    ret = fill_conn_pair(pair, client, server);
    if( ret != 0 ){
        kfree(pair);
        logs("Fail to fill conn_pair");
        return NULL;
    }
    add_to_hashlist(&pair->client);
    add_to_hashlist(&pair->server);
    atomic_set(&pair->refcnt, 1);
    return pair;
}

//forward data from value[0] to value[1]
int add_forward_fd(u32 value[2]) 
{
    int ret;
    conn_pair *retp;
    struct socket *client, *server;
    client = sockfd_lookup(value[0], &ret);
	if (!client){
        logs("Fail to get client socket");
		return ret;
    }
    server = sockfd_lookup(value[1], &ret);
	if (!server){
        logs("Fail to get server socket");
		return ret;
    }
    if( !(client->sk && server->sk) ){
        logs("Not sock %px %px", client->sk, server->sk);
        return 1;
    }
    retp = build_forward_relation(client->sk, server->sk);
    if( !retp ){
        return 1;
    }
    retp->fd[0] = value[0];
    retp->fd[1] = value[1];
    retp->task = current;
    tcp_set_state(client->sk, TCP_CLOSE);
    tcp_set_state(server->sk, TCP_CLOSE);
    forwarder_daemon = current;
    return 0;
}

conn* find_hash_conn( conn *link) 
{
    conn *ret;
    rcu_read_lock();
    hlist_for_each_entry_rcu( ret, &conn_pair_list[link->hash], node) {
        if( memcmp(&ret->local_ip, &link->local_ip, 12) == 0 ){
            if( ret->proto == link->proto) {
                rcu_read_unlock();
                return ret;
            }
        }
    }
    rcu_read_unlock();
    return NULL;
}

int string_all_conn_pair( char *str, int len)
{
    int ret;
    int total = 0;
    int bucket_num = (1 << conn_pair_list_order);
    conn *item;
    int i;
    len -= 64;   // reserve space for preventing out of bound
    for(i=0; i<bucket_num; i++) {
        rcu_read_lock();
        hlist_for_each_entry_rcu( item, &conn_pair_list[i], node) {
            if( total > len ){
                logs("str lengh is not enough");
                rcu_read_unlock();
                return total;;                
            }
            ret = sprintf(str, "%x:%d %x:%d", 
                item->remote_ip, item->remote_port, item->local_ip, item->local_port); /*!! maybe out of bound*/
            if( ret > 0 ) {
                total += ret;
                str += ret;
            }else{
                logs("sprintf fails");
                break;
            }
        }
        rcu_read_unlock();
    }
    return total;
}


void forward_flush(void)
{
    int i;
    conn *list;
    int bucket_num = 1 << conn_pair_list_order;
    struct hlist_node *tmp;
    for(i=0; i< bucket_num; i++) {
        hlist_for_each_entry_safe( list, tmp, &conn_pair_list[i], node) {
            conn_pair *cp = get_conn_pair(list);
            del_timer(&cp->timer);
            destroy_conn_pair(cp);
        }
    }
}

static void destroy_conn_pair_rcu_callback( struct rcu_head *head )
{
    conn_pair *cp = container_of( head,  conn_pair, rcu);
    kfree(cp);
}

void destroy_conn_pair( conn_pair *cp )
{
    if( atomic_dec_return(&cp->refcnt) != 0 ) {
        return;
    }
    hlist_del_rcu(&cp->client.node);
    hlist_del_rcu(&cp->server.node);
    __close_fd(cp->task->files, cp->fd[0]);
    __close_fd(cp->task->files, cp->fd[1]);

    logs("Success delete conn %x:%d->%x:%d", 
        ntohl(cp->client.remote_ip), ntohs(cp->client.remote_port),ntohl(cp->client.local_ip), ntohs(cp->client.local_ip));
    logs("Success delete conn %x:%d->%x:%d", 
        ntohl(cp->server.remote_ip), ntohs(cp->server.remote_port),ntohl(cp->server.local_ip), ntohs(cp->server.local_ip));
    call_rcu( &cp->rcu, destroy_conn_pair_rcu_callback);
}



static int forwarder_daemon_exit_handler(struct notifier_block *nb, unsigned long action, void *data)
{
    int i;
    conn *list;
    int bucket_num ;
    struct hlist_node *tmp;
    if( forwarder_daemon != data ) {
        return NOTIFY_OK;
    }
    bucket_num = 1 << conn_pair_list_order;
    logs("User forwarder daemon exited!");
    for(i=0; i< bucket_num; i++) {
        hlist_for_each_entry_safe( list, tmp, &conn_pair_list[i], node) {
            conn_pair *cp = get_conn_pair(list);
            del_timer(&cp->timer);
            destroy_conn_pair(cp);
        }
    }
    return NOTIFY_OK;
}

static struct notifier_block task_exit = {
    .notifier_call = forwarder_daemon_exit_handler,
};

void forward_init(void)
{
    int i;
    int bucket_num = 1 << conn_pair_list_order;
    conn_pair_list_mask = bucket_num -1;
    conn_pair_list = kmalloc(bucket_num * sizeof(*conn_pair_list), GFP_KERNEL);
    if( !conn_pair_list ){
        logs("Fails to kmalloc conn_pair_list");
        return;
    }
    for (i = 0; i < bucket_num; i++)
        INIT_HLIST_HEAD(&conn_pair_list[i]);
    profile_event_register(PROFILE_TASK_EXIT, &task_exit);
}


void forward_exit(void)
{
    int i;
    conn *list;
    int bucket_num = 1 << conn_pair_list_order;
    struct hlist_node *tmp;
    profile_event_unregister(PROFILE_TASK_EXIT, &task_exit);
    for(i=0; i< bucket_num; i++) {
        hlist_for_each_entry_safe( list, tmp, &conn_pair_list[i], node) {
            conn_pair *cp = get_conn_pair(list);
            del_timer(&cp->timer);
            destroy_conn_pair(cp);
        }
    }
    
    kfree(conn_pair_list);
}
