/* 
 * $Id$
 *
 *
 * Copyright (C) 2001-2003 Fhg Fokus
 *
 * This file is part of ser, a free SIP server.
 *
 * ser is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * For a license to use the ser software under conditions
 * other than those described here, or to purchase support for this
 * software, please contact iptel.org by e-mail at the following addresses:
 *    info@iptel.org
 *
 * ser is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License 
 * along with this program; if not, write to the Free Software 
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */


#include <errno.h>

#include "lock.h"
#include "../../dprint.h"


#ifdef FAST_LOCK
#include "../../mem/shm_mem.h"
#endif


#ifndef FAST_LOCK



/* return -1 if semget failed, -2 if semctl failed */
static int init_semaphore_set( int size )
{
	int new_semaphore, i;

	new_semaphore=semget ( IPC_PRIVATE, size, IPC_CREAT | IPC_PERMISSIONS );
	if (new_semaphore==-1) {
		DBG("DEBUG:init_semaphore_set(%d):  failure to allocate a"
					" semaphore: %s\n", size, strerror(errno));
		return -1;
	}
	for (i=0; i<size; i++) {
		union semun {
			int val;
			struct semid_ds *buf;
			ushort *array;
		} argument;
		/* binary lock */
		argument.val = +1;
		if (semctl( new_semaphore, i , SETVAL , argument )==-1) {
			DBG("DEBUG:init_semaphore_set:  failure to "
				"initialize a semaphore: %s\n", strerror(errno));
			if (semctl( entry_semaphore, 0 , IPC_RMID , 0 )==-1)
				DBG("DEBUG:init_semaphore_set:  failure to release"
					" a semaphore\n");
			return -2;
		}
	}
	return new_semaphore;
}



int change_semaphore( smart_lock *s  , int val )
{
	struct sembuf pbuf;
	int r;

	pbuf.sem_num = s->semaphore_index ;
	pbuf.sem_op =val;
	pbuf.sem_flg = 0;

tryagain:
	r=semop( s->semaphore_set, &pbuf ,  1 /* just 1 op */ );

	if (r==-1) {
		if (errno==EINTR) {
			DBG("signal received in a semaphore\n");
			goto tryagain;
		} else {
			LOG(L_CRIT, "ERROR:change_semaphore_pike(%x, %x, 1) : %s\n",
					s->semaphore_set, &pbuf,
					strerror(errno));
		}
	}
	return r;
}
#endif  /* !FAST_LOCK*/



/* creats NR locks; return 0 if error
*/
smart_lock* create_semaphores(int nr)
{
	int        i;
	smart_lock  *lock_set;
#ifndef FAST_LOCK
	int        sem_set;
#endif

	lock_set = (smart_lock*)shm_malloc(nr*sizeof(smart_lock));
	if (lock_set==0){
		LOG(L_CRIT, "ERROR:create_semaphores: out of pkg mem\n");
		goto error;
	}
#ifdef FAST_LOCK
	for(i=0;i<nr;i++) 
		init_lock(lock_set[i]);
#else
	if ((sem_set=init_semaphore_set(nr))<0) {
		LOG(L_CRIT, "ERROR:create_semaphores: semaphores "
			"initialization failure: %s\n",strerror(errno));
		goto error;
	}

	for (i=0; i<nr; i++) {
		lock_set[i].semaphore_set = sem_set;
		lock_set[i].semaphore_index = i;
	}
#endif
	return lock_set;
error:
	return 0;
}



void destroy_semaphores(smart_lock *sem_set)
{
#ifdef FAST_LOCK
	/* must check if someone uses them, for now just leave them allocated*/
	LOG(L_INFO, "INFO:lock_cleanup:  clean-up still not implemented"
		" properly \n");
#else
	LOG(L_INFO, "INFO:lock_cleanup:  clean-up still not implemented"
		" properly (no sibling check)\n");
	/* sibling double-check missing here; install a signal handler */

	if (sem_set && semctl( sem_set[0].entry_semaphore,0,IPC_RMID,0)==-1)
		LOG(L_ERR, "ERROR: lock_cleanup, entry_semaphore cleanup failed\n");
#endif
	shm_free((void*)sem_set);
	sem_set = 0;
}

