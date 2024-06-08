#ifndef _FORWARD_H
#define _FORWARD_H

#include "common.h"



//forward data from value[0] to value[1]
int add_forward_fd(u32 value[2]) ;
u32 hash_conn( conn *link);
int get_port_n( void );
void set_port_h(int port);
conn* find_hash_conn( conn *link) ;
int string_all_conn_pair( char *str, int len);
void destroy_conn_pair( conn_pair *link );
void forward_flush(void);
void forward_init(void);
void forward_exit(void);


#endif