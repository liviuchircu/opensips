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


#include "db.h"
#include "../sr_module.h"
#include "../mem/mem.h"

db_func_t dbf;


int bind_dbmod(char* mod)
{
	char* tmp, *p;
	int len;

	if (!mod) {
		LOG(L_ERR, "bind_dbmod(): Invalid database module name\n");
		return -1;
	}

	p = strchr(mod, ':');
	if (p) {
		len = p - mod;
		tmp = (char*)pkg_malloc(len + 1);
		if (!tmp) {
			LOG(L_ERR, "bind_dbmod(): No memory left\n");
			return -1;
		}
		memcpy(tmp, mod, len);
		tmp[len] = '\0';
	} else {
		tmp = mod;
	}

	db_use_table = (db_use_table_f)find_mod_export(tmp, "db_use_table", 2, 0);
	if (db_use_table == 0) goto err;

	db_init = (db_init_f)find_mod_export(tmp, "db_init", 1, 0);
	if (db_init == 0) goto err;

	db_close = (db_close_f)find_mod_export(tmp, "db_close", 2, 0);
	if (db_close == 0) goto err;

	db_query = (db_query_f)find_mod_export(tmp, "db_query", 2, 0);
	if (db_query == 0) goto err;

	db_raw_query = (db_raw_query_f)find_mod_export(tmp, "db_raw_query", 2, 0);
	if (db_raw_query == 0) goto err;

	db_free_query = (db_free_query_f)find_mod_export(tmp, "db_free_query", 2, 0);
	if (db_free_query == 0) goto err;

	db_insert = (db_insert_f)find_mod_export(tmp, "db_insert", 2, 0);
	if (db_insert == 0) goto err;

	db_delete = (db_delete_f)find_mod_export(tmp, "db_delete", 2, 0);
	if (db_delete == 0) goto err;

	db_update = (db_update_f)find_mod_export(tmp, "db_update", 2, 0);
	if (db_update == 0) goto err;

	return 0;

 err:
	if (tmp != mod) pkg_free(tmp);
	return -1;
}
