/*
 * $Id *
 */

#include "../config.h"
#include "../dprint.h"
#include "mem.h"

#ifdef PKG_MALLOC
#	ifdef VQ_MALLOC
#		include "vq_malloc.h"
#	else
#		include "q_malloc.h"
#	endif
#endif

#ifdef PKG_MALLOC
	char mem_pool[PKG_MEM_POOL_SIZE];
	#ifdef VQ_MALLOC
		struct vqm_block* mem_block;
	#else
		struct qm_block* mem_block;
	#endif
#endif

int init_mallocs()
{
#ifdef PKG_MALLOC
        /*init mem*/
	#ifdef VQ_MALLOC
        	mem_block=vqm_malloc_init(mem_pool, PKG_MEM_POOL_SIZE);
	#else
        	mem_block=qm_malloc_init(mem_pool, PKG_MEM_POOL_SIZE);
	#endif
        if (mem_block==0){
                LOG(L_CRIT, "could not initialize memory pool\n");
		return -1;
        }
#endif

#ifdef SHM_MEM
        if (shm_mem_init()<0) {
                LOG(L_CRIT, "could not initialize shared memory pool, exiting...\n");
                return -1;
        }
#endif
	return 0;

}


