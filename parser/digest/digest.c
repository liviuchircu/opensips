/*
 * $Id$
 *
 * Digest credentials parser interface
 */

#include "digest.h"
#include "../../mem/mem.h"  /* pkg_malloc */
#include "../../dprint.h"   /* Guess what */
#include <stdio.h>          /* printf */


/*
 * Create and initialize a new credentials structure
 */
static inline int new_credentials(struct hdr_field* _h)
{
	auth_body_t* b;

	b = (auth_body_t*)pkg_malloc(sizeof(auth_body_t));
	if (b == 0) {
		LOG(L_ERR, "parse_credentials(): No memory left\n");
		return -2;
	}
		
	init_dig_cred(&(b->digest));
	b->stale = 0;
	b->nonce_retries = 0;
	b->authorized = 0;

	_h->parsed = (void*)b;

	return 0;
}


/*
 * Parse digest credentials
 */
int parse_credentials(struct hdr_field* _h)
{
	int res;

	if (_h->parsed) {
		return 0;  /* Already parsed */
	}

	if (new_credentials(_h) < 0) {
		LOG(L_ERR, "parse_credentials(): Can't create new credentials\n");
		return -1;
	}

	res = parse_digest_cred(&(_h->body), &(((auth_body_t*)(_h->parsed))->digest));
	
	if (res != 0) {
		free_credentials((auth_body_t**)&(_h->parsed));
	}

	return res;
}


/*
 * Free all memory
 */
void free_credentials(auth_body_t** _b)
{
	pkg_free(*_b);
	*_b = 0;
}


/*
 * Check semantics of a digest credentials structure
 * Make sure that all attributes needed to verify response 
 * string are set or at least have a default value
 *
 * The returned value is logical OR of all errors encountered
 * during the check, see dig_err_t type for more details 
 */
dig_err_t check_dig_cred(dig_cred_t* _c)
{
	dig_err_t res = E_DIG_OK;

	     /* Username must be present */
	if (_c->username.s == 0) res |= E_DIG_USERNAME;

	     /* Realm must be present */
	if (_c->realm.s == 0) res |= E_DIG_REALM;

	     /* Nonce that was used must be specified */
	if (_c->nonce.s == 0) res |= E_DIG_NONCE;

	     /* URI must be specified */
	if (_c->uri.s == 0) res |= E_DIG_URI;

	     /* We cannot check credentials without response */
	if (_c->response.s == 0) res |= E_DIG_RESPONSE;

	     /* If QOP parameter is present, some additional
	      * requirements must be met
	      */
	if ((_c->qop.qop_parsed == QOP_AUTH) || (_c->qop.qop_parsed == QOP_AUTHINT)) {
		     /* CNONCE must be specified */
		if (_c->cnonce.s == 0) res |= E_DIG_CNONCE;
		     /* and also nonce count must be specified */
		if (_c->nc.s == 0) res |= E_DIG_NC;
	}
		
	return res;	
}


/*
 * Print credential structure content to stdout
 * Just for debugging
 */
void print_cred(dig_cred_t* _c)
{
	printf("===Digest credentials===\n");
	if (_c) {
		printf("Username  = \'%.*s\'\n", _c->username.len, _c->username.s);
		printf("Realm     = \'%.*s\'\n", _c->realm.len, _c->realm.s);
		printf("Nonce     = \'%.*s\'\n", _c->nonce.len, _c->nonce.s);
		printf("URI       = \'%.*s\'\n", _c->uri.len, _c->uri.s);
		printf("Response  = \'%.*s\'\n", _c->response.len, _c->response.s);
		printf("Algorithm = \'%.*s\'\n", _c->alg.alg_str.len, _c->alg.alg_str.s);
		printf("\\--parsed = ");

		switch(_c->alg.alg_parsed) {
		case ALG_UNSPEC:  printf("ALG_UNSPEC\n");  break;
		case ALG_MD5:     printf("ALG_MD5\n");     break;
		case ALG_MD5SESS: printf("ALG_MD5SESS\n"); break;
		case ALG_OTHER:   printf("ALG_OTHER\n");   break;
		}

		printf("Cnonce    = \'%.*s\'\n", _c->cnonce.len, _c->cnonce.s);
		printf("Opaque    = \'%.*s\'\n", _c->opaque.len, _c->opaque.s);
		printf("QOP       = \'%.*s\'\n", _c->qop.qop_str.len, _c->qop.qop_str.s);
		printf("\\--parsed = ");

		switch(_c->qop.qop_parsed) {
		case QOP_UNSPEC:  printf("QOP_UNSPEC\n");  break;
		case QOP_AUTH:    printf("QOP_AUTH\n");    break;
		case QOP_AUTHINT: printf("QOP_AUTHINT\n"); break;
		case QOP_OTHER:   printf("QOP_OTHER\n");   break;
		}
		printf("NC        = \'%.*s\'\n", _c->nc.len, _c->nc.s);
	}
	printf("===/Digest credentials===\n");
}


/*
 * Mark credentials as selected so functions
 * following authorize know which credentials
 * to use if the message contained more than
 * one
 */
int mark_authorized_cred(struct sip_msg* _m, struct hdr_field* _h)
{
	struct hdr_field* f;
	
	switch(_h->type) {
	case HDR_AUTHORIZATION: f = _m->authorization; break;
	case HDR_PROXYAUTH:     f = _m->proxy_auth;    break;
	default:
		LOG(L_ERR, "mark_authorized_cred(): Invalid header field type\n");
		return -1;
	}

	if (!(f->parsed)) {
		if (new_credentials(f) < 0) {
			LOG(L_ERR, "mark_authorized_cred(): Error in new_credentials\n");
			return -1;
		}
	}

	((auth_body_t*)(f->parsed))->authorized = _h;

	return 0;
}


/*
 * Get pointer to authorized credentials, if there are no
 * authorized credentials, 0 is returned
 */
int get_authorized_cred(struct hdr_field* _f, struct hdr_field** _h)
{
	if (_f && _f->parsed) {
		*_h = ((auth_body_t*)(_f->parsed))->authorized;
	} else {
		*_h = 0;
	}
	
	return 0;
}
