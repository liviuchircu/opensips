/*
 * presence_reginfo module - Presence Handling of reg events
 *
 * Copyright (C) 2011, 2023 Carsten Bock, carsten@ng-voice.com
 *
 * This file is part of opensips, a free SIP server.
 *
 * opensips is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version
 *
 * opensips is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301  USA
 */


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <sys/types.h>
#include <unistd.h>

#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../str.h"
#include "../../parser/msg_parser.h"
#include "../../mem/mem.h"
#include "../presence/bind_presence.h"
#include "add_events.h"
#include "presence_reginfo.h"

/* module functions */
static int mod_init(void);

/* module variables */
add_event_t pres_add_event;

/* module parameters */
int pres_reginfo_aggregate_presentities = 0;
unsigned int pres_reginfo_default_expires = 3600;

/* module exported parameters */
static param_export_t params[] = {
		{"default_expires", INT_PARAM, &pres_reginfo_default_expires},
		{"aggregate_presentities", INT_PARAM,
				&pres_reginfo_aggregate_presentities},
		{0, 0, 0}};

static const cmd_export_t cmds[] =
{
    {0, 0, {{0, 0, 0}}, 0}
};

/* module exports */
/* clang-format off */
static const dep_export_t deps = {
	{ /* OpenSIPS module dependencies */
		{ MOD_TYPE_DEFAULT, "presence", DEP_ABORT },
		{ MOD_TYPE_NULL, NULL, 0 },
	},
	{ /* modparam dependencies */
		{ NULL, NULL },
	},
};

/* module exports */
struct module_exports exports= {
    "presence_reginfo",		/* module name */
    MOD_TYPE_DEFAULT,           /* class of this module */
    MODULE_VERSION,				/* module version */
    DEFAULT_DLFLAGS,			/* dlopen flags */
    0,							/* load function */
    &deps,                      /* OpenSIPS module dependencies */
    cmds,						/* exported functions */
    0,							/* exported async functions */
    params,						/* exported parameters */
    0,							/* exported statistics */
    0,							/* exported MI functions */
    0,							/* exported pseudo-variables */
    0,			 				/* exported transformations */
    0,							/* extra processes */
    0,							/* module pre-initialization function */
    mod_init,					/* module initialization function */
    0,							/* response handling function */
    0,							/* destroy function */
    0,							/* per-child init function */
    0							/* reload confirm function */
};
/* clang-format on */

/*
 * init module function
 */
static int mod_init(void)
{
	presence_api_t pres;
	bind_presence_t bind_presence;

	bind_presence= (bind_presence_t)find_export("bind_presence", 0);
	if(!bind_presence) {
		LM_ERR("can't bind presence\n");
		return -1;
	}
	if(bind_presence(&pres) < 0) {
		LM_ERR("can't bind presence\n");
		return -1;
	}

	if(pres_reginfo_aggregate_presentities != 0
			&& pres_reginfo_aggregate_presentities != 1) {
		LM_ERR("invalid aggregate_presentities param value, should be 0 or "
			   "1\n");
		return -1;
	}

	pres_add_event = pres.add_event;
	if(pres_add_event == NULL) {
		LM_ERR("could not import add_event\n");
		return -1;
	}
	if(reginfo_add_events() < 0) {
		LM_ERR("failed to add reginfo-info events\n");
		return -1;
	}

	return 0;
}
