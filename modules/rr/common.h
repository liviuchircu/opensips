/*
 * Route & Record-Route module
 *
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
 *
 * History:
 * -------
 * 2003-03-27 Adapted to new RR parser (janakj)
 */

#ifndef COMMON_H
#define COMMON_H

#include "../../parser/msg_parser.h"
#include "../../parser/hf.h"
#include "../../str.h"


/*
 * Generate hash string that will be inserted in RR
 */
void generate_hash(void);


/*
 * Parse the message and find first occurence of
 * Route header field. The function returns -1 on
 * an parser error, 0 if there is a Route HF and
 * 1 if there is no Route HF.
 */
int find_first_route(struct sip_msg* _m);


/*
 * Rewrites Request-URI with string given in _s parameter
 *
 * Reuturn 0 on success, negative number on error
 */
int rewrite_RURI(struct sip_msg* _m, str* _s);


/*
 * Remove Top Most Route URI
 * Returns 0 on success, negative number on failure
 */
int remove_first_route(struct sip_msg* _m, struct hdr_field* _route);


/*
 * Insert a new Record-Route header field with lr parameter
 */
int record_route(struct sip_msg* _m, char* _s1, char* _s2);


/*
 * Insert a new Record_route header field with given IP address
 */
int record_route_ip(struct sip_msg* _m, char* _ip, char* _s2);


/*
 * Insert a new Record-Route header field without lr parameter
 */
int record_route_strict(struct sip_msg* _m, char* _s1, char* _s2);

#endif /* COMMON_H */
