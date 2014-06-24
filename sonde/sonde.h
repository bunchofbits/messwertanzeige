#ifndef _SONDE_H_
#define _SONDE_H_ "$Id: sonde.h,v 1.3 2010/05/25 12:32:36 walter Exp $"

struct raw_data {
	unsigned int hd;
	unsigned int nd;
	unsigned int echo;
	unsigned int koinz;
	unsigned int spannung;
        double temp[3];
	double druck;
	double feuchte;
  	double x;
	double y;
	double z;

};

extern int verbose;
#endif
