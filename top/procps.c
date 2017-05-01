/* vi: set sw=4 ts=4: */
/*
 * Utility routines.
 *
 * Copyright 1998 by Albert Cahalan; all rights reserved.
 * Copyright (C) 2002 by Vladimir Oleynik <dzo@simtreas.ru>
 * GNU Library General Public License Version 2, or any later version
 *
 */

#include <dirent.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <asm/page.h>

#include "libbb.h"

extern procps_status_t * procps_scan(int save_user_arg0)
{
	static DIR *dir;
	struct dirent *entry;
	static procps_status_t ret_status;
	char *name;
	int n;
	char status[32];
	char buf[1024];
	FILE *fp;
	procps_status_t curstatus;
	int pid;
	long tasknice;
	int num_threads;
	struct stat sb;

	if (!dir) {
		dir = opendir("/proc");
		if(!dir)
			bb_error_msg_and_die("Can't open /proc");
	}
	for(;;) {
		if((entry = readdir(dir)) == NULL) {
			closedir(dir);
			dir = 0;
			return 0;
		}
		name = entry->d_name;
		if (!(*name >= '0' && *name <= '9'))
			continue;

		memset(&curstatus, 0, sizeof(procps_status_t));
		pid = atoi(name);
		curstatus.pid = pid;

		sprintf(status, "/proc/%d", pid);
		if(stat(status, &sb))
			continue;
		bb_getpwuid(curstatus.user, sb.st_uid, sizeof(curstatus.user));

		sprintf(status, "/proc/%d/stat", pid);

		if((fp = fopen(status, "r")) == NULL)
			continue;
		name = fgets(buf, sizeof(buf), fp);
		fclose(fp);
		if(name == NULL)
			continue;
		name = strrchr(buf, ')'); /* split into "PID (cmd" and "<rest>" */
		if(name == 0 || name[1] != ' ')
			continue;
		*name = 0;
		sscanf(buf, "%*s (%15c", curstatus.short_cmd); /* command name */
		n = sscanf(name+2,
		"%c %d "							 /* state, ppid */
		"%*s %*s %*s %*s "     /* pgrp (pgid), session (sid), tty (tty_nr), tpgid (tty_pgrp) */
		"%*s %*s %*s %*s %*s " /* flags, min_flt, cmin_flt, maj_flt, cmaj_flt */
#ifdef CONFIG_FEATURE_TOP_CPU_USAGE_PERCENTAGE
		"%lu %lu "             /* utime, stime */
#else
		"%*s %*s "             /* utime, stime */
#endif
		"%*s %*s %*s "         /* cutime, cstime, priority */
		"%ld " 								 /* nice */
		"%d %*s %*s "         /* num_threads, it_real_value, start_time */
		"%*s "                 /* vsize */
		"%ld",                 /* rss */
		curstatus.state, &curstatus.ppid,
#ifdef CONFIG_FEATURE_TOP_CPU_USAGE_PERCENTAGE
		&curstatus.utime, &curstatus.stime,
#endif
		&tasknice,
		&num_threads,
		&curstatus.rss);
#ifdef CONFIG_FEATURE_TOP_CPU_USAGE_PERCENTAGE
		if(n != 7)
#else
		if(n != 5)
#endif
			continue;

		if (curstatus.rss == 0 && curstatus.state[0] != 'Z')
			curstatus.state[1] = 'W';
		else
			curstatus.state[1] = ' ';
		if (tasknice < 0)
			curstatus.state[2] = '<';
		else if (tasknice > 0)
			curstatus.state[2] = 'N';
		else
			curstatus.state[2] = ' ';

		//sprintf(curstatus.state+3, "%d", num_threads);

#ifdef PAGE_SHIFT
		curstatus.rss <<= (PAGE_SHIFT - 10);     /* 2**10 = 1kb */
#else
		curstatus.rss *= (getpagesize() >> 10);     /* 2**10 = 1kb */
#endif

		if(save_user_arg0) {
			sprintf(status, "/proc/%d/cmdline", pid);
			if((fp = fopen(status, "r")) == NULL)
				continue;
			if((n=fread(buf, 1, sizeof(buf)-1, fp)) > 0) {
				if(buf[n-1]=='\n')
					buf[--n] = 0;
				name = buf;
				while(n) {
					if(((unsigned char)*name) < ' ')
						*name = ' ';
					name++;
					n--;
				}
				*name = 0;
				if(buf[0])
					curstatus.cmd = strdup(buf);
				/* if NULL it work true also */
			}
			fclose(fp);
		}
		return memcpy(&ret_status, &curstatus, sizeof(procps_status_t));
	}
}

/* END CODE */
/*
Local Variables:
c-file-style: "linux"
c-basic-offset: 4
tab-width: 4
End:
*/
