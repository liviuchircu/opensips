/* 
 * $Id$
 *
 * Registrar module interface
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


#include "reg_mod.h"
#include <stdio.h>
#include "../../sr_module.h"
#include "../../timer.h"
#include "../../dprint.h"
#include "../../error.h"
#include "../usrloc/usrloc.h"

#include "save.h"
#include "lookup.h"


static int mod_init(void);                           /* Module init function */
static int domain_fixup(void** param, int param_no); /* Fixup that converts domain name */


int default_expires = 3600; /* Default expires value in seconds */
int default_q       = 0;    /* Default q value multiplied by 1000 */
int append_branches = 1;    /* If set to 1, lookup will put all contacts found in msg structure */

float def_q;                /* default_q converted to float in mod_init */


/*
 * sl_send_reply function pointer
 */
int (*sl_reply)(struct sip_msg* _m, char* _s1, char* _s2);


/*
 * Module exports structure
 */
struct module_exports exports = {
	"registrar", 
	(char*[]) {
		"save",
		"lookup"
	},
	(cmd_function[]) {
		save, 
		lookup
	},
	(int[]){1, 1},
	(fixup_function[]) {
		domain_fixup, 
		domain_fixup
	},
	2,
	
	(char*[]) { /* Module parameter names */
		"default_expires",
		"default_q",
		"append_branches"
	},
	(modparam_t[]) {   /* Module parameter types */
		INT_PARAM,
		INT_PARAM,
		INT_PARAM
	},
	(void*[]) {   /* Module parameter variable pointers */
		&default_expires,
		&default_q,
		&append_branches
	},
	3,         /* Number of module paramers */

	mod_init,   /* module initialization function */
	0,
	0,          /* destroy function */
	0,          /* oncancel function */
	0           /* Per-child init function */
};


/*
 * Initialize parent
 */
static int mod_init(void)
{
	printf( "Initializing registrar module\n");

             /*
              * We will need sl_send_reply from stateless
	      * module for sending replies
	      */
        sl_reply = find_export("sl_send_reply", 2);
	if (!sl_reply) {
		LOG(L_ERR, "This module requires sl module\n");
		return -1;
	}
	
	if (bind_usrloc() < 0) {
		LOG(L_ERR, "Can't find usrloc module\n");
		return -1;
	}

	def_q = (float)default_q / (float)1000;

	return 0;
}


/*
 * Convert char* parameter to udomain_t* pointer
 */
static int domain_fixup(void** param, int param_no)
{
	udomain_t* d;

	if (param_no == 1) {
		if (ul_register_udomain((char*)*param, &d) < 0) {
			LOG(L_ERR, "domain_fixup(): Error while registering domain\n");
			return E_UNSPEC;
		}

		*param = (void*)d;
	}
	return 0;
}
