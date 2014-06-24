/*
  testprogramm sonden ein/ausgabe
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <errno.h>
#include <string.h>
#include <fcntl.h>
#include "sonde.h"

#define BUFSIZE (4096)

static char *RCSID __attribute__((unused)) ="$Id: sonde.c,v 1.12 2011/12/15 08:57:22 walter Exp $";


extern int format(char *fmt,struct raw_data *dat); /* output.c */
extern int parse(unsigned char *buf, int size, struct raw_data *data); /* parse.c */

static int read_version(int fd, int silent);

int verbose;

enum { 
  VERBOSE =1, 
  GENITRON=2
};

struct conf {
  int file;
  char *device;
  int option;
  int wait;
  char *outfmt;
  int version;
  int speed;
};

static void _set_clr_rts(int fd,int aktion)
{
  int ret;
  int mod_stat = TIOCM_RTS;  

  ret = ioctl(fd, aktion, &mod_stat);

  if ( ret <0 )         /* Setzen RTS */
    {
      fprintf(stderr,"%s: error=%s\n",__func__, strerror(errno));
    }
  usleep(100);                             /* Warten bis Schnittstelle stabil */
  return;
}

static void set_rts(int fd)
{
  _set_clr_rts(fd,TIOCMBIS);
}



static void clr_rts(int fd)
{
  _set_clr_rts(fd,TIOCMBIC);
}


/*
  configuriere fd
  alte port config in oldterm zurückgeben
*/


void xtcgetattr(int fd,struct termios *term )
{
  int ret= tcgetattr(fd,term);
  if (ret<0) {
    fprintf(stderr,"%s: %s\n",__func__,strerror(errno));
    exit(EXIT_FAILURE);
  }
}

static void xtcsetattr(int fd, int action,struct termios *term ) 
{
  int ret= tcsetattr(fd,action,term);
  if (ret<0) {
    fprintf(stderr,"%s:%s\n",__func__,strerror(errno));
    exit(EXIT_FAILURE);
  }
}

static int find_baud(int fd, struct termios *term)
{
  speed_t speed;
  int ret;
  int cnt;
  cnt = 4;
  ret = -1;
  while (ret < 0 && cnt > 0)
    {
      speed = cfgetispeed(term);

      if(verbose > 0)
      	ret = read_version(fd, 1);
      else
      	ret = read_version(fd, 0);
      if(ret >= 0) // version richtig gelesen; baud richtig
	break;
            
      if(speed == B9600)
	{
	  if(verbose == 2)
	    printf("setting Baudrate 115200\n");
	  cfsetspeed(term, B115200);
	}
      else
	{
	  if(verbose == 2)
	    printf("setting Baudrate 9600\n");
	  cfsetspeed(term, B9600);
	}
      xtcsetattr(fd, TCSANOW, term);

      cnt--;
    }
  
  if(cnt == 0)
    return -1;

  
  return 0;
}

static int port_setup(int fd, struct termios *oldterm, struct conf *cnf )
{
  struct termios *term;
  int ret = 0;
  term=alloca(sizeof (struct termios) );

  xtcgetattr(fd,oldterm);

  xtcgetattr(fd,term);
  
  if(cnf->speed == 9600)
    term->c_cflag  = B9600|CS8|CSTOPB|CREAD|CLOCAL|HUPCL;
  else
    term->c_cflag  = B115200|CS8|CSTOPB|CREAD|CLOCAL|HUPCL;
  
  term->c_iflag  = IGNCR|IGNBRK|IGNPAR;
  term->c_oflag  = 0;
  term->c_lflag  = 0;

  term->c_cc[VMIN]  =  0;
  term->c_cc[VTIME] =  1;

  xtcsetattr(fd,TCSANOW,term);
  

  if(cnf->speed == 0) // wenn -s angegeben, nicht ueberpruefen
    ret = find_baud(fd, term);
  
  return ret;
}

static 
void usage()
{
  fprintf(stderr,"%s -F <device> [-s <speed>] [-w <sekunden>] [ -o <fmt>] [-v] [-V]\n",program_invocation_short_name);
  fprintf(stderr,"%s\n",RCSID);
  fprintf(stderr,"-w default 60s min 1s\n");
  fprintf(stderr,"-s speed in Baud; supported: 9600, 115200 default: 9600\n");
  fprintf(stderr,"-v Verbose mode\n");
  fprintf(stderr,"-o <fmt>\n");
  fprintf(stderr,"-V only Version output\n");
  fprintf(stderr,"fmt defines the output string\n");
  fprintf(stderr,"Interpreted sequences are:\n");
  fprintf(stderr,"%%T Temperatur\n");
  fprintf(stderr,"%%T[id] Temperatur Pos id=1..3\n");
  fprintf(stderr,"%%E Echo Pulses\n");
  fprintf(stderr,"%%N Niederdosis\n");
  fprintf(stderr,"%%H Hochdosis\n");
  fprintf(stderr,"%%K Koinzidenzflag\n");
  fprintf(stderr,"%%S Voltage flag\n");
  fprintf(stderr,"%%X X-Ausrichtung\n");
  fprintf(stderr,"%%Y Y-Ausrichtung\n");
  fprintf(stderr,"%%Z Z-Ausrichtung\n");
  fprintf(stderr,"%%F Feuchte\n");
  fprintf(stderr,"%%D Druck\n");
  fprintf(stderr,"%%%% Procent\n");
  fprintf(stderr,"all escape sequences \\a \\b \\f \\n \\r \\t \\v \n");
  fprintf(stderr,"%%s for epoch\n%%i for ISO8601 datetime format\n");
  exit(EXIT_FAILURE);
}

void parseopt(int argc,char *argv[], struct conf *cnf)
{

  int c;

  while(1)
    {
      c=getopt(argc,argv,"gf:F:w:o:vVs:");

      if (c<0)
	break;

      switch(c)
	{
	case 'f':
	  cnf->file =1;
	  
	case 'F':
	  cnf->device=strdup(optarg);
	  break;
	  
	case 's':
	  cnf->speed = atoi(optarg);
	  if(cnf->speed != 9600 && cnf->speed != 115200)
	    usage();
	  break;

	case 'w':
	  cnf->wait=atoi(optarg);
	  if (cnf->wait <  1 )
	    cnf->wait=1;
	  break;

	case 'v':
	  cnf->option=VERBOSE;
	  verbose++;
	  break;

	case 'g':
	  cnf->option=GENITRON;
	  verbose++;
	  break;

	case 'o':
	  cnf->outfmt=strdup(optarg);
	  break;

	case 'V':
	  cnf->version=1;
	  break;


	default:
	  usage();
	}

    }

  if ( cnf->outfmt == NULL || cnf->outfmt == 0 )
    cnf->outfmt="HD=%H ND=%N\n";


  if (cnf->device == NULL) {
    fprintf(stderr,"no input specified\n");
    usage();
  }


}
/* Based on busybox (file: "libbb/read.c") */
static ssize_t safe_read(int fd, void *buf, size_t count)
{
  ssize_t n;

  do {
    n = read(fd, buf, count);
  } while (n < 0 && errno == EINTR);

  return n;
}

/*
 * Read all of the supplied buffer from a file.
 * This does multiple reads as necessary.
 * Returns the amount read, or -1 on an error.
 * A short read is returned on an end of file.
 */
static ssize_t full_read(int fd, void *buf, size_t len)
{
  ssize_t cc;
  ssize_t total;

  total = 0;

  while (len) {
    cc = safe_read(fd, buf, len);
    if (cc < 0) {
      if (total) {
	/* we already have some! */
	/* user can do another read to know the error code */
	return total;
      }
      return cc; /* read() returns -1 on failure. */
    }
    if (cc == 0)
      break;
    buf = ((char *)buf) + cc;
    total += cc;
    len -= cc;
  }

  return total;
}

static int read_version(int fd, int output)
{
  struct timeval timeout;
  char *buf;
  char *version;
  fd_set rfds;
  int ret;
  int toRead;
  int cnt;

  FD_ZERO(&rfds);
  FD_SET(fd,&rfds);

  clr_rts(fd);
  write(fd,"V",1);
  tcdrain(fd);
  set_rts(fd);
  sleep(1);			
  
  buf = alloca(30);
  version = alloca(20);

  memset(buf,0,sizeof(*buf));

  timeout.tv_sec=1;
  timeout.tv_usec =0;
  ret=select(fd+1,&rfds,NULL, NULL, &timeout);

  if (ret<0) {
    if(output > 0)
      perror("select failed\n");
    exit(EXIT_FAILURE);
  }
  

  if (!ret) {
    if(output > 0)
      fprintf(stderr,"No data within timeout.\n");
    
    /* term = alloca(sizeof(struct termios)); */
    /* xtcgetattr(fd, term); */
    /* find_baud(fd,term); */
    
    return -1;
  }

  if ( ! FD_ISSET(fd, &rfds)) {
    if(output > 0)
      fprintf(stderr,"Unknown data source\n");
    exit(EXIT_FAILURE);
  }
  
  /* read 21 bytes */

  ret = 0;
  cnt = 0;
  toRead = 21;
  
  ret = full_read(fd, buf, 21);
  
  //  ret = read(fd,buf,21);

  sscanf(buf, "%s\n",version);	/* nur erste Zeile  */

  if(output > 0)
    printf("%s\n",version);	/* ausgabe: "V:x.xx" */
  
  return 0;
}

int read_data(int fd, struct raw_data *data, char *out, int flag)
{
  struct timeval timeout;
  static unsigned char buf[BUFSIZE];
  int ret,n,i;
  fd_set rfds;
  
  data->hd=0;
  data->nd=0;

  FD_ZERO(&rfds);
  FD_SET(fd,&rfds);

  clr_rts(fd);
  write(fd,"D",1);
  tcdrain(fd);			/* auf Chip warten */
  //usleep(200);
  /* 
     genitron needs 2 chars
     1. for wakeup
     2. for cmd
  */
  if ( flag == GENITRON )
    {
      write(fd,"D",1);
      tcdrain(fd);		
      usleep(200); 
    }

  set_rts(fd);
  sleep(1);			/* auf Sonde warten; Bei Zeiten <1sec instabil */
  memset(buf,0,sizeof(buf));

  timeout.tv_sec=1;
  timeout.tv_usec =0;
  ret=select(fd+1,&rfds,NULL, NULL, &timeout);

  if (ret<0) {
    perror("select failed\n");
    exit(EXIT_FAILURE);
  }


  if (!ret) {
    fprintf(stderr,"No data within timeout.\n");

    /* term = alloca(sizeof(struct termios)); */
    /* xtcgetattr(fd, term); */
    /* find_baud(fd,term); */
    
    return -1;
  }

  if ( ! FD_ISSET(fd, &rfds)) {
    fprintf(stderr,"Unknown data source\n");
    exit(EXIT_FAILURE);
  }

  /* while() */
  /*   { */
  /*     ret = saveRead(fd,buf,sizeof(buf), ); */
  /*     n=read(fd,buf, sizeof(buf)); */
  /*   } */

  n=full_read(fd, buf, sizeof(buf));
  //n=read(fd,buf, sizeof(buf));
  if (verbose==2) {
    printf("n=%d\n",n);
    for(i=0;i<n;i++) 
      printf("%02x ",buf[i]); if (i%18 == 0) printf("\n");	 
    printf("\n");
  }
	

  if (n >= 54) {
    parse(buf,n,data);                /* no error for now */
    format(out,data);		  /* no error for now */

  } else {
    ret= -1;
  }

  return ret;
}

int read_file(char *file, struct raw_data *data, char *out, int flag)
{
  char buf[BUFSIZE];
  char *pret;
  int ret;
  struct timeval tm;
  static struct timeval old_tm = {.tv_sec = 0, .tv_usec = 0};
  FILE *fp;

  memset(buf, 0, sizeof(buf));

  sleep(1);	// analog zu read_data, sonst Problem bei Option -w1

  fp = fopen(file, "r");
  if( fp == NULL)
    {
      perror("fdopen");
      return -1;
    }
  pret = fgets(buf, sizeof(buf), fp);
  if(pret != buf)
    {
      perror("fread");
      return -1;
    }
  
  tm.tv_sec = atoi(buf);
  
  if(old_tm.tv_sec >= tm.tv_sec)
    goto RETURN;

  old_tm.tv_sec = tm.tv_sec;

  /* Daten aus Datei lesen */
  ret = fread(buf, 1, BUFSIZE, fp);
  if(ret < 0)
    {
      perror("fread");
      return -1;
    }
  if(buf[ret-1] == '\n')
    buf[ret-1] = 0;
  
  parse((unsigned char *)buf,ret-1,data);                /* no error for now */
  format(out,data);		  /* no error for now */
  
  
  RETURN:
  fclose(fp);
  return 0;
}


#if 0
int xread(int fd)
{

  struct timeval timeout;
  static unsigned char buf[BUFSIZE];
  int ret,n,i;
  fd_set rfds;
  //      printf("%s\n",__func__);

  FD_ZERO(&rfds);
  FD_SET(fd,&rfds);

  clr_rts(fd);
  write(fd,"5",1);
  usleep(200); 

  set_rts(fd);
  sleep(1);
  memset(buf,0,sizeof(buf));

  timeout.tv_sec=1;
  timeout.tv_usec =0;
  ret=select(fd+1,&rfds,NULL, NULL, &timeout);

  if (ret<0) {
    perror("select failed\n");
    exit(EXIT_FAILURE);
  }


  if (!ret) {
    fprintf(stderr,"No data within timeout.\n");
    return -1;
  }

  if ( ! FD_ISSET(fd, &rfds)) {
    fprintf(stderr,"Unknown data source\n");
    exit(EXIT_FAILURE);
  }


  n=read(fd,buf, sizeof(buf));

  // if (verbose==2) 
  {
    printf("n=%d\n",n);
    for(i=0;i<n;i++) 
      printf("%02x ",buf[i]); if (i%18 == 0) printf("\n");	 
    printf("\n");
  }

  return ret;
}
#endif


int main(int argc,char *argv[]) {
  int fd;
  int ret;
  int cnt;
  struct termios *oldterm;
  struct conf config={ .wait=60, .version=0, .speed=0, .device=NULL, .file=0};
  struct raw_data raw={ .nd=0, .hd=0 } ;

  setbuf(stdout,NULL); /* disable buffering */
  parseopt(argc,argv,&config);

  if (config.option & VERBOSE ) {
    printf("Wait   : %d [s]\n",config.wait);
    printf("Device : %s\n",config.device);
    printf("File   : %d\n",config.file);
    printf("Baud   : %d\n",config.speed);
    printf("Verbose: %d\n",verbose);
    printf("Genitron: %s\n", config.option & GENITRON ? "yes" : "no" );
  }

  if(config.file == 0)
    {
      oldterm=malloc(sizeof(struct termio));
      fd=open(config.device,O_RDWR|O_EXCL|O_NOCTTY|O_NONBLOCK);
      
      if (fd<0) {
	fprintf(stderr,"cant open %s: %s\n",config.device,strerror(errno));
	exit(EXIT_FAILURE);
      }
      
      ret = port_setup(fd,oldterm, &config);
      
      if(ret < 0)
	{
	  xtcsetattr(fd,TCSANOW,oldterm);
	  close(fd);
	}
    }

  if(config.version == 1)
    {
      ret = -1;
      cnt = 0;
      while(ret < 0 && cnt < 5)
	{
	  ret = read_version(fd, 1);
	  cnt++;
	}
      
      return 0;
    }

  while(1) {

    if(config.file == 1)
      read_file(config.device, &raw, config.outfmt, 0);
    else
      read_data(fd, &raw, config.outfmt, 0 );

    sleep(config.wait -1);	/* sleep(1) in read_data */
  }

#if 0
  while(1) 
    {
      read_data(fd,&raw,config.outfmt,config.option & GENITRON );
      sleep(config.wait-1);  /* read_data has timeout of ca. 1s */
    }
#endif

  xtcsetattr(fd,TCSANOW,oldterm);
  close(fd);
  return 0;
}


