#ifndef _H_LOCAL_LOG_H_
#define _H_LOCAL_LOG_H_



#include "log_level.h"

#ifdef _EBPF_

#include <bpf/bpf_helpers.h>

#define print_debug(fmt, ... ) \
{ \
	if( print_level <= PRINT_DEBUG) \
    bpf_printk("[D] " fmt " [%s %d]\n", ##__VA_ARGS__, __FUNCTION__, __LINE__); \
}

#else

#include <stdio.h>

#define print_debug(fmt, ... ) \
{ \
	if( print_level <= PRINT_DEBUG) \
    printf("[D] " fmt " [%s %d]\n", ##__VA_ARGS__, __FUNCTION__, __LINE__); \
}

#endif

#endif