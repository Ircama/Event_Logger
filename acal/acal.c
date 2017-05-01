/*
acal.c - Astronomical Calendar - amacri 2006 - donationware software

Primary source:

SUNRISET.C - computes Sun rise/set times, start/end of twilight, and
             the length of the day at any date and latitude

             Date last modified: 05-Jul-1997

Written as DAYLEN.C, 1989-08-16

Modified to SUNRISET.C, 1992-12-01

(c) Paul Schlyter, 1989, 1992

Released to the public domain by Paul Schlyter, December 1992

amacri: 2005-11-01 - 2006-5-29
        SUNRISET.C was modified and merged with potm.c (in turn merging
        sdate.c, copyright 1986 by Marc Kaufman, revised to 31.10.92)
        potm.c provides support of moon ephemeris and phase.
        Added support of manual date and timezone.
        Added moon shape description.
        Added azimuth and elevation of the moon.
        Can be compiled to Linux, ARM Linux(-DARM), cygwin
        Code revised to fit the TomTom GO display; min. resolution:
         320x240; font: 6x11; lines x columns: 53x21
*/

/*
acal.c is part of Event_Logger, author Amacri - amacri@tiscali.it

Permission is hereby granted, without written agreement and without
license or royalty fees, to use, copy, modify, and distribute this
version for non commercial purpose, provided that the above notice
and the following four paragraphs appear in all copies.

In no event shall the Author Amacri be liable to any party for direct,
indirect, special, incidental, or consequential damages arising out of
the use of this software and its documentation,  even if Amacri has been
advised of the possibility of such damage.

The Author Amacri specifically disclaims any warranties, including, but
not limited to, the implied warranties of merchantability and fitness for
a particular purpose.  The software provided hereunder is on an "as is"
basis, and Amacri has no obligation to provide maintenance, support, updates,
enhancements, or modifications.

Event_Logger must not be distributed in ANY form that requires the recipient
to make a payment of any kind, except for donations to amacri in relation
to the implemented addenda. This restriction is not limited to monetary
payment - the restriction includes any form of payment. No permission
is implicitly provided to incorporate this software on CD/DVD or other
media for sale. E-commerce is explicitly disallowed.

Donations for addenda made by amacri in relation to Event_Logger development
are more than happily accepted! Event_Logger took a lot of development time,
as well as large number of testing and user support; if you really like this
software, consider sending donations; email to amacri@tiscali.it with subject
Donation to know how to perform this.
*/

#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include <time.h>

/**********************************************/
#include <sys/types.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/ioctl.h>
#include <stdlib.h>
#include <stdarg.h>

#define BUF_SIZE 1024

static char *compass[] = { /* points of the compass with resol. 1/16, that is 360 / 22.5 */
      "N",   /*   0   */
      "NNE", /*  22,5 */
      "NE",  /*  45   */
      "ENE", /*  67.5 */
      "E",   /*  90   */
      "ESE", /* 112.5 */
      "SE",  /* 135   */
      "SSE", /* 157.5 */
      "S",   /* 180   */
      "SSW", /* 202.5 */
      "SW",  /* 225   */
      "WSW", /* 247.5 = -67.5 */
      "W",   /* 270   */
      "WNW", /* 292.5 */
      "NW",  /* 315   */
      "NNW", /* 225,5 = -22.5 */
      "N"    /* 360 (again) */
      };

static char *description[] = {
      "new",                  /* 0, 0-1       totally dark                         */
      "waxing crescent",      /* 1, 2-88      increasing to full & quarter light   */
      "in its first quarter", /* 2, 89-91     increasing to full & half light      */
      "waxing gibbous",       /* 3, 92-178    increasing to full & > than half     */
      "full",                 /* 4, 179-181   fully lighted                        */
      "waning gibbous",       /* 5, 182-268   decreasing from full & > than half   */
      "in its last quarter",  /* 6, 269-271   decreasing from full & half light    */
      "waning crescent",      /* 7, 272-358   decreasing from full & quarter light */
      "new"                   /* 8, 359-360   AGAIN totally dark                   */
      };

#include "../fb.h"
extern int fb_FontX, fb_FontY, fb_MaxX, fb_MaxY, fb_Bold;
extern int fbfd, tsfd, fb_rotate, fb_x, fb_xr, fb_ys, fb_y, fb_col, fb_NumInk, fb_BgCol, fb_center, ts_x, ts_y, ts_pen;
extern unsigned short* fb;
#ifdef ARM
#define _timezone timezone
#define _daylight daylight
#define printf fbprintf
#endif

/**********************************************/

/* A macro to compute the number of days elapsed since 2000 Jan 0.0 */
/* (which is equal to 1999 Dec 31, 0h UT)                           */

#define days_since_2000_Jan_0(y,m,d) \
    (367L*(y)-((7*((y)+(((m)+9)/12)))/4)+((275*(m))/9)+(d)-730530L)

/* Some conversion factors between radians and degrees */

#ifndef PI
 #define PI        3.1415926535897932384
#endif

#define RADEG     ( 180.0 / PI )
#define DEGRAD    ( PI / 180.0 )

/* The trigonometric functions in degrees */

#define sind(x)  sin((x)*DEGRAD)
#define cosd(x)  cos((x)*DEGRAD)
#define tand(x)  tan((x)*DEGRAD)

#define atand(x)    (RADEG*atan(x))
#define asind(x)    (RADEG*asin(x))
#define acosd(x)    (RADEG*acos(x))
#define atan2d(y,x) (RADEG*atan2(y,x))

#define f1		(1. - (1./298.25))	/* 1-flattening of Earth     */
#define Pi			3.1415926535
#define Asec_Radian		((2.0 * Pi)/(360. * 60. * 60.))
#define Degree_to_Radian	((2.0 * Pi)/ 360.)
#define Tsec_to_Radian		((2.0 * Pi)/( 24. * 60.* 60.))
#define Sec_per_day		(24 * 60 * 60)
#define	J1970	/* 24 */40587.5	/* UNIX clock Epoch 1970 Jan 1 (0h UT) */
#define	J1985	/* 24 */46065.5	/* Epoch 1985 Jan 1 (0h UT) */
#define	J2000	/* 24 */51545.0	/* Epoch 2000 Jan 1 (12h UT) */
#define Delta_T		(54.6 + 0.9*(Julian_Day - J1985)/365.)	/* TDT - UT */
#define Round			0.5		/* for rounding to integer */

/* ref. to potm.c */
long	UTC, LocalTime, TDT, tim, tim2, localdst;
double	Julian_Day, MJD, Tu, Ru, T70, Local, GMST, LST;
double	zs, x;
double	la, lf, S, C, sp, cp, tp, Az, alt;
double	Eqt, Tua, L, G, e, eps, g, alpha, delta, sd, cd, lha, lhr, sh, ch;
struct	tm *gmtime();
double	Lm, lm, px, SD, am, dm, days;
double  AzM, altM; // amacri: Azimuth and Altitude of the moon
char	*tdate, *gmctime();
double replace_fmod(double x, double m);


/* Following are some macros around the "workhorse" function __daylen__ */
/* They mainly fill in the desired values for the reference altitude    */
/* below the horizon, and also selects whether this altitude should     */
/* refer to the Sun's center or its upper limb.                         */


/* This macro computes the length of the day, from sunrise to sunset. */
/* Sunrise/set is considered to occur when the Sun's upper limb is    */
/* 35 arc minutes below the horizon (this accounts for the refraction */
/* of the Earth's atmosphere).                                        */
#define day_length(year,month,day,lon,lat)  \
        __daylen__( year, month, day, lon, lat, -35.0/60.0, 1 )

/* This macro computes the length of the day, including civil twilight. */
/* Civil twilight starts/ends when the Sun's center is 6 degrees below  */
/* the horizon.                                                         */
#define day_civil_twilight_length(year,month,day,lon,lat)  \
        __daylen__( year, month, day, lon, lat, -6.0, 0 )

/* This macro computes the length of the day, incl. nautical twilight.  */
/* Nautical twilight starts/ends when the Sun's center is 12 degrees    */
/* below the horizon.                                                   */
#define day_nautical_twilight_length(year,month,day,lon,lat)  \
        __daylen__( year, month, day, lon, lat, -12.0, 0 )

/* This macro computes the length of the day, incl. astronomical twilight. */
/* Astronomical twilight starts/ends when the Sun's center is 18 degrees   */
/* below the horizon.                                                      */
#define day_astronomical_twilight_length(year,month,day,lon,lat)  \
        __daylen__( year, month, day, lon, lat, -18.0, 0 )


/* This macro computes times for sunrise/sunset.                      */
/* Sunrise/set is considered to occur when the Sun's upper limb is    */
/* 35 arc minutes below the horizon (this accounts for the refraction */
/* of the Earth's atmosphere).                                        */
#define sun_rise_set(year,month,day,lon,lat,rise,set)  \
        __sunriset__( year, month, day, lon, lat, -35.0/60.0, 1, rise, set )

/* This macro computes the start and end times of civil twilight.       */
/* Civil twilight starts/ends when the Sun's center is 6 degrees below  */
/* the horizon.                                                         */
#define civil_twilight(year,month,day,lon,lat,start,end)  \
        __sunriset__( year, month, day, lon, lat, -6.0, 0, start, end )

/* This macro computes the start and end times of nautical twilight.    */
/* Nautical twilight starts/ends when the Sun's center is 12 degrees    */
/* below the horizon.                                                   */
#define nautical_twilight(year,month,day,lon,lat,start,end)  \
        __sunriset__( year, month, day, lon, lat, -12.0, 0, start, end )

/* This macro computes the start and end times of astronomical twilight.   */
/* Astronomical twilight starts/ends when the Sun's center is 18 degrees   */
/* below the horizon.                                                      */
#define astronomical_twilight(year,month,day,lon,lat,start,end)  \
        __sunriset__( year, month, day, lon, lat, -18.0, 0, start, end )


/* Function prototypes */

double __daylen__( int year, int month, int day, double lon, double lat,
                   double altit, int upper_limb );

int __sunriset__( int year, int month, int day, double lon, double lat,
                  double altit, int upper_limb, double *rise, double *set );

void sunpos( double d, double *lon, double *r );

void sun_RA_dec( double d, double *RA, double *dec, double *r );

double revolution( double x );

double rev180( double x );

double GMST0( double d );

// Display decimal hours in hours and minutes (for relative times)
double showhrmn(double dhr) {
double hr,mn,hrmn;
hr=(int) dhr;
mn = (dhr - (double) hr)*0.6;
hrmn=hr+mn;
return(hrmn);
};

// Display decimal hours in hours and minutes, adding localdst (for absolute times, which must include local shifts)
double showhrmnL(double dhr) {
double hr,mn,hrmn;
dhr=dhr+localdst/3600;
hr=(int) dhr;
mn = (dhr - (double) hr)*0.6;
hrmn=hr+mn;
return(hrmn);
};

/* A small test program */

int main(int argc, char *argv[])
{
      int ManualTime,ManualTz,year,month,day;
      int hour, minute, second;
      double lon, lat, tz;
      double daylen, civlen, nautlen, astrlen;
      double rise, set, civ_start, civ_end, naut_start, naut_end,
             astr_start, astr_end;
      int    rs, civ, naut, astr;
      struct tm time_str;

#ifdef ARM
  fb_init();
	TsScreen_Init();

		/* clear screen & go to top */
	if (fb_MaxX >= 480) {
		fb_rect(0, 0, fb_MaxX-1, fb_MaxY-1, fb_colour(128, 255, 255)); // light cyan
		fb_rect(54, 0, fb_MaxX-1-54, fb_MaxY-1, fb_colour(0xff, 0xff, 0xff)); // white background
	}
	else
		fb_rect(0, 0, fb_MaxX-1, fb_MaxY-1, fb_colour(0xff, 0xff, 0xff)); // white background
	fb_rect(0, 0, fb_MaxX-1, fb_FontY-1, 0); // reverse the first line with the title (black background)
	
	fb_x=0;
	fb_y=0;
	fb_col=fb_colour(0xFF, 0xFF, 0x0); // Yellow (ink for the title)
	fb_NumInk=fb_colour(0xff, 0x00, 0x00); // ink colour for numeric digits is red

#endif
	fb_setfont(1);
	fb_center=1;
	if (fb_MaxX >= 480)
		printf("Event_Logger - Astronomical Calendar - Ephemerides of Sun and Moon");
	else
		printf("Astronomical Calendar - Ephemerides of Sun and Moon");
	fb_center=0;

#ifdef ARM
	fb_col=fb_colour(0x00, 0x00, 0x00); // Black (standard ink for the text)
	if (fb_MaxX >= 480) {
		fb_setfont(2);
		fb_xr=58;
		fb_y=11;
	}
	else {
		fb_y=15;
		fb_xr=2;
	}

	fb_x=0;

	  // flush pen input
  {
		ts_x=0; ts_y=0; ts_pen=0;
    while (TsScreen_pen(&ts_x,&ts_y,&ts_pen));
  }
#else
	printf("\n");
#endif

int Flag_All=0;
int Flag_Chk=0;
int Flag_Debug=0;
int Flag_LocalTime=0;

if ((argc > 1) && (strcmp(argv[1],"-c")==0))
{
     Flag_Chk=1;
     argc--;
     argv++;
}

if ((argc > 1) && (strcmp(argv[1],"-d")==0))
{
     Flag_Debug=1;
     argc--;
     argv++;
}

if ((argc > 1) && (strcmp(argv[1],"-l")==0))
{
     Flag_LocalTime=1;
     argc--;
     argv++;
}

if ((argc > 1) && (strcmp(argv[1],"-a")==0))
{
     Flag_All=1;
     argc--;
     argv++;
}

ManualTz=ManualTime=0;

if ((argc > 1) && (strcmp(argv[1],"-m")==0))
{
     ManualTz=1;
     argc--;
     argv++;
}

     if (argc == 5)
      {
	      localdst = atof(argv[--argc]);
	      if (!ManualTz)
        {
     	    tzset();				/* read _timezone */
			    localdst = (long) _timezone;
	      }
		    tz = localdst / -3600;

	      lon = atof(argv[--argc]);
	      lat = atof(argv[--argc]);

	      ManualTime=strcmp(argv[--argc], "A");

	      if (ManualTime)
	      {
    	     UTC = atoi(argv[argc]);
	         ManualTime=(UTC!=0);
  		     //gmtime_r(&UTC, &time_str);if (Flag_Chk) printf("UTC=%d %d-%d-%d %d:%02d:%02d isdst=%d ManualTime=%d ManualTz=%d Flag_LocalTime=%d timezone=%d daylight=%d localdst=%d tz=%f\n", UTC, time_str.tm_year+1900, time_str.tm_mon+1, time_str.tm_mday, time_str.tm_hour, time_str.tm_min, time_str.tm_sec, time_str.tm_isdst, ManualTime, ManualTz, Flag_LocalTime, _timezone, _daylight, localdst, tz);
	         if (Flag_Chk) printf("Manual UTC=%d %s\n",UTC,ctime(&UTC));
	      }
	      if (!ManualTime)
	      {
					// read system date and extract the year
				
					/** First get time **/
				    tzset();				/* read _timezone */
				    time(&UTC);				/* get time */
	         if (Flag_Chk) printf("Automatic UTC=%d %s\n",UTC,ctime(&UTC));
	      }
  		  if (Flag_Chk) printf("ManualTime=%d ManualTz=%d Flag_LocalTime=%d timezone=%d daylight=%d localdst=%d tz=%f\n", ManualTime, ManualTz, Flag_LocalTime, _timezone, _daylight, localdst, tz);
				if (Flag_LocalTime)
				      {
						    if (ManualTime) UTC=UTC-_timezone+localdst;
				      }
				else
				      {
						    if (!ManualTime) UTC=UTC+localdst;
						    if (ManualTime) UTC=UTC-_timezone;
						  }

					/** Next generate time_str **/
		    gmtime_r(&UTC, &time_str);
		    // subtract timezone as mktime calculates a local time including timezone, which should be managed by localdst
  		  if (Flag_Chk) printf("OK %d-%d-%d %d:%02d:%02d isdst=%d UTC=%d timezone=%d daylight=%d localdst=%d tz=%f\n", time_str.tm_year+1900, time_str.tm_mon+1, time_str.tm_mday, time_str.tm_hour, time_str.tm_min, time_str.tm_sec, time_str.tm_isdst, UTC, _timezone, _daylight, localdst, tz);
      }
      else
      {
      printf("Input year, month and day [e.g., 2005 11 02 or 0 0 0 for today]\n");
      scanf("%d %d %d", &year, &month, &day);

      printf("Input latitude, longitude and timezone [e.g., 45.3 9.5 +1]\n");
      scanf("%lf %lf %lf", &lat, &lon, &tz);
      }

		  year = time_str.tm_year+1900;
		  month = time_str.tm_mon + 1;
		  day = time_str.tm_mday;

      if (Flag_Chk) printf("%2d-%2d-%4d lat=%lf, lon=%lf\n",(int)day,(int)month,(int)year,lat,lon);

      //printf( "Longitude (+ is east) and latitude (+ is north) : " );
      //scanf( "%lf %lf", &lon, &lat );

            //printf( "Input date ( yyyy mm dd ) (ctrl-C exits): " );
            //scanf( "%d %d %d", &year, &month, &day );

            daylen  = day_length(year,month,day,lon,lat);
            civlen  = day_civil_twilight_length(year,month,day,lon,lat);
            nautlen = day_nautical_twilight_length(year,month,day,lon,lat);
            astrlen = day_astronomical_twilight_length(year,month,day,lon,lat);

if (Flag_All)
{
            printf( "Day length (hh.mm): %.2f. ", showhrmn(daylen) );
            printf( "With civil twilight %.2f\n", showhrmn(civlen) );
            printf( "With nautical/astronomical twilight: %.2f / %.2f\n", showhrmn(nautlen), showhrmn(astrlen) );
            printf( "Length of twilight: %.2f/%.2f/%.2f (civ/nau/astr)\n",
                  showhrmn((civlen-daylen)/2.0),showhrmn((nautlen-daylen)/2.0),showhrmn((astrlen-daylen)/2.0));
}
            rs   = sun_rise_set         ( year, month, day, lon, lat,
                                          &rise, &set );
            civ  = civil_twilight       ( year, month, day, lon, lat,
                                          &civ_start, &civ_end );
            naut = nautical_twilight    ( year, month, day, lon, lat,
                                          &naut_start, &naut_end );
            astr = astronomical_twilight( year, month, day, lon, lat,
                                          &astr_start, &astr_end );

            if (Flag_Chk) printf("rise=%lf, set=%lf, civ_start=%lf, civ_end=%lf, naut_start=%lf, naut_end=%lf,  astr_start=%lf, astr_end=%lf,  lat=%lf, lon=%lf\n",rise, set,civ_start, civ_end, naut_start, naut_end, astr_start, astr_end,lat,lon);

            printf( "Sun at south at %.2f; ", showhrmnL((rise+set)/2.0) );

		        fb_col=fb_colour(0x00, 0x00, 0xFF);

            switch( rs )
            {
                case 0:
                    printf( "rises at %.2f, sets at %.2f\n",
                             showhrmnL(rise), showhrmnL(set) );
                    break;
                case +1:
                    printf( "Sun above horizon\n" );
                    break;
                case -1:
                    printf( "Sun below horizon\n" );
                    break;
            }

		        fb_col=fb_colour(0x00, 0x00, 0x00);

            switch( civ )
            {
                case 0:
                    printf( "Civil twilight starts at %.2f, "
                            "ends at %.2f\n", showhrmnL(civ_start), showhrmnL(civ_end) );
                    break;
                case +1:
                    printf( "Never darker than civil twilight\n" );
                    break;
                case -1:
                    printf( "Never as bright as civil twilight\n" );
                    break;
            }

            switch( naut )
            {
                case 0:
                    printf( "Nautical twilight starts at %.2f, "
                            "ends at %.2f\n", showhrmnL(naut_start), showhrmnL(naut_end) );
                    break;
                case +1:
                    printf( "Never darker than nautical twilight\n" );
                    break;
                case -1:
                    printf( "Never as bright as nautical twilight\n" );
                    break;
            }

            switch( astr )
            {
                case 0:
                    printf( "Astronomical twilight starts at %.2f, "
                            "ends at %.2f\n", showhrmnL(astr_start), showhrmnL(astr_end) );
                    break;
                case +1:
                    printf( "Never darker than astronomical twilight\n" );
                    break;
                case -1:
                    printf( "Never as bright as astronomical twilight\n" );
                    break;
            }

    /*
    ** compute moonrise and moonset
    */
    /* at this point we digress to discuss UNIX differences.
     * In UCB UNIX we dont have ctime(), but do instead have asctime(),
     * which works from the structures created by gmtime() and localtime().
     * However, system time is kept in UTC (Greenwich), and the localtime
     * routine correctly handles daylight savings time.
     * Since the Regulus system only knows local time, a few direct
     * fiddles are needed.
     */
    
    /* correct apparent latitude for shape of Earth */

    lf = atan(f1*f1 * tan(lat * 3600. * Asec_Radian));
    sp = sin(lf);
    cp = cos(lf);
    tp = sp/cp;

    Local = lon*3600/15.;		/* Local apparent time correction */

if (Flag_Debug)
{
    printf("UTC=%d, tim=%d %d:%d:%d %d\n", UTC, tim, time_str.tm_hour, time_str.tm_min, time_str.tm_sec, localdst);

    //printf("%.24s GMT\n", gmctime(&UTC));
    //printf("UTC=%d, tim=%d %d:%d:%d %d\n", UTC, tim, time_str.tm_hour, time_str.tm_min, time_str.tm_sec, localdst);
}
    
    stuff(UTC);			/* start with local time info */
    
    /*
    ** Compute Terrestrial Dynamical Time
    ** (this used to be called Ephemeris Time)
    */
    
    TDT = UTC + (long)(Delta_T + Round);
    tim2 = UTC + (long)(Local + Round);	/* Compute Local Solar Time */

if (Flag_All)
{
    tdate= gmctime(&UTC);
    printf("Local Civil Time - ");
//    fb_BgCol=fb_colour(224, 255, 255); // light cyan
//    fb_col=fb_colour(0x00, 0x64, 0x00); // Dark Green
    fb_Bold=1; // Set bold attribute (bold font is 7x11)
    printf("%s", tdate);
    fb_Bold=0; // Reset bold attribute (standard font is 6x11)
//    fb_col=fb_colour(0x00, 0x00, 0x00); // Black
    fb_BgCol=-1;

    tdate= gmctime(&TDT);
		fb_setfont(1);
    printf("%.8s Terrestrial Dynamical Time\n", tdate+11);

    
    tdate= gmctime(&tim2);
    printf("%.8s Local Mean Time", tdate+11);

    printf(" - Julian Day 24%9.3f\n", Julian_Day);
}

    /*
    ** compute phase of moon
    */
    
		if (fb_MaxX >= 480)
			fb_setfont(2);

    moondata(UTC);
    Lm = replace_fmod(Lm-L, 360.);	/* phase is Lm - L (longitude of Sun) */
    lm = replace_fmod(Lm, 90.);		/* excess over phase boundary         */
    days=lm*36525./481267.883;
    #define MOONQUARTER 11
		fb_col=fb_colour(0x00, 0x00, 0xFF);
    printf("The Moon is %s: %d days and %.1f\n hours past ", description[(int)((Lm+MOONQUARTER)/90) + (int)( (Lm-MOONQUARTER) / 90)+(Lm>(MOONQUARTER-1))], (int) days, (days-(int) days)*24);
    if	(Lm <  90.)	printf("New. ");
    else if (Lm < 180.)	printf("First Qtr. ");
    else if (Lm < 270.)	printf("Full. ");
    else		printf("Last Qtr. ");
		fb_col=fb_colour(0x00, 0x00, 0x00);

    if (altM>0) // amacri (moon is visible)
    {
      fb_BgCol=fb_colour(250, 250, 210); // light goldenrod yellow
//			fb_col=fb_colour(0x00, 0x64, 0x00); // Dark Green
    }

		printf("Az=%.f[%s], Elev.=%.f dgr.\n",AzM/3600., compass[(int)(AzM/81000 /* = 3600 * 22.5 */ +0.5)], altM/3600.); // amacri 2006: added azimuth and elevation of the moon
    fb_col=fb_colour(0x00, 0x00, 0x00); // Black
    fb_BgCol=-1;

    tim2 = GMST + Round;
    tdate= gmctime(&tim2);

fb_setfont(1);

if (Flag_All)
{
    printf(" %.8s  Greenwich Mean Sidereal Time\n", tdate+11);
//    printf("GMST=%f, Round=%f tim2=%d tdate=%s\n", GMST, Round, tim2, tdate);
}
    
    tim2 = LST + Round;
    tdate= gmctime(&tim2);

if (Flag_All)
{
    printf(" %.8s  Local Sidereal Time\n", tdate+11);
//    printf("LST=%f, Round=%f tim2=%d tdate=%s\n", LST, Round, tim2, tdate);
}
    
    tim2= lha + Round;
    tdate= gmctime(&tim2);

if (Flag_All)
{
    printf(" %.8s  Local Hour Angle (LHA) of Sun\n", tdate+11);
}

		if (fb_MaxX >= 480)
		fb_setfont(2);

    printf("Sun: Declination %.3f Degrees,\n ",delta/3600.);
if (alt > -2999)
{
    fb_BgCol=fb_colour(250, 250, 210); // light goldenrod yellow (sun is visible)
//    fb_col=fb_colour(0x00, 0x64, 0x00); // Dark Green
}
    printf("Azimuth %.3f Deg. [%s], ",Az/3600., compass[(int)(Az/81000 /* = 3600 * 22.5 */ +0.5)]);
    printf("Elevation %.3f degr.\n",alt/3600.);
    fb_col=fb_colour(0x00, 0x00, 0x00); // Black
    fb_BgCol=-1;

		fb_setfont(1);

if (Flag_Debug)
{
    printf("1. UTC=%d, tim=%d %d:%d:%d %d\n", UTC, tim, time_str.tm_hour, time_str.tm_min, time_str.tm_sec, localdst);
}
    tim = UTC - (3600L * time_str.tm_hour + 60L * time_str.tm_min + time_str.tm_sec)
      + Sec_per_day/2;			/* about noon           */

if (Flag_Debug)
{
    printf("2. UTC=%d, tim=%d %d:%d:%d %d\n", UTC, tim, time_str.tm_hour, time_str.tm_min, time_str.tm_sec, localdst);
}
    tim = tim - Sec_per_day/2 - 31 - localdst /* - localdst = DEBUG!! */;	/* about start of day */
if (Flag_Debug)
{
    printf("3. UTC=%d, tim=%d %d:%d:%d %d\n", UTC, tim, time_str.tm_hour, time_str.tm_min, time_str.tm_sec, localdst);
}

		if (fb_MaxX >= 480)
			fb_setfont(2);

    zs = 90. + 34./60.;		/* zenith angle of rise/set */
    moonrise(tim, -1.0, zs, "Moonrise", " of tomorrow.");
    printf("   ");
    moonrise((long)(tim+Sec_per_day), -1.0, zs, "Tomorrow:", " of the day after.");
    printf("\n");
		if (fb_MaxX >= 480)
	    fb_y -= 1;
    moonrise(tim, 1.0, zs, "Moonset ", " of tomorrow.");
    printf("   ");
    moonrise((long)(tim+Sec_per_day), 1.0, zs, "Tomorrow:", " of the day after.");
    printf("\n");

		fb_setfont(1);
    fb_ys=0;

#ifdef LINUX_PRESS
  {
		ts_x=0; ts_y=0; ts_pen=0;
		struct timeval tv;
		fd_set readfds;
	  static TS_EVENT prev_event;

//		while (!TsScreen_pen(&x,&y,&pen));

		tv.tv_sec = 20;
		tv.tv_usec = 0;
		FD_ZERO (&readfds);
    FD_SET(tsfd, &readfds);
		select (tsfd+1, &readfds, NULL, NULL, &tv);
		if (FD_ISSET (tsfd, &readfds)) {
				   while (read(tsfd, &prev_event, sizeof(TS_EVENT)) == sizeof(TS_EVENT));
			}

		fb_rect(0, 0, fb_MaxX-1, fb_MaxY-1, fb_colour(0xff, 0xff, 0x00));
		tv.tv_sec = 1;
		tv.tv_usec = 0;
		FD_ZERO (&readfds);
    FD_SET(tsfd, &readfds);
		select (tsfd+1, &readfds, NULL, NULL, &tv);
    while (TsScreen_pen(&ts_x,&ts_y,&ts_pen));
		TsScreen_Exit;
  }
#endif
		exit(0);
}


/* The "workhorse" function for sun rise/set times */

int __sunriset__( int year, int month, int day, double lon, double lat,
                  double altit, int upper_limb, double *trise, double *tset )
/***************************************************************************/
/* Note: year,month,date = calendar date, 1801-2099 only.             */
/*       Eastern longitude positive, Western longitude negative       */
/*       Northern latitude positive, Southern latitude negative       */
/*       The longitude value IS critical in this function!            */
/*       altit = the altitude which the Sun should cross              */
/*               Set to -35/60 degrees for rise/set, -6 degrees       */
/*               for civil, -12 degrees for nautical and -18          */
/*               degrees for astronomical twilight.                   */
/*         upper_limb: non-zero -> upper limb, zero -> center         */
/*               Set to non-zero (e.g. 1) when computing rise/set     */
/*               times, and to zero when computing start/end of       */
/*               twilight.                                            */
/*        *rise = where to store the rise time                        */
/*        *set  = where to store the set  time                        */
/*                Both times are relative to the specified altitude,  */
/*                and thus this function can be used to comupte       */
/*                various twilight times, as well as rise/set times   */
/* Return value:  0 = sun rises/sets this day, times stored at        */
/*                    *trise and *tset.                               */
/*               +1 = sun above the specified "horizon" 24 hours.     */
/*                    *trise set to time when the sun is at south,    */
/*                    minus 12 hours while *tset is set to the south  */
/*                    time plus 12 hours. "Day" length = 24 hours     */
/*               -1 = sun is below the specified "horizon" 24 hours   */
/*                    "Day" length = 0 hours, *trise and *tset are    */
/*                    both set to the time when the sun is at south.  */
/*                                                                    */
/**********************************************************************/
{
      double  d,  /* Days since 2000 Jan 0.0 (negative before) */
      sr,         /* Solar distance, astronomical units */
      sRA,        /* Sun's Right Ascension */
      sdec,       /* Sun's declination */
      sradius,    /* Sun's apparent radius */
      t,          /* Diurnal arc */
      tsouth,     /* Time when Sun is at south */
      sidtime;    /* Local sidereal time */

      int rc = 0; /* Return cde from function - usually 0 */

      /* Compute d of 12h local mean solar time */
      d = days_since_2000_Jan_0(year,month,day) + 0.5 - lon/360.0;

      /* Compute local sideral time of this moment */
      sidtime = revolution( GMST0(d) + 180.0 + lon );

      /* Compute Sun's RA + Decl at this moment */
      sun_RA_dec( d, &sRA, &sdec, &sr );

      /* Compute time when Sun is at south - in hours UT */
      tsouth = 12.0 - rev180(sidtime - sRA)/15.0;

      /* Compute the Sun's apparent radius, degrees */
      sradius = 0.2666 / sr;

      /* Do correction to upper limb, if necessary */
      if ( upper_limb )
            altit -= sradius;

      /* Compute the diurnal arc that the Sun traverses to reach */
      /* the specified altitide altit: */
      {
            double cost;
            cost = ( sind(altit) - sind(lat) * sind(sdec) ) /
                  ( cosd(lat) * cosd(sdec) );
            if ( cost >= 1.0 )
                  rc = -1, t = 0.0;       /* Sun always below altit */
            else if ( cost <= -1.0 )
                  rc = +1, t = 12.0;      /* Sun always above altit */
            else
                  t = acosd(cost)/15.0;   /* The diurnal arc, hours */
      }

      /* Store rise and set times - in hours UT */
      *trise = tsouth - t;
      *tset  = tsouth + t;

      return rc;
}  /* __sunriset__ */



/* The "workhorse" function */


double __daylen__( int year, int month, int day, double lon, double lat,
                   double altit, int upper_limb )
/**********************************************************************/
/* Note: year,month,date = calendar date, 1801-2099 only.             */
/*       Eastern longitude positive, Western longitude negative       */
/*       Northern latitude positive, Southern latitude negative       */
/*       The longitude value is not critical. Set it to the correct   */
/*       longitude if you're picky, otherwise set to to, say, 0.0     */
/*       The latitude however IS critical - be sure to get it correct */
/*       altit = the altitude which the Sun should cross              */
/*               Set to -35/60 degrees for rise/set, -6 degrees       */
/*               for civil, -12 degrees for nautical and -18          */
/*               degrees for astronomical twilight.                   */
/*         upper_limb: non-zero -> upper limb, zero -> center         */
/*               Set to non-zero (e.g. 1) when computing day length   */
/*               and to zero when computing day+twilight length.      */
/**********************************************************************/
{
      double  d,  /* Days since 2000 Jan 0.0 (negative before) */
      obl_ecl,    /* Obliquity (inclination) of Earth's axis */
      sr,         /* Solar distance, astronomical units */
      slon,       /* True solar longitude */
      sin_sdecl,  /* Sine of Sun's declination */
      cos_sdecl,  /* Cosine of Sun's declination */
      sradius,    /* Sun's apparent radius */
      t;          /* Diurnal arc */

      /* Compute d of 12h local mean solar time */
      d = days_since_2000_Jan_0(year,month,day) + 0.5 - lon/360.0;
d=(int)d; /* amacri */

      /* Compute obliquity of ecliptic (inclination of Earth's axis) */
      obl_ecl = 23.4393 - 3.563E-7 * d;

      /* Compute Sun's position */
      sunpos( d, &slon, &sr );

      /* Compute sine and cosine of Sun's declination */
      sin_sdecl = sind(obl_ecl) * sind(slon);
      cos_sdecl = sqrt( 1.0 - sin_sdecl * sin_sdecl );

      /* Compute the Sun's apparent radius, degrees */
      sradius = 0.2666 / sr;

      /* Do correction to upper limb, if necessary */
      if ( upper_limb )
            altit -= sradius;

      /* Compute the diurnal arc that the Sun traverses to reach */
      /* the specified altitide altit: */
      {
            double cost;
            cost = ( sind(altit) - sind(lat) * sin_sdecl ) /
                  ( cosd(lat) * cos_sdecl );
            if ( cost >= 1.0 )
                  t = 0.0;                      /* Sun always below altit */
            else if ( cost <= -1.0 )
                  t = 24.0;                     /* Sun always above altit */
            else  t = (2.0/15.0) * acosd(cost); /* The diurnal arc, hours */
      }
      return t;
}  /* __daylen__ */


/* This function computes the Sun's position at any instant */

void sunpos( double d, double *lon, double *r )
/******************************************************/
/* Computes the Sun's ecliptic longitude and distance */
/* at an instant given in d, number of days since     */
/* 2000 Jan 0.0.  The Sun's ecliptic latitude is not  */
/* computed, since it's always very near 0.           */
/******************************************************/
{
      double M,         /* Mean anomaly of the Sun */
             w,         /* Mean longitude of perihelion */
                        /* Note: Sun's mean longitude = M + w */
             e,         /* Eccentricity of Earth's orbit */
             E,         /* Eccentric anomaly */
             x, y,      /* x, y coordinates in orbit */
             v;         /* True anomaly */

      /* Compute mean elements */
      M = revolution( 356.0470 + 0.9856002585 * d );
      w = 282.9404 + 4.70935E-5 * d;
      e = 0.016709 - 1.151E-9 * d;

      /* Compute true longitude and radius vector */
      E = M + e * RADEG * sind(M) * ( 1.0 + e * cosd(M) );
            x = cosd(E) - e;
      y = sqrt( 1.0 - e*e ) * sind(E);
      *r = sqrt( x*x + y*y );              /* Solar distance */
      v = atan2d( y, x );                  /* True anomaly */
      *lon = v + w;                        /* True solar longitude */
      if ( *lon >= 360.0 )
            *lon -= 360.0;                   /* Make it 0..360 degrees */
}

void sun_RA_dec( double d, double *RA, double *dec, double *r )
{
      double lon, obl_ecl, x, y, z;

      /* Compute Sun's ecliptical coordinates */
      sunpos( d, &lon, r );

      /* Compute ecliptic rectangular coordinates (z=0) */
      x = *r * cosd(lon);
      y = *r * sind(lon);

      /* Compute obliquity of ecliptic (inclination of Earth's axis) */
      obl_ecl = 23.4393 - 3.563E-7 * d;

      /* Convert to equatorial rectangular coordinates - x is uchanged */
      z = y * sind(obl_ecl);
      y = y * cosd(obl_ecl);

      /* Convert to spherical coordinates */
      *RA = atan2d( y, x );
      *dec = atan2d( z, sqrt(x*x + y*y) );

}  /* sun_RA_dec */


/******************************************************************/
/* This function reduces any angle to within the first revolution */
/* by subtracting or adding even multiples of 360.0 until the     */
/* result is >= 0.0 and < 360.0                                   */
/******************************************************************/

#define INV360    ( 1.0 / 360.0 )

double revolution( double x )
/*****************************************/
/* Reduce angle to within 0..360 degrees */
/*****************************************/
{
      return( x - 360.0 * floor( x * INV360 ) );
}  /* revolution */

double rev180( double x )
/*********************************************/
/* Reduce angle to within +180..+180 degrees */
/*********************************************/
{
      return( x - 360.0 * floor( x * INV360 + 0.5 ) );
}  /* revolution */


/*******************************************************************/
/* This function computes GMST0, the Greenwhich Mean Sidereal Time */
/* at 0h UT (i.e. the sidereal time at the Greenwhich meridian at  */
/* 0h UT).  GMST is then the sidereal time at Greenwich at any     */
/* time of the day.  I've generelized GMST0 as well, and define it */
/* as:  GMST0 = GMST - UT  --  this allows GMST0 to be computed at */
/* other times than 0h UT as well.  While this sounds somewhat     */
/* contradictory, it is very practical:  instead of computing      */
/* GMST like:                                                      */
/*                                                                 */
/*  GMST = (GMST0) + UT * (366.2422/365.2422)                      */
/*                                                                 */
/* where (GMST0) is the GMST last time UT was 0 hours, one simply  */
/* computes:                                                       */
/*                                                                 */
/*  GMST = GMST0 + UT                                              */
/*                                                                 */
/* where GMST0 is the GMST "at 0h UT" but at the current moment!   */
/* Defined in this way, GMST0 will increase with about 4 min a     */
/* day.  It also happens that GMST0 (in degrees, 1 hr = 15 degr)   */
/* is equal to the Sun's mean longitude plus/minus 180 degrees!    */
/* (if we neglect aberration, which amounts to 20 seconds of arc   */
/* or 1.33 seconds of time)                                        */
/*                                                                 */
/*******************************************************************/

double GMST0( double d )
{
      double sidtim0;
      /* Sidtime at 0h UT = L (Sun's mean longitude) + 180.0 degr  */
      /* L = M + w, as defined in sunpos().  Since I'm too lazy to */
      /* add these numbers, I'll let the C compiler do it for me.  */
      /* Any decent C compiler will add the constants at compile   */
      /* time, imposing no runtime or code overhead.               */
      sidtim0 = revolution( ( 180.0 + 356.0470 + 282.9404 ) +
                          ( 0.9856002585 + 4.70935E-5 ) * d );
      return sidtim0;
}  /* GMST0 */


/*******************************************************************/
/* MOONRISE was taken from the following program:                  */
/*******************************************************************/
/* <sdate.c>
 * Compute various useful times
 *
 * Written by Marc T. Kaufman
 *            14100 Donelson Place
 *            Los Altos Hills, CA 94022
 *            (415) 948-3777
 *
 * Based on : "Explanatory Supplement to the Astronomical Ephemeris
 *             and the American Ephemeris and Nautical Almanac",
 *             H.M. Nautical Almanac Office, London.  Updated from
 *             equations in the 1985 Astronomical Almanac.
 *
 * Copyright 1986 by Marc Kaufman
 *
 * Permission to use this program is granted, provided it is not sold.
 *
 * This program was originally written on a VAX, under 4.2bsd.
 *  it was then ported to a 68000 system under REGULUS (Alcyon's version
 *  of UNIX system III).  Major differences included: no 'double' and
 *  a default integer length of 'short'.  Having been through all that,
 *  porting to your machine should be easy.  Watch out for 'time' related
 *  functions and make sure your 'atan2' program works right.
 *
 *	850210	revised to 1985 Ephemeris - mtk
 *	921031  revised to use under AT&T Unix 3.x (3B2/3B1) - NJC
 */

double	Julian_Day, MJD, Tu, Ru, T70, Local, GMST, LST;
double	Eqt, Tua, L, G, e, eps, g, alpha, delta, sd, cd, lha, lhr, sh, ch;
double	la, lf, S, C, sp, cp, tp, Az, alt;
double	Lm, lm, px, SD, am, dm;
char	*tdate, *gmctime();

moonrise(t0, rs, z, s, ds)
long t0; // UTC of about the start of the day
double rs, z; // rs=-1: rise; rs=1: set.  z=zenith angle of rise/set (e.g., 90. + 34./60.)
char *s; // string (like "Moonrise" or "Moonset")
char *ds; // other string used when moonsize/set happens the day after (like "of day after" or "of tomorrow")
// E.g., zs = 90. + 34./60.;		/* zenith angle of rise/set */
//       moonrise(tim, -1.0, zs, "Moonrise", " of tomorrow");
{
#define SRATE	1.033863192	/* ratio of Moon's motion to Sun's motion */

  double	cz, dh, sd, cd;
  long	t1, t1L, dt;
  
//printf(" moonrise(t0=%d, rs=%f, z=%f, s=%s) ",t0,rs,z,s);
  moondata(t0);	/* get starting declination of Moon */
  
  /*
  ** compute zenith distance of phenomonon
  */

  cz = cos(z * Degree_to_Radian + SD /* -px */);
  
  /* first iteraton is forward only (to approx. phenom time) */
  sd = sin(dm);
  cd = cos(dm);
  dh = -tp*sd/cd + cz/(cp*cd);

  if ((dh < -1.0) || (dh > 1.0)) {
    printf("%.8s   none   ", s);
    return;
  }

  dh = acos(dh)*rs;
  dt = (long) replace_fmod( (double) (dh - am), (double) (2.0*Pi) ) * SRATE / Tsec_to_Radian;
  t1 = t0 + dt;
//printf("t0=%d t1=%d dt=%d|%f SD=%f cz=%f dm=%f tp=%f am=%f rs=%f dh=%f\n",t0,t1,dt,dh - am,SD,cz,dm,tp,am,rs,dh);
  
  do {	/* iterate */
    moondata(t1);	/* compute declination and current hour angle */
//printf("dt=%d dm=%f t1=%d cz=%f dh=%f\n",dt,dm,t1,cz,dh);
    cz = cos(z * Degree_to_Radian + SD /* -px */);
    sd = sin(dm);
    cd = cos(dm);
    
    dh= -tp*sd/cd + cz/(cp*cd);

    if ((dh < -1.0) || (dh > 1.0)) {
      printf("%.8s   none  ", s);
      return;
    }

    dh= acos(dh)*rs;
    dt= (dh - am) * SRATE / Tsec_to_Radian;
    t1 += dt;
  } while (dt);
//printf("t0=%d dt=%d dm=%f t1=%d cz=%f dh=%f\n",t0,dt,dm,t1,cz,dh);
  
//  if ((t1 - t0) >= Sec_per_day) {
//    printf("%.8s   none   ", s);
//    return;
//  }

  t1L=t1+30+localdst;  /* seconds since the Epoch, including local time and rounding to nearest minute */;
  strftime(tdate, 10, "%H:%M", gmtime(&t1L));
//printf("t0=%d dt=%d t1=%d t1-t0=%f day-t1+t0=%d\n",t0,dt,t1,(double) (t1-t0)/3600,Sec_per_day-t1+t0);
  if ((t1 - t0) >= Sec_per_day) strcat(tdate,ds);
  printf("%.9s %s ", s, tdate);

}

moondata(tim)
long	tim;
{
  double	lst, beta, rm, sa, ca, sl, cl, sb, cb, x, y, z, l, m, n, calt;
  
  /* compute location of the moon */
  /* Ephemeris elements from 1985 Almanac */
  
  timedata(tim);
  
  // Lm=Moon's mean longitude
  Lm= 218.32 + 481267.883*Tu
    + 6.29 * sin((134.9 + 477198.85*Tu)*Degree_to_Radian)
      - 1.27 * sin((259.2 - 413335.38*Tu)*Degree_to_Radian)
	+ 0.66 * sin((235.7 + 890534.23*Tu)*Degree_to_Radian)
	  + 0.21 * sin((269.9 + 954397.70*Tu)*Degree_to_Radian)
	    - 0.19 * sin((357.5 +  35999.05*Tu)*Degree_to_Radian)
	      - 0.11 * sin((186.6 + 966404.05*Tu)*Degree_to_Radian);
  
  beta=	  5.13 * sin(( 93.3 + 483202.03*Tu)*Degree_to_Radian)
    + 0.28 * sin((228.2 + 960400.87*Tu)*Degree_to_Radian)
      - 0.28 * sin((318.3 +   6003.18*Tu)*Degree_to_Radian)
	- 0.17 * sin((217.6 - 407332.20*Tu)*Degree_to_Radian);
  
  px= 0.9508
    + 0.0518 * cos((134.9 + 477198.85*Tu)*Degree_to_Radian)
      + 0.0095 * cos((259.2 - 413335.38*Tu)*Degree_to_Radian)
	+ 0.0078 * cos((235.7 + 890534.23*Tu)*Degree_to_Radian)
	  + 0.0028 * cos((269.9 + 954397.70*Tu)*Degree_to_Radian);
  
//printf("Lm=%f,beta=%f,px=%f\n",  Lm,beta,px);
  /*	SD= 0.2725 * px;	*/
  
  rm  = 1.0 / sin(px * Degree_to_Radian); // Distance
  
  lst = (100.46 + 36000.77*Tu) * Degree_to_Radian
    + ((tim % Sec_per_day) + Local) * Tsec_to_Radian;  //lst=Local Sidereal Time
//printf("Local=%f, lst=%f\n",  Local, lst);
  
  /* form geocentric direction cosines */
  
  sl = sin(Lm * Degree_to_Radian);
  cl = cos(Lm * Degree_to_Radian);
  sb = sin(beta* Degree_to_Radian);
  cb = cos(beta * Degree_to_Radian);
  
  l = cb * cl;
  m = 0.9175 * cb * sl - 0.3978 * sb;
  n = 0.3978 * cb * sl + 0.9175 * sb;
  
  /* R.A. and Dec of Moon, geocentric - as seen by an imaginary observer at the center of the Earth */

// Right Ascension (RA)
  am = atan2(m, l);
// Declination (Dec)
  dm = asin(n);
  
  /* topocentric rectangular coordinates */
  
  cd = cos(dm); // cos(Declination)
  sd = n; // sin(Declination)
  ca = cos(am); // cos(RA)
  sa = sin(am); // sin(RA)
  sl = sin(lst); // sin(local sideral time)
  cl = cos(lst); // cos(local sideral time)
  
  x = rm * cd *ca - cp * cl;
  y = rm * cd * sa - cp * sl;
  z = rm * sd - sp;
  
  /* finally, topocentric Hour-Angle and Dec */
  
  // RA=atan2(y, x)
  am = lst - atan2(y, x); // Moon's Local Hour Angle (lhr = HA = SIDTIME - RA)
  ca = cos(am); // cos(lhr)
  sa = sin(am); // sin(lhr)
// Right Ascension
  am = atan2(sa,ca); // (spherical coordinates) lhr, normalized -pi to pi
  rm = sqrt(x*x + y*y + z*z); // r moon (spherical coordinates)
// Declination
  dm = asin(z/rm); // Decl moon (spherical coordinates)
  px = asin(1.0 / rm); // Moon's parallax, i.e. the apparent size of the (equatorial) radius of the Earth, as seen from the Moon
  SD = 0.2725 * px;
//printf("SD=%f\n",SD);

/* amacri 2006-5-17: convert to Azimuth and Altitude */
  altM = asin(sp*sd + cp*cd*ca); // altitude of the moon in radians
  calt =  cos(altM);
  AzM = atan2(-cd * sa / calt, (sd*cp - cd*ca*sp) / calt); // Azimuth in radians
  AzM = replace_fmod(AzM / Asec_Radian, 1296000. /* 360.*3600. */); // in seconds
  altM = altM / Asec_Radian; // in seconds

}

timedata(tim)
long	tim;
{
  
  /* compute seconds from 2000 Jan 1.5 UT (Ephemeris Epoch) */
  /* the VAX Epoch is     1970 Jan 1.0 UT (Midnight on Jan 1) */
  
  Julian_Day = (tim/Sec_per_day) +
    (double)(tim % Sec_per_day)/Sec_per_day + J1970;
  MJD = Julian_Day -J2000;	/* Julian Days past Epoch */
  Tu  = MJD/36525.;		/* Julian Centuries past Epoch */
  
  /* compute Sidereal time */
  
  Ru = 24110.54841 + Tu * (8640184.812866
			  + Tu * (0.09304 - Tu * 6.2e-6));	/* seconds */
  GMST = (tim % Sec_per_day) + Sec_per_day + replace_fmod(Ru, (double)Sec_per_day);
  LST  = GMST + Local;
//  printf("Ru=%d, Tu=%d, GMST=%d, LST=%d\n",Ru,Tu,GMST,LST);
}

/* time functions, for Regulus */
char *gmctime(t)
long *t;
{
  long t1;
  
  t1 = *t + localdst;		/* convert to local time */
//    printf("t=%d, t1=%d, %s, %d\n", *t, t1, ctime(t), localdst);
//    printf("t=%d, t1=%d, %s, %d\n", *t, t1, ctime(&t1), localdst);

  return(asctime(gmtime(&t1)));
}

stuff(tim)
long tim;
{		/* main computation loop */
  
  timedata(tim);
  
  /* where is the Sun (angles are in seconds of arc) */
  /*	Low precision elements from 1985 Almanac   */
  
  L   = 280.460 + 0.9856474 * MJD;	/* Mean Longitde             */
  L   = replace_fmod(L, 360.);			/* corrected for aberration  */
  
  g   = 357.528 + 0.9856003 * MJD;	/* Mean Anomaly              */
  g   = replace_fmod(g, 360.);
  
  eps = 23.439 - 0.0000004 * MJD;	/* Mean Obiquity of Ecliptic */
  
{					/* convert to R.A. and DEC   */
  double Lr, gr, epsr, lr, ca, sa, R;
  double sA, cA, gphi;
  
  Lr = L * Degree_to_Radian;
  gr = g * Degree_to_Radian;
  epsr = eps * Degree_to_Radian;
  
  lr = (L + 1.915*sin(gr) + 0.020*sin(2.0*gr)) * Degree_to_Radian;
  
  sd = sin(lr) * sin(epsr); // sin(Declination)
  cd = sqrt(1.0 - sd*sd); // cos(Declination)
  sa = sin(lr) * cos(epsr);  //sin(RA)
  ca = cos(lr); //cos(RA)
  
  delta = asin(sd); // Declination in radians
  alpha = atan2(sa, ca); // RA in radians
  
  /* equation of time */
//  Eqt= (Lr - alpha) / Tsec_to_Radian; // amacri: notice that this is not used
  
  delta = delta / Asec_Radian; // Declination in seconds
  alpha = alpha / Tsec_to_Radian; // RA in seconds
  
  lhr = (LST - alpha) * Tsec_to_Radian;
  sh =  sin(lhr);
  ch =  cos(lhr);
  lhr= atan2(sh, ch);	/* normalized -pi to pi */
  lha= lhr / Tsec_to_Radian + Sec_per_day/2;
  
  /* convert to Azimuth and altitude */
  
  alt = asin(sd*sp + cd*ch*cp);
  ca =  cos(alt);
  sA =  -cd * sh / ca;
  cA =  (sd*cp - cd*ch*sp) / ca;
  Az = atan2(sA, cA) / Asec_Radian;
  Az = replace_fmod(Az, 1296000. /* 360.*3600. */);
  alt = alt / Asec_Radian;
}
}

/* double precision modulus, put in range 0 <= result < m */
double replace_fmod(x, m)
double	x, m;
{
  long i;
  
  i = fabs(x)/m;		/* compute integer part of x/m */
  if (x < 0)	return( x + (i+1)*m);
  else		return( x - i*m);
}

