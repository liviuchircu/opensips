/*
 * eXtended JABber module - worker implemetation
 *
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

/***
 * 2003-01-20 xj_worker_precess function cleaning - some part of it moved to
 * xj_worker_check_jcons function, by dcm
 *
 */
#include <string.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/time.h>
#include <sys/types.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

#include "../../dprint.h"
#include "../../timer.h"
#include "../../mem/mem.h"
#include "../../mem/shm_mem.h"
#include "../tm/tm_load.h"

#include "xjab_worker.h"
#include "xjab_util.h"
#include "xjab_jcon.h"
#include "xjab_dmsg.h"
#include "xode.h"
#include "xjab_presence.h"

#include "mdefines.h"

#define XJAB_RESOURCE "serXjab"

#define XJ_ADDRTR_NUL	0
#define XJ_ADDRTR_A2B	1
#define XJ_ADDRTR_B2A	2
#define XJ_ADDRTR_CON	4

#define XJ_MSG_POOL_SIZE	10

// proxy address
#define _PADDR(a)	((a)->aliases->proxy)

/** TM bind */
extern struct tm_binds tmb;

/** debug info */
int _xj_pid = 0;
int main_loop = 1;

/** **/

static str jab_gw_name = {"sip_to_jabber_gateway", 21};

/**
 * address corection
 * alias A~B: flag == 0 => A->B, otherwise B->A
 */
int xj_address_translation(str *src, str *dst, xj_jalias als, int flag)
{
	char *p, *p0;
	int i, ll;
	
	if(!src || !dst || !src->s || !dst->s )
		return -1; 
	
	if(!als || !als->jdm || !als->jdm->s || als->jdm->len <= 0)
		goto done;
	
	dst->len = 0;
#ifdef XJ_EXTRA_DEBUG
	DBG("XJAB:xj_address_translation:%d: - checking aliases\n", _xj_pid);
#endif
	p = src->s;

	while(p<(src->s + src->len)	&& *p != '@') 
		p++;
	if(*p != '@')
		goto done;

	p++;
	ll = src->s + src->len - p;

#ifdef XJ_EXTRA_DEBUG
	DBG("XJAB:xj_address_translation:%d: - domain is [%.*s]\n",_xj_pid,ll,p);
#endif
	
	/*** checking aliases */
	if(als->size > 0)
	{
		for(i=0; i<als->size; i++)
			if(als->a[i].len == ll && 
				!strncasecmp(p, als->a[i].s, als->a[i].len))
			{
				if(als->d[i])
				{
					if(flag & XJ_ADDRTR_A2B)
					{
						strncpy(dst->s, src->s, src->len);
						p0 = dst->s;
						while(p0 < dst->s + (p-src->s)) 
						{
							if(*p0 == als->dlm)
								*p0 = als->d[i];
							p0++;
						}
						return 0;
					}
					if(flag & XJ_ADDRTR_B2A)
					{
						strncpy(dst->s, src->s, src->len);
						p0 = dst->s;
						while(p0 < dst->s + (p-src->s)) 
						{
							if(*p0 == als->d[i])
								*p0 = als->dlm;
							p0++;
						}						
						return 0;
					}
				}
				goto done;
			}
	}

#ifdef XJ_EXTRA_DEBUG
	DBG("XJAB:xj_address_translation:%d: - doing address correction\n",
			_xj_pid);	
#endif
	
	if(flag & XJ_ADDRTR_A2B)
	{
		if(als->jdm->len != ll || strncasecmp(p, als->jdm->s, als->jdm->len))
		{
			DBG("XJA:xj_address_translation:%d: - wrong Jabber"
				" destination <%.*s>!\n", _xj_pid, src->len, src->s);
			return -1;
		}
		if(flag & XJ_ADDRTR_CON)
		{
#ifdef XJ_EXTRA_DEBUG
			DBG("XJAB:xj_address_translation:%d: - that is for"
				" Jabber conference\n", _xj_pid);
#endif
			p0 = p-1;
			while(p0 > src->s && *p0 != als->dlm)
				p0--;
			if(p0 <= src->s)
				return -1;
			p0--;
			while(p0 > src->s && *p0 != als->dlm)
				p0--;
			if(*p0 != als->dlm)
				return -1;
			dst->len = p - p0 - 2;
			strncpy(dst->s, p0+1, dst->len);
			dst->s[dst->len]=0;
			p = dst->s;
			while(p < (dst->s + dst->len) && *p!=als->dlm)
				p++;
			if(*p==als->dlm)
				*p = '@';
			return 0;
		}
#ifdef XJ_EXTRA_DEBUG
		DBG("XJAB:xj_address_translation:%d: - that is for"
			" Jabber network\n", _xj_pid);
#endif
		dst->len = p - src->s - 1;
		strncpy(dst->s, src->s, dst->len);
		dst->s[dst->len]=0;
		if((p = strchr(dst->s, als->dlm)) != NULL)
			*p = '@';
		else
		{
			DBG("XJA:xj_address_translation:%d: - wrong Jabber"
				" destination <%.*s>!!!\n", _xj_pid, src->len, src->s);
			return -1;
		}
		return 0;
	}
	if(flag & XJ_ADDRTR_B2A)
	{
		*(p-1) = als->dlm;
		p0 = src->s + src->len;
		while(p0 > p)
		{
			if(*p0 == '/')
			{
				src->len = p0 - src->s;
				*p0 = 0;
			}
			p0--;
		}
		strncpy(dst->s, src->s, src->len);
		dst->s[src->len] = '@';
		dst->s[src->len+1] = 0;
		strncat(dst->s, als->jdm->s, als->jdm->len);
		dst->len = strlen(dst->s);
		return 0;
	}

done:
	dst->s = src->s;
	dst->len = src->len;
	return 0;	
}

/**
 * worker implementation 
 * - jwl : pointer to the workers list
 * - jaddress : address of the jabber server
 * - jport : port of the jabber server
 * - rank : worker's rank
 * - db_con : connection to database
 * #return : 0 on success or <0 on error
 */
int xj_worker_process(xj_wlist jwl, char* jaddress, int jport, int rank,
		db_con_t* db_con)
{
	int pipe, ret, i, pos, maxfd, flag;
	xj_jcon_pool jcp;
	struct timeval tmv;
	fd_set set, mset;
	xj_sipmsg jsmsg;
	str sto;
	xj_jcon jbc = NULL;
	xj_jconf jcf = NULL;
	char *p, buff[1024], recv_buff[4096];
	int flags, nr, ltime = 0;
	xj_pres_cell prc = NULL;
	
	db_key_t keys[] = {"sip_id", "type"};
	db_val_t vals[] = { {DB_STRING, 0, {.string_val = buff}},
						{DB_INT, 0, {.int_val = 0}} };
	db_key_t col[] = {"jab_id", "jab_passwd"};
	db_res_t* res = NULL;

	_xj_pid = getpid();
	
	//signal(SIGTERM, xj_sig_handler);
	//signal(SIGINT, xj_sig_handler);
	//signal(SIGQUIT, xj_sig_handler);
	signal(SIGSEGV, xj_sig_handler);

	if(!jwl || !jwl->aliases || !jwl->aliases->jdm 
			|| !jaddress || rank >= jwl->len)
	{
		DBG("XJAB:xj_worker[%d]:%d: exiting - wrong parameters\n",
				rank, _xj_pid);
		return -1;
	}

	pipe = jwl->workers[rank].rpipe;
	DBG("XJAB:xj_worker[%d]:%d: started - pipe=<%d> : 1st message delay"
		" <%d>\n", rank, _xj_pid, pipe, jwl->delayt);
	if((jcp=xj_jcon_pool_init(jwl->maxj,XJ_MSG_POOL_SIZE,jwl->delayt))==NULL)
	{
		DBG("XJAB:xj_worker: cannot allocate the pool\n");
		return -1;
	}

	maxfd = pipe;
	tmv.tv_sec = jwl->sleept;
	tmv.tv_usec = 0;

	FD_ZERO(&set);
	FD_SET(pipe, &set);
	while(main_loop)
	{
		mset = set;

		tmv.tv_sec = (jcp->jmqueue.size == 0)?jwl->sleept:1;
#ifdef XJ_EXTRA_DEBUG
		//DBG("XJAB:xj_worker[%d]:%d: select waiting %ds - queue=%d\n",rank,
		//		_xj_pid, (int)tmv.tv_sec, jcp->jmqueue.size);
#endif
		tmv.tv_usec = 0;

		ret = select(maxfd+1, &mset, NULL, NULL, &tmv);
		/** check the queue AND conecction of head element is ready */
		for(i = 0; i<jcp->jmqueue.size && main_loop; i++)
		{
			if(jcp->jmqueue.jsm[i]==NULL || jcp->jmqueue.ojc[i]==NULL)
			{
				if(jcp->jmqueue.jsm[i]!=NULL)
				{
					xj_sipmsg_free(jcp->jmqueue.jsm[i]);
					jcp->jmqueue.jsm[i] = NULL;
					xj_jcon_pool_del_jmsg(jcp, i);
				}
				if(jcp->jmqueue.ojc[i]!=NULL)
					xj_jcon_pool_del_jmsg(jcp, i);
				continue;
			}
			if(jcp->jmqueue.expire[i] < get_ticks())
			{
#ifdef XJ_EXTRA_DEBUG
				DBG("XJAB:xj_worker:%d: message to %.*s is expired\n",
					_xj_pid, jcp->jmqueue.jsm[i]->to.len, 
					jcp->jmqueue.jsm[i]->to.s);
#endif
				xj_send_sip_msgz(_PADDR(jwl), jcp->jmqueue.jsm[i]->jkey->id, 
						&jcp->jmqueue.jsm[i]->to, XJ_DMSG_ERR_SENDIM,
						&jcp->jmqueue.ojc[i]->jkey->flag);
				if(jcp->jmqueue.jsm[i]!=NULL)
				{
					xj_sipmsg_free(jcp->jmqueue.jsm[i]);
					jcp->jmqueue.jsm[i] = NULL;
				}
				/** delete message from queue */
				xj_jcon_pool_del_jmsg(jcp, i);
				continue;
			}

#ifdef XJ_EXTRA_DEBUG
			DBG("XJAB:xj_worker:%d:%d: QUEUE: message[%d] from [%.*s]/to [%.*s]/"
					"body[%.*s] expires at %d\n",
					_xj_pid, get_ticks(), i, 
					jcp->jmqueue.jsm[i]->jkey->id->len,
					jcp->jmqueue.jsm[i]->jkey->id->s,
					jcp->jmqueue.jsm[i]->to.len,jcp->jmqueue.jsm[i]->to.s,
					jcp->jmqueue.jsm[i]->msg.len,jcp->jmqueue.jsm[i]->msg.s,
					jcp->jmqueue.expire[i]);
#endif
			if(xj_jcon_is_ready(jcp->jmqueue.ojc[i], jcp->jmqueue.jsm[i]->to.s,
					jcp->jmqueue.jsm[i]->to.len, jwl->aliases->dlm))
				continue;

			/*** address corection ***/
			flag = XJ_ADDRTR_A2B;
			if(!xj_jconf_check_addr(&jcp->jmqueue.jsm[i]->to,jwl->aliases->dlm))
				flag |= XJ_ADDRTR_CON;
			
			sto.s = buff; 
			sto.len = 0;
			if(xj_address_translation(&jcp->jmqueue.jsm[i]->to,
				&sto, jwl->aliases, flag) == 0)
			{
				/** send message from queue */
#ifdef XJ_EXTRA_DEBUG
				DBG("XJAB:xj_worker:%d: SENDING the message from "
					" local queue to Jabber network ...\n", _xj_pid);
#endif
				xj_jcon_send_msg(jcp->jmqueue.ojc[i],
					sto.s, sto.len,
					jcp->jmqueue.jsm[i]->msg.s,
					jcp->jmqueue.jsm[i]->msg.len,
					(flag&XJ_ADDRTR_CON)?XJ_JMSG_GROUPCHAT:XJ_JMSG_CHAT);
			}
			else
				DBG("XJAB:xj_worker:%d: ERROR SENDING the message from "
				" local queue to Jabber network ...\n", _xj_pid);
				
			if(jcp->jmqueue.jsm[i]!=NULL)
			{
				xj_sipmsg_free(jcp->jmqueue.jsm[i]);
				jcp->jmqueue.jsm[i] = NULL;
			}
			/** delete message from queue */
			xj_jcon_pool_del_jmsg(jcp, i);
		} // end MSG queue checking
		
		if(ret <= 0)
			goto step_x;
#ifdef XJ_EXTRA_DEBUG
		DBG("XJAB:xj_worker:%d: something is coming\n", _xj_pid);
#endif
		if(!FD_ISSET(pipe, &mset))
			goto step_y;
		
		if(read(pipe, &jsmsg, sizeof(jsmsg)) < sizeof(jsmsg))
		{
			DBG("XJAB:xj_worker:%d: BROKEN PIPE - exiting\n", _xj_pid);
			break;
		}

#ifdef XJ_EXTRA_DEBUG
		DBG("XJAB:xj_worker:%d: job <%p> from SER\n", _xj_pid, jsmsg);
#endif

		if(jsmsg == NULL || jsmsg->jkey==NULL || jsmsg->jkey->id==NULL)
			goto step_w;

		strncpy(buff, jsmsg->jkey->id->s, jsmsg->jkey->id->len);
		buff[jsmsg->jkey->id->len] = 0;

		jbc = xj_jcon_pool_get(jcp, jsmsg->jkey);
		
		switch(jsmsg->type)
		{
			case XJ_SEND_MESSAGE:
				if(!xj_jconf_check_addr(&jsmsg->to, jwl->aliases->dlm) &&
				(!jbc||!xj_jcon_get_jconf(jbc,&jsmsg->to,jwl->aliases->dlm)))
				{
					xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to,
						XJ_DMSG_ERR_NOTJCONF, NULL);
					goto step_w;
				}
				break;
			case XJ_REG_WATCHER:
			case XJ_JOIN_JCONF:
			case XJ_GO_ONLINE:
				break;
			case XJ_EXIT_JCONF:
				if(jbc == NULL)
					goto step_w;
				// close the conference session here
				if(jbc->nrjconf <= 0)
					goto step_w;
				if(!xj_jconf_check_addr(&jsmsg->to, jwl->aliases->dlm))
					xj_jcon_del_jconf(jbc, &jsmsg->to, jwl->aliases->dlm,
						XJ_JCMD_UNSUBSCRIBE);
				xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to,
					XJ_DMSG_INF_JCONFEXIT, NULL);
				goto step_w;
			case XJ_GO_OFFLINE:
				if(jbc != NULL)
					jbc->expire = ltime = -1;
				goto step_w;
			case XJ_DEL_WATCHER:
			default:
				goto step_w;
		}
		
		if(jbc != NULL)
		{
#ifdef XJ_EXTRA_DEBUG
			DBG("XJAB:xj_worker:%d: connection already exists"
				" for <%s> ...\n", _xj_pid, buff);
#endif
			xj_jcon_update(jbc, jwl->cachet);
			goto step_z;
		}
		
		// NO OPEN CONNECTION FOR THIS SIP ID
#ifdef XJ_EXTRA_DEBUG
		DBG("XJAB:xj_worker:%d: new connection for <%s>.\n", _xj_pid, buff);
#endif		
		if(db_query(db_con, keys, 0, vals, col, 2, 2, NULL, &res) != 0 ||
			RES_ROW_N(res) <= 0)
		{
#ifdef XJ_EXTRA_DEBUG
			DBG("XJAB:xj_worker:%d: no database result when looking"
				" for associated Jabber account\n", _xj_pid);
#endif
			xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to, 
				XJ_DMSG_ERR_JGWFORB, NULL);
			
			goto step_v;
		}
		
		jbc = xj_jcon_init(jaddress, jport);
		
		if(xj_jcon_connect(jbc))
		{
			DBG("XJAB:xj_worker:%d: Cannot connect"
				" to the Jabber server ...\n", _xj_pid);
			xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to, 
				XJ_DMSG_ERR_NOJSRV, NULL);

			goto step_v;
		}
		
#ifdef XJ_EXTRA_DEBUG
		DBG("XJAB:xj_worker: auth to jabber as: [%s] / [%s]\n",
			(char*)(ROW_VALUES(RES_ROWS(res))[0].val.string_val), 
			(char*)(ROW_VALUES(RES_ROWS(res))[1].val.string_val));
#endif		
		if(xj_jcon_user_auth(jbc,
			(char*)(ROW_VALUES(RES_ROWS(res))[0].val.string_val),
			(char*)(ROW_VALUES(RES_ROWS(res))[1].val.string_val),
			XJAB_RESOURCE) < 0)
		{
			DBG("XJAB:xj_worker:%d: Authentication to the Jabber server"
				" failed ...\n", _xj_pid);
			xj_jcon_disconnect(jbc);
			
			xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to, 
					XJ_DMSG_ERR_JAUTH, NULL);
			
			xj_jcon_free(jbc);
			goto step_v;
		}
		
		if(xj_jcon_set_attrs(jbc, jsmsg->jkey, jwl->cachet, jwl->delayt)
			|| xj_jcon_pool_add(jcp, jbc))
		{
			DBG("XJAB:xj_worker:%d: Keeping connection to Jabber server"
				" failed! Not enough memory ...\n", _xj_pid);
			xj_jcon_disconnect(jbc);
			xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to,	
					XJ_DMSG_ERR_JGWFULL, NULL);
			xj_jcon_free(jbc);
			goto step_v;
		}
								
		/** add socket descriptor to select */
#ifdef XJ_EXTRA_DEBUG
		DBG("XJAB:xj_worker:%d: add connection on <%d> \n", _xj_pid, jbc->sock);
#endif
		if(jbc->sock > maxfd)
			maxfd = jbc->sock;
		FD_SET(jbc->sock, &set);
										
		xj_jcon_get_roster(jbc);
		xj_jcon_send_presence(jbc, NULL, NULL, "Online", "9");
		
		/** wait for a while - the worker is tired */
		//sleep(3);
		
		if ((res != NULL) && (db_free_query(db_con,res) < 0))
		{
			DBG("XJAB:xj_worker:%d:Error while freeing"
				" SQL result - worker terminated\n", _xj_pid);
			return -1;
		}
		else
			res = NULL;

		if(jsmsg->type == XJ_GO_ONLINE)
			goto step_w;
		
step_z:
		if(jsmsg->type == XJ_REG_WATCHER)
		{ // register a presence watcher
			if(!xj_jconf_check_addr(&jsmsg->to, jwl->aliases->dlm))
			{ // is for a conference - ignore?!?!
#ifdef XJ_EXTRA_DEBUG
				DBG("XJAB:xj_worker:%d: presence request for a conference.\n",
					_xj_pid);
#endif
				// set as offline
				(*(jsmsg->cbf))(&jsmsg->to, 0, jsmsg->p);
				goto step_w;
			}
			
			sto.s = buff; 
			sto.len = 0;

			if(xj_address_translation(&jsmsg->to, &sto, jwl->aliases, 
					XJ_ADDRTR_A2B) == 0)
			{
				prc = xj_pres_list_check(jbc->plist, &sto);
				if(!prc)
				{
					prc = xj_pres_cell_new();
					if(!prc)
					{
						DBG("XJAB:xj_worker:%d: cannot create a presence"
							" cell for %.*s.\n", _xj_pid, sto.len, sto.s);
						goto step_w;
					}
					if(xj_pres_cell_init(prc, &sto, jsmsg->cbf, jsmsg->p)<0)
					{
						xj_pres_cell_free(prc);
						goto step_w;
					}
					if((prc = xj_pres_list_add(jbc->plist, prc))==NULL)
					{
						DBG("XJAB:xj_worker:%d: cannot add the presence"
							" cell for %.*s.\n", _xj_pid, sto.len, sto.s);
						goto step_w;
					}
					sto.s[sto.len] = 0;
					if(!xj_jcon_send_subscribe(jbc, sto.s, NULL, "subscribe"))
						prc->status = XJ_PRES_STATUS_WAIT; 
				}
				else
					xj_pres_cell_update(prc, jsmsg->cbf, jsmsg->p);
				// send presence info to SIP subscriber
				(*(prc->cbf))(&jsmsg->to, prc->state, prc->cbp);
			}
			goto step_w;
		}
		flag = 0;
		if(!xj_jconf_check_addr(&jsmsg->to, jwl->aliases->dlm))
		{
			if((jcf = xj_jcon_get_jconf(jbc, &jsmsg->to, jwl->aliases->dlm))
					!= NULL)
			{
				if((jsmsg->type == XJ_JOIN_JCONF) &&
					!(jcf->status & XJ_JCONF_READY || 
						jcf->status & XJ_JCONF_WAITING))
				{
					if(!xj_jcon_jconf_presence(jbc,jcf,NULL,"online"))
						jcf->status = XJ_JCONF_WAITING;
					else
					{
						// unable to join the conference 
						// --- send back to SIP user a msg
						xj_send_sip_msgz(_PADDR(jwl),jsmsg->jkey->id,&jsmsg->to,
							XJ_DMSG_ERR_JOINJCONF, &jbc->jkey->flag);
						goto step_w;
					}
				}
				flag |= XJ_ADDRTR_CON;
			}
			else
			{
				// unable to get the conference 
				// --- send back to SIP user a msg
				xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to,
						XJ_DMSG_ERR_NEWJCONF, &jbc->jkey->flag);
				goto step_w;
			}
		}
		if(jsmsg->type == XJ_JOIN_JCONF)
			goto step_w;
		
		// here will come only XJ_SEND_MESSAGE
		switch(xj_jcon_is_ready(jbc,jsmsg->to.s,jsmsg->to.len,jwl->aliases->dlm))
		{
			case 0:
#ifdef XJ_EXTRA_DEBUG
				DBG("XJAB:xj_worker:%d: SENDING the message to Jabber"
					" network ...\n", _xj_pid);
#endif
				/*** address corection ***/
				sto.s = buff; 
				sto.len = 0;
				flag |= XJ_ADDRTR_A2B;
				if(xj_address_translation(&jsmsg->to, &sto, jwl->aliases, 
							flag) == 0)
				{
					if(xj_jcon_send_msg(jbc, sto.s, sto.len,
						jsmsg->msg.s, jsmsg->msg.len,
						(flag&XJ_ADDRTR_CON)?XJ_JMSG_GROUPCHAT:XJ_JMSG_CHAT)<0)
							
						xj_send_sip_msgz(_PADDR(jwl),jsmsg->jkey->id,&jsmsg->to,
							XJ_DMSG_ERR_SENDJMSG, &jbc->jkey->flag);
				}
				else
					DBG("XJAB:xj_worker:%d: ERROR SENDING as Jabber"
						" message ...\n", _xj_pid);
						
				goto step_w;
		
			case 1:
#ifdef XJ_EXTRA_DEBUG
				DBG("XJAB:xj_worker:%d:SCHEDULING the message.\n", _xj_pid);
#endif
				if(xj_jcon_pool_add_jmsg(jcp, jsmsg, jbc) < 0)
				{
					DBG("XJAB:xj_worker:%d: SCHEDULING the message FAILED."
							" Message was dropped.\n",_xj_pid);
					xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to,
						XJ_DMSG_ERR_STOREJMSG, &jbc->jkey->flag);
					goto step_w;
				}
				else // skip freeing the SIP message - now is in queue
					goto step_y;
	
			case 2:
				xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to,
						XJ_DMSG_ERR_NOREGIM, &jbc->jkey->flag);
				goto step_w;
			case 3: // not joined to Jabber conference
				xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to,
						XJ_DMSG_ERR_NOTJCONF, &jbc->jkey->flag);
				goto step_w;
				
			default:
				xj_send_sip_msgz(_PADDR(jwl), jsmsg->jkey->id, &jsmsg->to,
						XJ_DMSG_ERR_SENDJMSG, &jbc->jkey->flag);
				goto step_w;
		}

step_v: // error connecting to Jabber server
		
		// cleaning jab_wlist
		xj_wlist_del(jwl, jsmsg->jkey, _xj_pid);

		// cleaning db_query
		if ((res != NULL) && (db_free_query(db_con,res) < 0))
		{
			DBG("XJAB:xj_worker:%d:Error while freeing"
				" SQL result - worker terminated\n", _xj_pid);
			return -1;
		}
		else
			res = NULL;

step_w:
		if(jsmsg!=NULL)
		{
			xj_sipmsg_free(jsmsg);
			jsmsg = NULL;
		}			

step_y:			 
		// check for new message from ... JABBER
		for(i = 0; i < jcp->len && main_loop; i++)
		{
			if(jcp->ojc[i] == NULL)
				continue;
#ifdef XJ_EXTRA_DEBUG
			DBG("XJAB:xj_worker:%d: checking socket <%d>"
				" ...\n", _xj_pid, jcp->ojc[i]->sock);
#endif
			if(!FD_ISSET(jcp->ojc[i]->sock, &mset))
				continue;
			pos = nr = 0;
			do
			{
				p = recv_buff;
				if(pos != 0)
				{
					while(pos < nr)
					{
						*p = recv_buff[pos];
						pos++;
						p++;
					}
					*p = 0;
					/**
					 * flush out the socket - set it to nonblocking 
					 */
 					flags = fcntl(jcp->ojc[i]->sock, F_GETFL, 0);
					if(flags!=-1 && !(flags & O_NONBLOCK))
   					{
    					/* set NONBLOCK bit to enable non-blocking */
    					fcntl(jcp->ojc[i]->sock, F_SETFL, flags|O_NONBLOCK);
   					}
				}
				
				if((nr = read(jcp->ojc[i]->sock, p,	
						sizeof(recv_buff)-(p-recv_buff))) == 0
					||(nr < 0 && errno != EAGAIN))
				{
					DBG("XJAB:xj_worker:%d: ERROR -"
						" connection to jabber lost on socket <%d> ...\n",
						_xj_pid, jcp->ojc[i]->sock);
					xj_send_sip_msgz(_PADDR(jwl), jcp->ojc[i]->jkey->id,
						&jab_gw_name,XJ_DMSG_ERR_DISCONNECTED,&jbc->jkey->flag);
					// make sure that will ckeck expired connections
					ltime = jcp->ojc[i]->expire = -1;
					FD_CLR(jcp->ojc[i]->sock, &set);
					goto step_xx;
				}
#ifdef XJ_EXTRA_DEBUG
				DBG("XJAB:xj_worker:%d: received: %dbytes Err:%d/EA:%d\n", 
						_xj_pid, nr, errno, EAGAIN);
#endif
				xj_jcon_update(jcp->ojc[i], jwl->cachet);

				if(nr>0)
					p[nr] = 0;
				nr = strlen(recv_buff);
				pos = 0;
#ifdef XJ_EXTRA_DEBUG
				DBG("XJAB:xj_worker: JMSG START ----------\n%.*s\n"
					" JABBER: JMSGL:%d END ----------\n", nr, recv_buff, nr);
#endif
			} while(xj_manage_jab(recv_buff, nr, &pos, jwl->aliases,
							jcp->ojc[i]) == 9	&& main_loop);
	
			/**
			 * flush out the socket - set it back to blocking 
			 */
 			flags = fcntl(jcp->ojc[i]->sock, F_GETFL, 0);
			if(flags!=-1 && (flags & O_NONBLOCK))
   			{
    			/* reset NONBLOCK bit to enable blocking */
    			fcntl(jcp->ojc[i]->sock, F_SETFL, flags & ~O_NONBLOCK);
   			}
#ifdef XJ_EXTRA_DEBUG
			DBG("XJAB:xj_worker:%d: msgs from socket <%d> parsed"
				" ...\n", _xj_pid, jcp->ojc[i]->sock);	
#endif
		} // end FOR(i = 0; i < jcp->len; i++)

step_x:
		if(ret < 0)
		{
			DBG("XJAB:xj_worker:%d: SIGNAL received!!!!!!!!\n", _xj_pid);
			maxfd = pipe;
			FD_ZERO(&set);
			FD_SET(pipe, &set);
			for(i = 0; i < jcp->len; i++)
			{
				if(jcp->ojc[i] != NULL)
				{
					FD_SET(jcp->ojc[i]->sock, &set);
					if( jcp->ojc[i]->sock > maxfd )
						maxfd = jcp->ojc[i]->sock;
				}
			}
		}
step_xx:
		if(ltime < 0 || ltime + jwl->sleept <= get_ticks())
		{
			ltime = get_ticks();
#ifdef XJ_EXTRA_DEBUG
			//DBG("XJAB:xj_worker:%d: scanning for expired connection\n",
			//	_xj_pid);
#endif
			xj_worker_check_jcons(jwl, jcp, ltime, &set);
		}
	} // END while

	DBG("XJAB:xj_worker:%d: cleaning procedure\n", _xj_pid);

	return 0;
} // end xj_worker_process

/**
 * parse incoming message from Jabber server
 */
int xj_manage_jab(char *buf, int len, int *pos, xj_jalias als, xj_jcon jbc)
{
	int i, err=0;
	char *p, *to, *from, *msg, *type, *emsg, *ecode, lbuf[4096], fbuf[128];
	xj_jconf jcf = NULL;
	str ts, tf;
	xode x, y, z;
	str *sid;
	xj_pres_cell prc = NULL;

	if(!jbc)
		return -1;

	sid = jbc->jkey->id;	
	x = xode_from_strx(buf, len, &err, &i);
#ifdef XJ_EXTRA_DEBUG
	DBG("XJAB:xj_parse_jab: XODE ret:%d pos:%d\n", err, i);
#endif	
	if(err && pos != NULL)
		*pos= i;
	if(x == NULL)
		return -1;
	
	lbuf[0] = 0;
	ecode = NULL;
	
	if(!strncasecmp(xode_get_name(x), "message", 7))
	{
#ifdef XJ_EXTRA_DEBUG
		DBG("XJAB:xj_manage_jab: jabber [message] received\n");
#endif
		if((to = xode_get_attrib(x, "to")) == NULL)
		{
#ifdef XJ_EXTRA_DEBUG
			DBG("XJAB:xj_manage_jab: missing 'to' attribute\n");
#endif
			err = -1;
			goto ready;
		}
		if((from = xode_get_attrib(x, "from")) == NULL)
		{
#ifdef XJ_EXTRA_DEBUG
			DBG("XJAB:xj_manage_jab: missing 'from' attribute\n");
#endif
			err = -1;
			goto ready;
		}
		if((y = xode_get_tag(x, "body")) == NULL
				|| (msg = xode_get_data(y)) == NULL)
		{
#ifdef XJ_EXTRA_DEBUG
			DBG("XJAB:xj_manage_jab: missing 'body' of message\n");
#endif
			err = -1;
			goto ready;
		}
		type = xode_get_attrib(x, "type");
		if(type != NULL && !strncasecmp(type, "error", 5))
		{
			if((y = xode_get_tag(x, "error")) == NULL
					|| (emsg = xode_get_data(y)) == NULL)
				strcpy(lbuf, "{Error sending following message} - ");
			else
			{
				ecode = xode_get_attrib(y, "code");
				strcpy(lbuf, "{Error (");
				if(ecode != NULL)
				{
					strcat(lbuf, ecode);
					strcat(lbuf, " - ");
				}
				strcat(lbuf, emsg);
				strcat(lbuf, ") when trying to send following messge}");
			}

		}

		// is from a conferece?!?!
		if((jcf=xj_jcon_check_jconf(jbc, from))!=NULL)
		{
			if(lbuf[0] == 0)
			{
				p = from + strlen(from);
				while(p>from && *p != '/')
					p--;
				if(*p == '/')
				{
					if(jcf->nick.len>0 
						&& !strncasecmp(p+1, jcf->nick.s, jcf->nick.len))
						goto ready;
					lbuf[0] = '[';
					lbuf[1] = 0;
					strcat(lbuf, p+1);
					strcat(lbuf, "] ");
				}
			}
			else
			{
				jcf->status = XJ_JCONF_NULL;
				xj_jcon_jconf_presence(jbc,jcf,NULL,"online");
			}
			strcat(lbuf, msg);
			ts.s = lbuf;
			ts.len = strlen(lbuf);
	
			if(xj_send_sip_msg(als->proxy, sid, &jcf->uri, &ts,
						&jbc->jkey->flag)<0)
				DBG("XJAB:xj_manage_jab: ERROR SIP MESSAGE was not sent!\n");
#ifdef XJ_EXTRA_DEBUG
			else
				DBG("XJAB:xj_manage_jab: SIP MESSAGE was sent!\n");
#endif
			goto ready;
		}

		strcat(lbuf, msg);
		ts.s = from;
		ts.len = strlen(from);
		tf.s = fbuf;
		tf.len = 0;
		if(xj_address_translation(&ts, &tf, als, XJ_ADDRTR_B2A) == 0)
		{
			ts.s = lbuf;
			ts.len = strlen(lbuf);
	
			if(xj_send_sip_msg(als->proxy, sid, &tf, &ts, &jbc->jkey->flag)<0)
				DBG("XJAB:xj_manage_jab: ERROR SIP MESSAGE was not sent ...\n");
#ifdef XJ_EXTRA_DEBUG
			else
				DBG("XJAB:xj_manage_jab: SIP MESSAGE was sent.\n");
#endif
		}
		goto ready;
	} // end MESSAGE
	
	/*** PRESENCE HANDLING ***/
	if(!strncasecmp(xode_get_name(x), "presence", 8))
	{
#ifdef XJ_EXTRA_DEBUG
		DBG("XJAB:xj_manage_jab: jabber [presence] received\n");
#endif
		type = xode_get_attrib(x, "type");
		from = xode_get_attrib(x, "from");
		if(from == NULL)
			goto ready;
		ts.s = from;
		p = from;
		while(p<from + strlen(from) && *p != '/')
					p++;
		if(*p == '/')
			ts.len = p - from;
		else
			ts.len = strlen(from);
		if(type!=NULL && !strncasecmp(type, "error", 5))
		{
			if((jcf=xj_jcon_check_jconf(jbc, from))!=NULL)
			{
				tf.s = from;
				tf.len = strlen(from);
				if((y = xode_get_tag(x, "error")) == NULL)
					goto ready;
				if ((p = xode_get_attrib(y, "code")) != NULL
						&& atoi(p) == 409)
				{
					xj_send_sip_msgz(als->proxy, sid, &tf,
							XJ_DMSG_ERR_JCONFNICK, &jbc->jkey->flag);
					goto ready;
				}
				xj_send_sip_msgz(als->proxy,sid,&tf,XJ_DMSG_ERR_JCONFREFUSED,
						&jbc->jkey->flag);
			}

			goto ready;
		}
		if(type!=NULL && !strncasecmp(type, "subscribe", 9))
		{
			xj_jcon_send_presence(jbc, from, "subscribed", NULL, NULL);
			goto ready;
		}
		if(type!=NULL && !strncasecmp(type, "unavailable", 11))
		{
#ifdef XJ_EXTRA_DEBUG
			DBG("XJAB:xj_manage_jab: user <%s> is offline\n", from);
#endif
			prc = xj_pres_list_check(jbc->plist, &ts);
			if(prc)
			{
				prc->state = XJ_PRES_STATE_OFFLINE;
				// call callback function
				if(prc->cbf)
				{
					tf.s = fbuf;
					tf.len = 0;
					if(xj_address_translation(&ts,&tf,als,XJ_ADDRTR_B2A)==0)
					{
#ifdef XJ_EXTRA_DEBUG
						DBG("XJAB:xj_manage_jab: calling CBF(%.*s,0)\n",
							tf.len, tf.s);
#endif
						(*(prc->cbf))(&tf, prc->state, prc->cbp);
					}
				}
			}
			goto ready;
		}
		if(type == NULL || !strncasecmp(type, "online", 6)
			|| !strncasecmp(type, "available", 9))
		{
			if(strchr(from, '@') == NULL)
			{
				if(!strncasecmp(from, XJ_AIM_NAME, XJ_AIM_LEN))
				{
					jbc->ready |= XJ_NET_AIM;
#ifdef XJ_EXTRA_DEBUG
					DBG("XJAB:xj_manage_jab: AIM network ready\n");
#endif
				}
				else if(!strncasecmp(from, XJ_ICQ_NAME, XJ_ICQ_LEN))
				{
					jbc->ready |= XJ_NET_ICQ;
#ifdef XJ_EXTRA_DEBUG
					DBG("XJAB:xj_manage_jab: ICQ network ready\n");
#endif
				}
				else if(!strncasecmp(from, XJ_MSN_NAME, XJ_MSN_LEN))
				{
					jbc->ready |= XJ_NET_MSN;
#ifdef XJ_EXTRA_DEBUG
					DBG("XJAB:xj_manage_jab: MSN network ready\n");
#endif
				}
				else if(!strncasecmp(from, XJ_YAH_NAME, XJ_YAH_LEN))
				{
					jbc->ready |= XJ_NET_YAH;
#ifdef XJ_EXTRA_DEBUG
					DBG("XJAB:xj_manage_jab: YAHOO network ready\n");
#endif
				}
			}
			else if((jcf=xj_jcon_check_jconf(jbc, from))!=NULL)
			{
				jcf->status = XJ_JCONF_READY;
#ifdef XJ_EXTRA_DEBUG
				DBG("XJAB:xj_manage_jab: %s conference ready\n", from);
#endif
			}
			else
			{
#ifdef XJ_EXTRA_DEBUG
				DBG("XJAB:xj_manage_jab: user <%s> is online\n", from);
#endif
				prc = xj_pres_list_check(jbc->plist, &ts);
				if(prc)
				{
					prc->state = XJ_PRES_STATE_ONLINE;
					// call callback function
					if(prc->cbf)
					{
						tf.s = fbuf;
						tf.len = 0;
						if(xj_address_translation(&ts,&tf,als,XJ_ADDRTR_B2A)
							==0)
						{
#ifdef XJ_EXTRA_DEBUG
							DBG("XJAB:xj_manage_jab: calling CBF(%.*s,1)\n",
								tf.len, tf.s);
#endif
							(*(prc->cbf))(&tf, prc->state, prc->cbp);
						}
					}
				}
				else
				{
#ifdef XJ_EXTRA_DEBUG
					DBG("XJAB:xj_manage_jab: user state received - creating"
						" presence cell for [%.*s]\n", ts.len, ts.s);
#endif
					prc = xj_pres_cell_new();
					if(prc == NULL)
					{
						DBG("XJAB:xj_manage_jab: cannot create presence"
							" cell for [%s]\n", from);
						goto ready;
					}
					if(xj_pres_cell_init(prc, &ts, NULL, NULL)<0)
					{
						DBG("XJAB:xj_manage_jab: cannot init presence"
							" cell for [%s]\n", from);
						xj_pres_cell_free(prc);
						goto ready;
					}
					prc = xj_pres_list_add(jbc->plist, prc);
					if(prc)
						prc->state = XJ_PRES_STATE_ONLINE;
				}
			}

		}
		
		goto ready;
	} // end PRESENCE
	
	if(!strncasecmp(xode_get_name(x), "iq", 2))
	{
#ifdef XJ_EXTRA_DEBUG
		DBG("XJAB:xj_manage_jab: jabber [iq] received\n");
#endif
		if(!strncasecmp(xode_get_attrib(x, "type"), "result", 6))
		{
			if((y = xode_get_tag(x, "query?xmlns=jabber:iq:roster")) == NULL)
				goto ready;
			z = xode_get_firstchild(y);
			while(z)
			{
				if(!strncasecmp(xode_get_name(z), "item", 5)
					&& (from = xode_get_attrib(z, "jid")) != NULL)
				{
					if(strchr(from, '@') == NULL)
					{ // transports
						if(!strncasecmp(from, XJ_AIM_NAME, XJ_AIM_LEN))
						{
							jbc->allowed |= XJ_NET_AIM;
#ifdef XJ_EXTRA_DEBUG
							DBG("XJAB:xj_manage_jab:AIM network available\n");
#endif
						}
						else if(!strncasecmp(from, XJ_ICQ_NAME, XJ_ICQ_LEN))
						{
							jbc->allowed |= XJ_NET_ICQ;
#ifdef XJ_EXTRA_DEBUG
							DBG("XJAB:xj_manage_jab:ICQ network available\n");
#endif
						}
						else if(!strncasecmp(from, XJ_MSN_NAME, XJ_MSN_LEN))
						{
							jbc->allowed |= XJ_NET_MSN;
#ifdef XJ_EXTRA_DEBUG
							DBG("XJAB:xj_manage_jab:MSN network available\n");
#endif
						}
						else if(!strncasecmp(from, XJ_YAH_NAME, XJ_YAH_LEN))
						{
							jbc->allowed |= XJ_NET_YAH;
#ifdef XJ_EXTRA_DEBUG
							DBG("XJAB:xj_manage_jab:YAHOO network available\n");
#endif
						}
						goto next_sibling;
					}
					/*** else
					{ // user item
						ts.s = from;
						ts.len = strlen(from);
						DBG("XJAB:xj_manage_jab:%s is in roster\n", from);
						if(xj_pres_list_check(jbc->plist, &ts))
						{
							DBG("XJAB:xj_manage_jab:%s already in presence"
								" list\n", from);
							goto next_sibling;
						}

						prc = xj_pres_cell_new();
						if(prc == NULL)
						{
							DBG("XJAB:xj_manage_jab: cannot create presence"
								" cell for %s\n", from);
							goto next_sibling;
						}
						if(xj_pres_cell_init(prc, &ts, NULL, NULL)<0)
						{
							DBG("XJAB:xj_manage_jab: cannot init presence"
								" cell for %s\n", from);
							xj_pres_cell_free(prc);
							goto next_sibling;
						}
						prc = xj_pres_list_add(jbc->plist, prc);
						if(prc)
						{
							p = xode_get_attrib(z, "subscription");
							if(p && !strncasecmp(p, "none", 4))
							{
								DBG("XJAB:xj_manage_jab: wait for permission"
									" from %s\n", from);
								prc->status = XJ_PRES_STATUS_WAIT; 
							}
							else
							{
								DBG("XJAB:xj_manage_jab: permission granted"
									" from %s\n", from);
								prc->status = XJ_PRES_STATUS_SUBS;
							}
						}
					} 
					***/
				}
next_sibling:
				z = xode_get_nextsibling(z);
			}
		}
		
		goto ready;
	} // end IQ

ready:
	xode_free(x);
	return err;
}

/**
 *
 */
void xj_sig_handler(int s) 
{
	//signal(SIGTERM, xj_sig_handler);
	//signal(SIGINT, xj_sig_handler);
	//signal(SIGQUIT, xj_sig_handler);
	signal(SIGSEGV, xj_sig_handler);
	main_loop = 0;
	DBG("XJAB:xj_worker:%d: SIGNAL received=%d\n **************", _xj_pid, s);
}

/*****************************     ****************************************/

/**
 * send a SIP MESSAGE message
 * - to : destination
 * - from : origin
 * - contact : contact header
 * - msg : body of the message
 * #return : 0 on success or <0 on error
 */
int xj_send_sip_msg(str *proxy, str *to, str *from, str *msg, int *cbp)
{
	str  msg_type = { "MESSAGE", 7};
	char buf[512];
	str  tfrom;
	str  str_hdr;
	int **pcbp = NULL;//, beg, end, crt;
	char buf1[1024];

	if( !to || !to->s || to->len <= 0 
			|| !from || !from->s || from->len <= 0 
			|| !msg || !msg->s || msg->len <= 0
			|| (cbp && *cbp!=0) )
		return -1;

	// from correction
	/****
	beg = crt = 0;
	end = -1;
	while(crt < from->len && from->s[crt]!='@')
	{
		if(from->s[crt]==delim)
		{
			beg = end + 1;
			end = crt;
		}
		crt++;
	}
	***/
	tfrom.len = 0;
	/***
	if(end > 0)
	{ // put display name
		buf[0] = '"';
		strncpy(buf+1,from->s+beg,end-beg);
		tfrom.len = end-beg+1;
		buf[tfrom.len++] = '"';
		buf[tfrom.len++] = ' ';
	}
	***/
	strncpy(buf+tfrom.len, "<sip:", 5);
	tfrom.len += 5;
	strncpy(buf+tfrom.len, from->s, from->len);
	tfrom.len += from->len;
	buf[tfrom.len++] = '>';
		
	tfrom.s = buf;
	
	// building Contact and Content-Type
	strcpy(buf1,"Content-Type: text/plain"CRLF"Contact: ");
	str_hdr.len = 24 + CRLF_LEN + 9;
	
	strncat(buf1,tfrom.s,tfrom.len);
	str_hdr.len += tfrom.len;
	//strncat(buf1,"sip:193.175.135.68:5060",23);
	//str_hdr.len += 23;
	
	strcat(buf1, CRLF);
	str_hdr.len += CRLF_LEN;
	str_hdr.s = buf1;
	if(cbp)
	{
#ifdef XJ_EXTRA_DEBUG
		DBG("XJAB:xj_send_sip_msg: uac callback parameter [%p==%d]\n", 
				cbp, *cbp);
#endif
		if((pcbp = (int**)shm_malloc(sizeof(int*))) == NULL)
			return -1;
		*pcbp = cbp;
		//return tmb.t_uac( &msg_type, to, &str_hdr , msg, &tfrom,
		//			xj_tuac_callback, (void*)pcbp, 0);
		return tmb.t_uac_dlg(&msg_type, /* Type of the message */
					proxy,              /* Real destination */
					to,                 /* Request-URI */
					to,                 /* To */
					&tfrom,             /* From */
					NULL,               /* To tag */
					NULL,               /* From tag */
					NULL,               /* CSeq */
					NULL,               /* Call-ID */
					&str_hdr,           /* Optional headers including CRLF */
					msg,                /* Message body */
					xj_tuac_callback,   /* Callback function */
					(void*)pcbp         /* Callback parameter */
			);
	}
	else
		//return tmb.t_uac( &msg_type, to, &str_hdr , msg, &tfrom, 0 , 0, 0);
		return tmb.t_uac_dlg(&msg_type, /* Type of the message */
					proxy,              /* Real destination */
					to,                 /* Request-URI */
					to,                 /* To */
					&tfrom,             /* From */
					NULL,               /* To tag */
					NULL,               /* From tag */
					NULL,               /* CSeq */
					NULL,               /* Call-ID */
					&str_hdr,           /* Optional headers including CRLF */
					msg,                /* Message body */
					0,                  /* Callback function */
					0                   /* Callback parameter */
			);
}

/**
 * send a SIP MESSAGE message
 * - to : destination
 * - from : origin
 * - contact : contact header
 * - msg : body of the message, string terminated by zero
 * #return : 0 on success or <0 on error
 */
int xj_send_sip_msgz(str *proxy, str *to, str *from, char *msg, int *cbp)
{
	str tstr;
	int n;

	if(!to || !from || !msg || (cbp && *cbp!=0))
		return -1;

	tstr.s = msg;
	tstr.len = strlen(msg);
	if((n = xj_send_sip_msg(proxy, to, from, &tstr, cbp)) < 0)
		DBG("JABBER: jab_send_sip_msgz: ERROR SIP MESSAGE wasn't sent to"
			" [%.*s]...\n", tstr.len, tstr.s);
#ifdef XJ_EXTRA_DEBUG
	else
		DBG("JABBER: jab_send_sip_msgz: SIP MESSAGE was sent to [%.*s]...\n",
			to->len, to->s);
#endif
	return n;
}

/**
 * send disconnected info to all SIP users associated with worker idx
 * and clean the entries from wlist
 */
int xj_wlist_clean_jobs(xj_wlist jwl, int idx, int fl)
{
	xj_jkey p;
	if(jwl==NULL || idx < 0 || idx >= jwl->len || !jwl->workers[idx].sip_ids)
		return -1;
	s_lock_at(jwl->sems, idx);
	while((p=(xj_jkey)delpos234(jwl->workers[idx].sip_ids, 0))!=NULL)
	{
		if(fl)
		{
#ifdef XJ_EXTRA_DEBUG
			DBG("XJAB:xj_wlist_send_info: sending disconnect message"
				" to <%.*s>\n",	p->id->len, p->id->s);
#endif
			xj_send_sip_msgz(_PADDR(jwl), p->id, &jab_gw_name,
					XJ_DMSG_INF_DISCONNECTED, NULL);
		}
		jwl->workers[idx].nr--;
		xj_jkey_free_p(p);
	}
	s_unlock_at(jwl->sems, idx);
	return 0;
}


/**
 * callback function for TM
 */
void xj_tuac_callback( struct cell *t, struct sip_msg *msg,
			int code, void *param)
{
#ifdef XJ_EXTRA_DEBUG
	DBG("XJAB: xj_tuac_callback: completed with status %d\n", code);
#endif
	if(!t->cbp)
	{
		DBG("XJAB: m_tuac_callback: parameter not received\n");
		return;
	}
#ifdef XJ_EXTRA_DEBUG
	DBG("XJAB: xj_tuac_callback: parameter [%p : ex-value=%d]\n", t->cbp,
					*(*((int**)t->cbp)) );
#endif
	if(code < 200 || code >= 300)
	{
#ifdef XJ_EXTRA_DEBUG
		DBG("XJAB: xj_tuac_callback: no 2XX return code - connection set"
			" as expired \n");
#endif
		*(*((int**)t->cbp)) = 1;	
	}
}

/**
 * check for expired conections
 */
void xj_worker_check_jcons(xj_wlist jwl, xj_jcon_pool jcp, int ltime, fd_set *pset)
{
	int i;
	xj_jconf jcf;
	
	for(i = 0; i < jcp->len && main_loop; i++)
	{
		if(jcp->ojc[i] == NULL)
			continue;
		if(jcp->ojc[i]->jkey->flag==0 &&
			jcp->ojc[i]->expire > ltime)
			continue;
			
#ifdef XJ_EXTRA_DEBUG
		DBG("XJAB:xj_worker:%d: connection expired for <%.*s> \n",
			_xj_pid, jcp->ojc[i]->jkey->id->len, jcp->ojc[i]->jkey->id->s);
#endif
		xj_send_sip_msgz(_PADDR(jwl), jcp->ojc[i]->jkey->id, &jab_gw_name,
				XJ_DMSG_INF_JOFFLINE, NULL);
#ifdef XJ_EXTRA_DEBUG
		DBG("XJAB:xj_worker:%d: connection's close flag =%d\n",
			_xj_pid, jcp->ojc[i]->jkey->flag);
#endif
		// CLEAN JAB_WLIST
		xj_wlist_del(jwl, jcp->ojc[i]->jkey, _xj_pid);

		// looking for open conference rooms
#ifdef XJ_EXTRA_DEBUG
		DBG("XJAB:xj_worker:%d: having %d open conferences\n", 
				_xj_pid, jcp->ojc[i]->nrjconf);
#endif
		while(jcp->ojc[i]->nrjconf > 0)
		{
			if((jcf=delpos234(jcp->ojc[i]->jconf,0))!=NULL)
			{
				// get out of room
				xj_jcon_jconf_presence(jcp->ojc[i],jcf, "unavailable", NULL);
				xj_jconf_free(jcf);
			}
			jcp->ojc[i]->nrjconf--;
		}

		// send offline presence to all subscribers
		if(jcp->ojc[i]->expire > 0 && jcp->ojc[i]->plist)
		{
#ifdef XJ_EXTRA_DEBUG
			DBG("XJAB:xj_worker:%d: sending offline status to SIP"
					"subscriber\n", _xj_pid);
#endif
			xj_pres_list_notifyall(jcp->ojc[i]->plist,
					XJ_PRES_STATE_OFFLINE);
		}
		FD_CLR(jcp->ojc[i]->sock, pset);
		xj_jcon_disconnect(jcp->ojc[i]);
		xj_jcon_free(jcp->ojc[i]);
		jcp->ojc[i] = NULL;
	}
}

/*****************************     ****************************************/

