#ifndef _H_CONNECTION_GLOBAL_H_
#define _H_CONNECTION_GLOBAL_H_

#include "types.h"

enum conn_pair_states
{
    PAIR_NONE = 0,
    PAIR_CONNECTION_STAGE1,           // accept a connection from cilent
    PAIR_RECEIVED_ROUTE,
    PAIR_CONNECTION_STAGE2,   // finish connecting to destin server
    PAIR_PAIRED,
    PAIR_FIN_1,
    PAIR_FIN_2,
    CONN_TIMEWAIT
};

enum NAT_TYPE
{
    DNAT = 1,
    SNAT
};

struct conn_key {
    u32 local_ip;       //local ip
    u32 remote_ip;      //destination ip
    u16 local_port;
    u16 remote_port;
    u8 proto;
};

/*
* For DNAT, alternatived fields includes dip, dport, seq and ack ;
* For SNAT, in addition to the above fields, sip, sport are included.
*/

struct conn{
    union{
        struct  {
            u32 local_ip;       //local ip
            u32 remote_ip;      //destination ip
            u16 local_port;
            u16 remote_port;
            u8 proto;
        };
        struct conn_key key;
    };
    u32 local_seq;      // seq number originated from local ip
    u32 remote_seq;     // replay seq number
    u32 seq_offset;
    u32 ack_offset;
    u16 hash;
    u8 direction;
    u8 state;
};

 struct conn_pair{
    union{
        struct conn pair[2];
        struct{
            struct conn stage1;
            struct conn stage2;
        }; 
    };
    int refcnt;
    u8 state;
    u8 proto;  //IPPROTO_TCP or IPPROTO_UDP
    u8 nat_type;
    void *extend[0];
} ;

enum CONN_STAGE{
    IS_STAGE1,
    IS_STAGE2,
    F_MAX,
};

static inline struct conn_pair *get_conn_pair( struct conn *link)
{
    struct conn_pair *p;
    if( link->direction == IS_STAGE1 ){
        p = container_of(link,  struct conn_pair, stage1);
    }else{
        p = container_of(link,  struct conn_pair, stage2);
    }
    return p;
}

static inline struct conn *opposite_conn( struct conn *link)
{
    if( link->direction == IS_STAGE2 ){
        return link+1;
    }else{
        return link-1;
    }
}

#endif