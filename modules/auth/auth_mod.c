/* 
 * $Id$ 
 */

#include "auth_mod.h"
#include <stdlib.h>
#include "../../sr_module.h"
#include "../../dprint.h"
#include "defs.h"
#include <string.h>
#include "auth.h"
#include "checks.h"
#include "group.h"
#include "../../ut.h"
#include "../../error.h"


/*
 * Module destroy function prototype
 */
static void destroy(void);


/*
 * Module child-init function prototype
 */
static int child_init(int rank);


/*
 * Module initialization function prototype
 */
static int mod_init(void);


static int challenge_fixup(void** param, int param_no);


/*
 * Pointer to reply function in stateless module
 */
int (*sl_reply)(struct sip_msg* _msg, char* _str1, char* _str2);


/*
 * Module parameter variables
 */
char* db_url       = "sql://janakj:heslo@localhost/ser";
char* user_column  = "user";
char* realm_column = "realm";
char* pass_column  = "ha1";

#ifdef USER_DOMAIN_HACK
char* pass_column_2 = "ha1b";
#endif

char* sec          = "4e9rhygt90ofw34e8hiof09tg"; /* Secret phrase used to generate nonce value */
char* grp_table    = "grp";                       /* Table name where group definitions are stored */
char* grp_user_col = "user";
char* grp_grp_col  = "grp";
int   calc_ha1     = 0;
int   nonce_expire = 300;
int   retry_count  = 5;

str secret;
db_con_t* db_handle;   /* Database connection handle */


/*
 * Module interface
 */
struct module_exports exports = {
	"auth", 
	(char*[]) { 
		"www_authorize",
		"proxy_authorize",
		"www_challenge",
		"proxy_challenge",
		"is_user",
		"is_in_group",
		"check_to",
		"check_from"
	},
	(cmd_function[]) {
		www_authorize,
		proxy_authorize,
		www_challenge,
		proxy_challenge,
		is_user,
		is_in_group,
		check_to,
		check_from
	},
	(int[]) {2, 2, 2, 2, 1, 1, 0, 0},
	(fixup_function[]) {
		NULL, NULL, challenge_fixup, challenge_fixup, NULL, NULL, NULL, NULL
	},
	8,
	
	(char*[]) {
		"db_url",              /* Database URL */
		"user_column",         /* User column name */
		"realm_column",        /* Realm column name */
		"password_column",     /* HA1/password column name */
#ifdef USER_DOMAIN_HACK
		"password_column_2",
#endif

		"secret",              /* Secret phrase used to generate nonce */
		"group_table",         /* Group table name */
		"group_user_column",   /* Group table user column name */
		"group_group_column",  /* Group table group column name */
		"calculate_ha1",       /* If set to yes, instead of ha1 value auth module will
                                        * fetch plaintext password from database and calculate
                                        * ha1 value itself */
		"nonce_expire",        /* After how many seconds nonce expires */
		"retry_count"          /* How many times a client is allowed to retry */
		
		
	},   /* Module parameter names */
	(modparam_t[]) {
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
#ifdef USER_DOMAIN_HACK
		STR_PARAM,
#endif
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
		STR_PARAM,
	        INT_PARAM,
		INT_PARAM,
		INT_PARAM
	},   /* Module parameter types */
	(void*[]) {
		&db_url,
		&user_column,
		&realm_column,
		&pass_column,
#ifdef USER_DOMAIN_HACK
		&pass_column_2,
#endif
		&sec,
		&grp_table,
		&grp_user_col,
		&grp_grp_col,
		&calc_ha1,
		&nonce_expire,
		&retry_count
		
	},   /* Module parameter variable pointers */
#ifdef USER_DOMAIN_HACK
	12,      /* Numberof module parameters */
#else
	11,      /* Number of module paramers */
#endif					     
	mod_init,   /* module initialization function */
	NULL,       /* response function */
	destroy,    /* destroy function */
	NULL,       /* oncancel function */
	child_init  /* child initialization function */
};


static int child_init(int rank)
{
	if (db_url == NULL) {
		LOG(L_ERR, "auth:init_child(): Use db_url parameter\n");
		return -1;
	}
	db_handle = db_init(db_url);
	if (!db_handle) {
		LOG(L_ERR, "auth:init_child(): Unable to connect database\n");
		return -1;
	}
	return 0;

}


static int mod_init(void)
{
	LOG(L_ERR, "auth module - initializing\n");
	
	     /* Find a database module */
	if (bind_dbmod()) {
		LOG(L_ERR, "mod_init(): Unable to bind database module\n");
		return -1;
	}

	sl_reply = find_export("sl_send_reply", 2);

	if (!sl_reply) {
		LOG(L_ERR, "auth:mod_init(): This module requires sl module\n");
		return -2;
	}

	     /* Precalculate secret string length */
	secret.s = sec;
	secret.len = strlen(secret.s);

	return 0;
}



static void destroy(void)
{
	db_close(db_handle);
}


static int challenge_fixup(void** param, int param_no)
{
	unsigned int qop;
	int err;
	
	if (param_no == 2) {
		qop = str2s(*param, strlen(*param), &err);

		if (err == 0) {
			free(*param);
			*param=(void*)qop;
			return 0;
		} else {
			LOG(L_ERR, "challenge_fixup(): Bad number <%s>\n",
			    (char*)(*param));
			return E_UNSPEC;
		}
	}

	return 0;
}
