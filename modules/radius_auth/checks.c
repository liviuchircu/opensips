/*
 * $Id$
 *
 * Checks if To and From header fields contain the same
 * username as digest credentials
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


#include "checks.h"
#include "../../str.h"
#include "../../dprint.h"
#include <string.h>
#include "defs.h"
#include "../../parser/digest/digest.h" /* get_authorized_cred */
#include "common.h"
#include "../../ut.h"


/*
 * Check if To header field contains the same username
 * as digest credentials
 */
static inline int check_username(struct sip_msg* _m, struct hdr_field* _h)
{
	struct hdr_field* h;
	auth_body_t* c;

#ifdef USER_DOMAIN_HACK
	char* ptr;
#endif

	str user;
	int len;

	if (!_h) {
		LOG(L_ERR, "check_username(): To HF not found\n");
		return -1;
	}

	get_authorized_cred(_m->authorization, &h);
	if (!h) {
		get_authorized_cred(_m->proxy_auth, &h);
		if (!h) {
			LOG(L_ERR, "is_user(): No authorized credentials found (error in scripts)\n");
			return -1;
		}
	}

	c = (auth_body_t*)(h->parsed);

	user.s = _h->body.s;
	user.len = _h->body.len;

	if (auth_get_username(&user) < 0) {
		LOG(L_ERR, "is_user(): Can't extract username\n");
		return -1;
	}

	if (!user.len) return -1;

	len = c->digest.username.len;

#ifdef USER_DOMAIN_HACK
	ptr = q_memchr(c->digest.username.s, '@', len);
	if (ptr) {
		len = ptr - c->digest.username.s;
	}
#endif

	if (user.len == len) {
		if (strncasecmp(user.s, c->digest.username.s, len) == 0) {
			DBG("check_username(): auth id and To username are equal\n");
			return 1;
		}
	}
	
	DBG("check_username(): auth id and To username differ\n");
	return -1;
}


/*
 * Check username part in To header field
 */
int check_to(struct sip_msg* _msg, char* _s1, char* _s2)
{
	return check_username(_msg, _msg->to);
}


/*
 * Check username part in From header field
 */
int check_from(struct sip_msg* _msg, char* _s1, char* _s2)
{
	return check_username(_msg, _msg->from);
}
