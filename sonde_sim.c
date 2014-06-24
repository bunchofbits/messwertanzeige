#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <errno.h>
#include <string.h>

struct opt
{
  int sondVers;
  int sleepTime;
};

static
void printHelp(char *progName)
{
  fprintf(stderr,"usage: %s [-v<version>] [-w<seconds>] [-h]\n\n",progName);
  fprintf(stderr,"-v <version>\tset the version to simulate; standard: 8\n");
  fprintf(stderr,"\t\t<=7: simulate GS07\n");
  fprintf(stderr,"\t\t>=8: simulate GS08\n");
  fprintf(stderr,"-w <seconds>\tset the time befor next output is generated; standard: 1\n");
  fprintf(stderr,"-h\t\tprint this help\n");
  fprintf(stderr,"\n");
  exit(0);
}

static
void parseopt(int argc, char *argv[], struct opt *opts)
{
  opts->sondVers = 8;
  opts->sleepTime = 1;
  
  int o;
  while(1)
    {
      o=getopt(argc, argv, "hv:w:");
      
      if(o < 0)
	break;
      
      switch(o)
	{
	case 'v':
	  opts->sondVers = atoi(optarg);
	  break;
	  
	case 'w':
	  opts->sleepTime = atoi(optarg);
	  if(opts->sleepTime <= 0)
	    opts->sleepTime = 1;
	  break;
	  
	case 'h':
	  
	default:
	  printHelp(argv[0]);
	  break;
	}
    }
}

static
int generateND(int min, int max)
{
  static int i = 0;
  i++;
  i %= 20;
  return i;

  int nd = min;
  double scl;

  scl = (double) (max-min)/(RAND_MAX-0);
  nd =  scl * random()+min;
  
  return nd;
}

static
double generateTemperature(double t1, double maxChange, double minChange, double min, double max)
{
  /* t1: aktuelle Temperatur */
  /* maxChange: wie viel soll sich der Wert maximal veraendern ? */
  /* minChange: wie viel soll sich der Wert minimal veraendern ? */
  /* max: wie hoch ist die maximaltemperatur ? */
  /* min: wie niedrig ist die minimaltemperatur ? */

  double randChange;		/* um wie viel veraendern? */
  int sign;			/* plus(1) oder minus(0) ? */
  int change;			/* ueberhaupt veraendern ? */
  double scl;			/* Scale factor */
  scl = (double) (maxChange-minChange)/(RAND_MAX-0);

  change = random() % 2;

  if(change == 0)
    return t1;			/* Wert nicht aendern */
  
  sign = random() % 2;		/* 1 == +; 0 == - */
  
  
  randChange = scl *random()+minChange;

  if(t1 <= min)
    sign = 1;
  else if(t1 >= max)
    sign = 0;
  
  switch(sign)
    {
    case 0:
      t1 -= randChange;
      break;
      
    case 1:
      t1 += randChange;
      break;
      
    default:
      fprintf(stderr,"%s: something went terribly wrong !\n",__func__);
      return t1;
    }
  
  return t1;
}

static
double generateFeuchte(double f, double maxChange, double minChange, double min, double max)
{
  /* f: aktuelle feuchte */
  /* maxChange: wie viel soll sich der Wert maximal veraendern ? */
  /* minChange: wie viel soll sich der Wert minimal veraendern ? */
  /* max: wie hoch ist die maximaltemperatur ? */
  /* min: wie niedrig ist die minimaltemperatur ? */
  
  static int cnt = -1;
  
  /* nur bei jedem vielfachen von 5. aendern */
  cnt++;
  cnt %= 5;
  if(cnt != 0)
    return f;
  
  f = generateTemperature(f, maxChange, minChange, min, max);
  return f;    
}

static
double generateDruck(double d, double maxChange, double minChange, double min, double max)
{
  /* d: aktuelle feuchte */
  /* maxChange: wie viel soll sich der Wert maximal veraendern ? */
  /* minChange: wie viel soll sich der Wert minimal veraendern ? */
  /* max: wie hoch ist die maximaltemperatur ? */
  /* min: wie niedrig ist die minimaltemperatur ? */
  
  static int cnt = -1;
  
  /* nur bei jedem vielfachen von 5. aendern */
  cnt++;
  cnt %= 5;
  if(cnt != 0)
    return d;
  
  d = generateTemperature(d, maxChange, minChange, min, max);
  return d;    
}

int main(int argc, char *argv[])
{
  static int cnt = 0;
  int nd;			/* niederdosis */
  double t1, t2, t3;		/* temperaturen */
  double d,f;			/* Druck, Rel. Feuchte */
  double x,y,z;			/* beschleunigung */  
  struct opt opts;
  parseopt(argc,argv,&opts);

  srandom(time(NULL));

  nd = 0;
  t1 = 20.0;
  t2 = 20.0;
  t3 = 20.0;
  d = 1000.0;
  f = 40.0;
  x = 0.0;
  y = 0.0;
  z = 0.0;
  
  
  while(1)
    {
      cnt++;
      if (cnt == 11)
	cnt -= 10;
      
      nd = generateND(0,10);
      t1 = generateTemperature(t1, 0.5, 0.01 , -20, +50);
      f = generateFeuchte(f, 0.5, 0.05 , 10, +100);
      if(opts.sondVers == 7)
	{
	  t2 = generateTemperature(t2, 1, 0.1 , -20, +50);
	  t3 = generateTemperature(t3, 2, 0.5 , -50, +80);
	  d = generateDruck(d, 0.1, 0.05 , 990, 1100);
	}
      
      printf("%d %g %g %g %g %g %g %g %g\n", 
	     nd, t1, t2, t3, d, f, x, y, z);
      fflush(stdout);
      sleep(opts.sleepTime);
    }
  
  return EXIT_SUCCESS;
}
