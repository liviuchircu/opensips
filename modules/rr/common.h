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
 */


#ifndef COMMON_H
#define COMMON_H

#include "../../parser/msg_parser.h"
#include "../../parser/hf.h"
#include "../../str.h"

#define RR_PREFIX "Record-Route: <sip:"
#define RR_PREFIX_LEN (sizeof(RR_PREFIX)-1)
#define RR_LR_TERM ";lr>\r\n"
#define RR_LR_TERM_LEN (sizeof(RR_LR_TERM)-1)
#define RR_SR_TERM ">\r\n"
#define RR_SR_TERM_LEN (sizeof(RR_SR_TERM)-1)

#define RR_FROMTAG ";ftag="
#define RR_FROMTAG_LEN (sizeof(RR_FROMTAG)-1)


extern char rr_hash[MD5_LEN];


/*
 * Generate hash string that will be inserted in RR
 */
void generate_hash(void);


int generate_rr_suffix(void);


/*
 * Parse the message and find first occurence of
 * Route header field. The function returns -1 on
 * an parser error, 0 if there is a Route HF and
 * 1 if there is no Route HF.
 */
int find_first_route(struct sip_msg* _m);


/*
 * Extracts URI from the topmost Route header field.
 *
 * Returns 0 on success, negative number on an error.
 */
int parse_first_route(struct hdr_field* _route, str* _s);


/*
 * Rewrites Request-URI with string given in _s parameter
 *
 * Reuturn 0 on success, negative number on error
 */
int rewrite_RURI(struct sip_msg* _m, str* _s);


/*
 * Remove Top Most Route URI
 * Returns 0 on success, negative number on failure
 * If there is another URI in the Route header field, it will
 * be stored in _uri parameter
 */
int remove_TMRoute(struct sip_msg* _m, struct hdr_field* _route, str* _uri);


#ifdef _OBSOL /* static now */
/*
 * Builds Record-Route line
 * Returns 0 on success, negative number on a failure
 * if _lr is set to 1, ;lr parameter will be used
 */
int build_RR(struct sip_msg* _m, str* _l, int _lr);
#endif


/*
 * Insert a new Record-Route Header Field
 * into a SIP message
 */
int insert_RR(struct sip_msg* _m, str* _l);


/*
 * Insert a new Record-Route header field
 */
int record_route(struct sip_msg* _m, char* _lr, char* _s);


#endif /* COMMON_H */
