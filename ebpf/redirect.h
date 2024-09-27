#ifndef H_CONNECTION_H
#define H_CONNECTION_H

#include "types.h"
#include "log.h"
#include "conn_pair.h"

#define TC_ACT_OK 0
#define ETH_P_IP  0x0800
#define COMMENT_LEN 64


struct event {
	struct conn conn;
	char comment[64];
};




#endif
