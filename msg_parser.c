/*
 * $Id$
 *
 * sip msg. header proxy parser 
 *
 */

#include <string.h>
#include <stdlib.h>

#include "msg_parser.h"
#include "parser_f.h"
#include "ut.h"
#include "error.h"
#include "dprint.h"

#ifdef DEBUG_DMALLOC
#include <dmalloc.h>
#endif



#define DEBUG



/* parses the first line, returns pointer to  next line  & fills fl;
   also  modifies buffer (to avoid extra copy ops) */
char* parse_first_line(char* buffer, unsigned int len, struct msg_start * fl)
{
	
	char *tmp;
	char* second;
	char* third;
	char* nl;
	int offset;
	/* int l; */
	char* end;
	char s1,s2,s3;
	
	/* grammar:
		request  =  method SP uri SP version CRLF
		response =  version SP status  SP reason  CRLF
		(version = "SIP/2.0")
	*/
	

	end=buffer+len;
	/* see if it's a reply (status) */

	/* jku  -- parse well-known methods */

	/* drop messages which are so short they are for sure useless;
           utilize knowledge of minimum size in parsing the first
	   token 
        */
	if (len <=16 ) {
		LOG(L_INFO, "ERROR: parse_first_line: message too short\n");
		goto error1;
	}

	tmp=buffer;
  	/* is it perhaps a reply, ie does it start with "SIP...." ? */
	if ( 	(*tmp=='S' || *tmp=='s') && 
		strncasecmp( tmp+1, SIP_VERSION+1, SIP_VERSION_LEN-1)==0 &&
		(*(tmp+SIP_VERSION_LEN)==' ')) {
			fl->type=SIP_REPLY;
			fl->u.reply.version.len=SIP_VERSION_LEN;
			tmp=buffer+SIP_VERSION_LEN;
	} else IFISMETHOD( INVITE, 'I' )
	else IFISMETHOD( CANCEL, 'C')
	else IFISMETHOD( ACK, 'A' )
	else IFISMETHOD( BYE, 'B' )
	/* if you want to add another method XXX, include METHOD_XXX in
           H-file (this is the value which you will take later in
           processing and define XXX_LEN as length of method name;
	   then just call IFISMETHOD( XXX, 'X' ) ... 'X' is the first
	   latter; everything must be capitals
	*/
	else {
		/* neither reply, nor any of known method requests, 
		   let's believe it is an unknown method request
        	*/
		tmp=eat_token_end(buffer,buffer+len);
		if ((tmp==buffer)||(tmp>=end)){
			LOG(L_INFO, "ERROR:parse_first_line: empty  or bad first line\n");
			goto error1;
		}
		if (*tmp!=' ') {
			LOG(L_INFO, "ERROR:parse_first_line: method not followed by SP\n");
			goto error1;
		}
		fl->type=SIP_REQUEST;
		fl->u.request.method_value=METHOD_OTHER;
		fl->u.request.method.len=tmp-buffer;
	}


	/* identifying type of message over now; 
	   tmp points at space after; go ahead */

	fl->u.request.method.s=buffer;  /* store ptr to first token */
	(*tmp)=0;			/* mark the 1st token end */
	second=tmp+1;			/* jump to second token */
	offset=second-buffer;

/* EoJku */
	
	/* next element */
	tmp=eat_token_end(second, second+len-offset);
	if (tmp>=end){
		goto error;
	}
	offset+=tmp-second;
	third=eat_space_end(tmp, tmp+len-offset);
	offset+=third-tmp;
	if ((third==tmp)||(tmp>=end)){
		goto error;
	}
	*tmp=0; /* mark the end of the token */
	fl->u.request.uri.s=second;
	fl->u.request.uri.len=tmp-second;

	/* jku: parse status code */
	if (fl->type==SIP_REPLY) {
		if (fl->u.request.uri.len!=3) {
			LOG(L_INFO, "ERROR:parse_first_line: len(status code)!=3: %s\n",
				second );
			goto error;
		}
		s1=*second; s2=*(second+1);s3=*(second+2);
		if (s1>='0' && s1<='9' && 
		    s2>='0' && s2<='9' &&
		    s3>='0' && s3<='9' ) {
			fl->u.reply.statusclass=s1-'0';
			fl->u.reply.statuscode=fl->u.reply.statusclass*100+10*(s2-'0')+(s3-'0');
		} else {
			LOG(L_INFO, "ERROR:parse_first_line: status_code non-numerical: %s\n",
				second );
			goto error;
		}
	}
	/* EoJku */

	/*  last part: for a request it must be the version, for a reply
	 *  it can contain almost anything, including spaces, so we don't care
	 *  about it*/
	if (fl->type==SIP_REQUEST){
		tmp=eat_token_end(third,third+len-offset);
		offset+=tmp-third;
		if ((tmp==third)||(tmp>=end)){
			goto error;
		}
		if (! is_empty_end(tmp, tmp+len-offset)){
			goto error;
		}
	}else{
		tmp=eat_token2_end(third,third+len-offset,'\r'); /* find end of line 
												  ('\n' or '\r') */
		if (tmp>=end){ /* no crlf in packet => invalid */
			goto error;
		}
		offset+=tmp-third;
	}
	nl=eat_line(tmp,len-offset);
	if (nl>=end){ /* no crlf in packet or only 1 line > invalid */
		goto error;
	}
	*tmp=0;
	fl->u.request.version.s=third;
	fl->u.request.version.len=tmp-third;
	
	return nl;

error:
	LOG(L_INFO, "ERROR:parse_first_line: bad %s first line\n", 
		(fl->type==SIP_REPLY)?"reply(status)":"request");
error1:
	fl->type=SIP_INVALID;
	LOG(L_INFO, "ERROR: at line 0 char %d\n", offset);
	/* skip  line */
	nl=eat_line(buffer,len);
	return nl;
}


#ifdef OLD_PARSER
/* returns integer field name type */
int field_name(char *s, int l)
{
	if (l<1) return HDR_OTHER;
	else if ((l==1) && ((*s=='v')||(*s=='V')))
		return HDR_VIA;
	else if (strcasecmp(s, "Via")==0)
		return  HDR_VIA;
/*	if ((strcmp(s, "To")==0)||(strcmp(s,"t")==0))
		return HDR_TO;*/
	return HDR_OTHER;
}
#endif


#ifndef OLD_PARSER
/* returns pointer to next header line, and fill hdr_f ;
 * if at end of header returns pointer to the last crlf  (always buf)*/
char* get_hdr_field(char* buf, char* end, struct hdr_field* hdr)
{

	char* tmp;
	char *match;
	struct via_body *vb;

	if ((*buf)=='\n' || (*buf)=='\r'){
		/* double crlf or lflf or crcr */
		DBG("found end of header\n");
		hdr->type=HDR_EOH;
		return buf;
	}

	tmp=parse_hname(buf, end, hdr);
	if (hdr->type==HDR_ERROR){
		LOG(L_ERR, "ERROR: get_hdr_field: bad header\n");
		goto error;
	}
	switch(hdr->type){
		case HDR_VIA:
			vb=malloc(sizeof(struct via_body));
			if (vb==0){
				LOG(L_ERR, "get_hdr_field: out of memory\n");
				goto error;
			}
			memset(vb,0,sizeof(struct via_body));

			hdr->body.s=tmp;
			tmp=parse_via(tmp, end, vb);
			if (vb->error==VIA_PARSE_ERROR){
				LOG(L_ERR, "ERROR: get_hdr_field: bad via\n");
				free(vb);
				goto error;
			}
			hdr->parsed=vb;
			vb->hdr.s=hdr->name.s;
			vb->hdr.len=hdr->name.len;
			/*vb->size=tmp-hdr->name.s;*/
			hdr->body.len=tmp-hdr->body.s;
			break;
		case HDR_TO:
		case HDR_FROM:
		case HDR_CSEQ:
		case HDR_CALLID:
		case HDR_CONTACT:
		case HDR_OTHER:
			/* just skip over it */
			hdr->body.s=tmp;
			/* find end of header */
			
			/* find lf */
			do{
				match=q_memchr(tmp, '\n', end-tmp);
				if (match){
					match++;
				#if 0
					/* null terminate*/
					*match=0;
					hdr->body.len=match-tmp;
					match++; /*skip*/
					tmp=match;
				#endif
				}else {
					tmp=end;
					LOG(L_ERR, "ERROR: get_hdr_field: bad body for <%s>(%d)\n",
							hdr->name.s, hdr->type);
					goto error;
				}
			}while( match<end &&( (*match==' ')||(*match=='\t') ) );
			*(match-1)=0; /*null terminate*/
			hdr->body.len=match-hdr->body.s;
			tmp=match;
			break;
		default:
			LOG(L_CRIT, "BUG: get_hdr_field: unknown header type %d\n",
					hdr->type);
			goto error;
	}

	return tmp;
error:
	DBG("get_hdr_field: error exit\n");
	hdr->type=HDR_ERROR;
	return tmp;
}

#else
/* returns pointer to next header line, and fill hdr_f */
char* get_hdr_field(char *buffer, unsigned int len, struct hdr_field*  hdr_f)
{
	/* grammar (rfc822):
		field = field-name ":" field-body CRLF
		field-body = text [ CRLF SP field-body ]
	   (CRLF in the field body must be removed)
	*/

	char* tmp, *tmp2;
	char* nl;
	char* body;
	int offset;
	int l;

	
	/* init content to the empty string */
	hdr_f->name.s="";
	hdr_f->name.len=0;
	hdr_f->body.s="";
	hdr_f->body.len=0;
	
	if ((*buffer=='\n')||(*buffer=='\r')){
		/* double crlf */
		tmp=eat_line(buffer,len);
		hdr_f->type=HDR_EOH;
		return tmp;
	}
#if 0	
	tmp=eat_token2_end(buffer, buffer+len, ':');
	if ((tmp==buffer) || (tmp-buffer==len) ||
		(is_empty_end(buffer, tmp))|| (*tmp!=':')){
		hdr_f->type=HDR_ERROR;
		goto error;
	}
	*tmp=0;
	/* take care of possible spaces (e.g: "Via  :") */
	tmp2=eat_token_end(buffer, tmp);
	/* in the worst case tmp2=buffer+tmp-buffer=tmp */
	*tmp2=0;
	l=tmp2-buffer;
	if (tmp2<tmp){
		tmp2++;
		/* catch things like: "Via foo bar:" */
		tmp2=eat_space_end(tmp2, tmp);
		if (tmp2!=tmp){
			hdr_f->type=HDR_ERROR;
			goto error;
		}
	}
#endif

	tmp=parse_hname(buffer, buffer+len, hdr_f);
	if (hdr_f->type==HDR_ERROR){
		LOG(L_ERR, "ERROR: get_hdr_field: bad header\n");
		goto error;
	}
	
#if 0
	hdr_f->type=field_name(buffer, l);
	body= ++tmp;
	hdr_f->name.s=buffer;
	hdr_f->name.len=l;
#endif
	offset=tmp-buffer;
	/* get all the lines in this field  body */
	do{
		nl=eat_line(tmp, len-offset);
		offset+=nl-tmp;
		tmp=nl;
	
	}while( (*tmp==' ' ||  *tmp=='\t') && (offset<len) );
	if (offset==len){
		hdr_f->type=HDR_ERROR;
		LOG(L_INFO, "ERROR: get_hdr_field: field body too  long\n");
		goto error;
	}
	*(tmp-1)=0; /* should be an LF */
	hdr_f->body.s=body;
	hdr_f->body.len=tmp-1-body;
error:
	return tmp;
}
#endif


char* parse_hostport(char* buf, str* host, short int* port)
{
	char *tmp;
	int err;
	
	host->s=buf;
	for(tmp=buf;(*tmp)&&(*tmp!=':');tmp++);
	host->len=tmp-buf;
	if (*tmp==0){
		*port=0;
	}else{
		*tmp=0;
		*port=str2s(tmp+1, strlen(tmp+1), &err);
		if (err ){
			LOG(L_INFO, 
					"ERROR: hostport: trailing chars in port number: %s\n",
					tmp+1);
			/* report error? */
		}
	}
	return host->s;
}



/* buf= pointer to begining of uri (sip:x@foo.bar:5060;a=b?h=i)
   len= len of uri
returns: fills uri & returns <0 on error or 0 if ok */
int parse_uri(char *buf, int len, struct sip_uri* uri)
{
	char* next, *end;
	char *user, *passwd, *host, *port, *params, *headers;
	int host_len, port_len, params_len, headers_len;
	int ret;
	

	ret=0;
	end=buf+len;
	memset(uri, 0, sizeof(struct sip_uri)); /* zero it all, just to be sure */
	/* look for "sip:"*/;
	next=q_memchr(buf, ':',  len);
	if ((next==0)||(strncmp(buf,"sip",next-buf)!=0)){
		LOG(L_DBG, "ERROR: parse_uri: bad sip uri\n");
		ret=E_UNSPEC;
		goto error;
	}
	buf=next+1; /* next char after ':' */
	if (buf>end){
		LOG(L_DBG, "ERROR: parse_uri: uri too short\n");
		ret=E_UNSPEC;
		goto error;
	}
	/*look for '@' */
	next=q_memchr(buf,'@', end-buf);
	if (next==0){
		/* no '@' found, => no userinfo */
		uri->user.s=0;
		uri->passwd.s=0;
		host=buf;
	}else{
		/* found it */
		user=buf;
		/* try to find passwd */
		passwd=q_memchr(user,':', next-user);
		if (passwd==0){
			/* no ':' found => no password */
			uri->passwd.s=0;
			uri->user.s=(char*)malloc(next-user+1);
			if (uri->user.s==0){
				LOG(L_ERR,"ERROR:parse_uri: memory allocation failure\n");
				ret=E_OUT_OF_MEM;
				goto error;
			}
			memcpy(uri->user.s, user, next-user);
			uri->user.len=next-user;
			uri->user.s[next-user]=0; /* null terminate it, 
									   usefull for easy printing*/
		}else{
			uri->user.s=(char*)malloc(passwd-user+1);
			if (uri->user.s==0){
				LOG(L_ERR,"ERROR:parse_uri: memory allocation failure\n");
				ret=E_OUT_OF_MEM;
				goto error;
			}
			memcpy(uri->user.s, user, passwd-user);
			uri->user.len=passwd-user;
			uri->user.s[passwd-user]=0;
			passwd++; /*skip ':' */
			uri->passwd.s=(char*)malloc(next-passwd+1);
			if (uri->passwd.s==0){
				LOG(L_ERR,"ERROR:parse_uri: memory allocation failure\n");
				ret=E_OUT_OF_MEM;
				goto error;
			}
			memcpy(uri->passwd.s, passwd, next-passwd);
			uri->passwd.len=next-passwd;
			uri->passwd.s[next-passwd]=0;
		}
		host=next+1; /* skip '@' */
	}
	/* try to find the rest */
	if(host>=end){
		LOG(L_DBG, "ERROR: parse_uri: missing hostport\n");
		ret=E_UNSPEC;
		goto error;
	}
	headers=q_memchr(host,'?',end-host);
	params=q_memchr(host,';',end-host);
	port=q_memchr(host,':',end-host);
	host_len=(port)?port-host:(params)?params-host:(headers)?headers-host:end-host;
	/* get host */
	uri->host.s=malloc(host_len+1);
	if (uri->host.s==0){
		LOG(L_ERR, "ERROR: parse_uri: memory allocation error\n");
		ret=E_OUT_OF_MEM;
		goto error;
	}
	memcpy(uri->host.s, host, host_len);
	uri->host.len=host_len;
	uri->host.s[host_len]=0;
	/* get port*/
	if ((port)&&(port+1<end)){
		port++;
		if ( ((params) &&(params<port))||((headers) &&(headers<port)) ){
			/* error -> invalid uri we found ';' or '?' before ':' */
			LOG(L_DBG, "ERROR: parse_uri: malformed sip uri\n");
			ret=E_UNSPEC;
			goto error;
		}
		port_len=(params)?params-port:(headers)?headers-port:end-port;
		uri->port.s=malloc(port_len+1);
		if (uri->port.s==0){
			LOG(L_ERR, "ERROR: parse_uri: memory allocation error\n");
			ret=E_OUT_OF_MEM;
			goto error;
		}
		memcpy(uri->port.s, port, port_len);
		uri->port.len=port_len;
		uri->port.s[port_len]=0;
	}else uri->port.s=0;
	/* get params */
	if ((params)&&(params+1<end)){
		params++;
		if ((headers) && (headers<params)){
			/* error -> invalid uri we found '?' or '?' before ';' */
			LOG(L_DBG, "ERROR: parse_uri: malformed sip uri\n");
			ret=E_UNSPEC;
			goto error;
		}
		params_len=(headers)?headers-params:end-params;
		uri->params.s=malloc(params_len+1);
		if (uri->params.s==0){
			LOG(L_ERR, "ERROR: parse_uri: memory allocation error\n");
			ret=E_OUT_OF_MEM;
			goto error;
		}
		memcpy(uri->params.s, params, params_len);
		uri->params.len=params_len;
		uri->params.s[params_len]=0;
	}else uri->params.s=0;
	/*get headers */
	if ((headers)&&(headers+1<end)){
		headers++;
		headers_len=end-headers;
		uri->headers.s=malloc(headers_len+1);
		if(uri->headers.s==0){
			LOG(L_ERR, "ERROR: parse_uri: memory allocation error\n");
			ret=E_OUT_OF_MEM;
			goto error;
		}
		memcpy(uri->headers.s, headers, headers_len);
		uri->headers.len=headers_len;
		uri->headers.s[headers_len]=0;
	}else uri->headers.s=0;
	
	return ret;
error:
	return ret;
}


#ifdef OLD_PARSER
/* parses a via body, returns next via (for compact vias) & fills vb,
 * the buffer should be null terminated! */
char* parse_via_body(char* buffer,unsigned int len, struct via_body * vb)
{
	/* format: sent-proto sent-by  *(";" params) [comment] 

	           sent-proto = name"/"version"/"transport
		   sent-by    = host [":" port]
		   
	*/

	char* tmp;
	char *name,*version, *transport, *comment, *params, *hostport;
	int name_len, version_len, transport_len, comment_len, params_len;
	char * next_via;
	str host;
	short int port;
	int offset;
	

	name=version=transport=comment=params=hostport=next_via=host.s=0;
	name_len=version_len=transport_len=comment_len=params_len=host.len=0;
	name=eat_space_end(buffer, buffer+len);
	if (name-buffer==len) goto error;
	offset=name-buffer;
	tmp=name;

	version=eat_token2_end(tmp,tmp+len-offset,'/');
	if (version+1-buffer>=len) goto error;
	*version=0;
	name_len=version-name;
	version++;
	offset+=version-tmp;
	
	transport=eat_token2_end(tmp,tmp+len-offset,'/');
	if (transport+1-buffer>=len) goto error;
	*transport=0;
	version_len=transport-version;
	transport++;
	offset+=transport-tmp;
	
	tmp=eat_token_end(transport,transport+len-offset);
	if (tmp+1-buffer>=len) goto error;
	*tmp=0;
	transport_len=tmp-transport;
	tmp++;
	offset+=tmp-transport;
	
	hostport=eat_space_end(tmp,tmp+len-offset);
	if (hostport+1-buffer>=len) goto error;
	offset+=hostport-tmp;

	/* find end of hostport */
 	for(tmp=hostport; (tmp-buffer)<len &&
			(*tmp!=' ')&&(*tmp!=';')&&(*tmp!=','); tmp++);
	if (tmp-buffer<len){
		switch (*tmp){
			case ' ':
				*tmp=0;
				tmp++;
				/*the rest is comment? */
				if (tmp-buffer<len){
					comment=tmp;
					/* eat the comment */
					for(;((tmp-buffer)<len)&&
						(*tmp!=',');tmp++);
					/* mark end of compact via (also end of comment)*/
					comment_len=tmp-comment;
					if (tmp-buffer<len){
						*tmp=0;
					}else break;
					/* eat space & ',' */
					for(tmp=tmp+1;((tmp-buffer)<len)&&
						(*tmp==' '|| *tmp==',');tmp++);
					
				}
				break;

			case ';':
				*tmp=0;
				tmp++;
				if (tmp-buffer>=len) goto error;
				params=tmp;
				/* eat till end, first space  or ',' */
				for(;((tmp-buffer)<len)&&
					(*tmp!=' '&& *tmp!=',');tmp++);
				params_len=tmp-params;
				if (tmp-buffer==len)  break;
				if (*tmp==' '){
					/* eat comment */
					*tmp=0;
					tmp++;
					comment=tmp;
					for(;((tmp-buffer)<len)&&
						(*tmp!=',');tmp++);
					comment_len=tmp-comment;
					if (tmp-buffer==len)  break;
				}
				/* mark end of via*/
				*tmp=0;
				
				/* eat space & ',' */
				for(tmp=tmp+1;((tmp-buffer)<len)&&
					(*tmp==' '|| *tmp==',');tmp++);
				break;

			case ',':
				*tmp=0;
				tmp++;
				if (tmp-buffer<len){
					/* eat space and ',' */
					for(;((tmp-buffer)<len)&&
						(*tmp==' '|| *tmp==',');
					   tmp++);
				}
		}
	}
	/* if we are not at the end of the body => we found another compact via */
	if (tmp-buffer<len) next_via=tmp;
	
	/* parse hostport */
	parse_hostport(hostport, &host, &port);
	vb->name.s=name;
	vb->name.len=name_len;
	vb->version.s=version;
	vb->version.len=version_len;
	vb->transport.s=transport;
	vb->transport.len=transport_len;
	vb->host.s=host.s;
	vb->host.len=host.len;
	vb->port=port;
	vb->params.s=params;
	vb->params.len=params_len;
	vb->comment.s=comment;
	vb->comment.len=comment_len;
	vb->next=next_via;
	vb->error=VIA_PARSE_OK;

	
	/* tmp points to end of body or to next via (if compact)*/
	
	return tmp;

error:
	vb->error=VIA_PARSE_ERROR;
	return tmp;
}
#endif


/* parse the headers and adds them to msg->headers and msg->to, from etc.
 * It stops when all the headers requested in flags were parsed, on error
 * (bad header) or end of headers */
int parse_headers(struct sip_msg* msg, int flags)
{
	struct hdr_field* hf;
	char* tmp;
	char* rest;
	char* end;
	
	end=msg->buf+msg->len;
	tmp=msg->unparsed;
	
	DBG("parse_headers: flags=%d\n", flags);
	while( tmp<end && (flags & msg->parsed_flag) != flags){
		hf=malloc(sizeof(struct hdr_field));
		memset(hf,0, sizeof(struct hdr_field));
		if (hf==0){
			LOG(L_ERR, "ERROR:parse_headers: memory allocation error\n");
			goto error;
		}
		hf->type=HDR_ERROR;
		rest=get_hdr_field(tmp, msg->buf+msg->len, hf);
		switch (hf->type){
			case HDR_ERROR:
				LOG(L_INFO,"ERROR: bad header  field\n");
				goto  error;
			case HDR_EOH:
				msg->eoh=tmp; /* or rest?*/
				msg->parsed_flag|=HDR_EOH;
				goto skip;
			case HDR_OTHER: /*do nothing*/
				break;
			case HDR_CALLID:
				if (msg->callid==0) msg->callid=hf;
				msg->parsed_flag|=HDR_CALLID;
				break;
			case HDR_TO:
				if (msg->to==0) msg->to=hf;
				msg->parsed_flag|=HDR_TO;
				break;
			case HDR_CSEQ:
				if (msg->cseq==0) msg->cseq=hf;
				msg->parsed_flag|=HDR_CSEQ;
				break;
			case HDR_FROM:
				if (msg->from==0) msg->from=hf;
				msg->parsed_flag|=HDR_FROM;
				break;
			case HDR_CONTACT:
				if (msg->contact==0) msg->contact=hf;
				msg->parsed_flag|=HDR_CONTACT;
				break;
			case HDR_VIA:
				msg->parsed_flag|=HDR_VIA;
				DBG("parse_headers: Via1 found, flags=%d\n", flags);
				if (msg->h_via1==0) {
					msg->h_via1=hf;
					msg->via1=hf->parsed;
					if (msg->via1->next){
						msg->via2=msg->via1->next;
						msg->parsed_flag|=HDR_VIA2;
					}
				}else if (msg->h_via2==0){
					msg->h_via2=hf;
					msg->via2=hf->parsed;
					msg->parsed_flag|=HDR_VIA2;
				DBG("parse_headers: Via2 found, flags=%d\n", flags);
				}
				break;
			default:
				LOG(L_CRIT, "BUG: parse_headers: unknown header type %d\n",
							hf->type);
				goto error;
		}
		/* add the header to the list*/
		if (msg->last_header==0){
			msg->headers=hf;
			msg->last_header=hf;
		}else{
			msg->last_header->next=hf;
			msg->last_header=hf;
		}
	#ifdef DEBUG
		DBG("header field type %d, name=<%s>, body=<%s>\n",
			hf->type, hf->name.s, hf->body.s);
	#endif
		tmp=rest;
	}
skip:
	msg->unparsed=tmp;
	return 0;
	
error:
	if (hf) free(hf);
	return -1;
}





/* returns 0 if ok, -1 for errors */
int parse_msg(char* buf, unsigned int len, struct sip_msg* msg)
{

	char *tmp, *bar;
	char* rest;
	char* first_via;
	char* second_via;
	struct msg_start *fl;
	struct hdr_field* hf;
	struct via_body *vb1, *vb2;
	int offset;
	int flags;

#ifdef OLD_PARSER
	/* init vb1 & vb2 to the null string */
	/*memset(&vb1,0, sizeof(struct via_body));
	memset(&vb2,0, sizeof(struct via_body));*/
	vb1=&(msg->via1);
	vb2=&(msg->via2);
	vb1->error=VIA_PARSE_ERROR;
	vb2->error=VIA_PARSE_ERROR;
#else
	vb1=vb2=0;
	hf=0;
#endif
	/* eat crlf from the beginning */
	for (tmp=buf; (*tmp=='\n' || *tmp=='\r')&&
			tmp-buf < len ; tmp++);
	offset=tmp-buf;
	fl=&(msg->first_line);
	rest=parse_first_line(tmp, len-offset, fl);
	offset+=rest-tmp;
	tmp=rest;
	switch(fl->type){
		case SIP_INVALID:
			DBG("parse_msg: invalid message\n");
			goto error;
			break;
		case SIP_REQUEST:
			DBG("SIP Request:\n");
			DBG(" method:  <%s>\n",fl->u.request.method);
			DBG(" uri:     <%s>\n",fl->u.request.uri);
			DBG(" version: <%s>\n",fl->u.request.version);
			flags=HDR_VIA;
			break;
		case SIP_REPLY:
			DBG("SIP Reply  (status):\n");
			DBG(" version: <%s>\n",fl->u.reply.version);
			DBG(" status:  <%s>\n",fl->u.reply.status);
			DBG(" reason:  <%s>\n",fl->u.reply.reason);
			flags=HDR_VIA|HDR_VIA2;
			break;
		default:
			DBG("unknown type %d\n",fl->type);
	}
	msg->unparsed=tmp;
	/*find first Via: */
	first_via=0;
	second_via=0;
	if (parse_headers(msg, flags)==-1) goto error;

#ifdef DEBUG
	/* dump parsed data */
	if (msg->via1){
		DBG(" first  via: <%s/%s/%s> <%s:%s(%d)>",
			msg->via1->name.s, msg->via1->version.s,
			msg->via1->transport.s, msg->via1->host.s,
			msg->via1->port_str, msg->via1->port);
		if (msg->via1->params.s)  DBG(";<%s>", msg->via1->params.s);
		if (msg->via1->comment.s) DBG(" <%s>", msg->via1->comment.s);
		DBG ("\n");
	}
#ifdef OLD_PARSER
	if (msg->via2){
		DBG(" second via: <%s/%s/%s> <%s:%d>",
				vb2->name.s, vb2->version.s, vb2->transport.s, vb2->host.s,
				vb2->port);
		if (vb2->params.s)  DBG(";<%s>", vb2->params.s);
		if (vb2->comment.s) DBG(" <%s>", vb2->comment.s);
		DBG("\n");
	}
#endif
#endif
	
	/* copy data into msg */
#if 0
#ifndef OLD_PARSER
	memcpy(&(msg->via1), vb1, sizeof(struct via_body));
	if (second_via) memcpy(&(msg->via2), vb2, sizeof(struct via_body));
	if (vb1) free(vb1);
	if (vb2) free(vb1);
#endif
#endif

#ifdef DEBUG
	DBG("exiting parse_msg\n");
#endif

	return 0;
	
error:
	if (hf) free(hf);
#ifndef OLD_PARSER
	if (vb1) free(vb1);
	if (vb2) free(vb1);
#endif
	return -1;
}



void free_uri(struct sip_uri* u)
{
	if (u){
		if (u->user.s) free(u->user.s);
		if (u->passwd.s) free(u->passwd.s);
		if (u->host.s) free(u->host.s);
		if (u->port.s) free(u->port.s);
		if (u->params.s) free(u->params.s);
		if (u->headers.s) free(u->headers.s);
	}
}



void free_via_list(struct via_body* vb)
{
	struct via_body* foo;
	while(vb){
		foo=vb;
		vb=vb->next;
		free(foo);
	}
}


/* frees a hdr_field structure, 
 * WARNING: it frees only parsed (and not name.s, body.s)*/
void clean_hdr_field(struct hdr_field* hf)
{
	if (hf->parsed){
		switch(hf->type){
			case HDR_VIA:
				free_via_list(hf->parsed);
				break;
			default:
				LOG(L_CRIT, "BUG: clean_hdr_field: unknown header type %d\n",
						hf->type);
		}
	}
}



/* frees a hdr_field list,
 * WARNING: frees only ->parsed and ->next*/
void free_hdr_field_lst(struct hdr_field* hf)
{
	struct hdr_field* foo;
	
	while(hf){
		foo=hf;
		hf=hf->next;
		clean_hdr_field(foo);
		free(foo);
	}
}
