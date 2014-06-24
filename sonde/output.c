#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include "sonde.h"


static char *RCSID __attribute__((unused)) ="$Id: output.c,v 1.3 2010/05/25 12:32:18 walter Exp $";

static char * decode_char(char f,struct raw_data *dat)
{
	static char buf[80];
	time_t now;
	switch (f) {
	case 'X':
		 snprintf(buf,sizeof(buf),"%g",dat->x);
		 return buf;
	case 'Y':
		 snprintf(buf,sizeof(buf),"%g",dat->y);
		 return buf;
	case 'Z':
		 snprintf(buf,sizeof(buf),"%g",dat->z);
		 return buf;

	case 'D':
		 snprintf(buf,sizeof(buf),"%g",dat->druck);
		 return buf;
	case 'F':
		 snprintf(buf,sizeof(buf),"%g",dat->feuchte);
		 return buf;

	case 'E':
		 snprintf(buf,sizeof(buf),"%d",dat->echo);
		 return buf;

	case 'N':
		 snprintf(buf,sizeof(buf),"%d",dat->nd);
		 return buf;

	case 'H':
		 snprintf(buf,sizeof(buf),"%d",dat->hd);
		 return buf;

	case 'K':
		 snprintf(buf,sizeof(buf),"%d",dat->koinz);
		 return buf;

	case 'S':
		 snprintf(buf,sizeof(buf),"%d",dat->spannung);
		 return buf;


	case 's':
		 snprintf(buf,sizeof(buf),"%ld",time(NULL));
		 return buf;
	case 'i':
		 time(&now);
		 strftime( buf,sizeof(buf),"%Y-%m-%dT%H:%M:%S",gmtime(&now)); 
		 return buf;

	case '%':
		return "%";

	default:
		fprintf(stderr,"unknown specifier %%%c\n",f);
		exit(EXIT_FAILURE);
	}

	return NULL;
}


static char *decode_T(char f,struct raw_data *dat)
{
	static char buf[80];
	int cnt;

	cnt=0;
	switch (f) {
	case '2':
		cnt=1;
		 break;
      	
	case '3':
		cnt=2;
		 break;
      		 
	default:
		cnt=0;
		
	}
	snprintf(buf,sizeof(buf),"%g",dat->temp[cnt]);
	return buf;
}



static int decode_esc(int c) {
	switch (c) {
	case 'a':			/* alert */
		return 0x07;
	case 'b':			/* backspace */
		return 0x08;
	case 'f':			/* formfeed */
		return 0x0c;
	case 'n':			/* new line */
		return 0x0a;
	case 'r':			/* carriage return */
		return 0x0d;
	case 't':			/* hor. tab */
		return 0x09;
	case 'v':			/* vert. tab */
		return 0x0b;
	default:
		return (char)c;
	}
}

int format(char *fmt,struct raw_data *dat)
{
	char *f;
	size_t p=0;
	FILE *fp=stdout;

	if ( fmt == NULL || *fmt==0 )
		return 0;

  	for(f=fmt; *f ; ++f) {

		/*
		  if current char == '%' and procent flag not set
		  set it and continue with string parsing
		*/

		if (*f=='%' && p == 0 ) {
			p=1;
			continue;
		}


		/* 
		   its a string flag set
		   now handle the next character
		*/

		if (p==1) {
			if (*f == 'T' )
			{
				f++;
				fprintf(fp,"%s",decode_T(*f,dat));
			}
			else
			{
				fprintf(fp,"%s",decode_char(*f,dat));
			}
			p=0;
			continue;
		}           


		if ( *f=='\\' && p == 0 ) {
			p=2;
			continue;
		}


		if (p==2) {
			p=0;
			fputc(decode_esc(*f),fp);
			continue;
		}

		fputc(*f,fp);

	} /* for */

	return 0;
}

