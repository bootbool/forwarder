#ifndef _H_LOG_GLOBAL_H_
#define _H_LOG_GLOBAL_H_

enum {
	PRINT_DEBUG = 0,
	PRINT_INFO,
	PRINT_ERR,
};

extern int print_level;


#ifdef __BPF__
#define __print bpf_printk
#endif

#ifdef __KERNEL__
#define __print printk
#endif

#ifdef __USERSPACE__
#define __print printf
#endif

#endif
