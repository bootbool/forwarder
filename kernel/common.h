#ifndef _COMMON_H
#define _COMMON_H

#include <linux/jhash.h>
#include <linux/kernel.h>


enum F_LIST_TYPE{
    IS_CLIENT,
    IS_SERVER,
    F_MAX,
};

#define ROOT_PROC "forwarder"
  
void set_port(int port);
int get_port( void );

typedef struct {
    struct hlist_node node;
    u32 local_ip;       //local ip
    u32 remote_ip;       //destination ip
    u16 local_port;
    u16 remote_port;
    u32 local_seq;  // from cilent to forwarder. Or from forwarder to server
    u32 remote_seq;  // replay seq number
    u32 seq_offset;
    u32 ack_offset;
    u16 hash;
    u8 direction;
    u8 proto;
    u8 state;
} conn;

static inline void set_u8_bit(u8 *p , u8 pos)
{
    *p = *p | (1<<pos);
}

static inline int test_u8_bit(u8 *p , u8 pos)
{
    return  *p & (1<<pos);
}


static inline int clear_u8_bit(u8 *p , u8 pos)
{
    return  *p = *p & ( ~(1<<pos));
}


typedef struct {
    union{
        conn pair[2];
        struct{
            conn client;
            conn server;
        }; 
    };
    struct rcu_head rcu;
    struct timer_list timer;
    struct task_struct *task;
    int fd[2];  //fd[0] is to client socket, fd[1] is to server socket.
    atomic_t refcnt;
    u8 state;
    u8 proto;  //IPPROTO_TCP or IPPROTO_UDP
    u8 flags;
} conn_pair;


static inline conn_pair *get_conn_pair( conn *link)
{
    conn_pair *p;
    if( link->direction == IS_CLIENT ){
        p = container_of(link,  conn_pair, client);
    }else{
        p = container_of(link,  conn_pair, server);
    }
    return p;
}

static inline conn *opposite_conn( conn *link)
{
    if( link->direction == IS_CLIENT ){
        return link+1;
    }else{
        return link-1;
    }
}

#endif

