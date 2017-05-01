#ifndef FB_H
#define FB_H

#include <stdlib.h>

extern unsigned short* fb;
extern int fb_rotate;

static inline int fb_colour(int r, int g, int b)
{
	return ((r >> 3) << 11) | ((g >> 2) << 5) | ((b >> 3) << 0);
}

static inline void fb_setpix(int x, int y, int col)
{
	if(fb == NULL)
		return;
	if((x < 0) || (x > 319))
		return;
	if((y < 0) || (y > 239))
		return;

	if(fb_rotate)
		fb[x * 240 + (239 - y)] = col;
		else fb[x + 240 * y] = col;
}

/* amacri 18-5-06: support of different screen and font sizes */
#define FB_FONTX_6x11 6  // X font size
#define FB_FONTY_6x11 11 // Y font size

// fontdata_7x14
#define FB_FONTX_7x14 7  // X font size
#define FB_FONTY_7x14 14 // Y font size

// fontdata_8x16
#define FB_FONTX_8x16 8  // X font size
#define FB_FONTY_8x16 16 // Y font size

// fontdata_10x18
#define FB_FONTX_10x18 10  // X font size
#define FB_FONTY_10x18 18 // Y font size

// fontdata_12x22
#define FB_FONTX_12x22 12  // X font size
#define FB_FONTY_12x22 22 // Y font size

#endif /* FB_H */
