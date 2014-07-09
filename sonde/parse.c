/* parser fuer sonden daten */
#define _GNU_SOURCE
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdint.h>

#include "sonde.h"
static char *RCSID __attribute__((unused)) ="$Id: parse.c,v 1.4 2010/05/25 12:32:01 walter Exp $";

struct  funktion  {
  char *pattern;
  int (*function) ();
};



static double do_sign(char vz, unsigned int v1)
{

  if (vz=='-')
    return -1.0*v1;
  if (vz=='+')
    return v1;

  return strtod ("NAN",NULL);
  //	return nan("NaN");
}


static int parse_hex(char *test, double *val)
{
  //	int v1,v2,v3;
  int ret;
  unsigned int vor,nach;
  /* int e1,e2; */
  char e[2];

  /* e2=0; */
  /* e1=0; */
  ret=sscanf(test,"%[+-]%x%[+-]%x",&e[0],&vor,&e[1],&nach);

  switch(ret) {
  case 4:
    *val=do_sign(e[0],vor)*pow(16,do_sign(e[1],nach));
    break;
  case 2:
    *val=do_sign(e[0],vor);
    break;
  default:
    *val=strtod ("NAN",NULL);
    ret=-1;
  }

  //	printf("%f\n",val);
  return ret;

}




#ifdef OLD_TEMP
static int temp_funktion(char *num, struct raw_data *data)
{
  int in=(int16_t)strtol(num,(char **)NULL, 16);
  if (verbose == 3)
    printf("%s: %s\n",__func__,num);

  data->temp=(double)in/256.0;
  return 0;
}
#else

/*
  die GS07 hat 3 temp angaben T1..3
  die GS05 hat nur T

*/

static int temp_funktion(char *token,char *num, struct raw_data *data)
{
  int cnt;

  if (verbose == 3)
    printf("%s: %s\n",__func__,num);

  cnt=0;
  if (token[1]=='2') cnt=1;
  if (token[1]=='3') cnt=2;

  if (parse_hex(num,&data->temp[cnt]) < 0 )
    fprintf(stderr,"%s:parse error: %s\n",__func__,num);

  return 0;
}
#endif



static int achse_funktion(char *token,char *num, struct raw_data *data)
{
  double value;

  if (verbose == 3)
    printf("%s: %s\n",__func__,num);

  if (parse_hex(num,&value) < 0 )
    fprintf(stderr,"%s:parse error: %s\n",__func__,num);

  if (token[0]=='X') data->x=value;
  if (token[0]=='Y') data->y=value;
  if (token[0]=='Z') data->z=value;

  return 0;
}


static int druck_funktion(char *token,char *num, struct raw_data *data)
{
  if (verbose == 3)
    printf("%s: %s\n",__func__,num);

  if (parse_hex(num,&data->druck) < 0 )
    fprintf(stderr,"%s:parse error: %s\n",__func__,num);

  return 0;
}

static int feuchte_funktion(char *token,char *num, struct raw_data *data)
{
  if (verbose == 3)
    printf("%s: %s\n",__func__,num);

  if (parse_hex(num,&data->feuchte) < 0 )
    fprintf(stderr,"%s:parse error: %s\n",__func__,num);

  return 0;
}



static int niederd_funktion(char *token,char *num, struct raw_data *data)
{
  if (verbose == 3)
    printf("%s: %s\n",__func__,num);

  data->nd=strtoul(num,(char **)NULL, 16);
  return 0;
}

static int hochd_funktion(char *token,char *num, struct raw_data *data)
{
  if (verbose == 3)
    printf("%s: %s\n",__func__,num);
  data->hd=strtoul(num,(char **)NULL, 16);
  return 0;
}

static int echo_funktion(char *token,char *num, struct raw_data *data)
{
  if (verbose == 3)
    printf("%s: %s\n",__func__,num);
  data->echo=strtoul(num,(char **)NULL, 16);
  return 0;
}

static int koinz_funktion(char *token,char *num, struct raw_data *data)
{
  if (verbose == 3)
    printf("%s: %s\n",__func__,num);
  data->koinz=strtoul(num,(char **)NULL, 16);
  return 0;
}

static int spannung_funktion(char *token,char *num, struct raw_data *data)
{
  if (verbose == 3)
    printf("%s: %s\n",__func__,num);
  data->spannung=strtoul(num,(char **)NULL, 16);
  return 0;
}





static struct funktion fktab[] ={
  {"N",niederd_funktion},
  {"H",hochd_funktion},
  {"K",koinz_funktion},
  {"E",echo_funktion},
  {"T",temp_funktion},
  {"T1",temp_funktion},
  {"T2",temp_funktion},
  {"T3",temp_funktion},
  {"S",spannung_funktion},
  {"X",achse_funktion},
  {"Y",achse_funktion},
  {"Z",achse_funktion},
  {"F",feuchte_funktion},
  {"D",druck_funktion}
  //  {NULL,NULL}

};

#define MAXFKTAB sizeof(fktab)/sizeof(fktab[0])


static void handle(char *str,struct raw_data *data) 
{
  char *rstr;  
  char *arg1,*arg2;
  int cnt=0;

  rstr=(char *)strsep(&str,":");
  if (str == NULL)
    {
      printf("%s: parse error\n",__func__);
      return ;
    }

  arg1=(char *)strdupa(rstr);   /* arg 1 */	
  arg2=(char *)strdupa(str);   /* arg 2 */

  for (cnt=0;cnt<MAXFKTAB;cnt++)
    {
      if (strcasecmp(fktab[cnt].pattern,arg1) == 0) {

	if (verbose == 3) 
	  printf("%s: found %s arg:%s\n",__func__,arg1,arg2);

	fktab[cnt].function(arg1,arg2,data);
	cnt=-1;
	break;
      }
    }

  if (cnt!= -1)     {
    printf("unknown name %s\n",arg1);
  }
    
}

int parse(unsigned char *buf, int size, struct raw_data *data)
{
  //	char *end;
  char *str,*rstr;
  size_t ende;
	
  ende=strcspn((char *)buf,"\r\n");
  if (ende<size)
    buf[ende]=0;
  else {
    fprintf(stderr,"no end found\n");
    return 0;
  }

#if 0	
  end=strchr(buf,'\r');

  if (end != NULL)
    *end=0;
#endif
  str=strdupa((char *)buf);
  
  if (verbose == 3)
    printf("%s: %s\n",__func__,str);

  while(1) {
    rstr=(char *)strsep(&str," \t");  
    if (rstr == NULL )  break;
    handle(rstr,data);
  } 

  return 0;
}
