/*
 * $Id$
 *
 * message printing
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
 *
 *
 * History:
 * ----------
 * 2003-02-28 scratchpad compatibility abandoned (jiri)
 * 2003-01-27  next baby-step to removing ZT - PRESERVE_ZT (jiri)
 * 2003-02-13  build_uac_request uses proto (andrei)
 */

#include "defs.h"


#include "../../comp_defs.h"
#include "../../hash_func.h"
#include "../../globals.h"
#include "t_funcs.h"
#include "../../dprint.h"
#include "../../config.h"
#include "../../parser/parser_f.h"
#include "../../ut.h"
#include "../../parser/msg_parser.h"
#include "t_msgbuilder.h"
#include "uac.h"



#define  append_mem_block(_d,_s,_len) \
		do{\
			memcpy((_d),(_s),(_len));\
			(_d) += (_len);\
		}while(0);

#define append_str(_p,_str) \
	do{  \
		memcpy((_p), (_str).s, (_str).len); \
		(_p)+=(_str).len;  \
 	} while(0);

/* Build a local request based on a previous request; main
   customers of this function are local ACK and local CANCEL
 */
char *build_local(struct cell *Trans,unsigned int branch,
	unsigned int *len, char *method, int method_len, str *to)
{
	char                *cancel_buf, *p, *via;
	unsigned int         via_len;
	struct hdr_field    *hdr;
	char branch_buf[MAX_BRANCH_PARAM_LEN];
	int branch_len;
	str branch_str;

	if ( Trans->uac[branch].last_received<100)
	{
		DBG("DEBUG: build_local: no response ever received"
			" : dropping local request! \n");
		goto error;
	}

	/* method, separators, version: "CANCEL sip:p2@iptel.org SIP/2.0" */
	*len=SIP_VERSION_LEN + method_len + 2 /* spaces */ + CRLF_LEN;
	*len+=Trans->uac[branch].uri.len;

	/*via*/
	if (!t_calc_branch(Trans,  branch, 
		branch_buf, &branch_len ))
		goto error;
	branch_str.s=branch_buf;
	branch_str.len=branch_len;
	via=via_builder(&via_len, Trans->uac[branch].request.dst.send_sock,
		&branch_str, 0, Trans->uac[branch].request.dst.proto );
	if (!via)
	{
		LOG(L_ERR, "ERROR: t_build_and_send_CANCEL: "
			"no via header got from builder\n");
		goto error;
	}
	*len+= via_len;
	/*headers*/
	*len+=Trans->from.len+Trans->callid.len+to->len+
		+Trans->cseq_n.len+1+method_len+CRLF_LEN; 


	/* copy'n'paste Route headers */
	if (!Trans->local) {
		for ( hdr=Trans->uas.request->headers ; hdr ; hdr=hdr->next )
			 if (hdr->type==HDR_ROUTE)
				*len+=hdr->len;
	}

	/* User Agent */
	if (server_signature) {
		*len += USER_AGENT_LEN + CRLF_LEN;
	}
	/* Content Length, EoM */
	*len+=CONTENT_LENGTH_LEN+1 + CRLF_LEN + CRLF_LEN;

	cancel_buf=shm_malloc( *len+1 );
	if (!cancel_buf)
	{
		LOG(L_ERR, "ERROR: t_build_and_send_CANCEL: cannot allocate memory\n");
		goto error01;
	}
	p = cancel_buf;

	append_mem_block( p, method, method_len );
	append_mem_block( p, " ", 1 );
	append_str( p, Trans->uac[branch].uri );
	append_mem_block( p, " " SIP_VERSION CRLF, 1+SIP_VERSION_LEN+CRLF_LEN );

	/* insert our via */
	append_mem_block(p,via,via_len);

	/*other headers*/
	append_str( p, Trans->from );
	append_str( p, Trans->callid );
	append_str( p, *to );

	append_str( p, Trans->cseq_n );
	append_mem_block( p, " ", 1 );
	append_mem_block( p, method, method_len );
	append_mem_block( p, CRLF, CRLF_LEN );

	if (!Trans->local)  {
		for ( hdr=Trans->uas.request->headers ; hdr ; hdr=hdr->next )
			if(hdr->type==HDR_ROUTE) {
				append_mem_block(p, hdr->name.s, hdr->len );
			}
	}

	/* User Agent header */
	if (server_signature) {
		append_mem_block(p,USER_AGENT CRLF, USER_AGENT_LEN+CRLF_LEN );
	}
	/* Content Length, EoM */
	append_mem_block(p, CONTENT_LENGTH "0" CRLF CRLF ,
		CONTENT_LENGTH_LEN+1 + CRLF_LEN + CRLF_LEN);
	*p=0;

	pkg_free(via);
	return cancel_buf;
error01:
	pkg_free(via);
error:
	return NULL;
}



char *build_uac_request_dlg(str* msg,           /* Method */
			    str* ruri,          /* Request-URI */
			    str* to,            /* To */
			    str* from,          /* From */
			    str* totag,         /* To header tag */
			    str* fromtag,       /* From header tag */
			    unsigned int cseq,  /* CSeq number */
			    str* callid,        /* Call-ID */
			    str* headers,       /* Headers to be appended */
			    str* body,          /* Body of the message */
			    int branch,         /* Branch */
			    struct cell *t,     
			    unsigned int *len,
				struct socket_info* send_sock
				)
{
	char *via, *buf, *w, content_len[10], cseq_str[10], branch_buf[MAX_BRANCH_PARAM_LEN];
	int content_len_len, cseq_str_len, branch_len;
	str branch_str;
	unsigned int via_len;

	buf=0;
	content_len_len = 0; /* Makes gcc happy */

	     /* 
	      * Print Content-Length
	      */
	if (body) {
		content_len_len = snprintf(content_len, sizeof(content_len), "%d", body->len);
		if (content_len_len == -1 || content_len_len>=sizeof(content_len)) {
			LOG(L_ERR, "ERROR: build_uac_request_dlg: content_len too big\n");
			return 0;
		}
	}
	
	     /* 
	      * Print CSeq 
	      */
	cseq_str_len = snprintf(cseq_str, sizeof(cseq_str), "%d", cseq);
	if (cseq_str_len == -1 || cseq_str_len >= sizeof(cseq_str)) {
		LOG(L_ERR, "ERROR: build_uac_request_dlg: cseq too big\n");
		return 0;
	}
	
	*len = msg->len + 1 + ruri->len + 1 + SIP_VERSION_LEN + CRLF_LEN;

	if (!t_calc_branch(t, branch, branch_buf, &branch_len)) {
		LOG(L_ERR, "ERROR: build_uac_request_dlg: branch calculation failed\n");
		goto error;
	}
	
	branch_str.s=branch_buf;
	branch_str.len=branch_len;
	via = via_builder(&via_len, send_sock,
			&branch_str, 0, send_sock->proto);
	if (!via) {
		LOG(L_ERR, "ERROR: build_uac_request_dlg: via building failed\n");
		goto error;
	}
	
	*len += via_len;
	
	/* header names and separators */
	*len +=   TO_LEN + CRLF_LEN
		+ FROM_LEN + CRLF_LEN
		+ CSEQ_LEN + CRLF_LEN
		+ CALLID_LEN + CRLF_LEN
		+ ((body) ? (CONTENT_LENGTH_LEN + CRLF_LEN) : 0)
		+ (server_signature ? USER_AGENT_LEN + CRLF_LEN : 0)
		+ CRLF_LEN; /* EoM */
	
	     /* header field value and body length */
	*len +=   to->len + 
			((totag && totag->len) ? (TOTAG_LEN + totag->len) : 0) /* To */
		+ from->len +  /* From */
			((fromtag && fromtag->len) ? FROMTAG_LEN + fromtag->len:0)
		+ cseq_str_len + 1 + msg->len                        /* CSeq */
		+ callid->len                                        /* Call-ID */
		+ ((body) ? (content_len_len) : 0)                   /* Content-Length */
		+ ((headers) ? (headers->len) : 0)                   /* Headers */
		+ ((body) ? (body->len) : 0);                        /* Body */
	
	buf = shm_malloc(*len + 1);
	if (!buf) {
		LOG(L_ERR, "ERROR: build_uac_request_dlg: no shmem\n");
		goto error1;
	}
	
	w = buf;

	     /* First line */
	memapp(w, msg->s, msg->len); 
	memapp(w, " ", 1); 

	t->uac[branch].uri.s = w; 
	t->uac[branch].uri.len = ruri->len;

	memapp(w, ruri->s, ruri->len); 
	memapp(w, " " SIP_VERSION CRLF, 1 + SIP_VERSION_LEN + CRLF_LEN);

	     /* First Via */
	memapp(w, via, via_len);

	     /* To */
	t->to.s = w;
	t->to.len= TO_LEN+to->len+CRLF_LEN;
	memapp(w, TO, TO_LEN);
	memapp(w, to->s, to->len);
	if (totag && totag->len ) {
		t->to.len += TOTAG_LEN + totag->len ;
		memapp(w, TOTAG, TOTAG_LEN);
		memapp(w, totag->s, totag->len);
	}
	memapp(w, CRLF, CRLF_LEN);

	     /* From */
	t->from.s = w;
	t->from.len = FROM_LEN + from->len + CRLF_LEN;
	memapp(w, FROM, FROM_LEN);
	memapp(w, from->s, from->len);
  	if (fromtag && fromtag->len ) { 
		t->from.len+= FROMTAG_LEN + fromtag->len;
		memapp(w, FROMTAG, FROMTAG_LEN);
		memapp(w, fromtag->s, fromtag->len);
	}
	memapp(w, CRLF, CRLF_LEN);
	
	     /* CSeq */
	t->cseq_n.s = w; 
	/* don't include method name and CRLF -- subsequent
	 * local reuqests ACK/CANCEl will add their own */
	t->cseq_n.len = CSEQ_LEN + cseq_str_len; 

	memapp(w, CSEQ, CSEQ_LEN);
	memapp(w, cseq_str, cseq_str_len);
	memapp(w, " ", 1);
	memapp(w, msg->s, msg->len);

	     /* Call-ID */
	t->callid.s = w + CRLF_LEN; 
	t->callid.len = CALLID_LEN + callid->len + CRLF_LEN;
	memapp(w, CRLF CALLID, CRLF_LEN + CALLID_LEN);
	memapp(w, callid->s, callid->len);
	memapp(w, CRLF, CRLF_LEN);
	
	     /* Content-Length */
	if (body) {
		memapp(w, CONTENT_LENGTH, CONTENT_LENGTH_LEN);
		memapp(w, content_len, content_len_len);
		memapp(w, CRLF, CRLF_LEN);
	}
	
	     /* Server signature */
	if (server_signature) {
		memapp(w, USER_AGENT CRLF, USER_AGENT_LEN + CRLF_LEN);
	}

	     /* Headers */
	if (headers) {
		memapp(w, headers->s, headers->len);
	}

	     /* EoH */
	memapp(w, CRLF, CRLF_LEN);
	
	     /* Body */
	if (body) {
		memapp(w, body->s, body->len);
	}

#ifdef EXTRA_DEBUG
	if (w-buf != *len ) abort();
#endif
	
 error1:
	pkg_free(via);	
 error:
	return buf;
}


int t_calc_branch(struct cell *t, 
	int b, char *branch, int *branch_len)
{
	return syn_branch ?
		branch_builder( t->hash_index,
			t->label, 0,
			b, branch, branch_len )
		: branch_builder( t->hash_index,
			0, t->md5,
			b, branch, branch_len );
}

