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

/* compatibility defs for emergency roll-back if things do not work ;
 * if that happens:
 * a) define PRESERVE_ZT (affects most of parser)
 * b) define DONT_REMOVE_ZT (affects first line)
 * c) define SCRATCH
 * d) undefine REMOVE_ALL_ZT (affects via)
 */


#ifndef _COMP_DEFS_H
#define _COMP_DEFS_H

/* preserve "old" parser which delimited header fields by zeros,
 * and included deliminitors in body (which was the only way to
 * learn length of the whole header field 
 */
#undef PRESERVE_ZT

/* go one step further and remove ZT from first line too */
#undef DONT_REMOVE_ZT

/* make it all -- move ZT away (Via) */
#define REMOVE_ALL_ZT

/* don't use scratchpad  anymore */
#undef SCRATCH


/* ------------------------------------------------------ */
/* don't touch this -- that's helper macros depending on
 * the backwards compatibility macros above */
#ifdef PRESERVE_ZT
#	define SET_ZT(_ch) (_ch)='\0'
#else
#	define SET_ZT(_ch)
#endif

#ifdef REMOVE_ALL_ZT
#	define VIA_ZT(_ch)
#else
#	define VIA_ZT(ch) (_ch)='\0'
#endif

#endif
