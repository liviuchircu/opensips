/*
 * $Id$
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


#ifndef _CPL_LOC_SET_H_
#define _CPL_LOC_SET_H_

#include <stdio.h>
#include <string.h>
#include <time.h>
#include <stdlib.h>

#include "../../mem/shm_mem.h"
#include "../../str.h"
#include "../../dprint.h"



struct location {
	struct address {
		str uri;
		unsigned int priority;
	}addr;
	struct location *next;
};



static inline void free_location( struct location *loc)
{
	shm_free( loc );
}



/* insert a new location into the set mantaining order by the prio val */
static inline int add_location(struct location **loc_set, char *uri_s,
											int uri_len, unsigned int prio)
{
	struct location *loc;
	struct location *foo, *bar;

	loc = (struct location*)shm_malloc( sizeof(struct location) );
	if (!loc) {
		LOG(L_ERR,"ERROR:add_location: no more free shm memory!\n");
		return -1;
	}

	loc->addr.uri.s = uri_s;
	loc->addr.uri.len = uri_len;
	loc->addr.priority = prio;

	/* find the proper place for the new location */
	foo = *loc_set;
	bar = 0;
	while(foo && foo->addr.priority>prio) {
		bar = foo;
		foo = foo->next;
	}
	if (!bar) {
		/* insert at the beginning */
		loc->next = *loc_set;
		*loc_set = loc;
	} else {
		/* insert after bar, before foo  */
		loc->next = foo;
		bar->next = loc;
	 }

	return 0;
}



static inline void remove_location(struct location **loc_set, char *uri_s,
																int uri_len)
{
	struct location *loc = *loc_set;
	struct location *prev_loc = 0;

	for( ; loc ; prev_loc=loc,loc=loc->next ) {
		if (loc->addr.uri.len==uri_len &&
		!memcpy(loc->addr.uri.s,uri_s,uri_len) )
			break;
	}

	if (loc) {
		DBG("DEBUG:remove_location: removing from loc_set <%.*s>\n",
			uri_len,uri_s);
		if (prev_loc)
			prev_loc->next=loc->next;
		else
			(*loc_set)=loc->next;
		shm_free( loc );
	} else {
		DBG("DEBUG:remove_location: no matching in loc_set for <%.*s>\n",
			uri_len,uri_s);
	}
}



static inline struct location *remove_first_location(struct location **loc_set)
{
	struct location *loc;

	if (!*loc_set)
		return 0;

	loc = *loc_set;
	*loc_set = (*loc_set)->next;
	return loc;
}



static inline void empty_location_set(struct location **loc_set)
{
	struct location *loc;

	while (*loc_set) {
		loc = (*loc_set)->next;
		shm_free(*loc_set);
		*loc_set = loc;
	}
	*loc_set = 0;
}


#endif


