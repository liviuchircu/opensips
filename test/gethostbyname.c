/*
 * $Id$
 *
 */

#include <stdio.h>
#include <errno.h>
#include <unistd.h>
#include <netdb.h>




static char* id="$Id$";
static char* version="gethostbyname 0.1";
static char* help_msg="\
Usage: gethostbyname   [-hV] -n host\n\
Options:\n\
    -n host       host name\n\
    -V            version number\n\
    -h            this help message\n\
";


int main(int argc, char** argv)
{
	char c;
	char* name;
	struct hostent* he;
	unsigned char** h;

	name=0;
	
	opterr=0;
	while ((c=getopt(argc, argv, "n:hV"))!=-1){
		switch(c){
			case 'n':
				name=optarg;
				break;
			case 'V':
				printf("version: %s\n", version);
				printf("%s\n", id);
				exit(0);
				break;
			case 'h':
				printf("version: %s\n", version);
				printf("%s", help_msg);
				exit(0);
				break;
			case '?':
				if (isprint(optopt))
					fprintf(stderr, "Unknown option `-%c�\n", optopt);
				else
					fprintf(stderr, "Unknown character `\\x%x�\n", optopt);
				goto error;
			case ':':
				fprintf(stderr, "Option `-%c� requires an argument.\n",
						optopt);
				goto error;
				break;
			default:
				abort();
		}
	}
	
	if (name==0){
		fprintf(stderr, "Missing domain name (-n name)\n");
		goto error;
	}
	
	he=gethostbyname(name);
	if (he==0) printf("no answer\n");
	else{
		printf("h_name=%s\n", he->h_name);
		for(h=he->h_aliases;*h;h++)
			printf("   alias=%s\n", *h);
		for(h=he->h_addr_list;*h;h++)
			printf("   ip=%d.%d.%d.%d\n", (*h)[0],(*h)[1],(*h)[2],(*h)[3] );
	}
	printf("done\n");
	exit(0);
error:
	exit(-1);
}
