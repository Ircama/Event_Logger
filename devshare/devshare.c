/*
devshare is part of Event_Logger, author Amacri - amacri@tiscali.it

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

Special thanks to:
	TTMPlayer Team <http://ttmplayer.homeunix.de>
	Bello <http://www.opentom.org/DataBackbone>,
	Joghurt <http://gps.dg4sfw.de>,
	Roussillat <http://www.webazar.org/tomtom/tripmaster.php?lang=uk>.

 * Compile with ARM cross-compiler:
 *    gcc -Wall -fPIC -ldl -shared -o devshare.so devshare.c
 */

#define DEBUG 0
// 0 = no debug
// 1 = debug with no time string
// 2 = debug with time string

// For NavCore 9.x or Event_Logger 9.x, both switches are needed:
#define SCREEN_GLITCH // screen issue of NavCore versions 8.35x with regular devices (or v9)
#define OLD_DEVICES // screen issue of NavCore versions 8.35x with old devices (or v9. Requires SCREEN_GLITCH)
#define SCREEN_PWR_STATE // add a delay of four seconds when suspending TomTom to allow reading the related panel splash screen

//#define WRAP_STRCPY // for debug
//#define WRAP_WRITE // for debug
//#define WRAP_DEBUG // strncmp, fopen, opendir, scandir, stat, fstat, fscanf for debug

#define DBG if(DEBUG) debug
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <dlfcn.h>
#include <stdarg.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <stropts.h>
#include <stdarg.h>
#include <time.h>
#include <sys/mman.h>
#include <linux/fb.h>
#include "watchdog.h"

static int fd_dev_gps1=-1;
static int fd_dev_gps2=-1;
static int fd_dev_ts1=-1;
static int fd_dev_ts2=-1;
static int el_nfd = -1;
static int fd_wd=-1;
int wd_timeout=0;
int wd_active=0;

#ifdef SCREEN_PWR_STATE
static int fd_displpower=-1;
#define DISPL_PWR "/sys/class/backlight/s3c/power"
#endif //SCREEN_PWR_STATE

#ifdef SCREEN_GLITCH
static int fd_dev_fb=-1;
static void * mmap_fb= (void *) -1;
static int mmap_L=0;
static int mmap_prot=PROT_NONE;
static void * mmap_ptr=NULL;
#ifdef OLD_DEVICES
static void * mmap_fbH = (void *) -1;
#endif // OLD_DEVICES
static int pid_fd=-1;
static int NoDisplay=0;
#endif //SCREEN_GLITCH

static char filename_H[ 1024 ]= "<unknown>";
static pid_t pid_H=-1;

#define DEV_WD "/dev/watchdog"
#define DEV_GPS1 "/var/run/gpspipe"
#define DEV_GPS2 "/dev/gpsdata"
#define DEV_FB "/dev/fb"
#define DEV_TS1 "/dev/ts"
#define DEV_TS2 "/dev/input/event0"
#define PID_PATHNAME "/var/run/el_runpid"
#define DEV_PATHNAME	"/tmp/el_np"
#define DEV_SYSID "/mnt/flash/sysfile/id"

#define _HIDDEN_ \
	__attribute__( ( visibility( "hidden" ) ) )

#define _CONSTRUCTOR_ \
	__attribute__( ( constructor ) ) _HIDDEN_

#define _DESTRUCTOR_ \
	__attribute__( ( destructor ) ) _HIDDEN_

/* Function prototypes */
static int CheckPid(char * Label, int loopFlag);
static int (*org_close)( int fd ) = NULL;
static int (*org_open)( const char *pathname, int flags, ... ) = NULL;
static ssize_t (*org_read)( int fd, void *buf, size_t count ) = NULL;
static int (*org_ioctl)(int fd, unsigned long int request, ... ) = NULL;
#ifdef WRAP_STRCPY
static char *(*org_strcpy)(char *dest, const char *src) = NULL;
#endif // WRAP_STRCPY
#ifdef WRAP_DEBUG
#include <dirent.h>
static int (*org_strncmp)(const char *s1, const char *s2, size_t n) = NULL;
static FILE *(*org_fopen)(const char * filename, const char * mode) = NULL;
static DIR *(*org_opendir)(const char *name) = NULL;
static int (*org_scandir)(const char *dir, struct dirent ***namelist, int (*filter)(const struct dirent *), int (*compar)(const void *, const void *)) = NULL;
static int (*org_stat)(const char *file_name, struct stat *buf) = NULL;
static int (*org_fstat)(int filedes, struct stat *buf) = NULL;
static int (*org_fscanf)(FILE * stream, const char * format, ... ) = NULL;
#endif // WRAP_DEBUG
static ssize_t (*org_write)( int fd, const void *buf, size_t count ) = NULL;
#ifdef SCREEN_GLITCH
static void *(*org_mmap)(void  *start,  size_t length, int prot, int flags, int fd, off_t offset) = NULL;
static int (*org_munmap)(void *addr, size_t len) = NULL;
static void * (*org_memcpy)(void * s1, const void * s2, size_t n) = NULL;
#endif //SCREEN_GLITCH
void debug( char *fmt, ... );
void wrapper_init( void );

_CONSTRUCTOR_ void wrapper_init( void );
//_DESTRUCTOR_ void wrapper_uninit( void );

void wrapper_init( void ) {
	char * s;
	unsetenv("LD_PRELOAD");
	if ((s = getenv("WD_TO")) != NULL)
		wd_timeout=atoi(s);
	org_open	= dlsym( RTLD_NEXT, "open" );
	org_close	= dlsym( RTLD_NEXT, "close" );
	org_read	= dlsym( RTLD_NEXT, "read" );
	org_ioctl	= dlsym( RTLD_NEXT, "ioctl" );
#ifdef SCREEN_GLITCH
	org_mmap	= dlsym( RTLD_NEXT, "mmap" );
	org_munmap	= dlsym( RTLD_NEXT, "munmap" );
	org_memcpy	= dlsym( RTLD_NEXT, "memcpy" );
#endif //SCREEN_GLITCH
#ifdef WRAP_STRCPY
  org_strcpy= dlsym( RTLD_NEXT, "strcpy" );
#endif //WRAP_STRCPY
#ifdef WRAP_DEBUG
  org_strncmp= dlsym( RTLD_NEXT, "strncmp" );
  org_fopen= dlsym( RTLD_NEXT, "fopen" );
  org_opendir= dlsym( RTLD_NEXT, "opendir" );
  org_scandir= dlsym( RTLD_NEXT, "scandir" );
  org_stat= dlsym( RTLD_NEXT, "stat" );
  org_fstat= dlsym( RTLD_NEXT, "fstat" );
  org_fscanf= dlsym( RTLD_NEXT, "fscanf" );
#endif //WRAP_STST
	org_write	= dlsym( RTLD_NEXT, "write" );
	pid_H = getpid();
	filename_H[ readlink( "/proc/self/exe", filename_H, sizeof( filename_H ) ) ] = 0;
  DBG( "Installing wrapper: debug = %d\n", DEBUG);
	DBG( "filename: %s\n", filename_H );
	DBG( "pid: %d, wd_timeout: %d\n", pid_H, wd_timeout );
	DBG( "_______________________________________________\n" );
}

void debug( char *fmt, ... ) {
#ifdef DEBUG
		va_list ap;
#if DEBUG>=2
		time_t 		p_Current_Time;
		struct tm * 	p_TimeInfo;

		p_Current_Time	= time	( NULL );
		p_TimeInfo		= localtime	( &p_Current_Time );

		printf("DEVSHARE - %d, %s, %02d:%02d:%02d: ", pid_H, filename_H, p_TimeInfo->tm_hour, p_TimeInfo->tm_min, p_TimeInfo->tm_sec);
#endif // DEBUG>=2
		va_start( ap, fmt );
		vprintf( fmt, ap );
		va_end( ap );
#endif // DEBUG
}

int close( int fd ) {
	 int el_err=errno;
   if(org_close==NULL) {
     org_close=dlsym( RTLD_NEXT, "close" );
     DBG("close: installing wrapper: org_close = %p\n", org_close);
   }
  if (fd==-1) {
		DBG( "close: wrong argument fd: %d\n", fd);
		errno=el_err;
		return org_close( fd );
	}
#ifdef SCREEN_PWR_STATE
	if (fd == fd_displpower ) {
			char DisplPwrState[10];
			int ret;

		org_close(fd);
		fd_displpower = -1;
		fd = org_open( DISPL_PWR, O_RDONLY|O_NONBLOCK|O_NOCTTY );
		if ((ret=org_read( fd, DisplPwrState, sizeof(DisplPwrState) - 1 )) > 0) {
			DBG( "close: read display power=%c\n", DisplPwrState[0] );
			if (DisplPwrState[0]=='4') { // Display is off (typical of starting a suspend operation...)
				org_ioctl(fd_wd, WDIOC_KEEPALIVE, 0);
				org_close(fd); fd = org_open( DISPL_PWR, O_WRONLY|O_NONBLOCK|O_NOCTTY ); // reopen write
				write(fd, "0", 1); // switch display on again to wait for pid to finish
				DBG( "close: checking pid before suspending...\n");
				CheckPid("suspend", 1);
				DBG( "close: pid checked. Suspending.\n");
				write(fd, "4", 1); // switch display off again
			}
			else
				DBG( "close: display power is %c, different from 4=off\n", DisplPwrState[0] ); // Will not be switched off...
		}
		else
			DBG( "close: failed to read display power: return code=%d\n", ret);
	}
#endif //SCREEN_PWR_STATE
	if ( ( fd == fd_dev_gps1 ) || ( fd == fd_dev_gps2 ) ) {
		if ( el_nfd != -1 )
			org_close(el_nfd);
		el_nfd = -1;
		DBG( "close: closed fd: %d. %s - %d\n", el_nfd, strerror(errno), errno );
		fd_dev_gps1 = -1;
		fd_dev_gps2 = -1;
	}
#ifdef SCREEN_GLITCH
	if ( fd == fd_dev_fb )
		fd_dev_fb = -1;
#endif //SCREEN_GLITCH
	if ( fd == fd_wd ) {
		wd_active=0;
		fd_wd = -1;
	}
	if ( fd == fd_dev_ts1 )
		fd_dev_ts1 = -1;
	if ( fd == fd_dev_ts2 )
		fd_dev_ts2 = -1;
	errno=el_err;
	return org_close( fd );
}

int open( const char *pathname, int flags, ... ) {
  va_list ap;
  void *argp = NULL;
	int fd=-1;
	int el_err=errno;

	if(org_open==NULL) {
		org_open=dlsym( RTLD_NEXT, "open" );
		DBG("open: installing wrapper: org_open = %p\n", org_open);
	}

  va_start(ap, flags);
  argp = (void *)va_arg(ap, void *);
  va_end(ap);

	errno=el_err;
	if ( ( fd = org_open( pathname, flags, argp ) ) != -1 ) {
			el_err=errno;
			DBG( "open: %s, %d\n", pathname, fd );
			if ( ! strcmp( pathname, DEV_GPS1 ) ) {
				DBG( "open: open( \"%s\", ... ) detected for GPS, fd: %d\n", pathname, fd );
				wd_active=0;

				if ( fd_dev_gps1 == -1 )
					fd_dev_gps1 = fd;
				if ( fd_dev_gps2 != -1 )
					fd_dev_gps2 = -1; // pipe has precedence
				if ( el_nfd == -1 )
					el_nfd = org_open( DEV_PATHNAME, O_WRONLY|O_NONBLOCK|O_NOCTTY );
				DBG( "open: opened fd: %d. %s - %s - %d\n", el_nfd, DEV_PATHNAME, strerror(errno), errno );
			} else
					if ( ! strcmp( pathname, DEV_GPS2 ) ) {
						DBG( "open: open( \"%s\", ... ) detected for GPS, fd: %d\n", pathname, fd );
						wd_active=0;

						if ( ( fd_dev_gps1 == -1 ) && ( fd_dev_gps2 == -1 ) ) // pipe has precedence
							fd_dev_gps2 = fd;
			
						if ( el_nfd == -1 )
							el_nfd = org_open( DEV_PATHNAME, O_WRONLY|O_NONBLOCK|O_NOCTTY );
		
						if ( ( el_nfd = org_open( DEV_PATHNAME, O_WRONLY|O_NONBLOCK|O_NOCTTY ) ) != -1 ) {
							DBG( "open: successfully opened fd for GPS: %d. %s - %s - %d\n", el_nfd, DEV_PATHNAME, strerror(errno), errno );
						} else
								DBG( "open: failed opening fd for GPS: %d. %s - %s - %d\n", el_nfd, DEV_PATHNAME, strerror(errno), errno );
					}
					else
#ifdef SCREEN_PWR_STATE
						if ( ( ! strcmp( pathname, DISPL_PWR ) ) && ((flags&O_WRONLY)>0) ) { // going to change the display power state
							DBG( "open: open( \"%s\", ... ) detected a change of display power state, fd: %d\n", pathname, fd );
							fd_displpower = fd;
						}
#endif //SCREEN_PWR_STATE
#ifdef SCREEN_GLITCH
					else
						if ( ! strcmp( pathname, DEV_FB ) ) {
							DBG( "open: open( \"%s\", ... ) detected for mmap, fd: %d\n", pathname, fd );
							if ( fd_dev_fb == -1 )
								fd_dev_fb = fd;
						}
#endif //SCREEN_GLITCH
					else
						if ( ! strcmp( pathname, DEV_WD ) ) {
							DBG( "open: open( \"%s\", ... ) detected for watchdog, fd: %d\n", pathname, fd );
							if ( fd_wd == -1 )
								fd_wd = fd;
							wd_active=1;
						}
					else
						if ( ! strcmp( pathname, DEV_TS1 ) ) {
							DBG( "open: open( \"%s\", ... ) detected for TS1, fd: %d\n", pathname, fd );
							if ( fd_dev_ts1 == -1 )
								fd_dev_ts1 = fd;		
						}
					else
						if ( ! strcmp( pathname, DEV_TS2 ) ) {
							DBG( "open: open( \"%s\", ... ) detected for TS2, fd: %d\n", pathname, fd );
							if ( fd_dev_ts2 == -1 )
								fd_dev_ts2 = fd;		
						}
			errno=el_err;
	}
	if (wd_active) {
		org_ioctl(fd_wd, WDIOC_KEEPALIVE, 0);
		DBG( "open: WDIOC_KEEPALIVE fd=%d errno=%s\n", fd_wd, strerror(errno) );
	  errno=el_err;
	}
	return fd;
}

#ifdef WRAP_WRITE
ssize_t write( int fd, const void *buf, size_t count ) {
	 int el_err=errno;
   if(org_write==NULL) {
     org_write=dlsym( RTLD_NEXT, "write" );
     DBG("write: installing wrapper: org_write = %p\n", org_write);
   }
   if ( (fd != -1) && ( fd == fd_dev_fb ) ) {
   		DBG( "write> fd=%d, buffer=%p, count=%d\n", fd, (void *) buf, (size_t) count );
	 		errno=0;
	 		return( (ssize_t) count);
	 }
   DBG( "write: fd=%d, buffer=%p, count=%d\n", fd, (void *) buf, (size_t) count );
	 errno=el_err;
   return ( org_write( fd, buf, count ) );
 }
#endif // WRAP_WRITE

ssize_t read( int fd, void *buf, size_t count ) {
	int el_err=errno;
	ssize_t size;
   if(org_read==NULL) {
     org_read=dlsym( RTLD_NEXT, "read" );
     DBG("read: installing wrapper: org_read = %p\n", org_read);
   }
#ifdef SCREEN_GLITCH
   if ( (fd != -1) && ( fd == fd_dev_fb ) ) {
   		DBG( "read> fd=%d, buffer=%p, count=%d\n", fd, (void *) buf, (size_t) count );
	 }
	 else
#endif //SCREEN_GLITCH
	   DBG( "read: fd=%d, buffer=%p, count=%d\n", fd, (void *) buf, (size_t) count );
	errno=el_err;
	size = org_read( fd, buf, count );
	el_err=errno;
	if ( ( ( fd == fd_dev_gps1 ) || ( fd == fd_dev_gps2 ) ) && (size>0) ) {
		//for (i=0;i<2;i++) {
			if ( el_nfd == -1 ) {
					if ( ( el_nfd = org_open( DEV_PATHNAME, O_WRONLY|O_NONBLOCK|O_NOCTTY ) ) != -1 ) {
						DBG( "read: successfully opened fd: %d. %s - %s - %d\n", el_nfd, DEV_PATHNAME, strerror(errno), errno );
					} else
						DBG( "read: failed opening fd: %d. %s - %s - %d\n", el_nfd, DEV_PATHNAME, strerror(errno), errno );
			}
			if ( el_nfd != -1 ) {
				if (org_write( el_nfd, buf, size ) == -1) {
					DBG( "read: write failed\n");
					org_close(el_nfd);
					el_nfd = -1;
				}
				//else
					//break;
			} else
					DBG( "read: file not opened\n");
		//}
		DBG( "%.*s", size, (char *) buf );
	}
#ifdef SCREEN_GLITCH
	else
	if ( ( ( fd == fd_dev_ts1 ) || ( fd == fd_dev_ts2 ) ) && (size>0) ) {
		switch (CheckPid("read", 0)) {
			case 0:
				errno=0;
		  	return 0; // no data returned
				break;
			case 1:
#ifdef OLD_DEVICES
	 			org_memcpy(mmap_fbH, mmap_fb, mmap_L);
#endif // OLD_DEVICES
			  errno=el_err;
				return org_read( fd, buf, count );
				break;
			case 2:
				break;
			}
	}
#endif //SCREEN_GLITCH
	errno=el_err;
	return size;
 }

#ifdef SCREEN_GLITCH
void *mmap (void  *start,  size_t length, int prot, int flags, int fd, off_t offset) {
   void * fb;
   int el_err=errno;
   
   if(org_mmap==NULL) {
     org_mmap=dlsym( RTLD_NEXT, "mmap" );
     DBG("mmap: installing wrapper: org_mmap = %p\n", org_mmap);
   }
   if (fd==fd_dev_fb) {
#ifdef OLD_DEVICES
	   mmap_fbH = org_mmap(start, length, prot, flags, fd, offset);
	   fb = malloc(length);
#else
	   fb = org_mmap(start, length, prot, flags, fd, offset);
#endif // OLD_DEVICES
		 mmap_L	= length;
		 mmap_fb = fb;
		 mmap_prot= prot;
		 mmap_ptr  = mmap_fb + mmap_L;
     DBG("mmap: mapping framebuffer: %d, %p, %p\n", mmap_L, mmap_fb, mmap_ptr);
   }
	   else
	   	 fb = org_mmap(start, length, prot, flags, fd, offset);
   DBG("mmap: %p, %p, %d, %d, %d, %d, %d.\n", fb, start, length, prot, flags, fd, (int) offset);
	 errno=el_err;
   return fb;
 }

void *memcpy(void * s1, const void * s2, size_t n) {
	 int el_err=errno;
   if(org_memcpy==NULL) {
     org_memcpy=dlsym( RTLD_NEXT, "memcpy" );
     DBG("memcpy: installing wrapper: org_memcpy = %p\n", org_memcpy);
   }
	 if ((s1>=mmap_fb) && (s1<mmap_ptr)) {
	 		void * ret;
			DBG( "memcpy: s1=%p, s2=%p, len=%d\n", (void *) s1, (void *) s2, (size_t) n );
			switch (CheckPid("memcpy", 0)) {
				case 0:
		  		NoDisplay=2;
#ifdef OLD_DEVICES
					errno=el_err;
		  		return org_memcpy(s1, s2, n);
#else // OLD_DEVICES
					errno=0;
			  	return(s1);
#endif // OLD_DEVICES
					break;
				case 1:
					errno=el_err;
	  			ret=org_memcpy(s1, s2, n);
#ifdef OLD_DEVICES
					el_err=errno;
	 				org_memcpy(mmap_fbH, mmap_fb, mmap_L);
					errno=el_err;
#endif // OLD_DEVICES
	  			return ret;
					break;
				case 2:
					break;
				}
		 errno=el_err;
		 ret=org_memcpy(s1, s2, n);
#ifdef OLD_DEVICES
		 el_err=errno;
		 org_memcpy(mmap_fbH, mmap_fb, mmap_L);
		 errno=el_err;
#endif // OLD_DEVICES
		 return ret;
	 }
	 errno=el_err;
   return(org_memcpy(s1, s2, n));
 }

int ioctl(int fd, unsigned long int request, ... ) {
  void *argp = NULL;
  va_list ap;

	int el_err=errno;
	if(org_ioctl==NULL) {
		org_ioctl=dlsym( RTLD_NEXT, "ioctl" );
		DBG("ioctl: installing wrapper: org_ioctl = %p\n", org_ioctl);
	}

  va_start(ap, request);
  argp = (void *)va_arg(ap, void *);
  va_end(ap);

	if ( (fd != -1) && ( fd == fd_wd ) && (request == WDIOC_SETTIMEOUT) && (wd_timeout > 0) ) {
		int *p = argp;
		int ret;
    DBG("ioctl> fd=%d. request=WDIOC_SETTIMEOUT. NavCore default timeout=%d. Requested timeout=%d...\n", fd, *p, wd_timeout);
		org_ioctl(fd, WDIOC_KEEPALIVE, 0);
		while ((ret=org_ioctl(fd, WDIOC_SETTIMEOUT, &wd_timeout))== -1)
			if (--wd_timeout == 0)
				break;
    el_err=errno;
    DBG("ioctl>    ...Timeout set to %d. Return code=%d. Errno=%s\n", wd_timeout, ret, ((ret==-1)?strerror(errno):"none") );
    errno=el_err;
		return(ret);
	}

#ifdef SCREEN_GLITCH
	if ( (fd != -1) && ( fd == fd_dev_fb ) && (request == FBIOPAN_DISPLAY) ) {
    DBG("ioctl> fd=%d request=%d argp=%p\n", fd, (int)request, (void *)argp);
		switch (CheckPid("ioctl", 0)) {
			case 0:
				NoDisplay=1;
#ifdef OLD_DEVICES
				( (struct fb_var_screeninfo *) argp ) -> yoffset=0; // force always displaying the first buffer
	    	errno=el_err;
				return (org_ioctl(fd, request, argp));
#endif // OLD_DEVICES
				break;
			case 1:
#ifdef OLD_DEVICES
				org_memcpy(mmap_fbH, mmap_fb, mmap_L);
#endif // OLD_DEVICES
    		errno=el_err;
			  return (org_ioctl(fd, request, argp));
				break;
			case 2:
				break;
			}
#ifdef OLD_DEVICES
  	org_memcpy(mmap_fbH, mmap_fb, mmap_L);
#endif // OLD_DEVICES
  }
#endif //SCREEN_GLITCH
  DBG("ioctl: fd=%d request=%d argp=%p\n", fd, (int)request, (void *)argp);
	errno=el_err;
  return (org_ioctl(fd, request, argp));
}

int munmap(void *addr, size_t len) {
	int el_err=errno;
	if(org_munmap==NULL) {
		org_munmap=dlsym( RTLD_NEXT, "munmap" );
		DBG("munmap: installing wrapper: org_munmap = %p\n", org_munmap);
	}
	if ( ( addr == mmap_fb ) && ( len == mmap_L ) ) {
    DBG("munmap: unmapping framebuffer: %d, %p, %p\n", mmap_L, mmap_fb, mmap_ptr);
		mmap_fb = (void *) -1;
		mmap_L = 0;
		mmap_prot = PROT_NONE;
		mmap_ptr = NULL;
#ifdef OLD_DEVICES
   	free(mmap_fb);
	  org_memcpy(mmap_fbH, mmap_fb, mmap_L);
#endif // OLD_DEVICES
  }
  DBG("munmap: %p, %d\n", addr, len);
	errno=el_err;
	return org_munmap( addr, len );
}
#endif // SCREEN_GLITCH

#ifdef WRAP_STRCPY
 char *strcpy(char *dest, const char *src) {
	 int el_err=errno;
   if(org_strcpy==NULL) {
     org_strcpy=dlsym( RTLD_NEXT, "strcpy" );
     DBG("strcpy: installing wrapper: org_strcpy = %p\n", org_strcpy);
   }
   DBG("strcpy: src=<%s>\n",src);
	 errno=el_err;
   return(org_strcpy(dest,src));
 }
#endif // WRAP_STRCPY

#ifdef WRAP_DEBUG
int strncmp(const char *s1, const char *s2, size_t n) {
	 int el_err=errno;
   if(org_strncmp==NULL) {
     org_strncmp=dlsym( RTLD_NEXT, "strncmp" );
     DBG("strncmp: installing wrapper: org_strncmp = %p\n", org_strncmp);
   }
   DBG("strncmp: s1=<%*s> s2=<%*s> bytes=<%d>\n",n, s1, n, s2, n);
	 errno=el_err;
   return(org_strncmp(s1, s2, n));
}

FILE *fopen(const char * filename, const char * mode) {
	 int el_err=errno;
   if(org_fopen==NULL) {
     org_fopen=dlsym( RTLD_NEXT, "fopen" );
     DBG("fopen: installing wrapper: org_fopen = %p\n", org_fopen);
   }
   DBG("fopen: filename=<%s>\n",filename);
	 errno=el_err;
   return(org_fopen(filename, mode));
}

DIR *opendir(const char *name) {
	 int el_err=errno;
   if(org_opendir==NULL) {
     org_opendir=dlsym( RTLD_NEXT, "opendir" );
     DBG("opendir: installing wrapper: org_opendir = %p\n", org_opendir);
   }
   DBG("opendir: dir=<%s>\n",name);
	 errno=el_err;
   return(org_opendir(name));
}

int scandir(const char *dir, struct dirent ***namelist,
			int (*filter)(const struct dirent *),
			int (*compar)(const void *, const void *)) {
	 int el_err=errno;
   if(org_scandir==NULL) {
     org_scandir=dlsym( RTLD_NEXT, "scandir" );
     DBG("scandir: installing wrapper: org_scandir = %p\n", org_scandir);
   }
   DBG("scandir: dir=<%s>\n",dir);
	 errno=el_err;
   return(org_scandir(dir,namelist,filter,compar));
}

int stat(const char *file_name, struct stat *buf) {
	 int el_err=errno;
   if(org_stat==NULL) {
     org_stat=dlsym( RTLD_NEXT, "stat" );
     DBG("stat: installing wrapper: org_stat = %p\n", org_stat);
   }
   DBG("stat: file_name=<%s>\n",file_name);
	 errno=el_err;
   return(org_stat(file_name,buf));
}

int fstat(int filedes, struct stat *buf) {
	 int el_err=errno;
   if(org_fstat==NULL) {
     org_fstat=dlsym( RTLD_NEXT, "fstat" );
     DBG("fstat: installing wrapper: org_fstat = %p\n", org_fstat);
   }
   DBG("fstat: filedes=<%d>\n",filedes);
	 errno=el_err;
   return(org_fstat(filedes,buf));
}

int fscanf(FILE * stream, const char * format, ... ) {
  void *argp = NULL;
  va_list ap;

	int el_err=errno;
	if(org_fscanf==NULL) {
		org_fscanf=dlsym( RTLD_NEXT, "fscanf" );
		DBG("fscanf: installing wrapper: org_fscanf = %p\n", org_fscanf);
	}
  va_start(ap, format);
  argp = (void *)va_arg(ap, void *);
  va_end(ap);
	DBG("fscanf: stream=<%d> format=<%s>\n", (int)stream, format);
	errno=el_err;
  return(org_fscanf(stream, format, argp));
}
#endif // WRAP_DEBUG

int CheckPid(char * Label, int loopFlag) {
	  if ((pid_fd = org_open( PID_PATHNAME, O_RDONLY|O_NONBLOCK|O_NOCTTY )) >= 0) {
			char PidString[32]="0";
			org_read(pid_fd, PidString, sizeof(PidString) - 1);
		  org_close(pid_fd);
	    DBG("%s: pid to check: %d\n", Label, atoi(PidString));
		  if ((abs(atoi(PidString)) > 0) && (kill(abs(atoi(PidString)), 0)== -1)) {
		  	unlink(PID_PATHNAME);
      	DBG("%s: removed %s\n", Label, PID_PATHNAME);
				if (!loopFlag)
					NoDisplay=0;
				return 1; // process end
			}
		  while ( (atoi(PidString) < 0) || ( (atoi(PidString) > 0) && (loopFlag > 0) ) )
			  if ((pid_fd = org_open( PID_PATHNAME, O_RDONLY|O_NONBLOCK|O_NOCTTY )) >= 0) {
					PidString[0]='0'; PidString[1]='\0';
					org_read(pid_fd, PidString, sizeof(PidString) - 1);
				  org_close(pid_fd);
				  if ((abs(atoi(PidString)) > 0) && (kill(abs(atoi(PidString)), 0)== -1)) {
				  	unlink(PID_PATHNAME);
		      	DBG("%s: removed %s\n", Label, PID_PATHNAME);
						if (!loopFlag)
							NoDisplay=0;
						return 1; // process end
					}
		  		usleep(300000); // 3/10 sec.
	  		} else
	  				break;
	  	return 0; // no data returned (or zero returned)
	  }
		if (!loopFlag)
			NoDisplay=0;
		return 2; // cannot open
}
