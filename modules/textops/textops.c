/*$Id$
 *
 * Example ser module, it implements the following commands:
 * search_append("key", "txt") - insert a "txt" after "key"
 * search_insert("key", "txt") -  insert "txt" before "key"
 * replace("txt1", "txt2")
 * search("txt")
 *
 * 
 */



#include "../../sr_module.h"
#include "../../dprint.h"
#include "../../data_lump.h"
#include "../../data_lump_rpl.h"
#include "../../error.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h> /* for regex */
#include <regex.h>

static int search_f(struct sip_msg*, char*, char*);
static int replace_f(struct sip_msg*, char*, char*);
static int search_append_f(struct sip_msg*, char*, char*);
static int append_to_reply_f(struct sip_msg* msg, char* key, char* str);

static int fixup_regex(void**, int);


static struct module_exports my_exports= {
	"textops",
	(char*[])	{
			"search",
			"search_append",
			"replace",
			"append_to_reply"
	},
	(cmd_function[]) {
			search_f,
			search_append_f,
			replace_f,
			append_to_reply_f
	},
	(int[]) {
			1,
			2,
			2,
			1
	},
	(fixup_function[]){
			fixup_regex,
			fixup_regex,
			fixup_regex,
			0
	},
	4,
	0, /* response function */
	0,  /* destroy function */
	0 /* on_cancel function */
};


struct module_exports* mod_register()
{
	fprintf(stderr, "%s - registering...\n", my_exports.name);
	return &my_exports;
}


static int search_f(struct sip_msg* msg, char* key, char* str2)
{
	/*we registered only 1 param, so we ignore str2*/
	regmatch_t pmatch;

	if (regexec((regex_t*) key, msg->orig, 1, &pmatch, 0)!=0) return -1;
	return 1;
}



static int search_append_f(struct sip_msg* msg, char* key, char* str)
{
	char* s;
	struct lump* l;
	regmatch_t pmatch;

	if (regexec((regex_t*) key, msg->orig, 1, &pmatch, 0)!=0) return -1;
	if (pmatch.rm_so!=-1){
		if ((l=anchor_lump(&msg->add_rm, pmatch.rm_eo, 0, 0))==0)
			return -1;
	}

	return insert_new_lump_after(l, str, strlen(str), 0)?1:-1;
}



static int replace_f(struct sip_msg* msg, char* key, char* str)
{
	struct lump* l;
	regmatch_t pmatch;

	if (regexec((regex_t*) key, msg->orig, 1, &pmatch, 0)!=0) return -1;
	if (pmatch.rm_so!=-1){
		if ((l=del_lump(&msg->add_rm, pmatch.rm_so,
						pmatch.rm_eo-pmatch.rm_so, 0))==0)
			return -1;
	}
	return insert_new_lump_after(l, str, strlen(str), 0)?1:-1;
}



static int fixup_regex(void** param, int param_no)
{
	regex_t* re;

	DBG("module - fixing %s\n", *param);
	if (param_no!=1) return 0;
	if ((re=malloc(sizeof(regex_t)))==0) return E_OUT_OF_MEM;
	if (regcomp(re, *param, REG_EXTENDED|REG_ICASE|REG_NEWLINE) ){
		free(re);
		LOG(L_ERR, "ERROR: %s : bad re %s\n", my_exports.name, *param);
		return E_BAD_RE;
	}
	/* free string */
	free(*param);
	/* replace it with the compiled re */
	*param=re;
	return 0;
}



static int append_to_reply_f(struct sip_msg* msg, char* key, char* str)
{
	struct lump_rpl *lump;

	lump = build_lump_rpl( key, strlen(key) );
	if (!lump)
	{
		LOG(L_ERR,"ERROR:append_to_reply : unable to create lump_rl\n");
		return -1;
	}
	add_lump_rpl( msg , lump );

	return 1;
}


