/*
 * $Id$
 *
 * - utility for generating to-tags
 *   in SER, to-tags consist of two parts: a fixed part
 *   which is bound to server instance and variable part
 *   which is bound to request -- that helps to recognize,
 *   who generated the to-tag in loops through the same
 *   server -- in such cases, fixed part is constant, but
 *   the variable part varies because it depends on 
 *   via
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


#ifndef _TAGS_H
#define _TAGS_H

#include "parser/msg_parser.h"
#include "crc.h"
#include "str.h"

#define TOTAG_LEN (MD5_LEN+CRC16_LEN+1)

/* generate variable part of to-tag for a request;
 * it will have length of CRC16_LEN, sufficiently
 * long buffer must be passed to the fucntion */
static inline void calc_crc_suffix( struct sip_msg *msg, char *tag_suffix)
{
	int ss_nr;
	str suffix_source[3];

	ss_nr=2;
	suffix_source[0]=msg->via1->host;
	suffix_source[1]=msg->via1->port_str;
	if (msg->via1->branch)
		suffix_source[ss_nr++]=msg->via1->branch->value;
        crcitt_string_array( tag_suffix, suffix_source, ss_nr );
}

static inline init_tags( char *tag, char **suffix, 
		char *signature, char separator )
{
	str src[3];

	src[0].s=signature; src[0].len=strlen(signature);
	src[1].s=sock_info[0].address_str.s;
	src[1].len=sock_info[0].address_str.len;
	src[2].s=sock_info[0].port_no_str.s;
	src[2].len=sock_info[0].port_no_str.len;

	MDStringArray( tag, src, 3 );

	tag[MD5_LEN]=separator;
	*suffix=tag+MD5_LEN+1;
}


#endif
