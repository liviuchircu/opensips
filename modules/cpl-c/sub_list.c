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

#include <stdlib.h>
#include <string.h>
#include "sub_list.h"

struct node*   append_to_list(struct node *head, unsigned char *offset,
																char *name)
{
	struct node *n;
	struct node *new_node;

	new_node = malloc(sizeof(struct node));
	if (!new_node)
		return 0;
	new_node->offset = offset;
	new_node->name = name;
	new_node->next = 0;
	if (head) {
		n = head;
		while (n->next)
			n = n->next;
		n->next = new_node;
	}

	return new_node;
}




unsigned char* search_the_list(struct node *head, char *name)
{
	struct node *n;

	n = head;
	while (n) {
		if (strcasecmp(n->name,name)==0)
			return n->offset;
		n = n->next;
	}
	return 0;
}




void delete_list(struct node* head)
{
	struct node *n;
;
	while (head) {
		n=head->next;
		free(head);
		head = n;
	}
}


