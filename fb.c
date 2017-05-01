/*
This special version of fb.c is part of Event_Logger,
author Amacri - amacri@tiscali.it

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

#ifndef NOZSH
#include "zsh.h"
#endif
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/mount.h>
#include <string.h>
#include "fb.h"
#include "font_6x11.c"
#include "font_7x14.c"
#include "font_8x16.c"
#include "font_10x18.c"
#include "font_12x22.c"
#include <linux/input.h>

#define BUF_SIZE 4095

#ifdef NOZSH
// function prototypes
void fb_rect (int x1, int y1, int x2, int y2, int col);
void fb_internal_putchar_6x11 (int x, int y, int col, char c);  // FB_FONT=1
void fb_internal_putchar_7x14 (int x, int y, int col, char c);  // FB_FONT=2
void fb_internal_putchar_8x16 (int x, int y, int col, char c);  // FB_FONT=3
void fb_internal_putchar_10x18 (int x, int y, int col, char c); // FB_FONT=4
void fb_internal_putchar_12x22 (int x, int y, int col, char c); // FB_FONT=5
void fb_printxy (int x, int y, int col, char* str);
void fb_init (void);
void fb_done (void);
int fbprintf (const char *format, ...);
int TsScreen_pen (int * x, int * y, int * pen);
int TsScreen_Init (void);
int flushTS (void);
int TsScreen_Exit (void);
int ts_press (double secs, int * ts_x, int * ts_y, int * ts_pen, int timeout);
int readTS (void);
#endif
void fbcenter(char * c);

int fbfd = -1;
long int screensize = 0;
long int scrsize = 0;
#ifdef ARM
struct fb_var_screeninfo vinfo;
struct fb_fix_screeninfo finfo;
#endif

char SystemTFTType[32]="Unknown";
char SystemShortName[32]="Unknown";
char SystemModelName[32]="Unknown";

#ifdef ARM
#include <linux/fb.h>
#include "Barc_ts.h"
#endif
#include "font_6x11.h" // amacri: this is IBM PC, not ISO-8859-1

int tsfd = -1;
int fb_x=-1 /* X pos */, fb_y=-1 /* Y pos */, fb_col=-1 /* Ink colour */, fb_NumInk=-1 /* Ink colour when numeric */, fb_BgCol=-1 /* Background colour*/;
int fb_font=1; // primary font 6x11
int fb_center=0; /* set to 1 to center a line */
int fb_DoNewLine=1 /* auto newline allowed */, ts_x=0, ts_y=0, ts_pen=0; // Touchscreen
int fb_NewLineDoneX=-1, fb_NewLineDoneY=-1; // -1=no newline done; otherwise x/y position of the just done newline
int fb_Bold=0 /* Bold Attribute */;
int fb_xs=0 /* X Shift */ , fb_ys=0 /* Y Shift */; // Additional shifts (positive or negative digits for compressed or expanded separations between chars

int fb_FontX=FB_FONTX_6x11;  // Default to 6x11 X font size
int fb_FontY=FB_FONTY_6x11; // Default to 6x11 Y font size

int fb_MaxX=-1; // X Screen size; e.g., 320
int fb_MaxY=-1; // Y Screen size; e.g., 240

int fb_xr=0; // X Screen region shift (0=null behaviour)
int fb_ylr=0; // MIN Y for Screen region (0=undefined)
int fb_yhr=0; // MAX Y for Screen region (0=undefined)

unsigned short* fb = NULL;

int fb_rotate;

static int pipe_exists=0;
const char *filename_devshare = "/tmp/el_np";

/* amacri: data for IBM PC to ISO Latin-1 code conversions. */
static unsigned char iso_to_ibmpc[] = { 0,
   1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31,
 32, 33, 34, 35, 36, 37, 38, 39, 40, 41, 42, 43, 44, 45, 46, 47, 48, 49, 50, 51, 52, 53, 54, 55, 56, 57, 58, 59, 60,
 61, 62, 63, 64, 65, 66, 67, 68, 69, 70, 71, 72, 73, 74, 75, 76, 77, 78, 79, 80, 81, 82, 83, 84, 85, 86, 87, 88, 89,
 90, 91, 92, 93, 94, 95, 96, 97, 98, 99, 100, 101, 102, 103, 104, 105, 106, 107, 108, 109, 110, 111, 112, 113, 114,
  115, 116, 117, 118, 119, 120, 121, 122, 123, 124, 125, 126, 127, 128,
  129, 130, 131, 132, 133, 134, 135, 136, 137, 138, 139, 140, 141, 142, 143, 144, 145, 146, 147, 148, 149, 150, 151,
  152, 153, 154, 155, 156, 157, 158, 159, 255, 173, 155, 156, 164, 157, 166, 21, 168, 169, 166, 174, 170, 173, 174,
  175, 248, 241, 253, 179, 180, 230, 20, 250, 184, 185, 167, 175, 172, 171, 190, 168, 192, 193, 194, 195, 142, 143,
  146, 128, 200, 144, 202, 203, 204, 205, 206, 207, 208, 165, 210, 211, 212, 213, 153, 215, 216, 217, 218, 219, 154,
  221, 222, 225, 133, 160, 131, 227, 132, 134, 145, 135, 138, 130, 136, 137, 141, 161, 140, 139, 240, 164, 149, 162,
  147, 245, 148, 246, 248, 151, 163, 150, 129, 253, 254, 152 };

static inline void fb_internal_setpix(int x, int y, int col)
{
	register int disp=fb_rotate ? x * fb_MaxY + (fb_MaxY-1 - y) : x + fb_MaxX * y ;
	fb[disp] = col;
	if (pipe_exists)
		fb[disp+scrsize] = col;
}

/**/
void
fb_rect(int x1, int y1, int x2, int y2, int col)
{
	int x;
	int y;

	if(fb == NULL)
		return;

	if(x1 < 0)
		x1 = 0;
	if(x1 > (fb_MaxX-1) )
		x1 = fb_MaxX-1;
	if(x2 < 0)
		x2 = 0;
	if(x2 > fb_MaxX-1)
		x2 = fb_MaxX-1;
	if(y1 < 0)
		y1 = 0;
	if(y1 > (fb_MaxY-1) )
		y1 = fb_MaxY-1;
	if(y2 < 0)
		y2 = 0;
	if(y2 > (fb_MaxY-1) )
		y2 = fb_MaxY-1;
	if(x1 > x2) {
		x = x1;
		x1 = x2;
		x2 = x;
	}
	if(y1 > y2) {
		y = y1;
		y1 = y2;
		y2 = y;
	}

	for(x = x1; x <= x2; x++) {
		for(y = y1; y <= y2; y++)
			fb_internal_setpix(x, y, col);
	}
}

/**/
void
fb_internal_putchar_6x11(int x, int y, int col, char c)
{
	int ix;
	int iy;
	unsigned char f;

	for(iy = 0; iy < FB_FONTY_6x11; iy++) {
		f = fontdata_6x11[c * FB_FONTY_6x11 + iy];
		for(ix = 0; ix < FB_FONTX_6x11; ix++) {
			if(f & 0x80)
				fb_internal_setpix(x + ix, y + iy, col);
			f <<= 1;
		}
	}
}

/**/
void
fb_internal_putchar_7x14(int x, int y, int col, char c)
{
	int ix;
	int iy;
	unsigned char f;

	for(iy = 0; iy < FB_FONTY_7x14; iy++) {
		f = fontdata_7x14[c * FB_FONTY_7x14 + iy];
		for(ix = 0; ix < FB_FONTX_7x14; ix++) {
			if(f & 0x80)
				fb_internal_setpix(x + ix, y + iy, col);
			f <<= 1;
		}
	}
}

/**/
void
fb_internal_putchar_8x16(int x, int y, int col, char c)
{
	int ix;
	int iy;
	unsigned char f;

	for(iy = 0; iy < FB_FONTY_8x16; iy++) {
		f = fontdata_8x16[c * FB_FONTY_8x16 + iy];
		for(ix = 0; ix < FB_FONTX_8x16; ix++) {
			if(f & 0x80)
				fb_internal_setpix(x + ix, y + iy, col);
			f <<= 1;
		}
	}
}

/**/
void
fb_internal_putchar_10x18(int x, int y, int col, char c)
{
	int ix;
	int iy;
	int f;
	int i;

	for(iy = 0; iy < FB_FONTY_10x18; iy++) {
		i=c * FB_FONTY_10x18 * 2 + iy * 2;
		f = (fontdata_10x18[i] << 8) + fontdata_10x18[i+1];
		f >>=(16-FB_FONTX_10x18);
		for(ix = 0; ix < FB_FONTX_10x18; ix++) {
			if ( f & (1 << (FB_FONTX_10x18-1)) )
				fb_internal_setpix(x + ix, y + iy, col);
			f <<= 1;
		}
	}
}

void (*fb_internal_putchar)(int x, int y, int col, char c)=fb_internal_putchar_6x11;

/**/
void
fb_setfont(int x)
{
    fb_font=x;
    switch (fb_font) {
    	case 2:
    					fb_FontX=FB_FONTX_7x14;
    					fb_FontY=FB_FONTY_7x14;
    					fb_internal_putchar=fb_internal_putchar_7x14;
    					break;
    	case 3:
    					fb_FontX=FB_FONTX_8x16;
    					fb_FontY=FB_FONTY_8x16;
    					fb_internal_putchar=fb_internal_putchar_8x16;
    					break;
    	case 4:
    					fb_FontX=FB_FONTX_10x18;
    					fb_FontY=FB_FONTY_10x18;
    					fb_internal_putchar=fb_internal_putchar_10x18;
    					break;
    	case 5:
    					fb_FontX=FB_FONTX_12x22;
    					fb_FontY=FB_FONTY_12x22;
    					fb_internal_putchar=fb_internal_putchar_12x22;
    					break;
    	default: fb_font=1;
    					fb_FontX=FB_FONTX_6x11;
    					fb_FontY=FB_FONTY_6x11;
    					fb_internal_putchar=fb_internal_putchar_6x11;
    					break;
    }
}

/**/
void
fb_internal_putchar_12x22(int x, int y, int col, char c)
{
	int ix;
	int iy;
	int f;
	int i;

	for(iy = 0; iy < FB_FONTY_12x22; iy++) {
		i=c * FB_FONTY_12x22 * 2 + iy * 2;
		f = (fontdata_12x22[i] << 8) + fontdata_12x22[i+1];
		f >>=(16-FB_FONTX_12x22);
		for(ix = 0; ix < FB_FONTX_12x22; ix++) {
			if ( f & (1 << (FB_FONTX_12x22-1)) )
				fb_internal_setpix(x + ix, y + iy, col);
			f <<= 1;
		}
	}
}

/**/
void
fb_printxy(int x, int y, int col, char* str)
{
	int ix;
	char *c;

	if(fb == NULL)
		return;

	c = str;
	ix = x;
	while(*c != '\0') {
		fb_internal_putchar(ix, y, col, iso_to_ibmpc[(int)*c++]);
		ix += fb_FontX;
	}
}

/**/
void
fb_init(void)
{
	int fd;
	int x;

	fb = NULL;

	if((fbfd = open("/dev/fb", O_RDWR)) < 0)
		return;


#ifdef ARM
  // Get fixed screen information
  if (ioctl(fbfd, FBIOGET_FSCREENINFO, &finfo)) 
  {
//    printf("Error reading fixed information.\n");
    return;
  }

  // Get variable screen information
  if (ioctl(fbfd, FBIOGET_VSCREENINFO, &vinfo)) 
  {
//    printf("Error reading variable information.\n");
    return;
  }

//  printf("%dx%d, %dbpp\n", vinfo.xres, vinfo.yres, vinfo.bits_per_pixel );

  fb_MaxX=vinfo.xres;
  fb_MaxY=vinfo.yres;
  // Figure out the size of the screen in bytes
  scrsize = vinfo.xres * vinfo.yres;
  screensize = scrsize * vinfo.bits_per_pixel / 8;
#endif

pipe_exists = access (filename_devshare, F_OK) == 0;

  // Map the device to memory
  fb = mmap(NULL, screensize * (pipe_exists + 1), PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);
//	fb = mmap(NULL, 320 * 240 * 2, PROT_READ | PROT_WRITE, MAP_SHARED, fbfd, 0);

	if(fb == (void*)-1)
	{
			fb = NULL;
		close(fbfd);
	}
	fb_rotate = 0;
	fd = -1;

	if((fd = open("/proc/barcelona/modelname", O_RDONLY)) >= 0)
	  {
	  x= read(fd, SystemModelName, sizeof(SystemModelName) - 1);
		if(x > 0) SystemModelName[x] = '\0';
		if((x > 0) && (SystemModelName[x - 1] == '\n'))
			SystemModelName[x - 1] = '\0';
	  close (fd);
	  }
	if((fd = open("/proc/barcelona/shortname", O_RDONLY)) >= 0)
	  {
	  x = read(fd, SystemShortName, sizeof(SystemShortName) - 1);
		if(x > 0) SystemShortName[x] = '\0';
		if((x > 0) && (SystemShortName[x - 1] == '\n'))
			SystemShortName[x - 1] = '\0';
	  close (fd);
	  }
	if((fd = open("/proc/barcelona/usbname", O_RDONLY)) >= 0)
	  {
	  x = read(fd, SystemShortName, sizeof(SystemShortName) - 1);
		if(x > 0) SystemShortName[x] = '\0';
		if((x > 0) && (SystemShortName[x - 1] == '\n'))
			SystemShortName[x - 1] = '\0';
	  close (fd);
	  }
	if((fd = open("/proc/barcelona/tfttype", O_RDONLY)) < 0) /* amacri 11-11-06: changed from shortname to tfttype */
		goto failed;
	if((x = read(fd, SystemTFTType, sizeof(SystemTFTType) - 1)) < 0)
		goto failed;
	close(fd);
	if(x > 0) SystemTFTType[x] = '\0';
	if((x > 0) && (SystemTFTType[x - 1] == '\n'))
		SystemTFTType[x - 1] = '\0';

	if(strcmp(SystemTFTType, "2") == 0) { // TomTom GO, TomTom GO 300, TomTom GO 500, TomTom GO 700
		fb_rotate = 1;
#ifdef ARM
	  fb_MaxX=vinfo.yres;
	  fb_MaxY=vinfo.xres;
#endif
	} else if(strcmp(SystemTFTType, "3") == 0) { // TomTom ONE, TomTom RIDER
		fb_rotate = 0;
	}
#ifdef ARM
	 else if(vinfo.yres>vinfo.xres) { /* amacri 18-5-06: possible support of other screens */
		fb_rotate = 1;
	  fb_MaxX=vinfo.yres;
	  fb_MaxY=vinfo.xres;
	}
#endif
	return;

failed:
	if(fd >= 0)
		close(fd);
}

/**/
void
fb_done(void)
{
//	if(fb != NULL)
//		munmap(fb, 320 * 240 * 2);
	if( (fb != NULL) && (screensize > 0) )
		munmap(fb, screensize);
	if (fbfd >= 0)
	  close(fbfd);
}

/* ====================================================================================== */
/* = amacri: fbprintf, TsScreen_Init, TsScreen_Exit, TsScreen_pen */
/* ====================================================================================== */

#include <stdio.h>
#include <stdarg.h>

void fbcenter(char * c) {
int chars=0;
while ((*c != '\0') && (*c++ != '\n'))
	chars++;
if (!chars)
	return;
chars = (fb_MaxX-chars*(fb_FontX+fb_xs))/2;
if (chars > fb_x)
	fb_x=chars;
}

/**/
int
fbprintf(const char *format, ...)
{
  va_list ap;
	char FbBuffer[BUF_SIZE];
	char * c = FbBuffer;
	int len;
	int first=0;

va_start(ap, format);
	if ((len = vsprintf(c, format, ap)) >= BUF_SIZE)
        return 0;
va_end(ap);

	if(fb == NULL)
			fb_init();

	if(fb == NULL)
	{
          char FOut[]="/dev/tty";
          FILE *out;

                out = fopen(FOut, "w");
                if ( out == NULL )
                   {    fprintf(stderr, "\nCannot open %s\n", FOut);
		        printf(format, c);
                   }
                else {
		fprintf(out, format, c);
                fclose(out);
                }
		return 0;
	}

	if (fb_col < 0) //reset colour when  < 0
	  fb_col=0;

  if ( (fb_x < 0) || (fb_y < 0) ) //reset x coord when < 0
    fb_x = fb_y = 0;

  if (fb_x+fb_xr+fb_xs > fb_MaxX-fb_FontX-1) // do a newline if needed
  {
  	fb_x=0;
	  if (fb_DoNewLine)
	     fb_y += (fb_FontY+fb_ys);
  	fb_NewLineDoneX=fb_x;
  	fb_NewLineDoneY=fb_y;
  }
  if ( (fb_ylr>0) && (fb_y+fb_ys < fb_ylr) ) // do nothing if above region
     return 2;
  if ( (fb_yhr>0) && (fb_y+fb_ys > fb_yhr-fb_FontY-1) ) // do nothing if below region
     return 2;
  if (fb_y+fb_ys > fb_MaxY-fb_FontY-1) // reset to home instead of scrolling
     fb_y=0;

		if (first>0) { // if not going to process the first string of printf, add a separator space first
  		if (fb_BgCol>0) // do background colour if needed
  		   fb_rect(fb_x+fb_xr, fb_y, fb_x+fb_xr+fb_FontX+fb_xs, fb_y+fb_FontY+fb_ys, fb_BgCol);
  		if (fb_BgCol<=0) // clear the character space if no background colour is set
			   fb_internal_putchar(fb_x+fb_xr, fb_y, fb_col, ' ');
			fb_x += (fb_FontX+fb_xs); // go to next char position (this is the real separator for multiple printf strings)
		  if (fb_x+fb_xr+fb_xs > fb_MaxX-fb_FontX-1) // do a newline if needed
		  {
		  	fb_x=0;
			  if (fb_DoNewLine)
					  fb_y += (fb_FontY+fb_ys);
		  	fb_NewLineDoneX=fb_x;
		  	fb_NewLineDoneY=fb_y;
		  }
		  if ( (fb_ylr>0) && (fb_y+fb_ys < fb_ylr) ) // do nothing if above region
		     return 2;
		  if ( (fb_yhr>0) && (fb_y+fb_ys > fb_yhr-fb_FontY-1) ) // do nothing if below region
		     return 2;
		  if (fb_y+fb_ys > fb_MaxY-fb_FontY-1) // reset to home instead of scrolling
		     fb_y=0;
    }

	if (fb_center)
		fbcenter(c);

		while(*c != '\0') {
			if (*c == '\n') {  // process a newline character
						 if((fb_NewLineDoneX!=fb_x) || (fb_NewLineDoneY!=fb_y)) {
				   	 fb_x=0;
				     fb_y += (fb_FontY+fb_ys);
						 if (fb_center)
							 fbcenter(c+1);
						}
					fb_NewLineDoneX=-1;
					fb_NewLineDoneY=-1;
			    }
			else {
					fb_NewLineDoneX=-1;
					fb_NewLineDoneY=-1;
		  		if (fb_BgCol>0) // print background colour when available
		  		   fb_rect(fb_x+fb_xr, fb_y, fb_x+fb_xr+fb_FontX+fb_xs+(fb_Bold?1:0), fb_y+fb_FontY+fb_ys, fb_BgCol);
					fb_internal_putchar(fb_x+fb_xr, fb_y, (isdigit(*c)?(fb_NumInk>0?fb_NumInk:fb_col):fb_col), iso_to_ibmpc[(int)*c]); // print char
					if (fb_Bold && fb_x+fb_FontX+1<fb_MaxX) // do bold if needed
					   fb_internal_putchar((++fb_x)+fb_xr, fb_y, (isdigit(*c)?(fb_NumInk>0?fb_NumInk:fb_col):fb_col), iso_to_ibmpc[(int)*c]);
					first++; // char counter
					fb_x += (fb_FontX+fb_xs); // go to next char position
	    }
		  if (fb_x+fb_xr+fb_xs > fb_MaxX-fb_FontX-1) // do a newline if needed
		  {
		  	fb_x=0;
			  if (fb_DoNewLine)
					  fb_y += (fb_FontY+fb_ys);
		  	fb_NewLineDoneX=fb_x;
		  	fb_NewLineDoneY=fb_y;
		  }
		  if ( (fb_ylr>0) && (fb_y+fb_ys < fb_ylr) ) // do nothing if above region
		     return 2;
		  if ( (fb_yhr>0) && (fb_y+fb_ys > fb_yhr-fb_FontY-1) ) // do nothing if below region
		     return 2;
		  if (fb_y+fb_ys > fb_MaxY-fb_FontY-1) // reset to home instead of scrolling
		     fb_y=0;
			c++; // process next printf char
		}
	return 1;
}

#ifdef ARM
struct input_event in_ev;
static TS_EVENT prev_event;
static int have_previous = 0;
static MATRIX ts_matrix;
#endif
static int eventMode = 0; // 1=new or 0=old touchscreen method

// return 0=no event
// return 1=event has been read
/**/
int
TsScreen_pen(int * x, int * y, int * pen)
{
  if (tsfd < 0)
  {
		if ((TsScreen_Init()) == -1)
		   return 0;
  }

  if (tsfd<0)
    return 0;

#ifdef ARM
  TS_EVENT new_event;

	if (eventMode) {
		int retval = readTS ();
		if (retval == 0) {
			flushTS();
			return(retval);
		}
	  *x = prev_event.x;
	  *y = prev_event.y;
	  *pen = prev_event.pressure;
		flushTS();
		return(retval);
  }
 
  if (!have_previous)
    if (read(tsfd, &prev_event, sizeof(TS_EVENT)) == sizeof(TS_EVENT))
      have_previous = 1;
  
  // if we still don't have an event, there are no events pending, and we can just return
  if (!have_previous)
    return 0;
  
  // We have an event
  memcpy(&new_event, &prev_event,sizeof(TS_EVENT));
  have_previous = 0;

  if (read(tsfd, &prev_event, sizeof(TS_EVENT)) == sizeof(TS_EVENT))
     have_previous = 1;

  while (have_previous && (prev_event.pressure != 0) == (new_event.pressure != 0))
  {
    memcpy(&new_event, &prev_event,sizeof(TS_EVENT));
    have_previous = 0;
	  if (read(tsfd, &prev_event, sizeof(TS_EVENT)) == sizeof(TS_EVENT))
	     have_previous = 1;
  }
  if(fb_rotate) {
	  *x = new_event.y;
	  *y = new_event.x;
	} else {
	  *x = new_event.x;
	  *y = new_event.y;
  }
  *pen = new_event.pressure;
#endif
  return 1;
}


/**/
int
TsScreen_Init(void)
{
	TsScreen_Exit();
	if (tsfd == -1) { // Old mode?
	        tsfd = open("/dev/ts", O_RDONLY | O_NOCTTY | O_NONBLOCK);
	        if (tsfd == -1) { // Seems not, try the new mode
	                tsfd = open("/dev/input/event0", O_RDONLY | O_NOCTTY | O_NONBLOCK);
	                if (tsfd != -1) { // Ok, that worked.
	                        eventMode = 1; // Remember that!
	                        FILE * cal_file = fopen ("/mnt/flash/sysfile/cal","rb");
#ifdef ARM
	                        if ((cal_file == NULL) || (fread (&ts_matrix.xMin, sizeof(int), 4, cal_file) != 4)) {
															    return (-1); // comment out to show the error splash screen
															    // debug
																	fb_rect(0, 0, fb_MaxX-1, fb_MaxY-1, fb_colour(0xff, 0xff, 0x00));
																	fb_x=0;
																	fb_y=0;
																	fb_col=fb_colour(0x00, 0x00, 0x00);
															    fbprintf("\ntouchscreen: Error reading calibration file /mnt/flash/sysfile/cal.\n");
															    sleep(5);
															    tsfd = -1;
	                                close(tsfd);
															    return (-1);
	                        }
#endif
	                        fclose (cal_file);
	                } else {
								    return (-1); // comment out to show the error splash screen
								    // debug
										fb_rect(0, 0, fb_MaxX-1, fb_MaxY-1, fb_colour(0xff, 0xff, 0x00));
										fb_x=0;
										fb_y=0;
										fb_col=fb_colour(0x00, 0x00, 0x00);
								    fbprintf("\ntouchscreen: Error opening /dev/ts and /dev/input/event0.\n");
								    sleep(5);
								    tsfd = -1;
								    return (-1);
	                }
	        } else {
#ifdef ARM
								  ioctl(tsfd,TS_SET_RAW_OFF,NULL);
								  return(0);
#else
								  return (-1);
#endif
								 }
	}
	return(tsfd<0?-1:0);
}


/**/
int
flushTS(void)
{
        int i, count=0;

				for (i=0;i<2;i++) {     // tune here the debounce timing
				    		usleep(70000); // tune here the debounce timing
				        while (readTS ())
				                count++;
				}
        return count;
}


/**/
int
TsScreen_Exit(void)
{
	int ret;

    if (tsfd!=-1) {
      flushTS();
      ret=close(tsfd);
	  	if (ret>=0)
	  	   {
	  	   	tsfd=-1;
	        return(ret);
	       }
    }
return(-1);
}

#include <time.h>
#include <sys/time.h>


/**/
int
ts_press(double secs, int * ts_x, int * ts_y, int * ts_pen, int timeout)
{
		struct timeval tv;
		fd_set readfds;

    if (secs - (int) secs == 0)
       tv.tv_usec = 0;
      else tv.tv_usec = (secs- (int) secs)*1000000;

    tv.tv_sec = (int) secs;

		if (tsfd<0)
		    if ((TsScreen_Init()) == -1)
		       return 2; // cannot open touchscreen (failed TsScreen_Init)
		if (tsfd<0)
		    return 1; // cannot open touchscreen

//		   ts_x=0; ts_y=0; ts_pen=0;
//		   while (!TsScreen_pen(ts_x,ts_y,ts_pen));

		FD_ZERO (&readfds);
    FD_SET(tsfd, &readfds);
		select (tsfd+1, &readfds, NULL, NULL, &tv);
#ifdef ARM
		if (FD_ISSET (tsfd, &readfds)) {
			    struct timeval t, ot;
			    struct timezone timez;
			    gettimeofday(&t, &timez);
			    ot.tv_sec=t.tv_sec+(timeout>0?timeout:100);
					while (ot.tv_sec>t.tv_sec) {
					  // read pen coords
				      *ts_x=0, *ts_y=0, *ts_pen=0;
					    while (TsScreen_pen(ts_x,ts_y,ts_pen)); // flush pen input
						  if (! timeout)
							  return 0;
							tv.tv_sec = 0;
							tv.tv_usec = 200000;
							FD_ZERO (&readfds);
						    FD_SET(tsfd, &readfds);
					      select (tsfd+1, &readfds, NULL, NULL, &tv);
		            if (!FD_ISSET (tsfd, &readfds)) {
		            	// flush
						       int ts_x=0, ts_y=0, ts_pen=0;
						       while (TsScreen_pen(&ts_x,&ts_y,&ts_pen)); // read pen coords
					         return(0); // a key was pressed
							  }
			          gettimeofday(&t, &timez);
					 }
						  if (! timeout)
							  return 0;
					 return(4); // press timeout occurred
			}
#endif
		// flush pen input
    *ts_x=0; *ts_y=0; *ts_pen=0;
	  while (TsScreen_pen(ts_x,ts_y,ts_pen));
		return 3; // no key pressed
}


// return 0=no event
// return 1=event has been read
/**/
int
readTS(void)
{
#ifdef ARM
	int retval = read(tsfd, &in_ev, sizeof(struct input_event)) == sizeof(struct input_event) ? 1 : 0;
	while (retval>0) {
	  if (in_ev.type==EV_ABS && in_ev.code==ABS_X) {
	          prev_event.x = in_ev.value;
	          prev_event.x = (((prev_event.x - ts_matrix.xMin) * (vinfo.xres-30)) / (ts_matrix.xMax - ts_matrix.xMin)) +15;
	          if (prev_event.x < 0)
	                  prev_event.x = 0;
	          else if ((unsigned)prev_event.x > vinfo.xres)
	                  prev_event.x = vinfo.xres;
	          prev_event.x = vinfo.xres - prev_event.x;
	          if (prev_event.y!=-1) // -> full dataset.
	                  return 1;
	  } else if (in_ev.type==EV_ABS && in_ev.code==ABS_Y) {
	          prev_event.y = in_ev.value;
	          prev_event.y = (((prev_event.y - ts_matrix.yMin) * (vinfo.yres-30)) / (ts_matrix.yMax - ts_matrix.yMin)) +15;
	          if (prev_event.y < 0)
	                  prev_event.y = 0;
	          else if ((unsigned)prev_event.y > vinfo.yres)
	                  prev_event.y = vinfo.yres;
	          prev_event.y = vinfo.yres - prev_event.y;
	          if (prev_event.x!=-1) // -> full dataset.
	                  return 1;
	  } else if (in_ev.type==EV_KEY && in_ev.code==BTN_TOUCH) {
	          if (in_ev.value) {
	             prev_event.pressure = 255;
	             prev_event.x = -1; // Reset...
	             prev_event.y = -1;
	          } else {
	             prev_event.pressure = 0;
	             return 1; // button-release -> another full dataset.
	          }
	         }
	  retval = read(tsfd, &in_ev, sizeof(struct input_event)) == sizeof(struct input_event) ? 1 : 0;
	}
	return retval;
#else
	return (0);
#endif
}
