/*
SMS Server Tools
Copyright (C) 2000-2002 Stefan Frings

This program is free software unless you got it under another license directly
from the author. You can redistribute it and/or modify it under the terms of
the GNU General Public License as published by the Free Software Foundation.
Either version 2 of the License, or (at your option) any later version.

http://www.isis.de/members/~s.frings
mailto:s.frings@mail.isis.de
*/


#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <syslog.h>
#include "../../ut.h"
#include "libsms_charset.h"
#include "libsms_modem.h"
#include "libsms_sms.h"
#include "sms_funcs.h"



#define set_date(_date,_Pointer) {\
	(_date)[0] = (_Pointer)[3];\
	(_date)[1] = (_Pointer)[2];\
	(_date)[2] = '-';\
	(_date)[3] = (_Pointer)[5];\
	(_date)[4] = (_Pointer)[4];\
	(_date)[5] = '-';\
	(_date)[6] = (_Pointer)[1];\
	(_date)[7] = (_Pointer)[0];}

#define set_time( _time , _Pointer) {\
	(_time)[0] = (_Pointer)[1];\
	(_time)[1] = (_Pointer)[0];\
	(_time)[2] = ':';\
	(_time)[3] = (_Pointer)[3];\
	(_time)[4] = (_Pointer)[2];\
	(_time)[5] = ':';\
	(_time)[6] = (_Pointer)[5];\
	(_time)[7] = (_Pointer)[4];}




/* converts an octet to a 8-Bit value */
int octet2bin(char* octet)
{
	int result=0;

	if (octet[0]>57)
		result=octet[0]-55;
	else
		result=octet[0]-48;
	result=result<<4;
	if (octet[1]>57)
		result+=octet[1]-55;
	else
		result+=octet[1]-48;
	return result;
}




/* converts a PDU-String to Ascii; the first octet is the length
   return the length of ascii */
int pdu2ascii(char* pdu, char* ascii)
{
	int bitposition=0;
	int byteposition;
	int byteoffset;
	int charcounter;
	int bitcounter;
	int count;
	int octetcounter;
	char c;
	char binary[500];

	/* First convert all octets to bytes */
	count=octet2bin(pdu);
	for (octetcounter=0; octetcounter<count; octetcounter++)
		binary[octetcounter]=octet2bin(pdu+(octetcounter<<1)+2);

	/* Then convert from 8-Bit to 7-Bit encapsulated in 8 bit */
	for (charcounter=0; charcounter<count; charcounter++) {
		c=0;
		for (bitcounter=0; bitcounter<7; bitcounter++) {
			byteposition=bitposition/8;
			byteoffset=bitposition%8;
			if (binary[byteposition]&(1<<byteoffset))
				c=c|128;
			bitposition++;
			c=(c>>1)&127; /* The shift fills with 1, but I want 0 */
		}
		if (/*cs_convert*/1)
			ascii[charcounter]=sms2ascii(c);
		else if (c==0)
			ascii[charcounter]=183;
		else
			ascii[charcounter]=c;
	}
	ascii[count]=0;
	return count;
}




int pdu2binary(char* pdu, char* binary)
{
	int count;
	int octetcounter;

	count=octet2bin(pdu);
	for (octetcounter=0; octetcounter<count; octetcounter++)
		binary[octetcounter]=octet2bin(pdu+(octetcounter<<1)+2);
	binary[count]=0;
	return count;
}




/* reads a SMS from the SIM-memory 1-10 */
/* returns number of SIM memory if successful */
/* on digicom the return value can be != sim */
int fetchsms(struct modem *mdm, int sim, char* pdu)
{
	char command[16];
	char answer[512];
	char* position;
	char* beginning;
	char* end;
	int  foo,err;
	int  clen;

	// Digicom reports date+time only with AT+CMGL
	if (mdm->mode==MODE_DIGICOM) {
		put_command(mdm->fd,"AT+CMGL=\"ALL\"\r",14,answer,
			sizeof(answer),200,0);
		/* search for beginning of the answer */
		position=strstr(answer,"+CMGL: ");
		if (position) {
			end=position+7;
			while (*end<'9' && *end>'0') end++;
			if (end==position+7) {
				foo = str2s(position+7,end-position-7,&err);
				if (!err) {
					DBG("DEBUG:fetchsms:Found a message at memory %i\n",foo);
					sim=foo;
				}
				position = 0;
			}
			position = 0;
		}
	} else {
		DBG("DEBUG:fetchsms:Trying to get stored message %i\n",sim);
		clen=sprintf(command,"AT+CMGR=%i\r",sim);
		put_command(mdm->fd,command,clen,answer,sizeof(answer),50,0);
		/* search for beginning of the answer */
		position=strstr(answer,"+CMGR:");
	}

	/* keine SMS empfangen, weil Modem nicht mit +CMGR 
	oder +CMGL geantwortet hat */
	if (position==0)
		return 0;
	beginning=position+7;
	/* keine SMS, weil Modem mit +CMGR: 0,,0 geantwortet hat */
	if (strstr(answer,",,0\r"))
		return 0;

	/* After that we have the PDU or ASCII string */
	for( end=beginning ; *end && *end!='\r' ; end++ );
	if ( !*end || end-beginning<4)
		return 0;
	for( end=end+1 ; *end && *end!='\r' ; end++ );
	if ( !*end || end-beginning<4)
		return 0;
	/* Now we have the end of the PDU or ASCII string */
	*end=0;
	strcpy(pdu,beginning);

	return sim;
}




/* deletes the selected sms from the sim card */
void deletesms(struct modem *mdm, int sim) {
	char command[32];
	char answer[128];
	int  clen;

	DBG("DEBUG:deletesms: Deleting message %i !\n",sim);
	clen = sprintf(command,"AT+CMGD=%i\r",sim);
	put_command(mdm->fd, command, clen, answer, sizeof(answer), 50, 0);
}




// checks the size of the SIM memory
int check_memory(struct modem *mdm, int flag)
{
	char  answer[500];
	char* posi;
	int   laenge;
	int   err,foo;
	int   j, out;

	for(out=0,j=0;!out && j<10; j++) 
	{
		if (put_command(mdm->fd,"AT+CPMS?\r",9,answer,sizeof(answer),50,0)
		&& (posi=strstr(answer,"+CPMS:"))!=0 )
		{
			// Modem supports CPMS command. Read memory size
			if ( (posi=strchr(posi,','))!=0 ) {
				posi++;
				if ( (laenge=strcspn(posi,",\r"))!=0 ) {
					if (flag==USED_MEM ) {
						foo = str2s(posi,laenge,&err);
						if (err) {
							LOG(L_ERR,"ERROR:sms_check_memory: unable to "
								"convert into integer used_memory from CPMS"
								" response\n");
						} else {
							DBG("DEBUG:sms_check_memory: Used memory is %i\n",
								foo);
							return foo;
						}
					}
					posi+=laenge+1;
					if ( (laenge=strcspn(posi,",\r"))!=0 ) {
						foo = str2s(posi,laenge,&err);
						if (err) {
							LOG(L_ERR,"ERROR:sms_check_memory: unable to"
								"convert into integer max_memory from CPMS"
								" response\n");
						} else {
							DBG("DEBUG:sms_check_memory: Max memory is %i\n",
								foo);
							return foo;
						}
					}
				}
			} /* if(strstr) */
		} /* if(put_command) */
		/* if we are here ->  some error happend */
		if (checkmodem(mdm)!=0) {
			LOG(L_WARN,"WARNING:sms_check_memory: something happend with the"
				" modem -> was reinit -> let's retry\n");
		} else {
			LOG(L_ERR,"ERROR:sms_check_memory: modem seems to be ok, but we"
				"had an error? I give up!\n");
			out = 1;
		}
	} /* for */

	if (out==0)
		LOG(L_ERR,"ERROR:sms_check_memory: modem does not respond after 10"
			"reties! I give up :-(\n");

	return -1;
}




/* splits an ASCII string into the parts */
/* returns length of ascii */
int splitascii(struct modem *mdm, char *source, struct incame_sms *sms)
{
	char* start;
	char* end;

	/* the text is after the \r */
	for( start=source ; *start && *start!='\r' ; start++ );
	if (!*start)
		return 1;
	start++;
	strcpy(sms->ascii,start);
	/* get the senders MSISDN */
	start=strstr(source,"\",\"");
	if (start==0) {
		sms->userdatalength=strlen(sms->ascii);
		return 1;
	}
	start+=3;
	end=strstr(start,"\",");
	if (end==0) {
		sms->userdatalength=strlen(sms->ascii);
		return 1;
	}
	*end=0;
	strcpy(sms->sender,start);
	/* Siemens M20 inserts the senders name between MSISDN and date */
	start=end+3;
	// Workaround for Thomas Stoeckel //
	if (start[0]=='\"')
		start++;
	if (start[2]!='/')  { // if next is not a date is must be the name
		end=strstr(start,"\",");
		if (end==0) {
			sms->userdatalength=strlen(sms->ascii);
			return 1;
		}
		*end=0;
		strcpy(sms->name,start);
	}
	/* Get the date */
	start=end+3;
	sprintf(sms->date,"%c%c-%c%c-%c%c",start[3],start[4],start[0],start[1],
		start[6],start[7]);
	/* Get the time */
	start+=9;
	sprintf(sms->time,"%c%c:%c%c:%c%c",start[0],start[1],start[3],start[4],
		start[7],start[7]);
	sms->userdatalength=strlen(sms->ascii);
	return 1;
}




/* Subroutine for splitpdu() for messages type 0 (SMS-Deliver)
   Returns the length of the ascii string
   In binary mode ascii contains the binary SMS */
int split_type_0(struct sms_msg *sms_messg, char* Pointer,
												struct incame_sms *sms)
{
	int Length;
	int padding;

	Length=octet2bin(Pointer);
	padding=Length%2;
	Pointer+=4;
	memcpy(sms->sender,Pointer,Length+padding);
	swapchars(sms->sender,Length+padding);
	/* remove Padding characters after swapping */
	sms->sender[Length]=0;
	Pointer=Pointer+Length+padding+3;
	if ((Pointer[0] & 4)==4)
		sms_messg->is_binary=1;
	else
		sms_messg->is_binary=0;
	Pointer++;
	set_date(sms->date,Pointer);
	Pointer=Pointer+6;
	set_time(sms->time,Pointer);
	Pointer=Pointer+8;
	if (sms_messg->is_binary)
		sms->userdatalength = pdu2binary(Pointer,sms->ascii);
	else
		sms->userdatalength = pdu2ascii(Pointer,sms->ascii);
	return 1;
}




/* Subroutine for splitpdu() for messages type 2 (Staus Report)
   Returns the length of the ascii string. In binary mode ascii 
   contains the binary SMS */
int split_type_2(struct sms_msg *sms_messg, char* position,
												struct incame_sms *sms)
{
	int length;
	int padding;
	int status;
	char temp[32];

	strcpy(sms->ascii,"SMS STATUS REPORT\n");
	// get recipient address
	position+=2;
	length=octet2bin(position);
	padding=length%2;
	position+=4;
	memcpy(sms->sender,position,length+padding);
	sms->sender[length]=0;
	swapchars(sms->sender,length);
	strcat(sms->ascii,"\nDischarge_timestamp: ");
	// get SMSC timestamp
	position+=length+padding;
	set_date(sms->date,position);
	set_time(sms->time,position+6);
	// get Discharge timestamp
	position+=14;
	set_date(temp,position);
	*(temp+DATE_LEN) = ' ';
	set_time(temp+DATE_LEN+1,position+6);
	temp[DATE_LEN+1+TIME_LEN] = 0;
	strcat(sms->ascii,temp);
	strcat(sms->ascii,"\nStatus: ");
	// get Status
	position+=14;
	status=octet2bin(position);
	sprintf(temp,"%i,",status);
	strcat(sms->ascii,temp);
	switch (status) {
		case 0: strcat(sms->ascii,"Ok,short message received by the SME");
			break;
		case 1: strcat(sms->ascii,"Ok,short message forwarded by the SC to"
			" the SME but the SC is unable to confirm delivery");
			break;
		case 2: strcat(sms->ascii,"Ok,short message replaced by the SC");
			break;
		case 32: strcat(sms->ascii,"Still trying,congestion");
			break;
		case 33: strcat(sms->ascii,"Still trying,SME busy");
			break;
		case 34: strcat(sms->ascii,"Still trying,no response from SME");
			break;
		case 35: strcat(sms->ascii,"Still trying,service rejected");
			break;
		case 36: strcat(sms->ascii,"Still trying,quality of service not"
			" available");
			break;
		case 37: strcat(sms->ascii,"Still trying,error in SME");
			break;
		case 64: strcat(sms->ascii,"Error,remote procedure error");
			break;
		case 65: strcat(sms->ascii,"Error,incompatible destination");
			break;
		case 66: strcat(sms->ascii,"Error,connection rejected by SME");
			break;
		case 67: strcat(sms->ascii,"Error,not obtainable");
			break;
		case 68: strcat(sms->ascii,"Error,quality of service not available");
			break;
		case 69: strcat(sms->ascii,"Error,no interworking available");
			break;
		case 70: strcat(sms->ascii,"Error,SM validity period expired");
			break;
		case 71: strcat(sms->ascii,"Error,SM deleted by originating SME");
			break;
		case 72: strcat(sms->ascii,"Error,SM deleted by SC administration");
			break;
		case 73: strcat(sms->ascii,"Error,SM does not exist");
			break;
		case 96: strcat(sms->ascii,"Error,congestion");
			break;
		case 97: strcat(sms->ascii,"Error,SME busy");
			break;
		case 98: strcat(sms->ascii,"Error,no response from SME");
			break;
		case 99: strcat(sms->ascii,"Error,service rejected");
			break;
		case 100: strcat(sms->ascii,"Error,quality of service not available");
			break;
		case 101: strcat(sms->ascii,"Error,error in SME");
			break;
		default: strcat(sms->ascii,"unknown");
	}
	sms->is_statusreport=1;
	sms->userdatalength=strlen(sms->ascii);
	return 0;
}




/* Splits a PDU string into the parts */
/* Returns the length of the ascii string. In binary mode ascii contains the binary SMS */
int splitpdu(struct modem *mdm, struct sms_msg *sms_messg, char* pdu,
												struct incame_sms *sms)
{
	int Length;
	int Type;
	int foo;
	char* Pointer;
	char* start;
	char* end;

	/* Get the senders Name if given. Depends on the modem. */
	start=strstr(pdu,"\",\"");
	if (start!=0) {
		start+=3;
		end=strstr(start,"\",");
		if (end!=0) {
			memcpy(sms->name,start,end-start);
			sms->name[end-start]=0;
		}
	} else
		end=pdu;

	/* the pdu is after the first \r */
	for( start=end+1 ; *start && *start!='\r' ; start++ );
	if (!*start)
		return 0;
	pdu=++start;
	/* removes unwanted ctrl chars at the beginning */
	while ( *pdu && (*pdu<=' '))
		pdu++;
	Pointer=pdu;
	if (mdm->mode!=MODE_OLD) {
		/* get senders smsc */
		Length=octet2bin(pdu)*2-2;
		if (Length>0) {
			Pointer=pdu+4;
			memcpy(sms->smsc,Pointer,Length);
			swapchars(sms->smsc,Length);
			/* remove Padding characters after swapping */
			if (sms->smsc[Length-1]=='F')
				sms->smsc[Length-1]=0;
			else
				sms->smsc[Length] = 0;
		}
		Pointer=pdu+Length+4;
	}
	/* is UDH bit set? */
	if (octet2bin(Pointer)&4)
		sms_messg->udh=1;
	else
		sms_messg->udh=0;
	Type=octet2bin(Pointer) & 3;
	Pointer+=2;
	if (Type==0) // SMS Deliver
		return split_type_0(sms_messg, Pointer, sms);
	else if (Type==2)  // Status Report
		return split_type_2(sms_messg, Pointer, sms);
	// Unsupported type
	foo = sprintf(sms->ascii,"Message format (%i) is not supported. "
		"Cannot decode.\n%s\n",Type,pdu);
	sms->userdatalength = foo;
	return 1;
}




int getsms( struct incame_sms *sms, struct modem *mdm, int sim)
{
	struct sms_msg sms_messg;
	char   pdu[500];
	int    found;
	int    ret;

	found=fetchsms(mdm,sim,pdu);
	if (!found) {
		LOG(L_ERR,"ERROR:getsms: unable to fetch sms %d!\n",sim);
		goto error;
	}

	memset( &sms_messg, 0, sizeof(struct sms_msg) );
	memset( sms, 0, sizeof(struct incame_sms) );
	/* Ok, now we split the PDU string into parts and show it */
	if (mdm->mode==MODE_ASCII || mdm->mode==MODE_DIGICOM)
		ret = splitascii(mdm, pdu, sms);
	else
		ret = splitpdu(mdm, &sms_messg, pdu, sms);

	if (ret==-1) {
		LOG(L_ERR,"ERROR:getsms: unable split pdu/ascii!\n");
		goto error;
	}

	DBG("DEBUG:getsms: from %s, sent on %s %s\n",sms->sender,sms->date,
		sms->time);

	deletesms(mdm,found);

	return 1;
error:
	return -1;
}


