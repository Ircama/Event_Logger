/*
*
* Driver for the H3600 Touch Screen and other Atmel controlled devices.
*
* Copyright 2000 Compaq Computer Corporation.
*
* Use consistent with the GNU GPL is permitted,
* provided that this copyright notice is
* preserved in its entirety in all copies and derived works.
*
* COMPAQ COMPUTER CORPORATION MAKES NO WARRANTIES, EXPRESSED OR IMPLIED,
* AS TO THE USEFULNESS OR CORRECTNESS OF THIS CODE OR ITS
* FITNESS FOR ANY PARTICULAR PURPOSE.
*
* Author: Charles Flynn.
*
* Author: SW.LEE <hitchcar@sec.samsung.com> Modified for S3C2410 

 * Copyright (C) 2004,2005 TomTom BV <http://www.tomtom.com/>
 * Author: Dimitry Andric <dimitry.andric@tomtom.com>

*/


#ifndef __S3C2410_TS_H__
#define __S3C2410_TS_H__

#include <linux/ioctl.h>

#ifdef __cplusplus 
extern "C" { 
#endif /* __cplusplus */ 

typedef struct s3c2410_ts_calibration {
        int xscale;
        int xtrans;
        int yscale;
        int ytrans;
        int xyswap;
} TS_CAL;

typedef struct s3c2410_ts_event {
				short pressure;
				short x;
				short y;
				short pad;
} TS_EVENT;


/* Deprecated - do not use */
typedef struct s3c2410_ts_return {
        unsigned short pressure;
        unsigned short x;
        unsigned short y;
        unsigned short pad;
} TS_RET;


/* IOCTL cmds  user or kernel space */

/* Use 'f' as magic number */
#define IOC_S3C2410_TS_MAGIC  'f'


#define TS_GET_CAL	_IOR(IOC_S3C2410_TS_MAGIC, 10, struct s3c2410_ts_calibration)
#define TS_SET_CAL	_IOW(IOC_S3C2410_TS_MAGIC, 11, struct s3c2410_ts_calibration)
#define  TS_SET_RAW_ON  _IOW(IOC_S3C2410_TS_MAGIC,14,0)
#define  TS_SET_RAW_OFF  _IOW(IOC_S3C2410_TS_MAGIC,15,0)

/* From the original calibrate.h: */

typedef struct Matrix
{
	/* This arrangement of values facilitates 
	 *  calculations within getDisplayPoint() */
	long An;		/* A = An/Divider */
	long Bn;		/* B = Bn/Divider */
	long Cn;		/* C = Cn/Divider */
	long Dn;		/* D = Dn/Divider */
	long En;		/* E = En/Divider */
	long Fn;		/* F = Fn/Divider */
	long Divider;
	int xMin;		// Stored on FLASH address 0x10c
	int xMax;		// Stored on FLASH address 0x110
	int yMin;		// Stored on FLASH address 0x104
	int yMax;		// Stored on FLASH address 0x108
} MATRIX;

#ifdef __cplusplus
} 
#endif /* __cplusplus */ 

#endif /* __S3C2410_TS_H__ */
