#ifndef _H_TYPES_GLOBAL_H_
#define _H_TYPES_GLOBAL_H_

typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long int u64;


enum ERR_CODE
{
	ERRCODE_OK = 1,
	ERRCODE_FAIL,
	ERRCODE_VALUE,
	ERRCODE_FORMAT
};

#define typeof_member(T, m)	typeof(((T*)0)->m)

//#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)

#define container_of(ptr, type, member) ({				\
	void *__mptr = (void *)(ptr);					\
	((type *)(__mptr - offsetof(type, member))); })




#endif
