/*
 * $Id$
 */


#ifndef _SIP_MSG_H
#define _SIP_MSG_H

#include "../../msg_parser.h"

#include "sh_malloc.h"

#define sip_msg_cloner(p_msg) \
    sip_msg_cloner_2(p_msg)

#define sip_msg_free(_p_msg) shm_free( (_p_msg ))
#define sip_msg_free_unsafe(_p_msg) shm_free_unsafe( (_p_msg) )
/*    sip_msg_free_2(p_msg) */


struct sip_msg*  sip_msg_cloner_1( struct sip_msg *org_msg );
struct sip_msg*  sip_msg_cloner_2( struct sip_msg *org_msg );
void                     sip_msg_free_1( struct sip_msg *org_msg );
/* void                     sip_msg_free_2( struct sip_msg *org_msg ); */


#endif
