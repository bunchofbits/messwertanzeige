#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <assert.h> 		/* drawRotated */
#include <errno.h>
#include <termios.h>
#include <fcntl.h>
#include <stdarg.h>
#include <locale.h>
#include <signal.h>

#include <Xm/Protocols.h>

#include <sys/types.h>
#include <sys/wait.h>

#include <Xm/Xm.h>
#include <Xm/Form.h>
#include <Xm/PushB.h>
#include <Xm/DrawingA.h>
#include <Xm/TextF.h>
#include <Xm/Label.h>
#include <Xm/LabelG.h>

#include <Xm/Frame.h>
#include <Xm/RowColumn.h>

/* #define DEBUG */

#define BUFSIZE 240

#define COLOR_GRID         0x444444 /* farbe fuer Gitter */
#define COLOR_POINTER_MASK 0xff0000 /* Zeigerfarbe */
#define COLOR_1_SEC        0x004400 /* Farbe 1 sec Daten */
#define COLOR_10_SEC    0x00bb00 /* Farbe 10 sec Daten */
#define COLOR_30_SEC    0xff00ff /* Farbe 30 sec Daten */
#define COLOR_60_SEC    0x00ffff /* Farbe 60 sec Daten */
#define COLOR_GLAETTUNG 0xff0000 /* Farbe geglaettete Daten */

static void cleanup(int ex, void *arg);
static void updateImages(double val, int is_new_val);

static int pointerColors[] = {
  0xff0000,			/* Rot */
  0xff1100,			/* . */
  0xff2200,			/* . */
  0xff3300,			/* . */
  0xff4400,			/* . */
  0xff5500,			/* . */
  0xff6600,			/* . */
  0xff7700,			/* . */
  0xff8800,			/* . */
  0xff9900,			/* . */
  0xffaa00,			/* . */
  0xffbb00,			/* . */
  0xffcc00,			/* . */
  0xffdd00,			/* . */
  0xffee00,			/* . */
  0xffff00,			/* Gelb */
  0xeeff00,			/* . */
  0xddff00,			/* . */
  0xccff00,			/* . */
  0xbbff00,			/* . */
  0xaaff00,			/* . */
  0x99ff00,			/* . */
  0x88ff00,			/* . */
  0x77ff00,			/* . */
  0x66ff00,			/* . */
  0x55ff00,			/* . */
  0x44ff00,			/* . */
  0x33ff00,			/* . */
  0x22ff00,			/* . */
  0x11ff00,			/* . */
  0x00ff00			/* Gruen */
};

struct drawStruct{
  void (*backgroundFillFunc)(void);
  GC gc;			/* GC fuer pixmap */
  XGCValues gcv;		/* GCValues */
  int tiefe;			/* farbtiefe */
  Pixmap backMap;		/* hindergrund pixmap */
  Pixmap foreMap;		/* vordergrund pixmap */
  int l_bord;			/* left Border */
  int r_bord;			/* right Border */
  int top_bord;			/* top Border */
  int bot_bord;			/* bottom Border */
  int vert_lines;		/* # vertical lines */
  int hor_lines;		/* # horizontal lines */
  Widget draw;		 
  Dimension sizeX;
  Dimension sizeY;
  Display *dpy;
};

struct global{
  int verbose;			/* verboselevel */
  int print_help;		/* flag to call print_help() */

  int is_test;			/* flag if programtest */
  int sondTyp;			/* Sondentyp */
  int use_stdin;
  int time_base;
  char *exponent_alpha;
  char *port;
  int graph1;
  int graph2;
  int graph3;
  int graph4;
  int graph5;
  
  int no_table;
  int no_analog;
  int no_timeline;

  double max_value;
  
  GC gc_pointer;		/* GC fuer pointer */

  int win_sizeX;		/* fenster X */
  int win_sizeY;		/* fenster Y */

  struct drawStruct draw_point;
  struct drawStruct draw_avg;
  struct drawStruct draw_sens;

  double val1[BUFSIZE];		/* 1sec werte */
  double val2[BUFSIZE];		/* durchschnittswerte */
  double val3[BUFSIZE];		/* durchschnittswerte 30sec */
  double val6[BUFSIZE];		/* durchschnittswerte 60sec */
  double avg_exp[BUFSIZE];	/* gleitender mittelwert*/
  Widget sens_lout;		/* Layout mit Sensor -Widgets */
  
  int drawPointTo[2];		/* [0] x-Wert; [1] y-Wert; Punkt an den Zeiger laufen soll */
  int pointer_radius;		/* radius fuer zeigerlaenge */
  double pointer_winkel_ziel_exp; /* zielwinkel fuer Glattungszeiger */
  double pointer_winkel_ziel;	  /* zielwinkel fuer Zeiger - ungeglaettet */
} glob;

static XrmOptionDescRec options[] = {
  {"-verbose",          "*verbose",             XrmoptionNoArg,       "True"},
  {"-no_table",         "*no_table",            XrmoptionNoArg,       "True"},
  {"-no_analog",        "*no_analog",           XrmoptionNoArg,       "True"},
  {"-no_timeline",      "*no_timeline",         XrmoptionNoArg,       "True"},
  {"-help",		"*print_help",        	XrmoptionNoArg,       "True"},
  {"-test",		"*is_test",        	XrmoptionNoArg,       "True"},
  {"-typ",		"*sondTyp",        	XrmoptionSepArg,	NULL},
  {"-use_stdin",        "*use_stdin",           XrmoptionNoArg,       "True"},
  {"-timebase", 	"*time_base",        	XrmoptionSepArg,	NULL},
  {"-exponent",		"*exponent",        	XrmoptionSepArg,	NULL},
  {"-port",		"*port",        	XrmoptionSepArg,	NULL},
  {"-raw_graph",        "*raw_graph",           XrmoptionSepArg,      "True"},
  {"-graph_1",          "*avg1",                XrmoptionSepArg,	NULL},
  {"-graph_2",          "*avg2",                XrmoptionSepArg,	NULL},
  {"-graph_3",          "*avg3",                XrmoptionSepArg,	NULL},
  {"-expo_graph",       "*expo_graph",          XrmoptionSepArg,      "True"}
};

//XLoadResources resources;
#define Offset(field) (XtOffsetOf(struct global, field))
static XtResource my_resources[] = {
  {"verbose", XtCValue, XtRBoolean, sizeof(Boolean),
   Offset(verbose), XtRString, "False"},

  {"no_table", XtCValue, XtRBoolean, sizeof(Boolean),
   Offset(no_table), XtRString, "False"},

  {"no_analog", XtCValue, XtRBoolean, sizeof(Boolean),
   Offset(no_analog), XtRString, "False"},

  {"no_timeline", XtCValue, XtRBoolean, sizeof(Boolean),
   Offset(no_timeline), XtRString, "False"},

  {"print_help", XtCBoolean, XtRBoolean, sizeof(Boolean),
   Offset(print_help), XtRString, "False"},

  {"is_test", XtCBoolean, XtRBoolean, sizeof(Boolean),
   Offset(is_test), XtRString, "False"},

  {"sondTyp", XtCValue, XmRInt, sizeof(int),
   Offset(sondTyp), XmRString, "7"},

  {"time_base", XtCValue, XmRInt, sizeof(int),
   Offset(time_base), XmRString, "1"},

  {"use_stdin", XtCBoolean, XtRBoolean, sizeof(Boolean),
   Offset(use_stdin), XtRString, "False"},

  {"exponent", XtCValue, XmRString, sizeof(char *),
   Offset(exponent_alpha), XmRString, "0.3"},

  {"port", XtCValue, XmRString, sizeof(char *),
   Offset(port), XmRString, "/dev/ttyS0"},

  {"raw_graph", XtCValue, XmRBoolean, sizeof(Boolean),
   Offset(graph1), XmRString, "1"},

  {"avg1", XtCValue, XmRInt, sizeof(int),
   Offset(graph2), XmRString, "10"},

  {"avg2", XtCValue, XmRInt, sizeof(int),
   Offset(graph3), XmRString, "30"},

  {"avg3", XtCValue, XmRInt, sizeof(int),
   Offset(graph4), XmRString, "60"},

  {"expo_graph", XtCValue, XmRBoolean, sizeof(Boolean),
   Offset(graph5), XmRString, "1"}
};
#undef Offset

char *fallback[] = {
  /* GS 07 */
  "*.background: black",
  "*.sht11tLbl.labelString: Temperatur",
  "*.sht11tValLbl.labelString: ",
  "*.sht11tValLbl.foreground: green",
  "*.sht11fLbl.labelString: Rel. Feuchte",
  "*.sht11fValLbl.labelString: ",
  "*.sht11fValLbl.foreground: green",
  "*.sht11ImgLbl.labelType: XmPIXMAP",
  "*.sht11ImgLbl.labelPixmap: imgs/SHT11.xpm",

  "*.bmp085tLbl.labelString: Temperatur",
  "*.bmp085tValLbl.labelString: ",
  "*.bmp085tValLbl.foreground: green",
  "*.bmp085dLbl.labelString: Luftdruck",
  "*.bmp085dValLbl.labelString: ",
  "*.bmp085dValLbl.foreground: green",
  "*.bmp085ImgLbl.labelType: XmPIXMAP",
  "*.bmp085ImgLbl.labelPixmap: imgs/BMP085.xpm",

  "*.bma150tLbl.labelString: Temperatur",
  "*.bma150tValLbl.labelString: ",
  "*.bma150tValLbl.foreground: green",
  "*.bma150xLbl.labelString: Beschleunigung X",
  "*.bma150xValLbl.labelString: ",
  "*.bma150xValLbl.foreground: green",
  "*.bma150yLbl.labelString: Beschleunigung Y",
  "*.bma150yValLbl.labelString: ",
  "*.bma150yValLbl.foreground: green",
  "*.bma150zLbl.labelString: Beschleunigung Z",
  "*.bma150zValLbl.labelString: ",
  "*.bma150zValLbl.foreground: green",
  "*.bma150ImgLbl.labelType: XmPIXMAP",
  "*.bma150ImgLbl.labelPixmap: imgs/BMA150.xpm",

  /* GS08 */
  "*.sht21tLbl.labelString: Temperatur",
  "*.sht21tValLbl.labelString: ",
  "*.sht21tValLbl.foreground: green",
  
  "*.sht21fLbl.labelString: Rel. Feuchte",
  "*.sht21fValLbl.labelString: ",
  "*.sht21fValLbl.foreground: green",
  "*.sht21ImgLbl.labelType: XmPIXMAP",
  "*.sht21ImgLbl.labelPixmap: imgs/SHT21.xpm",

  //  "*.sht21ImgLbl.background: green",
  
  NULL
};

/* Based on Gnuplot DrawRotated() */
static
void drawRotatedString(Display *dpy, GC gc, Drawable d, 
		       int xdest, int ydest, 
		       double angle,
		       const char *str, int len)
{
  int x, y;
  double src_x, src_y;
  double dest_x, dest_y;
  XFontStruct *font = XLoadQueryFont(dpy, "fixed");
  int width = XTextWidth(font, str, len);
  int height = font->ascent + font->descent;
  double src_cen_x = (double)width * 0.5;
  double src_cen_y = (double)height * 0.5;
  static const double deg2rad = .01745329251994329576; /* atan2(1, 1) / 45.0; */
  double sa = sin(angle * deg2rad);
  double ca = cos(angle * deg2rad);
  int dest_width = (double)height * fabs(sa) + (double)width * fabs(ca) + 2;
  int dest_height = (double)width * fabs(sa) + (double)height * fabs(ca) + 2;
  double dest_cen_x = (double)dest_width * 0.5;
  double dest_cen_y = (double)dest_height * 0.5;
  char* data = (char*) malloc(dest_width * dest_height * sizeof(char));
  Pixmap pixmap_src = XCreatePixmap(dpy, DefaultRootWindow(dpy), (unsigned int)width, (unsigned int)height, 1);
  XImage *image_src;
  XImage *image_dest;
  unsigned long fgpixel = 0;
  unsigned long bgpixel = 0;
  GC gcTmp;
  int scr = DefaultScreen(dpy);

  unsigned long gcFunctionMask = GCFunction;
  XGCValues gcValues;
  int gcCurrentFunction = 0;
  Status s;

  /* bitmapGC is static, so that is has to be initialized only once */
  static GC bitmapGC = (GC) 0;

  gcTmp = XCreateGC(dpy, d, 0, (XGCValues *) 0);
  XCopyGC(dpy, gc, 0, gcTmp);


  /* eventually initialize bitmapGC */
  if ((GC)0 == bitmapGC) {
    bitmapGC = XCreateGC(dpy, pixmap_src, 0, (XGCValues *) 0);
    XSetForeground(dpy, bitmapGC, 1);
    XSetBackground(dpy, bitmapGC, 0);
  }

  s = XGetGCValues(dpy, gc, gcFunctionMask|GCForeground|GCBackground, &gcValues);
  if (s) {
    /* success */
    fgpixel = gcValues.foreground;
    bgpixel = gcValues.background;
    gcCurrentFunction = gcValues.function; /* save current function */
  }

  /* set font for the bitmap GC */
  if (font)
    XSetFont(dpy, bitmapGC, font->fid);

  /* draw string to the source bitmap */
  XDrawImageString(dpy, pixmap_src, bitmapGC, 0, font->ascent, str, len);

  /* create XImage's of depth 1 */
  /* source from pixmap */
  image_src = XGetImage(dpy, pixmap_src, 0, 0, (unsigned int)width, (unsigned int)height,
			1, XYPixmap /* ZPixmap, XYBitmap */ );

  /* empty dest */
  assert(data);
  memset((void*)data, 0, (size_t)dest_width * dest_height);
  image_dest = XCreateImage(dpy, DefaultVisual(dpy, DefaultScreen(dpy)), 1, XYBitmap,
			    0, data, (unsigned int)dest_width, (unsigned int)dest_height, 8, 0);

#define RotateX(_x, _y) (( (_x) * ca + (_y) * sa + dest_cen_x))
#define RotateY(_x, _y) ((-(_x) * sa + (_y) * ca + dest_cen_y))
  /* copy & rotate from source --> dest */
  for (y = 0, src_y = -src_cen_y; y < height; y++, src_y++) {
    for (x = 0, src_x = -src_cen_x; x < width; x++, src_x++) {
      /* TODO: move some operations outside the inner loop (joze) */
      dest_x = rint(RotateX(src_x, src_y));
      dest_y = rint(RotateY(src_x, src_y));
      if (dest_x >= 0 && dest_x < dest_width && dest_y >= 0 && dest_y < dest_height)
	XPutPixel(image_dest, (int)dest_x, (int)dest_y, XGetPixel(image_src, x, y));
    }
  }

  assert(s); /* Previous success in reading XGetGCValues() */
  /* Force pixels of new text to black, background unchanged */
  /*gcValues.function = GXand;*/
  gcValues.background = BlackPixel(dpy, scr);
  gcValues.foreground = WhitePixel(dpy, scr);
  XChangeGC(dpy, gcTmp, /*gcFunctionMask|*/GCBackground|GCForeground, &gcValues);
  XPutImage(dpy, d, gcTmp, image_dest, 0, 0, xdest, ydest, dest_width, dest_height);
  
  XFreePixmap(dpy, pixmap_src);
  XDestroyImage(image_src);
  XDestroyImage(image_dest); 
}

static
void set_res(Widget w, char *res, char *buf)
{
  XtVaSetValues(w, XtVaTypedArg,
		res, XtRString, buf, strlen(buf) + 1, /* setze Ressource */
		NULL);
}

static
Widget getWidgetByName(Widget ref, char *name)
{
  return XtNameToWidget(ref, name);
}

static
void printWidget(Widget w, const char *fmt, ...){
  
  char *cmd;
  
  va_list ap;
  va_start(ap, fmt);

  vasprintf(&cmd, fmt, ap );
  
  va_end(ap);
  set_res(w, XmNlabelString, cmd); /* nur fuer Label */
  free(cmd);
}

static
double calc_uSv(double count)
{
  /* Calculate the ODL in uGy/h */
  double diff;
  double odl=0.0;
  double eigen_nd= 15;		/* eigene dosis des zaehlrohrs */
  double empf_nd = 972;		/* relative Empfindlichkeit */
  double totzeit_nd = 3.3e-06;	/* Totzeit vom Zaehlrohr */
  double korr1_nd = -2.45e-06;	/* faktor */
  double korr2_nd = -1.17e-11;	/* faktor */
  double korr3_nd = 9.16e-17;	/* faktor */
  double korr4_nd = -1.64e-22;	/* faktor */

  double  Nob= 1/totzeit_nd; /* max Impulse */

  /* nur rechnen wenn count > eigeneffekt */
  if (count > eigen_nd )
    {
      diff=count-eigen_nd;

      if (count > Nob)
	count = Nob;

      odl = diff/(empf_nd*(1.0-korr1_nd*count
			   +korr2_nd*count*count
			   -korr3_nd*count*count*count
			   +korr4_nd*count*count*count*count
			   )    
		  );
    }
  /* kann bei overflow passieren */
  if (odl<0) odl=-2.0;

  return odl;
}

static
void expose_cb(Widget w, XtPointer clientData, XtPointer callData)
{  
  struct drawStruct *str = (struct drawStruct *)clientData;  
  //  puts(__func__);
  XCopyArea( str->dpy, str->foreMap, XtWindow(str->draw), str->gc,
	     0, 0, str->sizeX, str->sizeY, 0, 0);
}

static
void resize_cb(Widget w, XtPointer clientData, XtPointer callData)
{
  struct drawStruct *str = (struct drawStruct *)clientData;
  Dimension width, height;
  XtVaGetValues(w,
		XmNheight, &height,
		XmNwidth, &width,
		NULL);
  str->sizeX = width;
  str->sizeY = height;
  
  XFreePixmap(str->dpy, str->backMap);
  str->backMap = XCreatePixmap(str->dpy,
			       RootWindowOfScreen(XtScreen(str->draw)),
			       str->sizeX, str->sizeY, str->tiefe);

  XFreePixmap(str->dpy, str->foreMap);
  str->foreMap = XCreatePixmap(str->dpy,
			       RootWindowOfScreen(XtScreen(str->draw)),
			       str->sizeX, str->sizeY, str->tiefe);
  
  str->backgroundFillFunc();

  updateImages(99, 0); // no new value
}

static
int getSondTyp()
{
  FILE *fp;
  double v;
  int vers;
  int ret;
  char *call;
  asprintf(&call, "sonde -F %s -V", glob.port); 
  fp = popen(call, "r"); /* Pipe open */
  if(fp == NULL)
    {
      perror("popen error");
      return -1;
    }
  free(call);

  alarm(10); // 10 sec timeout
  ret = fscanf(fp, "V:%lf\n", &v);
  if(ret < 1)
    {
      perror("error reading QIS Version");
      exit(1);
    }
  alarm(0); // reset alarm

  if(v == 1.61)
    vers = 7;
  else if(v == 2.71)
    vers = 8;

  pclose(fp);

  return vers;
}

static
void drawPointer(Widget w, int x, int y, int color)
{
  if(XtWindow(w) == 0)
    return;

  /* printf("dpy    : %p\n",glob.draw_point.dpy); */
  /* printf("backMap: %p\n",glob.draw_point.backMap); */
  /* printf("foreMap: %p\n",glob.draw_point.foreMap); */
  /* printf("gc     : %p\n",glob.draw_point.gc); */
  /* Fenster loeschen  */
  XCopyArea( glob.draw_point.dpy,
	     glob.draw_point.backMap, 
	     glob.draw_point.foreMap, 
	     glob.draw_point.gc,
	     0, 0, 
	     glob.draw_point.sizeX, glob.draw_point.sizeY, 
	     0, 0);
  XSetPlaneMask(glob.draw_point.dpy, glob.gc_pointer, color); /* Neue Zeigerfarbe setzten */
  
  XDrawLine(glob.draw_point.dpy, glob.draw_point.foreMap, glob.gc_pointer, 
	    glob.draw_point.sizeX/2, glob.draw_point.sizeY, x, y);
  
  expose_cb(NULL, &glob.draw_point, NULL);
}

static
void setExpPointer_tm(XtPointer clientData, XtIntervalId *id)
{
  static int i = 0;			/* schrittzaehler */
  static int x = 0;			/* punkt x */
  static int y = 0;			/* punkt y */
  static double winkel = M_PI;		/* position des zeigers; am anfang links */
  int anz_schritt = 50;
  double winkel_ziel = glob.pointer_winkel_ziel_exp; /* sonst spiegelverkehrt */
  static double winkel_schritt = 0.0;		     /* schrittweite */
  int color;
  double scl;

  if(winkel_schritt == 0.0)	/* immer nur am anfang */
    winkel_schritt = ((winkel_ziel - winkel) /anz_schritt); /* schrittweite ausrechnen */
  
  
  if(i == anz_schritt)
    {
      i = 0;
      winkel_schritt = 0.0;
      return;
    }
  i++;
  
  winkel += winkel_schritt;

  /* Zeigerfarbe finden */
  /* erg =       (       Anzahl der Farben   /    "laenge" der anzeige (pi)  ); */
  scl = (double) ( (sizeof(pointerColors) / sizeof(pointerColors[0])) / M_PI );
  color = pointerColors[(int) (scl * winkel)];
  
  /* zielposition finden */
  x = glob.pointer_radius *sin( winkel +(M_PI/2)) + glob.draw_point.sizeX/2;
  y = glob.pointer_radius *cos( winkel +(M_PI/2)) + glob.draw_point.sizeY;

  drawPointer(glob.draw_point.draw, x, y, color);
  
  XtAppAddTimeOut(XtWidgetToApplicationContext(glob.draw_point.draw), 
		  500/anz_schritt, setExpPointer_tm, clientData);
}

static
double average(int elements, int cnt, double saveIn[])
{
  /* elements: anzahl der elemente ueber die ein mittelwert gebildet werden soll */
  /* cnt: aktuelle position im ringspeicher */
  int i;
  double avg = 0.0;
  int anzahl = cnt%elements;	/* anzahl der elemente ueber die der mittelwert gebildet wird */
  int pos = cnt - anzahl;	/* position des ersten werts fuer den mittelwert */

  /* werte aufaddieren */
  for(i = 0; i <= anzahl; i++)
    avg += glob.val1[pos+i];
  
  /* mittelwerte auffuellen */
  for(i = 0; i <= anzahl; i++)
    saveIn[pos+i] = avg / (anzahl +1);

  return avg / (anzahl +1) ;
}

static
double calc_exp_avg(int cnt, double saveIn[])
{
  double vor;			      /* vorganger */
  double alpha = atof(glob.exponent_alpha); // glob.exponent_alpha; /* factor */
  vor = saveIn[(cnt+BUFSIZE-1)%BUFSIZE];

  /* ergebnis =factor * aktueller Wert+ (1-factor)* vorgaenger */
  saveIn[cnt] = alpha * glob.val1[cnt] + (1-alpha) * vor;
  
  return saveIn[cnt];
}

static
void draw_graph_on_pixmap(int len, int cnt, double *val, int color)
{
  double rval;
  double min = 0.0;
  int factor;
  int i;
  int n;
  XPoint points[len];		/* punkte fuer 1 sec daten */
  double pnt_dist;
  int draw_height;
  int draw_width;

  Dimension pix_width, pix_height;
  XtVaGetValues(glob.draw_avg.draw,
		XmNwidth, &pix_width,
		XmNheight, &pix_height,
		NULL);

  draw_width = pix_width-glob.draw_avg.l_bord-glob.draw_avg.r_bord;
  draw_height = pix_height-glob.draw_avg.top_bord-glob.draw_avg.bot_bord;
  
  pnt_dist = (double) draw_width/len;

  factor = ((draw_height - 0.0) / (glob.max_value - min));
  memset(points, 0, sizeof(XPoint) * len);
  n = 0;

  /* beim aeltesten wert anfangen zu zeichnen */
  for(i = 1; i < len+1; i++) 
    {
      rval = factor * val[(cnt+i)%len]; // cnt= actual_val ;; cnt+1=oldest_val
      points[n].x = glob.draw_avg.l_bord + i *pnt_dist;
      points[n].y = glob.draw_avg.top_bord + (draw_height - rval);
      n++;
    }
  
  XSetForeground(glob.draw_avg.dpy, glob.draw_avg.gc, color); 

  XDrawLines(glob.draw_avg.dpy, glob.draw_avg.foreMap, glob.draw_avg.gc,
	     points, n, CoordModeOrigin );
}

static
void draw_averages(int cnt)
{
  int anz_pnt_1;		/* anzazhl punkte 1 sec daten */
				/* Mittelwerte Zeichnen */
  XCopyArea( glob.draw_avg.dpy,	    /* display */
	     glob.draw_avg.backMap, /* src */
	     glob.draw_avg.foreMap, /* dest */
	     glob.draw_avg.gc,	    /* gc */
	     0, 0,		    /* src width, src height */
	     glob.draw_avg.sizeX, glob.draw_avg.sizeY, /* width, height */
	     0, 0);		/* dest width, dest height */
  anz_pnt_1 = 0;

  if(glob.graph1 > 0)
    draw_graph_on_pixmap(BUFSIZE, cnt, glob.val1, COLOR_1_SEC);
  if(glob.graph2> 0)
    draw_graph_on_pixmap(BUFSIZE, cnt, glob.val2, COLOR_10_SEC);
  if(glob.graph3 > 0)
    draw_graph_on_pixmap(BUFSIZE, cnt, glob.val3, COLOR_30_SEC);
  if(glob.graph4 > 0)
    draw_graph_on_pixmap(BUFSIZE, cnt, glob.val6, COLOR_60_SEC);
  if(glob.graph5 > 0)
    draw_graph_on_pixmap(BUFSIZE, cnt, glob.avg_exp, COLOR_GLAETTUNG);
  
  expose_cb(NULL, (XtPointer) &glob.draw_avg , NULL);
}

static
void draw_analog()
{
  XtAppAddTimeOut(XtWidgetToApplicationContext(glob.draw_point.draw), 1, setExpPointer_tm, NULL);
}

static
void updateImages(double val, int is_new_val)
{ 
  static int cnt = -1;
  double rval;
  double min = 0.0;
  //  int x1, y1;

  if(is_new_val)
    {
      cnt++;
      cnt %= BUFSIZE;
  
      if(val > glob.max_value)		/* wert zu hoch fuer anzeige */
	val = glob.max_value;
  
      glob.val1[cnt] = val;
      if(glob.graph2)
	average(glob.graph2, cnt, glob.val2); /* mittelwert ausrechnen und aufaddieren */

      if(glob.graph3)
	average(glob.graph3, cnt, glob.val3); /* mittelwert ausrechnen und aufaddieren */

      if(glob.graph4)
	average(glob.graph4, cnt, glob.val6); /* mittelwert ausrechnen und aufaddieren */

      rval = calc_exp_avg(cnt, glob.avg_exp); /* glaettung */
  
      rval=M_PI*rval/(glob.max_value - min);	/* zielwinkel [0...1] */
  
      //glob.pointer_winkel_ziel = M_PI*glob.val[cnt]/(max - min); /* zielwinkel grauer zeiger */
      glob.pointer_winkel_ziel_exp = M_PI - rval; /* zielwinkel farbiger zeiger */

      /* x1 = glob.draw_point.sizeX/2; */
      /* y1 = glob.draw_point.sizeY; */
    }
  
  if(!glob.no_analog)
    draw_analog();

  if(!glob.no_timeline)
    draw_averages(cnt);
}

static
void updateSensors_gs08(double temp, double feuchte)
{
  Widget tw;			/* Temperatur Widget */
  Widget fw;			/* Feuchte Widget */
  
  tw = getWidgetByName(glob.sens_lout, "SENSORION SHT21Frm"); /* Frame */
  tw = getWidgetByName(tw, "sht21Row");     /* RowColumn -Widget  */

  fw = getWidgetByName(tw, "sht21fValLbl"); /* Feuchte Label */
  tw = getWidgetByName(tw, "sht21tValLbl"); /* Temperatur Label */
  
  printWidget(tw, "%.01lf", temp);
  printWidget(fw, "%.01lf", feuchte); 

  /* Force Update */
  XFlush(XtDisplay(tw));
  XSync(XtDisplay(tw), 0);
}

static
void updateSensors_gs07(
			double t1, 
			double t2, 
			double t3, 
			double d, 
			double f, 
			double x, double y, double z
			)
{
  Widget t1_w;			/* Temperatur 1 Widget */
  Widget t2_w;			/* Temperatur 2 Widget */
  Widget t3_w;			/* Temperatur 3 Widget */
  Widget d_w;			/* Druck Widget */
  Widget f_w;			/* Feuchte Widget */
  Widget x_w;			/* Beschleunigung X Widget */
  Widget y_w;			/* Beschleunigung Y Widget */
  Widget z_w;			/* Beschleunigung Z Widget */
  
  t1_w = getWidgetByName(glob.sens_lout, "SENSORION SHT11Frm"); /* get SHT11 Frame */
  t1_w = getWidgetByName(t1_w, "sht11Row"); /* get SHT11 RowColumn -Widget */

  f_w = getWidgetByName(t1_w, "sht11fValLbl"); /* get SHT11 Feuchte Label*/
  t1_w = getWidgetByName(t1_w, "sht11tValLbl"); /* get SHT11 Temperatur Label */

  t2_w = getWidgetByName(glob.sens_lout, "BOSCH BMP085Frm"); /* get BMP085 Frame */
  t2_w = getWidgetByName(t2_w, "bmp085Row"); /* get SHT11 RowColumn -Widget */

  d_w = getWidgetByName(t2_w, "bmp085dValLbl");
  t2_w = getWidgetByName(t2_w, "bmp085tValLbl");

  t3_w = getWidgetByName(glob.sens_lout, "BOSCH BMA150Frm");
  t3_w = getWidgetByName(t3_w, "bma150Row");

  x_w = getWidgetByName(t3_w, "bma150xValLbl");
  y_w = getWidgetByName(t3_w, "bma150yValLbl");
  z_w = getWidgetByName(t3_w, "bma150zValLbl");
  t3_w = getWidgetByName(t3_w, "bma150tValLbl");
  
  printWidget(t1_w, "%.01lf",t1);
  printWidget(f_w, "%.01lf",f);
  
  printWidget(t2_w, "%.01lf",t2);
  printWidget(d_w, "%.01lf", d);

  printWidget(t3_w, "%.01lf", t3);
  printWidget(x_w, "%.01lf", x);
  printWidget(y_w, "%.01lf", y);
  printWidget(z_w, "%.01lf", z);

  /* Force Update */
  XFlush(XtDisplay(t1_w));
  XSync(XtDisplay(t1_w), 0);  
}

static
void update_ev(XtPointer clientData, int *fd, XtInputId *id)
{
  static int cnt = 0;
  static char buf[1024];
  int rc;			/* read Count */
  int nd;
  double t1, t2, t3;		/* alle Temperaturen */
  double d, f;			/* Druck, Feuchte */
  double x, y, z;		/* Beschleunigung X,Y,Z */

  double usv;			/* Wert in uSv */
  cnt++;

  memset(buf,0,sizeof(buf));

  if(glob.is_test > 1)
    {
      fprintf(stderr,"# # # # # # # # # # # # # # # # # # # # # # # #\n");
      fprintf(stderr,"# WARNUNG: PROGRAMM IM TESTMODUS              #\n");
      fprintf(stderr,"# SIMULIERTE SONDENVERSION: %d                #\n",glob.is_test);
      fprintf(stderr,"# SONDE_SIM WIRD AUFGERUFEN UND ABGEFRAGT !!! #\n");
      fprintf(stderr,"# # # # # # # # # # # # # # # # # # # # # # # #\n");
    }

  /* Sondendaten lesen */
  rc = read(*fd, buf, sizeof(buf));
  if(rc < 0)
    {
      fprintf(stderr,"Error reading Data: %s\n", strerror(errno));
      return;
    }

  if(strcmp("ERR_EXIT", buf) == 0) // child told to Exit caused by Error
    exit(0);

  /* Daten aufbereiten */
  if(buf[0] == 'V') // Version gelesen
    return;
    
  if(
     sscanf(buf, "%d %lf %lf %lf %lf %lf %lf %lf %lf",
	    &nd, &t1, &t2, &t3, &d, &f, &x, &y, &z ) 
     != 9
     )
    return;

  if(glob.is_test > 1)
    {
      printf("Niederdosis:\t\t%d\n", nd);
      printf("Temperatur1:\t\t%03lf\nTemperatur2:\t\t%.03lf\nTemperatur3:\t\t%.03lf\n", t1, t2, t3);
      printf("Druck:\t\t\t%.03lf\nFeuchte:\t\t%.03lf\n", d, f);
      printf("Beschleungigung X:\t%.03lf\nBeschleungigung Y:\t%.03lf\nBeschleungigung Z:\t%.03lf\n", x, y, z);
      printf("\n\n");
    }

  if(nd > 1000)
    nd = 1000;			/* zu hoch fuer 1 sek */
  

  /* Sensordaten anzeigen */
  if(!glob.no_table)
    switch(glob.sondTyp)
      {
      case 7:
	updateSensors_gs07(t1, t2, t3, d, f, x, y, z );
	break;
	
      case 8:
	updateSensors_gs08(t1, f);
	break;
	
      default:
	fprintf(stderr, "%s: illegal sondTyp: %d\n",__func__, glob.sondTyp);
      }


  /* uSv/h berechnen */
  usv = calc_uSv(nd*(60/glob.time_base));	/* hochrechnen auf eine Minute */
  
  /* GUI updaten */
  updateImages(usv, 1);
}

static
void draw_legend(int x1, int y1)
{
  int x2,y2;		/* koordinaten */
  char leg_name[100];
  
  if(glob.graph1 > 0)
    {
      x2 = x1+10;
      y2 = y1+10;
      
      XSetForeground(glob.draw_avg.dpy, glob.draw_avg.gc, COLOR_1_SEC);
      XDrawLine(glob.draw_avg.dpy, glob.draw_avg.backMap, glob.draw_avg.gc,
		x1,y1,x2,y2);
      memset(leg_name, 0, sizeof(leg_name));
      snprintf(leg_name, 100, "%d sec", glob.graph1 * glob.time_base);

      drawRotatedString(glob.draw_avg.dpy, glob.draw_avg.gc, 
			glob.draw_avg.backMap, x2+5, y1, 0, leg_name, strlen(leg_name));
    }

  if(glob.graph2> 0)
    {
      y1 += 30;
      x2 = x1+10;
      y2 = y1+10;
      
      XSetForeground(glob.draw_avg.dpy, glob.draw_avg.gc, COLOR_10_SEC);
      XDrawLine(glob.draw_avg.dpy, glob.draw_avg.backMap, glob.draw_avg.gc,
		x1,y1,x2,y2);

      memset(leg_name, 0, sizeof(leg_name));
      snprintf(leg_name, 100, "%d sec", glob.graph2 * glob.time_base);

      drawRotatedString(glob.draw_avg.dpy, glob.draw_avg.gc, 
			glob.draw_avg.backMap, x2+5, y1, 0, leg_name, strlen(leg_name));
    }

  if(glob.graph3> 0)
    {
      y1 += 30;
      x2 = x1+10;
      y2 = y1+10;
      
      XSetForeground(glob.draw_avg.dpy, glob.draw_avg.gc, COLOR_30_SEC);
      XDrawLine(glob.draw_avg.dpy, glob.draw_avg.backMap, glob.draw_avg.gc,
		x1,y1,x2,y2);

      memset(leg_name, 0, sizeof(leg_name));
      snprintf(leg_name, 100, "%d sec", glob.graph3 * glob.time_base);

      drawRotatedString(glob.draw_avg.dpy, glob.draw_avg.gc, 
			glob.draw_avg.backMap, x2+5, y1, 0, leg_name, strlen(leg_name));
    }

  if(glob.graph4> 0)
    {
      y1 += 30;
      x2 = x1+10;
      y2 = y1+10;
      
      XSetForeground(glob.draw_avg.dpy, glob.draw_avg.gc, COLOR_60_SEC);
      XDrawLine(glob.draw_avg.dpy, glob.draw_avg.backMap, glob.draw_avg.gc,
		x1,y1,x2,y2);

      memset(leg_name, 0, sizeof(leg_name));
      snprintf(leg_name, 100, "%d sec", glob.graph4 * glob.time_base);

      drawRotatedString(glob.draw_avg.dpy, glob.draw_avg.gc, 
			glob.draw_avg.backMap, x2+5, y1, 0, leg_name, strlen(leg_name));
    }

  if(glob.graph5> 0)
    {
      y1 += 30;
      x2 = x1+10;
      y2 = y1+10;
      
      XSetForeground(glob.draw_avg.dpy, glob.draw_avg.gc, COLOR_GLAETTUNG);
      XDrawLine(glob.draw_avg.dpy, glob.draw_avg.backMap, glob.draw_avg.gc,
		x1,y1,x2,y2);

      drawRotatedString(glob.draw_avg.dpy, glob.draw_avg.gc, 
			glob.draw_avg.backMap, x2+5, y1, 0, "Glaettung", 9);
    }
}

static
void fillPointerBackground()
{
  double i;
  int x1,x2,y1,y2;		   /* Punkte fuer Striche */
  int x = glob.draw_point.sizeX/2; /* Mittelpunkt X */
  int y = glob.draw_point.sizeY;   /* Mittelpunkt Y*/
  
  int anz_strich = 9;		/* Anzahl Unterteilstriche */
  char *val;
  int cnt = 0;
  anz_strich++; // + letzter Strich

  /* puts(__func__); */
  /* printf("dpy    : %p\n",glob.draw_point.dpy); */
  /* printf("backMap: %p\n",glob.draw_point.backMap); */
  /* printf("foreMap: %p\n",glob.draw_point.foreMap); */
  /* printf("gc     : %p\n",glob.draw_point.gc); */

  /* * * * * * * * * * * * * * * */
  /*  draw_pointer Map fuellen   */
  /* * * * * * * * * * * * * * * */
  XSetForeground(glob.draw_point.dpy, glob.draw_point.gc,
  		 BlackPixelOfScreen(XtScreen(glob.draw_point.draw))); /* Black */

  XFillRectangle(glob.draw_point.dpy, glob.draw_point.backMap, glob.draw_point.gc,
  		 0, 0, glob.draw_point.sizeX, glob.draw_point.sizeY); /* Background */

  XSetLineAttributes(glob.draw_point.dpy, glob.draw_point.gc, 2, 0, 0, 1);
  XSetForeground(glob.draw_point.dpy, glob.draw_point.gc,
  		 WhitePixelOfScreen(XtScreen(glob.draw_point.draw)));

  /* if(glob.draw_point.sizeX < glob.draw_point.sizeY) */
    glob.pointer_radius = glob.draw_point.sizeX/2;
  /* else */
  /*   glob.pointer_radius = glob.draw_point.sizeY/2; */

  /* printf("width : %d\n", glob.draw_point.sizeX); */
  /* printf("height: %d\n", glob.draw_point.sizeY); */
  /* printf("radius: %d\n",glob.pointer_radius); */
  
  XDrawArc(glob.draw_point.dpy, glob.draw_point.backMap, glob.draw_point.gc,
	   0, glob.draw_point.sizeY-glob.pointer_radius, glob.pointer_radius*2,
	   glob.pointer_radius*2, 0, 180*64);

  for(i = 0.0; i <= M_PI; i += M_PI/(anz_strich))
    {
      x1 = glob.pointer_radius *sin(i + (M_PI/2)) +x; /* pos. anfang unterteilstrich */
      y1 = glob.pointer_radius *cos(i + (M_PI/2)) +y; /* pos. anfang unterteilstrich */

      x2 = (glob.pointer_radius-10) *sin(i + (M_PI/2)) +x; /* pos. ende unterteilstrich */
      y2 = (glob.pointer_radius-10) *cos(i + (M_PI/2)) +y; /* pos. ende unterteilstrich */
      
      XDrawLine(glob.draw_point.dpy, glob.draw_point.backMap, glob.draw_point.gc,x1,y1,x2,y2);

      /* Werte Labels zeichnen */
      /*    Beschriftung: schleifendurchlaufe * ( Maximalwert / Anzahl_der_Striche ) */
      asprintf(&val, "%.2f",glob.max_value - (cnt *(glob.max_value/(anz_strich))));
      
      /* Zielposition Beschriftung */
      x2 = x2- (strlen(val)*2 );
      y2 = y2 + 5;

      rot = 90-((anz_strich/180)*cnt);
      drawRotatedString(glob.draw_point.dpy, glob.draw_point.gc, glob.draw_point.backMap,
  			x2, y2, /* x: ein Zeichen ~4 Pixel -> haelfte; y: 5px unterhalb*/
  			-90+(180/anz_strich)*cnt,
  			val, strlen(val));
      cnt++;
      free(val);
    }
  
  x = glob.draw_point.sizeX / 10;
  y = glob.draw_point.sizeY / 10;

  XDrawString(glob.draw_point.dpy, glob.draw_point.backMap, glob.draw_point.gc,
  	      (glob.draw_point.sizeX/2)-14 , glob.draw_point.sizeY-glob.draw_point.sizeY/5, "uSv/h", 5);
  
  XtVaSetValues(glob.draw_point.draw,
  		XmNbackgroundPixmap, glob.draw_point.backMap,
  		NULL);

}

static
void fillAverageBackground()
{
  double i;
  double x1,x2,y1,y2;		   /* Punkte fuer Striche */
  
  int anz_strich;		/* Anzahl Unterteilstriche */
  char *val;
  Dimension pix_width, pix_height;
  int draw_width, draw_height;
  glob.draw_avg.l_bord = 30;
  glob.draw_avg.r_bord = 90;
  glob.draw_avg.top_bord = 30;
  glob.draw_avg.bot_bord = 35;
  
  int l_bord = glob.draw_avg.l_bord;
  int r_bord = glob.draw_avg.r_bord;
  int top_bord = glob.draw_avg.top_bord;
  int bot_bord = glob.draw_avg.bot_bord;

  double line_dist;
  XtVaGetValues(glob.draw_avg.draw,
  		XmNwidth, &pix_width,
  		XmNheight, &pix_height,
  		NULL);
  //  puts(__func__);
  XSetForeground(glob.draw_avg.dpy, glob.draw_avg.gc, 
		 BlackPixelOfScreen(XtScreen(glob.draw_avg.draw))); /* Black */

  XFillRectangle(glob.draw_avg.dpy, glob.draw_avg.backMap, glob.draw_avg.gc,
  		 0, 0, glob.draw_avg.sizeX, glob.draw_avg.sizeY); /* Background */

  XSetLineAttributes(glob.draw_avg.dpy, glob.draw_avg.gc, 1, 0, 1, 1); /* 1px line width */
  draw_width = pix_width -l_bord-r_bord;
  draw_height = pix_height -top_bord - bot_bord;

  
#ifdef DEBUG  
  /* left top */
  XSetForeground(glob.draw_avg.dpy, glob.draw_avg.gc, 0x008800);
  XDrawRectangle(glob.draw_avg.dpy, glob.draw_avg.backMap, glob.draw_avg.gc,
		 0,0,l_bord-1, top_bord-1);
  /* left */
  XSetForeground(glob.draw_avg.dpy, glob.draw_avg.gc, 0x008800);
  XDrawRectangle(glob.draw_avg.dpy, glob.draw_avg.backMap, glob.draw_avg.gc,
		 0,top_bord, l_bord-1, draw_height-1);
  /* left bottom */
  XSetForeground(glob.draw_avg.dpy, glob.draw_avg.gc, 0x008800);
  XDrawRectangle(glob.draw_avg.dpy, glob.draw_avg.backMap, glob.draw_avg.gc,
		 0, top_bord+draw_height, l_bord-1, bot_bord-1);
  /* top */
  XSetForeground(glob.draw_avg.dpy, glob.draw_avg.gc, 0x008800);
  XDrawRectangle(glob.draw_avg.dpy, glob.draw_avg.backMap, glob.draw_avg.gc,
		 l_bord, 0, draw_width-1, top_bord-1);
  /* top right */
  XSetForeground(glob.draw_avg.dpy, glob.draw_avg.gc, 0x008800);
  XDrawRectangle(glob.draw_avg.dpy, glob.draw_avg.backMap, glob.draw_avg.gc,
		 l_bord+draw_width, 0, r_bord-1,top_bord-1);
  /* right */
  XSetForeground(glob.draw_avg.dpy, glob.draw_avg.gc, 0x008800);
  XDrawRectangle(glob.draw_avg.dpy, glob.draw_avg.backMap, glob.draw_avg.gc,
		 l_bord+draw_width, top_bord, r_bord-1, draw_height-1);
  /* right bottom */
  XSetForeground(glob.draw_avg.dpy, glob.draw_avg.gc, 0x008800);
  XDrawRectangle(glob.draw_avg.dpy, glob.draw_avg.backMap, glob.draw_avg.gc,
		 l_bord+draw_width, top_bord+draw_height, r_bord-1, bot_bord-1);
  /* bottom */
  XSetForeground(glob.draw_avg.dpy, glob.draw_avg.gc, 0x008800);
  XDrawRectangle(glob.draw_avg.dpy, glob.draw_avg.backMap, glob.draw_avg.gc,
		 l_bord, top_bord+draw_height, draw_width, bot_bord-1);
  /* middle */
  XSetForeground(glob.draw_avg.dpy, glob.draw_avg.gc, 0xffff00);
  XDrawRectangle(glob.draw_avg.dpy, glob.draw_avg.backMap, glob.draw_avg.gc,
  		 l_bord, top_bord, draw_width, draw_height);
#endif

  XSetForeground(glob.draw_avg.dpy, glob.draw_avg.gc, COLOR_GRID);
  
  /* Vertical */
  x1 = l_bord;
  y1 = top_bord;
  x2 = x1;
  y2 = pix_height-bot_bord;

  glob.draw_avg.vert_lines = 24;
  anz_strich = glob.draw_avg.vert_lines;

  line_dist = (double) draw_width/anz_strich;
    
  for(i = 0; i < anz_strich +1; i++){ 

    XDrawLine(glob.draw_avg.dpy, glob.draw_avg.backMap, glob.draw_avg.gc, 
	      x1, y1, x2, y2);

    /* Beschriftung */
    if((int) i % 6 == 0 && i != 0){ 
      int tmp = (int) i*(glob.time_base*10);
      asprintf(&val,"%d", (glob.time_base*anz_strich*10)- tmp); // 10 Werte pro Vert. Strich
      drawRotatedString(glob.draw_avg.dpy, glob.draw_avg.gc, glob.draw_avg.backMap, 
			x1-((strlen(val)*3)),  // strlen(val)*char_pix_width(~3)/2
			y2, 0, val, strlen(val));
      free(val);
    }
    
    x1 += line_dist;
    x2 = x1;
  }
  
  asprintf(&val,"Zeit in s");
  drawRotatedString(glob.draw_avg.dpy, glob.draw_avg.gc, glob.draw_avg.backMap,
  		    x1-80, y2+15, 0, val, strlen(val));
  free(val);

  i--; // ran 1 time too much
  
  /* Horizontal */
  x1 = l_bord;
  y1 = top_bord;
  x2 = x1 + line_dist*i;
  y2 = top_bord;
  
  asprintf(&val,"Wert in uSv/h");
  drawRotatedString(glob.draw_avg.dpy, glob.draw_avg.gc, glob.draw_avg.backMap,
  		    x1-20, y2-25, 0, val, strlen(val));
  free(val);

  glob.draw_avg.hor_lines = 5;
  anz_strich = glob.draw_avg.hor_lines;

  line_dist = (double) draw_height/anz_strich;
  for(i = 0; i < anz_strich +1; i++)
    {
      XDrawLine(glob.draw_avg.dpy, glob.draw_avg.backMap, glob.draw_avg.gc,
  		x1, y1, x2, y2);

      asprintf(&val,"%f", glob.max_value - i*(glob.max_value/anz_strich));
      drawRotatedString(glob.draw_avg.dpy, glob.draw_avg.gc, glob.draw_avg.backMap,
  			x1-20, y2-8, 0, val, 3);
      free(val);

      y1 += line_dist;
      y2 = y1;
    }
  
  draw_legend(pix_width-80, (pix_height/10));
   
  XtVaSetValues(glob.draw_avg.draw,
		XmNbackgroundPixmap, glob.draw_avg.backMap,
		NULL);
}

static
void fillDrawStruct(Widget topLout, struct drawStruct *str){
  str->draw = XtVaCreateManagedWidget("drawArea", xmDrawingAreaWidgetClass, topLout,
				      XmNheight, str->sizeY,
				      XmNwidth, str->sizeX,
				      NULL);

  XtAddCallback(str->draw, XmNresizeCallback, resize_cb, str);
  XtAddCallback(str->draw, XmNexposeCallback, expose_cb, str);

  str->dpy = XtDisplay(str->draw);	/* get Display */

  XtVaGetValues ( str->draw,  
		  XmNforeground, &str->gcv.foreground,
                  XmNbackground, &str->gcv.background,
                  XmNdepth,      &str->tiefe,
                  NULL  );
  str->gc = XCreateGC(str->dpy, RootWindowOfScreen(XtScreen(str->draw)), 
		      GCForeground | GCBackground, &str->gcv);
  
  
  str->backMap = XCreatePixmap(str->dpy, 
			       RootWindowOfScreen(XtScreen(str->draw)), 
			       str->sizeX, str->sizeY, 
			       str->tiefe);
  
  str->foreMap = XCreatePixmap(str->dpy,
			       RootWindowOfScreen(XtScreen(str->draw)), 
			       str->sizeX, str->sizeY, 
			       str->tiefe);
  
  XSetForeground(str->dpy, str->gc, BlackPixelOfScreen(XtScreen(str->draw)));
  XFillRectangle(str->dpy, str->foreMap, str->gc, 0,0, str->sizeX, str->sizeY);
  XSetForeground(str->dpy, str->gc, WhitePixelOfScreen(XtScreen(str->draw)));
}

static
Widget createFrame_withText(Widget w,char *name)
{
  Arg args[5];
  int n=0;
  Widget frame,label;
  char *frmName;
  
  asprintf(&frmName, "%sFrm",name);

  frame = XmCreateFrame(w,frmName,args,n);

  XtSetArg(args[n],XmNframeChildType,XmFRAME_TITLE_CHILD); n++;
  XtSetArg(args[n],XmNchildVerticalAlignment,XmALIGNMENT_BASELINE_TOP);n++;
  label=XmCreateLabelGadget(frame,name,args,n);
  XtManageChild(label);
  XtManageChild(frame);
  
  free(frmName);
  
  return frame;
}

static
Widget createSensWidget(Widget toplevel)
{
  Widget sens_lout;
  Widget img_row;		/* RowC fuer images */
  
  Widget sht11Frm;		/* SHT11 / SHT21 Frame */
  Widget sht11Row;		/* SHT11 / SHT21 RowColumn */
  Widget sht11t;		/* SHT11 / SHT21 Temperatur */
  Widget sht11f;		/* SHT11 / SHT21 Rel. Feuchte */
  Widget sht11Img;		/* SHT11 / SHT21 Image */
  
  Widget bmp085Frm;		/* BMP085 Frame */
  Widget bmp085Row;		/* BMP085 RowColumn */
  Widget bmp085t;		/* BMP085 Temperatur */
  Widget bmp085d;		/* BMP085 Luftdruck */
  Widget bmp085Img;		/* BMP085 Image */
  
  Widget bma150Frm;		/* BMA150 Frame */
  Widget bma150Row;		/* BMA150 RowColumn */
  Widget bma150t;		/* BMA150 Temperatur */
  Widget bma150x;		/* BMA150 Beschleunigung X */
  Widget bma150y;		/* BMA150 Beschleunigung Y */
  Widget bma150z;		/* BMA150 Beschleunigung Z */
  Widget bma150Img;		/* BMA150 Image */
  
  sens_lout = XtVaCreateManagedWidget("sensLout", xmFormWidgetClass, toplevel, NULL);
  img_row = XtVaCreateManagedWidget("sensImgRow", xmRowColumnWidgetClass, sens_lout, 
				    XmNorientation, XmHORIZONTAL, NULL);
  
  switch(glob.sondTyp)
    {
    case 7:
      sht11Frm = createFrame_withText(sens_lout, "SENSORION SHT11");
      sht11Row = XtVaCreateManagedWidget("sht11Row", xmRowColumnWidgetClass, sht11Frm,
					 XmNorientation, XmHORIZONTAL,
					 XmNnumColumns, 2,
					 XmNpacking, XmPACK_COLUMN,
					 NULL);
      sht11t = XtVaCreateManagedWidget("sht11tLbl", xmLabelWidgetClass, sht11Row, NULL);
      sht11t = XtVaCreateManagedWidget("sht11tValLbl", xmLabelWidgetClass, sht11Row, NULL);
  
      sht11f = XtVaCreateManagedWidget("sht11fLbl", xmLabelWidgetClass, sht11Row, NULL);
      sht11f = XtVaCreateManagedWidget("sht11fValLbl", xmLabelWidgetClass, sht11Row, NULL);
      sht11Img = XtVaCreateManagedWidget("sht11ImgLbl", xmLabelWidgetClass, img_row, NULL);
  
      bmp085Frm = createFrame_withText(sens_lout,"BOSCH BMP085");
      bmp085Row = XtVaCreateManagedWidget("bmp085Row", xmRowColumnWidgetClass, bmp085Frm,
					  XmNorientation, XmHORIZONTAL,
					  XmNnumColumns, 2,
					  XmNpacking, XmPACK_COLUMN,
					  NULL);
      bmp085t = XtVaCreateManagedWidget("bmp085tLbl", xmLabelWidgetClass, bmp085Row, NULL);
      bmp085t = XtVaCreateManagedWidget("bmp085tValLbl", xmLabelWidgetClass, bmp085Row, NULL);
      bmp085d = XtVaCreateManagedWidget("bmp085dLbl", xmLabelWidgetClass, bmp085Row, NULL);
      bmp085d = XtVaCreateManagedWidget("bmp085dValLbl", xmLabelWidgetClass, bmp085Row, NULL);
      bmp085Img = XtVaCreateManagedWidget("bmp085ImgLbl", xmLabelWidgetClass, img_row, NULL);
  
      bma150Frm = createFrame_withText(sens_lout, "BOSCH BMA150");
      bma150Row = XtVaCreateManagedWidget("bma150Row", xmRowColumnWidgetClass, bma150Frm, 
					  XmNorientation, XmHORIZONTAL,
					  XmNnumColumns, 4,
					  XmNpacking, XmPACK_COLUMN,
					  NULL);
      bma150t = XtVaCreateManagedWidget("bma150tLbl", xmLabelWidgetClass, bma150Row, NULL);
      bma150t = XtVaCreateManagedWidget("bma150tValLbl", xmLabelWidgetClass, bma150Row, NULL);
      bma150x = XtVaCreateManagedWidget("bma150xLbl", xmLabelWidgetClass, bma150Row, NULL);
      bma150x = XtVaCreateManagedWidget("bma150xValLbl", xmLabelWidgetClass, bma150Row, NULL);
      bma150y = XtVaCreateManagedWidget("bma150yLbl", xmLabelWidgetClass, bma150Row, NULL);
      bma150y = XtVaCreateManagedWidget("bma150yValLbl", xmLabelWidgetClass, bma150Row, NULL);
      bma150z = XtVaCreateManagedWidget("bma150zLbl", xmLabelWidgetClass, bma150Row, NULL);
      bma150z = XtVaCreateManagedWidget("bma150zValLbl", xmLabelWidgetClass, bma150Row, NULL);
      bma150Img = XtVaCreateManagedWidget("bma150ImgLbl", xmLabelWidgetClass, img_row, NULL);

      XtVaSetValues(sht11Frm,
		    XmNtopOffset, 10,
		    XmNtopAttachment, XmATTACH_FORM,
		    XmNleftOffset, 5,
		    XmNleftAttachment, XmATTACH_FORM,
		    NULL);
  
      XtVaSetValues(bmp085Frm,
		    XmNtopOffset, 10,
		    XmNtopWidget, sht11Frm,
		    XmNtopAttachment, XmATTACH_WIDGET,
		    XmNleftOffset, 5,
		    XmNleftAttachment, XmATTACH_FORM,
		    NULL);

      XtVaSetValues(bma150Frm,
		    XmNtopOffset, 10,
		    XmNtopWidget, bmp085Frm,
		    XmNtopAttachment, XmATTACH_WIDGET,
		    XmNleftOffset, 5,
		    XmNleftAttachment, XmATTACH_FORM,
		    NULL);
      
      XtVaSetValues(img_row,
		    XmNtopWidget, bma150Frm,
		    XmNtopOffset, 10,
		    XmNtopAttachment, XmATTACH_WIDGET,
		    XmNleftAttachment, XmATTACH_FORM,
		    NULL);
      break;
      
    case 8:
      sht11Frm = createFrame_withText(sens_lout, "SENSORION SHT21");
      sht11Row = XtVaCreateManagedWidget("sht21Row", xmRowColumnWidgetClass, sht11Frm,
 					 XmNorientation, XmHORIZONTAL,
					 XmNnumColumns, 2,
					 XmNpacking, XmPACK_COLUMN,
					 NULL);
      sht11t = XtVaCreateManagedWidget("sht21tLbl", xmLabelWidgetClass, sht11Row, NULL);
      sht11t = XtVaCreateManagedWidget("sht21tValLbl", xmLabelWidgetClass, sht11Row, NULL);
  
      sht11f = XtVaCreateManagedWidget("sht21fLbl", xmLabelWidgetClass, sht11Row, NULL);
      sht11f = XtVaCreateManagedWidget("sht21fValLbl", xmLabelWidgetClass, sht11Row, NULL);
      
      sht11Img = XtVaCreateManagedWidget("sht21ImgLbl", xmLabelWidgetClass, img_row, NULL);

      XtVaSetValues(sht11Frm,
		    XmNtopOffset, 10,
		    XmNtopAttachment, XmATTACH_FORM,
		    XmNleftOffset, 5,
		    XmNleftAttachment, XmATTACH_FORM,
		    NULL);

      XtVaSetValues(img_row,
		    XmNtopWidget, sht11Frm,
		    XmNtopOffset, 10,
		    XmNtopAttachment, XmATTACH_WIDGET,
		    XmNleftAttachment, XmATTACH_FORM,
		    NULL);
      break;
      
    default:
      fprintf(stderr, "%s: illegal sondTyp: %d\n",__func__,glob.sondTyp);
      break;
    }
  
  return sens_lout;
}

static 
void initGUI(Widget toplevel)
{
  Widget topLout;
  Widget *win_elems[3];
  int elem_cnt = 0;

  /* Anzeigegroessen festlegen */
  glob.win_sizeX = 900;
  glob.win_sizeY = 460;
  
  glob.draw_point.sizeX = (glob.win_sizeX/3)*2;
  glob.draw_point.sizeY = (glob.win_sizeY/3)*2;
  glob.draw_point.backgroundFillFunc = fillPointerBackground;
  
  glob.draw_avg.sizeX = glob.win_sizeX;
  glob.draw_avg.sizeY = 350;
  glob.draw_avg.backgroundFillFunc = fillAverageBackground;

  glob.draw_sens.sizeX = (glob.win_sizeX/3)*1;
  glob.draw_sens.sizeY = glob.draw_point.sizeY;

  topLout = XtVaCreateManagedWidget("topLout", xmFormWidgetClass, toplevel, NULL);
  if(!glob.no_table)
    {
      glob.sens_lout = createSensWidget(topLout);
      win_elems[elem_cnt] = &glob.sens_lout;
      elem_cnt++;
    }

  if(!glob.no_analog)
    {
      fillDrawStruct(topLout, &glob.draw_point);
      win_elems[elem_cnt] = &glob.draw_point.draw;
      elem_cnt++;
    }
  
  if(!glob.no_timeline)
    {
      fillDrawStruct(topLout, &glob.draw_avg);
      win_elems[elem_cnt] = &glob.draw_avg.draw;
      elem_cnt++;
    }

  XtVaSetValues(*win_elems[0], 	/* min. one Element */
		XmNtopAttachment, XmATTACH_FORM,
		XmNleftAttachment, XmATTACH_FORM,
		NULL);

  if(elem_cnt == 1)		/* just one Element */
    {
      XtVaSetValues(*win_elems[0],
		    XmNrightAttachment, XmATTACH_FORM,
		    XmNbottomAttachment, XmATTACH_FORM,
		    NULL);
    }

  /* FIXME: richig anhaengen !*/
  else if(elem_cnt == 2) // 2 elements given
    {
      if(glob.no_table) // only timeline and analog
      	{
	  XtVaSetValues(*win_elems[0],
      			XmNleftOffset, 80,
			//	XmNrightAttachment, XmATTACH_FORM, // error occures here
	  		NULL);

      	  XtVaSetValues(*win_elems[1],
      			XmNtopOffset, 10,
      			XmNtopWidget, *win_elems[0],
      			XmNtopAttachment, XmATTACH_WIDGET,
      			XmNleftAttachment, XmATTACH_FORM,
			XmNrightAttachment, XmATTACH_FORM,
			XmNbottomAttachment, XmATTACH_FORM,
      			NULL);
      	}
      else // Table and another module
	{
	  XtVaSetValues(*win_elems[0],
	  		XmNbottomAttachment, XmATTACH_FORM,
	  		NULL);
	  XtVaSetValues(*win_elems[1],
			XmNtopAttachment, XmATTACH_FORM,
			XmNleftOffset, 10,
			XmNleftWidget, *win_elems[0],
			XmNleftAttachment, XmATTACH_WIDGET,
			XmNrightAttachment, XmATTACH_FORM,
			XmNbottomAttachment, XmATTACH_FORM,
			NULL);
	}   
    }
  else if(elem_cnt > 2) // All is shown
    {
      XtVaSetValues(*win_elems[1],
		    XmNleftOffset, 10,
		    XmNleftWidget, *win_elems[0],
		    XmNleftAttachment, XmATTACH_WIDGET,
		    //XmNrightAttachment, XmATTACH_FORM, // FIXME: crash! 
		    XmNbottomWidget, *win_elems[0],
		    XmNbottomAttachment, XmATTACH_OPPOSITE_WIDGET,
		    NULL);
      XtVaSetValues(*win_elems[2],
		    XmNtopOffset, 10,
		    XmNtopWidget, *win_elems[0],
		    XmNtopAttachment, XmATTACH_WIDGET,
		    XmNleftAttachment, XmATTACH_FORM,
		    XmNrightAttachment, XmATTACH_FORM,
		    XmNbottomAttachment, XmATTACH_FORM,
		    NULL);
    }
    
  /* Hintergrund Pixmaps fuellen*/
  if(!glob.no_timeline)
    glob.draw_avg.backgroundFillFunc(); // FIXME: add resize_cb instead of direct call

  if(!glob.no_analog)
    {
      glob.draw_point.backgroundFillFunc(); // FIXME: add resize_cb instead of direct call
      glob.gc_pointer = XCreateGC(glob.draw_point.dpy, 
				  RootWindowOfScreen(XtScreen(glob.draw_point.draw)), 0, 0);
      
      /* GC fuer Analoganzeige einstellen*/
      XSetForeground(glob.draw_point.dpy, glob.gc_pointer, 0xffffff);
      XSetBackground(glob.draw_point.dpy, glob.gc_pointer, 0);
      XSetFunction(glob.draw_point.dpy, glob.gc_pointer, GXxor);
      XSetLineAttributes(glob.draw_point.dpy, glob.gc_pointer, 5, 0, 0, 1); /* 5 pix width */
    }
}

static
int runSonde(XtAppContext app)
{
  int pty;			/* pseudo terminal */
  int fd;			/* file descriptor */
  int rc;			/* read count */
  static int pid;		/* process ID */

  if(glob.use_stdin)
    {
      XtAppAddInput(app, fileno(stdin), (XtPointer) XtInputReadMask, update_ev, NULL);
      return 0;
    }
  
  pty = posix_openpt(O_RDWR);	/* open pseudo terminal */
  if(pty < 0)
    {
      fprintf(stderr,"Error posix_openpt: %s\n",strerror(errno));
      return -1;
    }
  
  if(grantpt(pty) != 0)
    {
      fprintf(stderr,"Error grantpt: %s\n",strerror(errno));
      return -1;
    }
  
  if(unlockpt(pty) != 0)	/* unlock pty */
    {
      fprintf(stderr,"Error unlokpt: %s\n",strerror(errno));
      return -1;
    }

  fd = open(ptsname(pty), O_RDWR); /* open pty */
  if(fd < 0)
    {
      fprintf(stderr,"Error open PTY: %s\n",strerror(errno));
      return -1;
    }
  
  pid = fork();
  
  if(pid < 0)
    {
      fprintf(stderr, "Fork error: %s\n",strerror(errno));
      return -1;
    }
  
  if(pid)
    {
      /* Parent */
      close(fd);		/* no need for open pty */
      on_exit(cleanup, &pid );  /* call cleanup() at exit() */

      /* call update() at input */
      XtAppAddInput(app, pty, (XtPointer) XtInputReadMask, update_ev, NULL);
    }
  else
    {
      /* Child */
      struct termios org_term_settings; /* Saved terminal settings */
      struct termios new_term_settings; /* Current terminal settings */
      char *speed = alloca(sizeof(char)*10);
      close(pty);			

      rc = tcgetattr(fd, &org_term_settings);

      new_term_settings = org_term_settings;
      cfmakeraw (&new_term_settings);
      tcsetattr (fd, TCSANOW, &new_term_settings);

      /* Close standard in,out and err (current terminal) */
      close(STDIN_FILENO);
      close(STDOUT_FILENO);
      //      close(STDERR_FILENO);
      

      dup(fd);			/* TY becomes standard in (0) */
      dup(fd);			/* TY becomes standard out (1) */
      //      dup(fd);			/* TY becomes standard err (2) */
      
      switch(glob.sondTyp)
      	{
      	case 7:
      	  speed = "9600";
      	  break;
	  
      	case 8:
      	  speed = "115200";
      	  break;
	  
      	default:
      	  speed = "";
      	}

      char *new_path;
      asprintf(&new_path, "%s:.", getenv("PATH"));
      setenv("PATH", new_path, 1); // overwrite PATH ($PATH:.)
      free(new_path);

      if(glob.is_test > 0)
	rc = execlp("sonde_sim", "", NULL);
      else
	rc = execlp("sonde", "",
		    "-F", glob.port,
		    "-w", "1",
		    "-s", speed,
		    "-o", "%N %T1 %T2 %T3 %D %F %X %Y %Z\n",
		    NULL);
      if(rc < 0)
	{
	  printf("ERR_EXIT");
	  fflush(stdout);
	  fprintf(stderr,"Error execl: %s\n",strerror(errno));
	  _exit(1);
	}

      _exit(0);			/* child Exit */
    }
  
  return pty;
}

static
void exit_cb(Widget w, XtPointer clientData, XtPointer callData)
{
  exit(0);
}

static
void cleanup(int ex, void *arg)
{
  int pid = *(int*) arg;
  if(ex > 0)
    kill(pid, SIGTERM);
}

static
void sigchld_hdl (int sig)
{
  //puts("starting sigchld");
  /* Wait for all dead processes.
   * We use a non-blocking call to be sure this signal handler will not
   * block if a child was cleaned up in another part of the program. */
  while (waitpid(-1, NULL, WNOHANG) > 0) {
  }
  // puts("Leaving sigchld");
  exit(0);
	
  /* if( child_is_dead_cb ) child_is_dead_cb(); */
}

static
void sigalrm_hdl(int sig)
{
  fprintf(stderr,"Could not read Version of Sonde within 10s! Exiting\n");
  system("killall sonde"); // kill runing sonde program
  exit(1);
}

static
void print_help(char *call)
{
  printf("usage: %s [OPTIONS]\n", call);
  printf("-help              Prints this help and exits\n");
  printf("-verbose           Prints all switches and flags to stdout\n");
  printf("-no_table          Disables the Table\n");
  printf("-no_analog         Disables the Analogpointer\n");
  printf("-no_timeline       Disables the Timeline\n");
  printf("-raw_graph <1|0>   [En|Dis]ables the %dsec Graph (Raw Data Graph)\n", glob.time_base);
  printf("                     --> Default: Enabled (1)\n");
  printf("-expo_graph <1|0>  [En|Dis]ables the Exponential Graph\n");
  printf("                     --> Default: Enabled (1)\n");
  printf("-graph_1 <Number>  Sets the amount of values over witch the average of Graph 1 is built\n");
  printf("                   Disable Graph1 by giving 0\n");
  printf("                     --> Default: 10\n");
  printf("-graph_2 <Number>  Sets the amount of values over witch the average of Graph 2 is built\n");
  printf("                   Disable Graph1 by giving 0\n");
  printf("                     --> Default: 30\n");
  printf("-graph_3 <Number>  Sets the amount of values over witch the average of Graph 3 is built\n");
  printf("                   Disable Graph1 by giving 0\n");
  printf("                     --> Default: 60\n");
  printf("-typ <7|8>         Switches the Probetype to GS07 or GS08\n");
  printf("-use_stdin         Uses stdin for Datainput\n");
  printf("-timebase <tmbs>   Sets the Timebase of the incoming Data to tmbs\n");
  printf("                     --> Default: 1 (Means every second)\n");
  printf("-exponent <exp>    Sets the alpha for the exponential Average to exp\n");
  printf("                     --> Default: 0.3\n");
  printf("-test              Uses the program 'sonde_sim' for input\n"); 
  printf("                    eq. to \"(./)sonde_sim | %s\"\n", call);
  exit(0);
}

int main(int argc, char *argv[])
{
  Widget toplevel;
  XtAppContext app;
  int pty;
  struct sigaction act;
  Atom WM_DELETE_WINDOW;
    
  memset (&act, 0, sizeof(act));
  act.sa_handler = sigchld_hdl;
  
  XtSetLanguageProc(NULL,NULL,NULL);
  
  /* open SessionShell; not resizeable; size 900x700*/
  toplevel = XtVaOpenApplication(&app,"messwertanzeige", options, XtNumber(options), 
				 &argc, argv, fallback,
				 sessionShellWidgetClass, 
				 /* XmNmaxWidth, 900, */
				 /* XmNminWidth, 900, */
				 /* XmNmaxHeight, 700, */
				 /* XmNminHeight, 700, */
				 NULL);
  XtGetApplicationResources( toplevel, (XtPointer) &glob, 
			     my_resources, XtNumber(my_resources),
			     NULL, (Cardinal) 0);
  
  if(glob.print_help)
    print_help(argv[0]); // calls exit(0)

  if(glob.no_table)
    if(glob.no_analog)
      if(glob.no_timeline)
	{
	  printf("All modules disabled\nExiting\n");
	  exit(0);
	}
  /* on close: cal exit_cb */
  WM_DELETE_WINDOW = XInternAtom(XtDisplay(toplevel), "WM_DELETE_WINDOW", False);
  XmAddWMProtocolCallback(toplevel, WM_DELETE_WINDOW, exit_cb, NULL); 

  setlocale(LC_ALL,"C");	/* sscanf: '.' statt ',' als Trenner bei float/double */

  glob.max_value = 1.0;
  
  signal(SIGALRM, sigalrm_hdl);
  
  if(glob.verbose > 0)
    {
      printf("options: \n");
      printf("test           : %d\n", glob.is_test);
      printf("typ            : %d\n", glob.sondTyp);
      printf("use_stdin      : %d\n", glob.use_stdin);
      printf("Timebase       : %d\n", glob.time_base);
      printf("exponent_alpha : %s\n", glob.exponent_alpha);
      printf("Graph 1 (raw)  : %d\n", glob.graph1);
      printf("Graph 2        : %d\n", glob.graph2);
      printf("Graph 3        : %d\n", glob.graph3);
      printf("Graph 4        : %d\n", glob.graph4);
      printf("Graph 5 (expo) : %d\n", glob.graph5);
      printf("No Table       : %d\n", glob.no_table);
      printf("No Analog      : %d\n", glob.no_analog);
      printf("No Timeline    : %d\n", glob.no_timeline);
    }

  if(!glob.is_test && !glob.use_stdin)
    glob.sondTyp = getSondTyp(); 

  if(glob.sondTyp < 0)
    {
      fprintf(stderr,"Error reading SondTyp");
      exit(1);
    }

  pty = runSonde(app);
  if(pty < 0)
    {
      fprintf(stderr,"Error run Sonde");
      exit(1);
    }
  
  initGUI(toplevel);
  
  memset(glob.val1, 0, sizeof(double) *BUFSIZE);
  memset(glob.val2, 0, sizeof(double) *BUFSIZE);
  memset(glob.val3, 0, sizeof(double) *BUFSIZE);
  memset(glob.avg_exp, 0, sizeof(double) *BUFSIZE);
  
  if (sigaction(SIGCHLD, &act, 0)) {
    perror ("sigaction: SIGCHLD");
    return EXIT_FAILURE;
  }

#if 0
  act.sa_handler = sigterm_hdl;
  sigemptyset(&act.sa_mask);
  act.sa_flags = SA_RESTART;

  if (sigaction(SIGKILL, &act, 0)) {
    perror ("sigaction: SIGKILL");
    return EXIT_FAILURE;
  }
#endif
  
  XtRealizeWidget(toplevel);
  XtAppMainLoop(app);
  
  return 0;
}
