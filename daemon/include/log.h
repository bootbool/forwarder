#ifndef _H_LOCAL_LOG_H_
#define _H_LOCAL_LOG_H_

#include "userspace.h"

#include "global_log.h"


#define __print printf

#define print_debug(fmt, ... ) \
{ \
	if( print_level <= PRINT_DEBUG) \
    __print("[D] " fmt " [%s %d]\n", ##__VA_ARGS__, __FUNCTION__, __LINE__); \
}

#define print_debug_raw(fmt, ... ) \
{ \
	if( print_level <= PRINT_DEBUG) \
    __print( fmt,  ##__VA_ARGS__); \
}


#define print_info(fmt, ... ) \
{ \
	if( print_level <= PRINT_INFO) \
    __print("[I] " fmt " [%s %d]\n" ,  ##__VA_ARGS__, __FUNCTION__, __LINE__); \
}

#define print_err(fmt, ... ) \
{ \
	if( print_level <= PRINT_ERR) \
    __print("[E] " fmt " [%s %d]\n", ##__VA_ARGS__, __FUNCTION__, __LINE__); \
}

#endif
