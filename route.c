/*
 * $Id$
 *
 * SIP routing engine
 *
 */
 
#include <stdlib.h>
#include <sys/types.h>
#include <regex.h>
#include <netdb.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>

#include "route.h"
#include "dprint.h"
#include "proxy.h"
#include "action.h"
#include "sr_module.h"

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif

/* main routing script table  */
struct action* rlist[RT_NO];


static int fix_actions(struct action* a); /*fwd declaration*/


/* traverses an expr tree and compiles the REs where necessary) 
 * returns: 0 for ok, <0 if errors */
static int fix_expr(struct expr* exp)
{
	regex_t* re;
	int ret;
	
	ret=E_BUG;
	if (exp==0){
		LOG(L_CRIT, "BUG: fix_expr: null pointer\n");
		return E_BUG;
	}
	if (exp->type==EXP_T){
		switch(exp->op){
			case AND_OP:
			case OR_OP:
						if ((ret=fix_expr(exp->l.expr))!=0)
							return ret;
						ret=fix_expr(exp->r.expr);
						break;
			case NOT_OP:
						ret=fix_expr(exp->l.expr);
						break;
			default:
						LOG(L_CRIT, "BUG: fix_expr: unknown op %d\n",
								exp->op);
		}
	}else if (exp->type==ELEM_T){
			if (exp->op==MATCH_OP){
				if (exp->subtype==STRING_ST){
					re=(regex_t*)malloc(sizeof(regex_t));
					if (re==0){
						LOG(L_CRIT, "ERROR: fix_expr: memory allocation"
								" failure\n");
						return E_OUT_OF_MEM;
					}
					if (regcomp(re, (char*) exp->r.param,
								REG_EXTENDED|REG_NOSUB|REG_ICASE) ){
						LOG(L_CRIT, "ERROR: fix_expr : bad re \"%s\"\n",
									(char*) exp->r.param);
						free(re);
						return E_BAD_RE;
					}
					/* replace the string with the re */
					free(exp->r.param);
					exp->r.param=re;
					exp->subtype=RE_ST;
				}else if (exp->subtype!=RE_ST){
					LOG(L_CRIT, "BUG: fix_expr : invalid type for match\n");
					return E_BUG;
				}
			}
			if (exp->l.operand==ACTION_O){
				ret=fix_actions((struct action*)exp->r.param);
				if (ret!=0){
					LOG(L_CRIT, "ERROR: fix_expr : fix_actions error\n");
					return ret;
				}
			}
			ret=0;
	}
	return ret;
}



/* adds the proxies in the proxy list & resolves the hostnames */
/* returns 0 if ok, <0 on error */
static int fix_actions(struct action* a)
{
	struct action *t;
	struct proxy_l* p;
	char *tmp;
	int ret,r;
	struct sr_module* mod;
	
	if (a==0){
		LOG(L_CRIT,"BUG: fix_actions: null pointer\n");
		return E_BUG;
	}
	for(t=a; t!=0; t=t->next){
		switch(t->type){
			case FORWARD_T:
			case SEND_T:
					switch(t->p1_type){
						case NUMBER_ST:
						case IP_ST: /* for now ip_st==number_st*/
							tmp=strdup(inet_ntoa(
										*(struct in_addr*)&t->p1.number));
							if (tmp==0){
								LOG(L_CRIT, "ERROR: fix_actions:"
										"memory allocation failure\n");
								return E_OUT_OF_MEM;
							}
							t->p1_type=STRING_ST;
							t->p1.string=tmp;
							/* no break */
						case STRING_ST:
							p=add_proxy(t->p1.string, t->p2.number);
							if (p==0) return E_BAD_ADDRESS;
							t->p1.data=p;
							t->p1_type=PROXY_ST;
							break;
						case URIHOST_ST:
							break;
						default:
							LOG(L_CRIT, "BUG: fix_actions: invalid type"
									"%d (should be string or number)\n",
										t->type);
							return E_BUG;
					}
					break;
			case IF_T:
				if (t->p1_type!=EXPR_ST){
					LOG(L_CRIT, "BUG: fix_actions: invalid subtype"
								"%d for if (should be expr)\n",
								t->p1_type);
					return E_BUG;
				}else if( (t->p2_type!=ACTIONS_ST)&&(t->p2_type!=NOSUBTYPE) ){
					LOG(L_CRIT, "BUG: fix_actions: invalid subtype"
								"%d for if() {...} (should be action)\n",
								t->p2_type);
					return E_BUG;
				}else if( (t->p3_type!=ACTIONS_ST)&&(t->p3_type!=NOSUBTYPE) ){
					LOG(L_CRIT, "BUG: fix_actions: invalid subtype"
								"%d for if() {} else{...}(should be action)\n",
								t->p3_type);
					return E_BUG;
				}
				if (t->p1.data){
					if ((ret=fix_expr((struct expr*)t->p1.data))<0)
						return ret;
				}
				if ( (t->p2_type==ACTIONS_ST)&&(t->p2.data) ){
					if ((ret=fix_actions((struct action*)t->p2.data))<0)
						return ret;
				}
				if ( (t->p3_type==ACTIONS_ST)&&(t->p3.data) ){
						if ((ret=fix_actions((struct action*)t->p3.data))<0)
						return ret;
				}
				break;
			case MODULE_T:
				if ((mod=find_module(t->p1.data, &r))!=0){
					DBG("fixing %s %s\n", mod->path,
							mod->exports->cmd_names[r]);
					if (mod->exports->fixup_pointers[r]){
						if (mod->exports->param_no[r]>0){
							ret=mod->exports->fixup_pointers[r](&t->p2.data,
																1);
							t->p2_type=MODFIXUP_ST;
							if (ret<0) return ret;
						}
						if (mod->exports->param_no[r]>1){
							ret=mod->exports->fixup_pointers[r](&t->p3.data,
																2);
							t->p3_type=MODFIXUP_ST;
							if (ret<0) return ret;
						}
					}
				}
			
		}
	}
	return 0;
}



/* eval_elem helping function, returns str op param */
static int comp_str(char* str, void* param, int op, int subtype)
{
	int ret;
	
	ret=-1;
	if (op==EQUAL_OP){
		if (subtype!=STRING_ST){
			LOG(L_CRIT, "BUG: comp_str: bad type %d, "
					"string expected\n", subtype);
			goto error;
		}
		ret=(strcasecmp(str, (char*)param)==0);
	}else if (op==MATCH_OP){
		if (subtype!=RE_ST){
			LOG(L_CRIT, "BUG: comp_str: bad type %d, "
					" RE expected\n", subtype);
			goto error;
		}
		ret=(regexec((regex_t*)param, str, 0, 0, 0)==0);
	}else{
		LOG(L_CRIT, "BUG: comp_str: unknown op %d\n", op);
		goto error;
	}
	return ret;
	
error:
	return -1;
}



/* eval_elem helping function, returns a op param */
static int comp_ip(unsigned a, void* param, int op, int subtype)
{
	struct hostent* he;
	char ** h;
	int ret;

	ret=-1;
	switch(subtype){
		case NET_ST:
			ret=(a&((struct net*)param)->mask)==((struct net*)param)->ip;
			break;
		case STRING_ST:
		case RE_ST:
			/* 1: compare with ip2str*/
			ret=comp_str(inet_ntoa(*(struct in_addr*)&a), param, op,
						subtype);
			if (ret==1) break;
			/* 2: (slow) rev dns the address
			 * and compare with all the aliases */
			he=gethostbyaddr((char*)&a, sizeof(a), AF_INET);
			if (he==0){
				LOG(L_DBG, "comp_ip: could not rev_resolve %x\n", a);
				ret=0;
			}else{
				/*  compare with primayry host name */
				ret=comp_str(he->h_name, param, op, subtype);
				/* compare with all the aliases */
				for(h=he->h_aliases; (ret!=1) && (*h); h++){
					ret=comp_str(*h, param, op, subtype);
				}
			}
			break;
		default:
			LOG(L_CRIT, "BUG: comp_ip: invalid type for "
						" src_ip or dst_ip (%d)\n", subtype);
			ret=-1;
	}
	return ret;
	
error:
	return -1;
}



/* returns: 0/1 (false/true) or -1 on error, -127 EXPR_DROP */
static int eval_elem(struct expr* e, struct sip_msg* msg)
{

	int ret;
	ret=E_BUG;
	
	if (e->type!=ELEM_T){
		LOG(L_CRIT," BUG: eval_elem: invalid type\n");
		goto error;
	}
	switch(e->l.operand){
		case METHOD_O:
				ret=comp_str(msg->first_line.u.request.method.s, e->r.param,
								e->op, e->subtype);
				break;
		case URI_O:
				if(msg->new_uri.s){
					ret=comp_str(msg->new_uri.s, e->r.param,
									e->op, e->subtype);
				}else{
					ret=comp_str(msg->first_line.u.request.uri.s, e->r.param,
									e->op, e->subtype);
				}
				break;
		case SRCIP_O:
				ret=comp_ip(msg->src_ip, e->r.param, e->op, e->subtype);
				break;
		case DSTIP_O:
				ret=comp_ip(msg->dst_ip, e->r.param, e->op, e->subtype);
				break;
		case NUMBER_O:
				ret=!(!e->r.intval); /* !! to transform it in {0,1} */
				break;
		case ACTION_O:
				ret=run_actions( (struct action*)e->r.param, msg);
				if (ret<=0) ret=(ret==0)?EXPR_DROP:0;
				else ret=1;
				break;
		default:
				LOG(L_CRIT, "BUG: eval_elem: invalid operand %d\n",
							e->l.operand);
	}
	return ret;
error:
	return -1;
}



/* ret= 0/1 (true/false) ,  -1 on error or EXPR_DROP (-127)  */
int eval_expr(struct expr* e, struct sip_msg* msg)
{
	static int rec_lev=0;
	int ret;
	
	rec_lev++;
	if (rec_lev>MAX_REC_LEV){
		LOG(L_CRIT, "ERROR: eval_expr: too many expressions (%d)\n",
				rec_lev);
		ret=-1;
		goto skip;
	}
	
	if (e->type==ELEM_T){
		ret=eval_elem(e, msg);
	}else if (e->type==EXP_T){
		switch(e->op){
			case AND_OP:
				ret=eval_expr(e->l.expr, msg);
				/* if error or false stop evaluating the rest */
				if (ret!=1) break;
				ret=eval_expr(e->r.expr, msg); /*ret1 is 1*/
				break;
			case OR_OP:
				ret=eval_expr(e->l.expr, msg);
				/* if true or error stop evaluating the rest */
				if (ret!=0) break;
				ret=eval_expr(e->r.expr, msg); /* ret1 is 0 */
				break;
			case NOT_OP:
				ret=eval_expr(e->l.expr, msg);
				if (ret<0) break;
				ret= ! ret;
				break;
			default:
				LOG(L_CRIT, "BUG: eval_expr: unknown op %d\n", e->op);
				ret=-1;
		}
	}else{
		LOG(L_CRIT, "BUG: eval_expr: unknown type %d\n", e->type);
		ret=-1;
	}

skip:
	rec_lev--;
	return ret;
}


/* adds an action list to head; a must be null terminated (last a->next=0))*/
void push(struct action* a, struct action** head)
{
	struct action *t;
	if (*head==0){
		*head=a;
		return;
	}
	for (t=*head; t->next;t=t->next);
	t->next=a;
}




int add_actions(struct action* a, struct action** head)
{
	int ret;

	LOG(L_DBG, "add_actions: fixing actions...\n");
	if ((ret=fix_actions(a))!=0) goto error;
	push(a,head);
	return 0;
	
error:
	return ret;
}



/* fixes all action tables */
/* returns 0 if ok , <0 on error */
int fix_rls()
{
	int i,ret;
	for(i=0;i<RT_NO;i++){
		if(rlist[i]){
			if ((ret=fix_actions(rlist[i]))!=0){
				return ret;
			}
		}
	}
	return 0;
}


/* debug function, prints main routing table */
void print_rl()
{
	struct action* t;
	int i,j;

	for(j=0; j<RT_NO; j++){
		if (rlist[j]==0){
			if (j==0) DBG("WARNING: the main routing table is empty\n");
			continue;
		}
		DBG("routing table %d:\n",j);
		for (t=rlist[j],i=0; t; i++, t=t->next){
			print_action(t);
		}
		DBG("\n");
	}
}


