/*
This file is part of Event_Logger, author Amacri - amacri@tiscali.it

Permission is hereby granted, without written agreement and without
license or royalty fees, to use, copy, modify, and distribute this
software and to distribute modified versions of this software for non
commercial purpose, provided that the above notice and the following four
paragraphs appear in all copies of this software.

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
to make a payment of any kind, except for donations to amacri. This
restriction is not limited to monetary payment - the restriction includes
any form of payment. No permission is implicitly provided to incorporate
this software on CD/DVD or other media for sale. E-commerce is explicitly
disallowed.

Donations are more than happily accepted! Event_Logger took a lot
of development time, as well as large number of testing and user
support; if you really like this software, consider sending donations;
email to amacri@tiscali.it with subject Donation to know how to perform this.
*/

#include <stdio.h>
#include <unistd.h>
#include <fcntl.h>
#include "watchdog.h"

int main(int argc, const char *argv[])
{
	int Flag_Chk=0;
	int fd=open("/dev/watchdog",O_WRONLY);

if ((argc > 1) && (strcmp(argv[1],"-c")==0))
{
     Flag_Chk=1;
     argc--;
     argv++;
}

while(1)
{
	if(fd==-1)
	{
		if (Flag_Chk==0) {
			perror("watchdog - open");
			exit(1);
		}
		else {
			close(fd);
			sleep(5);
			fd=open("/dev/watchdog",O_WRONLY);
			continue;
		}
	}
	while(1)
	{
		if (ioctl(fd, WDIOC_KEEPALIVE, 0) == -1) {
			perror("watchdog - ioctl");
			close(fd);
			sleep(5);
			fd=open("/dev/watchdog",O_WRONLY);
			continue;
		}
		//write(fd,"\0",1);
		//fsync(fd);
		sleep(5);
	}
if (Flag_Chk==0)
	exit(1);
sleep(5);
}
}
