/*
 * fast arhitecture specific locking
 *
 * $Id$
 *
 * 
 */



#ifndef fastlock_h
#define fastlock_h


#include <sched.h>




typedef  volatile int fl_lock_t;



#define init_lock( l ) (l)=0



/*test and set lock, ret 1 if lock held by someone else, 0 otherwise*/
inline static int tsl(fl_lock_t* lock)
{
	volatile char val;
#ifdef __i386
	
	val=1;
	asm volatile( 
		" xchg %b0, %1" : "=q" (val), "=m" (*lock) : "0" (val) : "memory"
	);
	return val;
#elif defined __sparc64
	asm volatile(
			"ldstub [%1], %0 \n\t"
			"membar #StoreStore | #StoreLoad \n\t"
			: "=r"(val) : "r"(lock):"memory"
	);
#endif
}



inline static void get_lock(fl_lock_t* lock)
{
	
	while(tsl(lock)){
		sched_yield();
	}
}



inline static void release_lock(fl_lock_t* lock)
{
	char val;

#ifdef __i386
	val=0;
	asm volatile(
		" xchg %b0, %1" : "=q" (val), "=m" (*lock) : "0" (val) : "memory"
	); /* hmm, maybe lock; movb $0, [%1] would be faster ???*/
#elif defined __sparc64
	asm volatile(
			"membar #LoadStore | #StoreStore \n\t" /*is this really needed?*/
			"stb %%g0, [%0] \n\t"
			: /*no output*/
			: "r" (lock)
			: "memory"
	);
#endif
}



#endif
